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
class BrickLoader : public brayns::Loader
{
public:
    BrickLoader(brayns::Scene& scene);
    ~BrickLoader();

    static std::set<std::string> getSupportedDataTypes();

    brayns::ModelDescriptorPtr importFromBlob(brayns::Blob&& blob,
                                              const size_t index,
                                              const size_t materialID) final;

    brayns::ModelDescriptorPtr importFromFile(const std::string& filename,
                                              const size_t index,
                                              const size_t materialID) final;

    void exportToFile(const brayns::ModelDescriptorPtr modelDescriptor,
                      const std::string& filename);

private:
    std::string _readString(std::ifstream& f);
};
