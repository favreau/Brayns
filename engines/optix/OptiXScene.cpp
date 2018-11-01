/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of Brayns <https://github.com/BlueBrain/Brayns>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "OptiXScene.h"
#include "OptiXContext.h"
#include "OptiXMaterial.h"
#include "OptiXModel.h"
#include "OptiXUtils.h"

#include <brayns/common/light/DirectionalLight.h>
#include <brayns/common/light/PointLight.h>
#include <brayns/common/log.h>
#include <brayns/common/material/Material.h>
#include <brayns/parameters/ParametersManager.h>

#include <optixu/optixu_math_stream_namespace.h>

namespace brayns
{
OptiXScene::OptiXScene(ParametersManager& parametersManager)
    : Scene(parametersManager)
    , _lightBuffer(nullptr)
{
    _backgroundMaterial = std::make_shared<OptiXMaterial>();
}

OptiXScene::~OptiXScene() {}

bool OptiXScene::commitLights()
{
    if (_lights.empty())
    {
        BRAYNS_ERROR << "No lights are currently defined" << std::endl;
        return false;
    }

    _optixLights.clear();
    for (auto light : _lights)
    {
        PointLight* pointLight = dynamic_cast<PointLight*>(light.get());
        if (pointLight != 0)
        {
            const Vector3f& position = pointLight->getPosition();
            const Vector3f& color = pointLight->getColor();
            BasicLight optixLight = {{position.x(), position.y(), position.z()},
                                     {color.x(), color.y(), color.z()},
                                     1, // Casts shadows
                                     size_t(LightType::point)};
            _optixLights.push_back(optixLight);
        }
        else
        {
            DirectionalLight* directionalLight =
                dynamic_cast<DirectionalLight*>(light.get());
            if (directionalLight)
            {
                const Vector3f& direction = directionalLight->getDirection();
                const Vector3f& color = directionalLight->getColor();
                BasicLight optixLight = {{direction.x(), direction.y(),
                                          direction.z()},
                                         {color.x(), color.y(), color.z()},
                                         1, // Casts shadows
                                         size_t(LightType::directional)};
                _optixLights.push_back(optixLight);
            }
        }
    }

    if (_lightBuffer)
        _lightBuffer->destroy();

    auto context = OptiXContext::get().getOptixContext();
    _lightBuffer = context->createBuffer(RT_BUFFER_INPUT);
    _lightBuffer->setFormat(RT_FORMAT_USER);
    _lightBuffer->setElementSize(sizeof(BasicLight));
    _lightBuffer->setSize(_optixLights.size());
    memcpy(_lightBuffer->map(), &_optixLights[0], sizeof(_optixLights));
    _lightBuffer->unmap();
    context["lights"]->set(_lightBuffer);

    return true;
}

ModelPtr OptiXScene::createModel() const
{
    return std::make_unique<OptiXModel>();
}

void OptiXScene::commit()
{
    const bool rebuildScene = isModified();
    const bool addRemoveVolumes = _commitVolumeData();

    commitTransferFunctionData();

    // copy the list to avoid locking the mutex
    ModelDescriptors modelDescriptors;
    {
        auto lock = acquireReadAccess();
        modelDescriptors = _modelDescriptors;
    }

    if (!rebuildScene && !addRemoveVolumes)
    {
        // check for dirty models aka their geometry has been altered
        bool doUpdate = false;
        for (auto& modelDescriptor : modelDescriptors)
        {
            auto& model = modelDescriptor->getModel();
            if (model.dirty())
            {
                model.commit();
                // need to continue re-adding the models to update the bounding
                // box model to reflect the new model size
                doUpdate = true;
            }
        }
        if (!doUpdate)
            return;
    }

    _activeModels.clear();

    BRAYNS_DEBUG << "Committing scene" << std::endl;
    auto context = OptiXContext::get().getOptixContext();

    // Background material
    if (!_backgroundTextureSampler)
    {
        try
        {
            auto texture =
                _backgroundMaterial->getTexture(TextureType::diffuse);
            _backgroundTextureSampler =
                OptiXContext::get().createTextureSampler(texture);

            context["envmap"]->setTextureSampler(_backgroundTextureSampler);
        }
        catch (const std::runtime_error&)
        {
        }
    }

    // Geometry
    if (_rootGroup)
        _rootGroup->destroy();

    _rootGroup = OptiXContext::get().createGroup();

    for (size_t i = 0; i < _modelDescriptors.size(); ++i)
    {
        auto& modelDescriptor = _modelDescriptors[i];
        if (!modelDescriptor->getEnabled())
            continue;

        // keep models from being deleted via removeModel() as long as we use
        // them here
        _activeModels.push_back(modelDescriptor);

        auto& impl = static_cast<OptiXModel&>(modelDescriptor->getModel());

        if (modelDescriptor->getVisible())
        {
            const auto geometryGroup = impl.getGeometryGroup();
            ::optix::Transform xform = context->createTransform();
            xform->setMatrix(false, ::optix::Matrix4x4::identity().getData(),
                             0); // TODO
            xform->setChild(geometryGroup);
            _rootGroup->addChild(xform);
            BRAYNS_DEBUG << "Group has " << geometryGroup->getChildCount()
                         << " children" << std::endl;
        }

        if (modelDescriptor->getBoundingBox())
        {
            const auto boundingBoxGroup = impl.getBoundingBoxGroup();
            const auto& modelBounds = modelDescriptor->getModel().getBounds();

            // scale and move the unit-sized bounding box geometry to the
            // model size/scale first, then apply the instance transform
            ::optix::Transform xform = context->createTransform();
            const Vector3f pos =
                modelBounds.getCenter() / modelBounds.getSize() -
                Vector3f(0.5f);
            const Vector3f size = modelBounds.getSize();
            auto m = ::optix::Matrix4x4::translate({pos.x(), pos.y(), pos.z()});
            m.scale({size.x(), size.y(), size.z()});
            xform->setMatrix(false, m.getData(), 0); // TODO

            xform->setChild(boundingBoxGroup);
            _rootGroup->addChild(xform);
        }
    }
    _computeBounds();

    BRAYNS_DEBUG << "Root has " << _rootGroup->getChildCount() << " children"
                 << std::endl;

    context["top_object"]->set(_rootGroup);
    context["top_shadower"]->set(_rootGroup);
    context->validate();
}

bool OptiXScene::commitTransferFunctionData()
{
    if (!_transferFunction.isModified())
        return false;

    BRAYNS_DEBUG << "Committing transfer function data" << std::endl;

    auto context = OptiXContext::get().getOptixContext();
    if (!_colorMapBuffer)
        _colorMapBuffer =
            context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4,
                                  _transferFunction.getDiffuseColors().size());
    uint64_t size =
        _transferFunction.getDiffuseColors().size() * 4 * sizeof(float);
    memcpy(_colorMapBuffer->map(), _transferFunction.getDiffuseColors().data(),
           size);
    _colorMapBuffer->unmap();

    size = _transferFunction.getEmissionIntensities().size() * sizeof(float);
    if (!_emissionIntensityMapBuffer)
        _emissionIntensityMapBuffer = context->createBuffer(
            RT_BUFFER_INPUT, RT_FORMAT_FLOAT3,
            _transferFunction.getEmissionIntensities().size());

    memcpy(_emissionIntensityMapBuffer->map(),
           _transferFunction.getEmissionIntensities().data(), size);
    _emissionIntensityMapBuffer->unmap();

    context["colorMap"]->setBuffer(_colorMapBuffer);
    context["emissionIntensityMap"]->setBuffer(_emissionIntensityMapBuffer);
    context["colorMapMinValue"]->setFloat(
        _transferFunction.getValuesRange().x());
    context["colorMapRange"]->setFloat(_transferFunction.getValuesRange().y() -
                                       _transferFunction.getValuesRange().x());
    context["colorMapSize"]->setUint(
        _transferFunction.getDiffuseColors().size());

    _transferFunction.resetModified();
    markModified(true);
    return true;
}

SharedDataVolumePtr OptiXScene::createSharedDataVolume(
    const Vector3ui& dimensions, const Vector3f& spacing,
    const DataType type) const
{
    return std::make_shared<OptiXVolume>(
        dimensions, spacing, type, _parametersManager.getVolumeParameters());
}

BrickedVolumePtr OptiXScene::createBrickedVolume(
    const Vector3ui& /*dimensions*/, const Vector3f& /*spacing*/,
    const DataType /*type*/) const
{
    throw std::runtime_error("OptiXScene::createBrickedVolume not implemented");
}

void OptiXScene::_commitSimulationData() {}

bool OptiXScene::_commitVolumeData()
{
    if (_optixVolume)
        _optixVolume->commit();
    return false;
}

} // namespace brayns
