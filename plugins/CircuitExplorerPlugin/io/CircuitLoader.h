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

#include "api/CircuitExplorerParams.h"
#include "types.h"

#include <brayns/common/loader/Loader.h>
#include <brayns/common/types.h>

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

private:
    class Impl;
    std::unique_ptr<const Impl> _impl;
};
