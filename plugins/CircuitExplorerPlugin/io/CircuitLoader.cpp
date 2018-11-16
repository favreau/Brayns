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

#include "CircuitLoader.h"
#include "CircuitLoaderCommon.h"
#include "CircuitSimulationHandler.h"
#include "MorphologyLoader.h"
#include "log.h"
#include "types.h"

#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>

#include <boost/tokenizer.hpp>

#if BRAYNS_USE_ASSIMP
#include <brayns/io/MeshLoader.h>
#endif

namespace
{
std::string CIRCUIT_COLOR_SCHEME[7] = {"none",
                                       "neuron by id",
                                       "neuron by type",
                                       "neuron by layer",
                                       "neuron by mtype",
                                       "neuron by etype",
                                       "neuron by target"};
std::string CIRCUIT_ON_OFF[2] = {"off", "on"};
} // namespace

CircuitLoader::CircuitLoader(
    brayns::Scene& scene,
    const brayns::ApplicationParameters& applicationParameters,
    brayns::AnimationParameters& animationParameters,
    const CircuitAttributes& circuitAttributes,
    const MorphologyAttributes& morphologyAttributes)
    : Loader(scene)
    , _applicationParameters(applicationParameters)
    , _animationParameters(animationParameters)
    , _circuitAttributes(circuitAttributes)
    , _morphologyAttributes(morphologyAttributes)
{
}

CircuitLoader::~CircuitLoader() {}

std::set<std::string> CircuitLoader::getSupportedDataTypes()
{
    return {"BlueConfig", "BlueConfig3", "CircuitConfig", "circuit"};
}

std::vector<std::string> CircuitLoader::_getTargetsAsStrings(
    const std::string& targets) const
{
    std::vector<std::string> result;
    boost::char_separator<char> separator(",");
    boost::tokenizer<boost::char_separator<char>> tokens(targets, separator);
    for_each(tokens.begin(), tokens.end(),
             [&result](const std::string& s) { result.push_back(s); });
    return result;
}

brayns::ModelDescriptorPtr CircuitLoader::importCircuit(
    const std::string& circuitConfig)
{
    bool returnValue = true;

    std::vector<std::string> targets =
        _getTargetsAsStrings(_circuitAttributes.targets);
    brayns::ModelDescriptorPtr modelDesc;
    try
    {
        // Model (one for the whole circuit)
        auto model = _scene.createModel();

        // Open Circuit and select GIDs according to specified target
        const brion::BlueConfig bc(circuitConfig);
        const brain::Circuit circuit(bc);
        const auto circuitDensity = _circuitAttributes.density / 100.f;

        brain::GIDSet allGids;
        GIDOffsets targetGIDOffsets;
        targetGIDOffsets.push_back(0);

        strings localTargets;
        if (targets.empty())
            localTargets.push_back(bc.getCircuitTarget());
        else
            localTargets = targets;

        for (const auto& target : localTargets)
        {
            const auto targetGids =
                circuit.getRandomGIDs(circuitDensity, target,
                                      _circuitAttributes.randomSeed);
            const brayns::Matrix4fs& allTransformations =
                circuit.getTransforms(targetGids);

            brain::GIDSet gids;
            const auto& aabb = _circuitAttributes.aabb;
            brayns::Boxd boundingBox({aabb[0], aabb[1], aabb[2]},
                                     {aabb[3], aabb[4], aabb[5]});

            if (boundingBox.getSize() == brayns::Vector3f(0.f))
                gids = targetGids;
            else
            {
                auto gidIterator = targetGids.begin();
                for (size_t i = 0; i < allTransformations.size(); ++i)
                {
                    if (boundingBox.isIn(brayns::Vector3d(
                            allTransformations[i].getTranslation())))
                        gids.insert(*gidIterator);
                    ++gidIterator;
                }
            }

            if (gids.empty())
            {
                PLUGIN_ERROR << "Target " << target
                             << " does not contain any cells" << std::endl;
                continue;
            }

            PLUGIN_INFO << "Target " << target << ": " << gids.size()
                        << " cells" << std::endl;
            allGids.insert(gids.begin(), gids.end());
            targetGIDOffsets.push_back(allGids.size());
        }

        if (allGids.empty())
            return {};

        // Load simulation information from compartment report
        CompartmentReportPtr compartmentReport{nullptr};
        CircuitSimulationHandlerPtr simulationHandler{nullptr};
        if (!_circuitAttributes.report.empty())
        {
            try
            {
                auto handler = std::make_shared<CircuitSimulationHandler>(
                    _animationParameters,
                    bc.getReportSource(_circuitAttributes.report), allGids);
                _scene.setSimulationHandler(handler);

                compartmentReport = handler->getCompartmentReport();
                // Only keep simulated GIDs
                if (compartmentReport)
                {
                    allGids = compartmentReport->getGIDs();
                    simulationHandler = handler;
                }
            }
            catch (const std::exception& e)
            {
                PLUGIN_ERROR << e.what() << std::endl;
            }
        }

        const brayns::Matrix4fs& transformations =
            circuit.getTransforms(allGids);
        _logLoadedGIDs(allGids);

        const auto layerIds = _populateLayerIds(bc, allGids);
        const auto& electrophysiologyTypes =
            circuit.getElectrophysiologyTypes(allGids);
        const auto& morphologyTypes = circuit.getMorphologyTypes(allGids);

        // Import meshes
#if BRAYNS_USE_ASSIMP
        const auto& meshedMorphologiesFolder = _circuitAttributes.meshFolder;
        if (!meshedMorphologiesFolder.empty())
            returnValue =
                returnValue &&
                _importMeshes(*model, allGids, transformations,
                              targetGIDOffsets, layerIds, morphologyTypes,
                              electrophysiologyTypes);
#else
        throw std::runtime_error(
            "assimp dependency is required to load meshes");
#endif

        // Import morphologies
        const auto useSimulationModel = _circuitAttributes.useSimulationModel;
        model->useSimulationModel(useSimulationModel);
        if (_circuitAttributes.meshFolder.empty() || useSimulationModel)
        {
            MorphologyLoader morphLoader(_scene, _morphologyAttributes);
            returnValue =
                returnValue &&
                _importMorphologies(circuit, *model, allGids, transformations,
                                    targetGIDOffsets, compartmentReport,
                                    morphLoader, layerIds, morphologyTypes,
                                    electrophysiologyTypes);
        }

        // Create materials
        model->createMissingMaterials(simulationHandler != nullptr);

        brayns::ModelMetadata metadata = {
            {"Density", std::to_string(_circuitAttributes.density)},
            {"Report", _circuitAttributes.report},
            {"Targets", _circuitAttributes.targets},
            {"Color scheme", CIRCUIT_COLOR_SCHEME[static_cast<size_t>(
                                 _circuitAttributes.colorScheme)]},
            {"Use simulation model",
             CIRCUIT_ON_OFF[static_cast<size_t>(
                 _circuitAttributes.useSimulationModel)]},
            {"Mesh filename pattern", _circuitAttributes.meshFilenamePattern},
            {"Mesh folder", _circuitAttributes.meshFolder},
            {"Number of neurons", std::to_string(allGids.size())}};

        // Compute circuit center
        brayns::Boxf circuitCenter;
        for (const auto& transformation : transformations)
            circuitCenter.merge(transformation.getTranslation());

        brayns::Transformation transformation;
        transformation.setRotationCenter(circuitCenter.getCenter());
        modelDesc =
            std::make_shared<brayns::ModelDescriptor>(std::move(model),
                                                      "Circuit", circuitConfig,
                                                      metadata);
        modelDesc->setTransformation(transformation);
    }
    catch (const std::exception& error)
    {
        PLUGIN_ERROR << "Failed to open " << circuitConfig << ": "
                     << error.what() << std::endl;
        return {};
    }

    if (returnValue)
        return modelDesc;
    return {};
}

/**
 * @brief _getMaterialFromSectionType return a material determined by the
 * --color-scheme geometry parameter
 * @param index Index of the element to which the material will attached
 * @param material Material that is forced in case geometry parameters do not
 * apply
 * @param sectionType Section type of the geometry to which the material will be
 * applied
 * @return Material ID determined by the geometry parameters
 */
size_t CircuitLoader::_getMaterialFromCircuitAttributes(
    const uint64_t index, const size_t material,
    const GIDOffsets& targetGIDOffsets, const size_ts& layerIds,
    const size_ts& morphologyTypes, const size_ts& electrophysiologyTypes,
    bool isMesh) const
{
    if (material != brayns::NO_MATERIAL)
        return material;

    if (!isMesh && _circuitAttributes.useSimulationModel)
        return 0;

    size_t materialId = 0;
    switch (_circuitAttributes.colorScheme)
    {
    case CircuitColorScheme::neuron_by_id:
        materialId = index;
        break;
    case CircuitColorScheme::neuron_by_target:
        for (size_t i = 0; i < targetGIDOffsets.size() - 1; ++i)
            if (index >= targetGIDOffsets[i] && index < targetGIDOffsets[i + 1])
            {
                materialId = i;
                break;
            }
        break;
    case CircuitColorScheme::neuron_by_etype:
        if (index < electrophysiologyTypes.size())
            materialId = electrophysiologyTypes[index];
        else
            PLUGIN_DEBUG << "Failed to get neuron E-type" << std::endl;
        break;
    case CircuitColorScheme::neuron_by_mtype:
        if (index < morphologyTypes.size())
            materialId = morphologyTypes[index];
        else
            PLUGIN_DEBUG << "Failed to get neuron M-type" << std::endl;
        break;
    case CircuitColorScheme::neuron_by_layer:
        if (index < layerIds.size())
            materialId = layerIds[index];
        else
            PLUGIN_DEBUG << "Failed to get neuron layer" << std::endl;
        break;
    default:
        materialId = brayns::NO_MATERIAL;
    }
    return materialId;
}

size_ts CircuitLoader::_populateLayerIds(const brion::BlueConfig& blueConfig,
                                         const brain::GIDSet& gids) const
{
    size_ts layerIds;
    try
    {
        brion::Circuit brionCircuit(blueConfig.getCircuitSource());
        for (const auto& a : brionCircuit.get(gids, brion::NEURON_LAYER))
            layerIds.push_back(std::stoi(a[0]));
    }
    catch (...)
    {
        if (_circuitAttributes.colorScheme ==
            CircuitColorScheme::neuron_by_layer)
            PLUGIN_ERROR << "Only MVD2 format is currently supported by Brion "
                            "circuits. Color scheme by layer not available for "
                            "this circuit"
                         << std::endl;
    }
    return layerIds;
}

void CircuitLoader::_logLoadedGIDs(const brain::GIDSet& gids) const
{
    std::stringstream gidsStr;
    for (const auto& gid : gids)
        gidsStr << gid << " ";
    PLUGIN_DEBUG << "Loaded GIDs: " << gidsStr.str() << std::endl;
}

std::string CircuitLoader::_getMeshFilenameFromGID(const uint64_t gid) const
{
    const auto meshedMorphologiesFolder = _circuitAttributes.meshFolder;
    auto meshFilenamePattern = _circuitAttributes.meshFilenamePattern;
    const std::string gidAsString = std::to_string(gid);
    const std::string GID = "{gid}";
    if (!meshFilenamePattern.empty())
        meshFilenamePattern.replace(meshFilenamePattern.find(GID), GID.length(),
                                    gidAsString);
    else
        meshFilenamePattern = gidAsString;
    return meshedMorphologiesFolder + "/" + meshFilenamePattern;
}

#if BRAYNS_USE_ASSIMP
bool CircuitLoader::_importMeshes(brayns::Model& model,
                                  const brain::GIDSet& gids,
                                  const brayns::Matrix4fs& transformations,
                                  const GIDOffsets& targetGIDOffsets,
                                  const size_ts& layerIds,
                                  const size_ts& morphologyTypes,
                                  const size_ts& electrophysiologyTypes) const
{
    brayns::MeshLoader meshLoader(_scene,
                                  static_cast<brayns::GeometryQuality>(
                                      _morphologyAttributes.geometryQuality));

    size_t loadingFailures = 0;

    size_t meshIndex = 0;
    std::stringstream message;
    message << "Loading " << gids.size() << " meshes...";
    for (const auto& gid : gids)
    {
        const size_t materialId =
            _getMaterialFromCircuitAttributes(meshIndex, brayns::NO_MATERIAL,
                                              targetGIDOffsets, layerIds,
                                              morphologyTypes,
                                              electrophysiologyTypes, true);

        // Load mesh from file
        const auto transformation = _circuitAttributes.meshTransformation
                                        ? transformations[meshIndex]
                                        : brayns::Matrix4f();
        try
        {
            meshLoader.importMesh(_getMeshFilenameFromGID(gid), model,
                                  transformation, materialId);
        }
        catch (...)
        {
            ++loadingFailures;
        }
        ++meshIndex;
    }
    if (loadingFailures != 0)
        PLUGIN_WARN << "Failed to import " << loadingFailures << " meshes"
                    << std::endl;
    return true;
}
#endif

bool CircuitLoader::_importMorphologies(
    const brain::Circuit& circuit, brayns::Model& model,
    const brain::GIDSet& gids, const brayns::Matrix4fs& transformations,
    const GIDOffsets& targetGIDOffsets, CompartmentReportPtr compartmentReport,
    MorphologyLoader& morphLoader, const size_ts& layerIds,
    const size_ts& morphologyTypes, const size_ts& electrophysiologyTypes) const
{
    const brain::URIs& uris = circuit.getMorphologyURIs(gids);
    size_t loadingFailures = 0;
    std::stringstream message;
    message << "Loading " << uris.size() << " morphologies...";
    std::atomic_size_t current{0};
    std::exception_ptr cancelException;
    for (uint64_t morphologyIndex = 0; morphologyIndex < uris.size();
         ++morphologyIndex)
    {
        ++current;

        try
        {
            ParallelModelContainer modelContainer;
            const auto& uri = uris[morphologyIndex];

            const auto materialId = _getMaterialFromCircuitAttributes(
                morphologyIndex, brayns::NO_MATERIAL, targetGIDOffsets,
                layerIds, morphologyTypes, electrophysiologyTypes, false);

            morphLoader.setDefaultMaterialId(materialId);

            if (!morphLoader._importMorphology(uri, morphologyIndex,
                                               transformations[morphologyIndex],
                                               compartmentReport,
                                               modelContainer))
                ++loadingFailures;
            modelContainer.addSpheresToModel(model);
            modelContainer.addCylindersToModel(model);
            modelContainer.addConesToModel(model);
            modelContainer.addSDFGeometriesToModel(model);
        }
        catch (...)
        {
            cancelException = std::current_exception();
            morphologyIndex = uris.size();
        }
    }

    if (cancelException)
        std::rethrow_exception(cancelException);

    if (loadingFailures != 0)
    {
        PLUGIN_ERROR << loadingFailures << " could not be loaded" << std::endl;
        return false;
    }
    return true;
}

brayns::ModelDescriptorPtr CircuitLoader::importFromFile(
    const std::string& fileName, const size_t /*index*/,
    const size_t /*defaultMaterialId*/)
{
    return importCircuit(fileName);
}

brayns::ModelDescriptorPtr CircuitLoader::importFromBlob(
    brayns::Blob&& /*blob*/, const size_t /*index*/,
    const size_t /*materialID*/)
{
    throw std::runtime_error("Load circuit from memory not supported");
}
