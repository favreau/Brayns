/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
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

#include "SolRScene.h"
#include "SolRMaterial.h"
#include "SolRModel.h"
#include "SolRVolume.h"

#include <brayns/common/ImageManager.h>
#include <brayns/common/Transformation.h>
#include <brayns/common/light/DirectionalLight.h>
#include <brayns/common/light/PointLight.h>
#include <brayns/common/log.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/simulation/AbstractSimulationHandler.h>

#include <brayns/parameters/GeometryParameters.h>
#include <brayns/parameters/ParametersManager.h>
#include <brayns/parameters/SceneParameters.h>

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#else
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif // USE_OPENCL
#endif // USE_CUDA

namespace brayns
{
SolRScene::SolRScene(solr::GPUKernel* kernel,
                     ParametersManager& parametersManager)
    : Scene(parametersManager)
    , _kernel(kernel)
{
    _backgroundMaterial = std::make_shared<SolRMaterial>(_kernel);
}

SolRScene::~SolRScene() {}

void SolRScene::commit()
{
    for (auto modelDescriptor : _modelDescriptors)
    {
        if (!modelDescriptor->getEnabled())
            continue;

        modelDescriptor->getModel().commit();
        BRAYNS_DEBUG << "Committing " << modelDescriptor->getName()
                     << std::endl;
    }

    _computeBounds();
}

bool SolRScene::commitLights()
{
    BRAYNS_WARN << "SolRModel::commitLights not properly implemented"
                << std::endl;

    const size_t materialId = 0;
    auto model = createModel();
    auto material = model->createMaterial(materialId, "Default light");
    material->setEmission(1.f);
    material->commit();

    auto id = _kernel->addPrimitive(solr::ptSphere);
    _kernel->setPrimitive(id, -10.f, 5.f, -10.f, 1.f, 0.f, 0.f, materialId);
    _kernel->setPrimitiveIsMovable(id, false);

    return true;
}

bool SolRScene::commitTransferFunctionData()
{
    BRAYNS_ERROR << "SolRModel::commitTransferFunctionData not implemented"
                 << std::endl;
    return false;
}

ModelPtr SolRScene::createModel() const
{
    return std::make_unique<SolRModel>(_kernel);
}

SharedDataVolumePtr SolRScene::createSharedDataVolume(
    const Vector3ui& /*dimensions*/, const Vector3f& /*spacing*/,
    const DataType /*type*/) const
{
    BRAYNS_ERROR << "SolRModel::buildBoundingBox not implemented" << std::endl;
    return nullptr;
}

BrickedVolumePtr SolRScene::createBrickedVolume(const Vector3ui& /*dimensions*/,
                                                const Vector3f& /*spacing*/,
                                                const DataType /*type*/) const
{
    BRAYNS_ERROR << "SolRModel::buildBoundingBox not implemented" << std::endl;
    return nullptr;
}
} // namespace brayns
