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

#ifndef SYNAPSELOADER_H
#define SYNAPSELOADER_H

#pragma once

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>
#include <brayns/parameters/GeometryParameters.h>

#include <brain/brain.h>

#include <vector>

/**
 * Load circuit from BlueConfig or CircuitConfig file, including simulation.
 */
class SynapseLoader : public brayns::Loader
{
public:
    SynapseLoader(brayns::Scene& scene,
                  brayns::ParametersManager& parametersManager);
    ~SynapseLoader();

    static std::set<std::string> getSupportedDataTypes();

    brayns::ModelDescriptorPtr importFromBlob(brayns::Blob&& blob,
                                              const size_t index,
                                              const size_t materialID) final;

    brayns::ModelDescriptorPtr importFromFile(const std::string& filename,
                                              const size_t index,
                                              const size_t materialID) final;

    /**
     * @brief Imports synapses from a circuit for the given target name
     * @param circuitConfig URI of the Circuit Config file
     * @param gid Gid of the neuron
     * @param scene Scene into which the circuit is imported
     * @return True if the circuit is successfully loaded, false if the circuit
     * contains no cells.
     */
    brayns::ModelDescriptorPtr importSynapsesFromGIDs(
        const std::string& circuitConfig, const uint32_t& gid,
        const brayns::Vector3fs& colors, const float lightEmission);

private:
    brayns::ParametersManager& _parametersManager;
};

#endif // SYNAPSELOADER_H
