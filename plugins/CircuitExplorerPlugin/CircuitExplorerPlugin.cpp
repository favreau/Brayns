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
#include "io/BrickLoader.h"
#include "io/CircuitLoader.h"
#include "io/CircuitSimulationHandler.h"
#include "io/MorphologyLoader.h"
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

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Random.h>
#include <CGAL/Triangulation_vertex_base_with_info_3.h>
#include <CGAL/convex_hull_3.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Polyhedron_3<K> Polyhedron_3;
typedef K::Point_3 Point_3;
typedef K::Segment_3 Segment_3;
typedef K::Triangle_3 Triangle_3;

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
                    ([& scene = _scene, &params = _synapseAttributes ] {
                        return std::make_unique<SynapseLoader>(scene, params);
                    }));

    REGISTER_LOADER(MorphologyLoader,
                    ([& scene = _scene, &params = _morphologyAttributes ] {
                        return std::make_unique<MorphologyLoader>(scene,
                                                                  params);
                    }));

    REGISTER_LOADER(CircuitLoader,
                    ([& scene = _scene, &params = _parametersManager,
                      &circuitAttributes = _circuitAttributes,
                      &morphologyAttributes = _morphologyAttributes] {
                        return std::make_unique<CircuitLoader>(
                            scene, params.getApplicationParameters(),
                            params.getAnimationParameters(), circuitAttributes,
                            morphologyAttributes);
                    }));

    if (actionInterface)
    {
        actionInterface->registerNotification<MaterialDescriptor>(
            "setMaterial",
            [&](const MaterialDescriptor& param) { _setMaterial(param); });

        actionInterface->registerNotification<MaterialsDescriptor>(
            "setMaterials",
            [&](const MaterialsDescriptor& param) { _setMaterials(param); });

        actionInterface->registerNotification<SynapseAttributes>(
            "setSynapsesAttributes", [&](const SynapseAttributes& param) {
                _setSynapseAttributes(param);
            });

        actionInterface->registerNotification<LoadModelFromCache>(
            "loadModelFromCache", [&](const LoadModelFromCache& param) {
                _loadModelFromCache(param);
            });

        actionInterface->registerNotification<SaveModelToCache>(
            "saveModelToCache",
            [&](const SaveModelToCache& param) { _saveModelToCache(param); });

        actionInterface->registerNotification<MorphologyAttributes>(
            "setMorphologyAttributes", [&](const MorphologyAttributes& param) {
                _setMorphologyAttributes(param);
            });

        actionInterface->registerNotification<CircuitAttributes>(
            "setCircuitAttributes", [&](const CircuitAttributes& param) {
                _setCircuitAttributes(param);
            });

        actionInterface->registerNotification<ConnectionsPerValue>(
            "setConnectionsPerValue", [&](const ConnectionsPerValue& param) {
                _setConnectionsPerValue(param);
            });
    }
}

void CircuitExplorerPlugin::preRender()
{
    if (_dirty)
        _scene.markModified();
    _dirty = false;
}

void CircuitExplorerPlugin::_setMaterial(const MaterialDescriptor& md)
{
    auto modelDescriptor = _scene.getModel(md.modelId);
    if (modelDescriptor)
        try
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
                material->setCastSimulationData(md.simulationDataCast);
                material->setShadingMode(
                    static_cast<brayns::MaterialShadingMode>(md.shadingMode));
                material->commit();

                _dirty = true;
            }
            else
                PLUGIN_INFO << "Material " << md.materialId
                            << " is not registered in model " << md.modelId
                            << std::endl;
        }
        catch (const std::runtime_error& e)
        {
            PLUGIN_INFO << e.what() << std::endl;
        }
    else
        PLUGIN_INFO << "Model " << md.modelId << " is not registered"
                    << std::endl;
}

void CircuitExplorerPlugin::_setMaterials(const MaterialsDescriptor& md)
{
    for (const auto modelId : md.modelIds)
    {
        auto modelDescriptor = _scene.getModel(modelId);
        if (modelDescriptor)
        {
            size_t id = 0;
            for (const auto materialId : md.materialIds)
            {
                try
                {
                    auto material =
                        modelDescriptor->getModel().getMaterial(materialId);
                    if (material)
                    {
                        const size_t index = id * 3;
                        material->setDiffuseColor(
                            {md.diffuseColors[index],
                             md.diffuseColors[index + 1],
                             md.diffuseColors[index + 2]});
                        material->setSpecularColor(
                            {md.specularColors[index],
                             md.specularColors[index + 1],
                             md.specularColors[index + 2]});

                        material->setSpecularExponent(md.specularExponents[id]);
                        material->setReflectionIndex(md.reflectionIndices[id]);
                        material->setOpacity(md.opacities[id]);
                        material->setRefractionIndex(md.refractionIndices[id]);
                        material->setEmission(md.emissions[id]);
                        material->setGlossiness(md.glossinesses[id]);
                        material->setCastSimulationData(
                            md.simulationDataCasts[id]);
                        material->setShadingMode(
                            static_cast<brayns::MaterialShadingMode>(
                                md.shadingModes[id]));
                        material->commit();
                    }
                }
                catch (const std::runtime_error& e)
                {
                    PLUGIN_INFO << e.what() << std::endl;
                }
                ++id;
            }
            _dirty = true;
        }
        else
            PLUGIN_INFO << "Model " << modelId << " is not registered"
                        << std::endl;
    }
}

void CircuitExplorerPlugin::_setSynapseAttributes(
    const SynapseAttributes& param)
{
    try
    {
        _synapseAttributes = param;
        SynapseLoader loader(_scene, _synapseAttributes);
        brayns::Vector3fs colors;
        for (const auto& htmlColor : _synapseAttributes.htmlColors)
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
            loader.importSynapsesFromGIDs(_synapseAttributes, colors);

        _scene.addModel(modelDescriptor);

        PLUGIN_INFO << "Synapses successfully added for GID "
                    << _synapseAttributes.gid << std::endl;
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

void CircuitExplorerPlugin::_setMorphologyAttributes(
    const MorphologyAttributes& morphologyAttributes)
{
    _morphologyAttributes = morphologyAttributes;
    PLUGIN_INFO << "Morphology attributes successfully set" << std::endl;
    _dirty = true;
}

void CircuitExplorerPlugin::_setCircuitAttributes(
    const CircuitAttributes& circuitAttributes)
{
    _circuitAttributes = circuitAttributes;
    PLUGIN_INFO << "Circuit attributes successfully set" << std::endl;
    _dirty = true;
}

void CircuitExplorerPlugin::_loadModelFromCache(
    const LoadModelFromCache& loadModel)
{
    try
    {
        BrickLoader brickLoader(_scene);
        auto modelDescriptor =
            brickLoader.importFromFile(loadModel.path, 0, brayns::NO_MATERIAL);
        if (modelDescriptor)
        {
            modelDescriptor->setName(loadModel.name);
            _scene.addModel(modelDescriptor);
        }
        _dirty = true;
    }
    catch (const std::runtime_error& e)
    {
        PLUGIN_ERROR << e.what() << std::endl;
    }
}

void CircuitExplorerPlugin::_saveModelToCache(const SaveModelToCache& saveModel)
{
    auto modelDescriptor = _scene.getModel(saveModel.modelId);
    if (modelDescriptor)
    {
        BrickLoader brickLoader(_scene);
        brickLoader.exportToFile(modelDescriptor, saveModel.path);
    }
    else
        PLUGIN_ERROR << "Model " << saveModel.modelId << " is not registered"
                     << std::endl;
}

void CircuitExplorerPlugin::_setConnectionsPerValue(
    const ConnectionsPerValue& cpv)
{
    auto handler = _scene.getUserDataHandler();
    if (!handler)
    {
        BRAYNS_ERROR << "Scene has not user data handler" << std::endl;
        return;
    }

    std::map<size_t, std::vector<brayns::Vector4f>> connections;
    const float OFFSET_MAGIC = 1e6f;

    auto modelDescriptor = _scene.getModel(cpv.modelId);
    if (modelDescriptor)
    {
        auto& model = modelDescriptor->getModel();
        for (const auto& spheres : model.getSpheres())
        {
            for (const auto& s : spheres.second)
            {
                const float* data =
                    static_cast<float*>(handler->getFrameData(cpv.frame));

                const uint64 index =
                    (uint64)(s.texture_coords.x() * OFFSET_MAGIC) << 32 |
                    (uint32)(s.texture_coords.y() * OFFSET_MAGIC);
                const float value = data[index];
                if (abs(value - cpv.value) < cpv.epsilon)
                    connections[spheres.first].push_back(
                        {s.center.x(), s.center.y(), s.center.z(), s.radius});
            }
        }

        if (!connections.empty())
        {
            auto connectionModel = _scene.createModel();
            bool addModel = false;

            for (const auto& connection : connections)
            {
                connectionModel->createMaterial(connection.first,
                                                std::to_string(
                                                    connection.first));

                std::vector<Point_3> points;
                for (const auto& c : connection.second)
                    points.push_back({c.x(), c.y(), c.z()});

                CGAL::Object obj;
                // compute convex hull of non-collinear points
                CGAL::convex_hull_3(points.begin(), points.end(), obj);
                if (const Polyhedron_3* poly =
                        CGAL::object_cast<Polyhedron_3>(&obj))
                {
                    PLUGIN_INFO << "The convex hull contains "
                                << poly->size_of_vertices() << " vertices"
                                << std::endl;

                    for (auto eit = poly->edges_begin();
                         eit != poly->edges_end(); ++eit)
                    {
                        Point_3 a = eit->vertex()->point();
                        Point_3 b = eit->opposite()->vertex()->point();
                        const brayns::Cylinder cylinder(
                            brayns::Vector3f(a.x(), a.y(), a.z()),
                            brayns::Vector3f(b.x(), b.y(), b.z()), 2.f);
                        connectionModel->addCylinder(connection.first,
                                                     cylinder);
                        addModel = true;
                    }
                }
                else
                    PLUGIN_ERROR << "something else" << std::endl;

                if (addModel)
                {
                    auto modelDesc = std::make_shared<brayns::ModelDescriptor>(
                        std::move(connectionModel),
                        "Connection for value " + std::to_string(cpv.value));

                    _scene.addModel(modelDesc);
                    _dirty = true;
                }
            }
        }
        else
            PLUGIN_INFO << "No connections added for value "
                        << std::to_string(cpv.value) << std::endl;
    }
    else
        PLUGIN_INFO << "Model " << cpv.modelId << " is not registered"
                    << std::endl;
}

extern "C" brayns::ExtensionPlugin* brayns_plugin_create(brayns::PluginAPI* api,
                                                         int argc, char** argv)
{
    return new CircuitExplorerPlugin(api->getScene(),
                                     api->getParametersManager(),
                                     api->getActionInterface(), argc, argv);
}
