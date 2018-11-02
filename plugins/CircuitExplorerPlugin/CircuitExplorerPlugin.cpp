/* Copyright (c) 2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
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

#include "CircuitExplorerPlugin.h"
#include "io/SynapseLoader.h"
#include "log.h"

#include <brayns/common/ActionInterface.h>
#include <brayns/common/Progress.h>
#include <brayns/common/geometry/Streamline.h>
#include <brayns/common/material/Material.h>
#include <brayns/common/renderer/FrameBuffer.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/parameters/ParametersManager.h>
#include <brayns/pluginapi/PluginAPI.h>

#define REGISTER_LOADER(LOADER, FUNC) \
    registry.registerLoader({std::bind(&LOADER::getSupportedDataTypes), FUNC});

CircuitExplorerPlugin::CircuitExplorerPlugin(
    brayns::Scene& scene, brayns::ParametersManager& parametersManager,
    brayns::ActionInterface* actionInterface, int /*argc*/, char** /*argv*/)
    : ExtensionPlugin()
    , _scene(scene)
    , _parametersManager(parametersManager)
{
    auto& registry = _scene.getLoaderRegistry();
    REGISTER_LOADER(SynapseLoader,
                    ([& scene = _scene, &params = _parametersManager ] {
                        return std::make_unique<SynapseLoader>(scene, params);
                    }));

    if (actionInterface)
    {
        actionInterface->registerNotification<MaterialDescriptor>(
            "material", [&](const MaterialDescriptor& param) {
                _updateMaterialFromJson(param);
            });

        actionInterface->registerNotification<SynapsesDescriptor>(
            "synapses", [&](const SynapsesDescriptor& param) {
                _updateSynapsesFromJson(param);
            });

        actionInterface->registerNotification<LoadModelFromCache>(
            "loadModelFromCache", [&](const LoadModelFromCache& param) {
                _loadModelFromCache(param);
            });

        actionInterface->registerNotification<SaveModelToCache>(
            "saveModelToCache",
            [&](const SaveModelToCache& param) { _saveModelToCache(param); });
    }
}

void CircuitExplorerPlugin::preRender()
{
    if (_dirty)
        _scene.markModified();
    _dirty = false;
}

void CircuitExplorerPlugin::_updateMaterialFromJson(
    const MaterialDescriptor& md)
{
    try
    {
        auto modelDescriptor = _scene.getModel(md.modelId);
        if (modelDescriptor)
        {
            auto material =
                modelDescriptor->getModel().getMaterial(md.materialId);
            if (material)
            {
                material->setDiffuseColor({md.diffuseColor[0],
                                           md.diffuseColor[1],
                                           md.diffuseColor[2]});
                material->setSpecularColor({md.specularColor[0],
                                            md.specularColor[1],
                                            md.specularColor[2]});

                material->setSpecularExponent(md.specularExponent);
                material->setReflectionIndex(md.reflectionIndex);
                material->setOpacity(md.opacity);
                material->setRefractionIndex(md.refractionIndex);
                material->setEmission(md.emission);
                material->setGlossiness(md.glossiness);
                material->setCastSimulationData(md.castSimulationData);

                material->setShadingMode(brayns::MaterialShadingMode::none);
                if (md.shadingMode == "diffuse")
                    material->setShadingMode(
                        brayns::MaterialShadingMode::diffuse);
                else if (md.shadingMode == "electron")
                    material->setShadingMode(
                        brayns::MaterialShadingMode::electron);

                material->commit();
            }
        }
        else
            PLUGIN_ERROR << "Model " << md.modelId << " is not registered"
                         << std::endl;
        _dirty = true;
    }
    catch (const std::runtime_error& e)
    {
        PLUGIN_ERROR << e.what() << std::endl;
    }
    catch (...)
    {
        PLUGIN_ERROR
            << "Unexpected exception occured in _updateMaterialFromJson"
            << std::endl;
    }
}

void CircuitExplorerPlugin::_updateSynapsesFromJson(
    const SynapsesDescriptor& sd)
{
    try
    {
        SynapseLoader loader(_scene, _parametersManager);
        brayns::Vector3fs colors;
        for (const auto& htmlColor : sd.htmlColors)
        {
            auto hexCode = htmlColor;
            if (hexCode.at(0) == '#')
            {
                hexCode = hexCode.erase(0, 1);
            }
            int r, g, b;
            std::istringstream(hexCode.substr(0, 2)) >> std::hex >> r;
            std::istringstream(hexCode.substr(2, 2)) >> std::hex >> g;
            std::istringstream(hexCode.substr(4, 2)) >> std::hex >> b;

            brayns::Vector3f color{r / 255.f, g / 255.f, b / 255.f};
            colors.push_back(color);
        }
        const auto modelDescriptor =
            loader.importSynapsesFromGIDs(sd.circuitConfiguration, sd.gid,
                                          colors, sd.lightEmission);

        _scene.addModel(modelDescriptor);

        PLUGIN_INFO << "Synapses successfully added for GID " << sd.gid
                    << std::endl;
        _dirty = true;
    }
    catch (const std::runtime_error& e)
    {
        PLUGIN_ERROR << e.what() << std::endl;
    }
    catch (...)
    {
        PLUGIN_ERROR
            << "Unexpected exception occured in _updateMaterialFromJson"
            << std::endl;
    }
}

void CircuitExplorerPlugin::_loadModelFromCache(
    const LoadModelFromCache& loadModel)
{
    auto model = _scene.createModel();
    auto modelDescriptor =
        std::make_shared<brayns::ModelDescriptor>(std::move(model),
                                                  loadModel.name);
    modelDescriptor->load(loadModel.filename);
    _scene.addModel(modelDescriptor);
    _dirty = true;
}

void CircuitExplorerPlugin::_saveModelToCache(const SaveModelToCache& saveModel)
{
    auto modelDescriptor = _scene.getModel(saveModel.modelId);
    modelDescriptor->save(saveModel.filename);
    _dirty = true;
}

extern "C" brayns::ExtensionPlugin* brayns_plugin_create(brayns::PluginAPI* api,
                                                         int argc, char** argv)
{
    return new CircuitExplorerPlugin(api->getScene(),
                                     api->getParametersManager(),
                                     api->getActionInterface(), argc, argv);
}
