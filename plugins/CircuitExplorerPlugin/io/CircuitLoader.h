/* Copyright (c) 2015-2016, EPFL/Blue Brain Project
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

#pragma once

#include "MorphologyLoader.h"
#include "api/CircuitExplorerParams.h"
#include "types.h"

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>

#include <brain/brain.h>
#include <brion/brion.h>

#include <set>
#include <vector>

namespace servus
{
class URI;
}

/**
 * Load circuit from BlueConfig or CircuitConfig file, including simulation.
 */
class CircuitLoader : public brayns::Loader
{
public:
    CircuitLoader(brayns::Scene& scene,
                  const brayns::ApplicationParameters& applicationParameters,
                  brayns::AnimationParameters& animationParameters,
                  const CircuitAttributes& circuitAttributes,
                  const MorphologyAttributes& morphologyAttributes);
    ~CircuitLoader();

    static std::set<std::string> getSupportedDataTypes();

    brayns::ModelDescriptorPtr importFromBlob(brayns::Blob&& blob,
                                              const size_t index,
                                              const size_t materialID) final;

    brayns::ModelDescriptorPtr importFromFile(const std::string& filename,
                                              const size_t index,
                                              const size_t materialID) final;

    /**
     * @brief Imports morphology from a circuit for the given target name
     * @param circuitConfig URI of the Circuit Config file
     * @return ModelDescriptor if the circuit is successfully loaded, nullptr if
     * the circuit contains no cells.
     */
    brayns::ModelDescriptorPtr importCircuit(const std::string& circuitConfig);

    /**
     * @brief _populateLayerIds populates the neuron layer IDs. This is
     * currently only supported for the MVD2 format.
     * @param blueConfig Configuration of the circuit
     * @param gids GIDs of the neurons
     */
    size_ts _populateLayerIds(const brion::BlueConfig& blueConfig,
                              const brain::GIDSet& gids) const;

private:
    std::vector<std::string> _getTargetsAsStrings(
        const std::string& targets) const;

    /**
     * @brief _logLoadedGIDs Logs selected GIDs for debugging purpose
     * @param gids to trace
     */
    void _logLoadedGIDs(const brain::GIDSet& gids) const;

    std::string _getMeshFilenameFromGID(const uint64_t gid) const;

#if BRAYNS_USE_ASSIMP
    bool _importMeshes(brayns::Model& model, const brain::GIDSet& gids,
                       const brayns::Matrix4fs& transformations,
                       const GIDOffsets& targetGIDOffsets,
                       const size_ts& layerIds, const size_ts& morphologyTypes,
                       const size_ts& electrophysiologyTypes) const;
#endif

    bool _importMorphologies(const brain::Circuit& circuit,
                             brayns::Model& model, const brain::GIDSet& gids,
                             const brayns::Matrix4fs& transformations,
                             const GIDOffsets& targetGIDOffsets,
                             CompartmentReportPtr compartmentReport,
                             MorphologyLoader& morphLoader,
                             const size_ts& layerIds,
                             const size_ts& morphologyTypes,
                             const size_ts& electrophysiologyTypes) const;

    /**
     * @brief _getMaterialFromSectionType return a material determined by the
     * --color-scheme geometry parameter
     * @param index Index of the element to which the material will attached
     * @param material Material that is forced in case geometry parameters
     * do not apply
     * @param sectionType Section type of the geometry to which the material
     * will be applied
     * @return Material ID determined by the geometry parameters
     */
    size_t _getMaterialFromCircuitAttributes(
        const uint64_t index, const size_t material,
        const GIDOffsets& targetGIDOffsets, const size_ts& layerIds,
        const size_ts& morphologyTypes, const size_ts& electrophysiologyTypes,
        bool isMesh = false) const;

    const brayns::ApplicationParameters& _applicationParameters;
    brayns::AnimationParameters& _animationParameters;
    const CircuitAttributes& _circuitAttributes;
    const MorphologyAttributes& _morphologyAttributes;
};
