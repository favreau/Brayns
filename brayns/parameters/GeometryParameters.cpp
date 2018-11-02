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

#include "GeometryParameters.h"
#include <brayns/common/log.h>
#include <brayns/common/types.h>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

namespace
{
const std::string PARAM_NEST_CIRCUIT = "nest-circuit";
const std::string PARAM_NEST_REPORT = "nest-report";
const std::string PARAM_RADIUS_MULTIPLIER = "radius-multiplier";
const std::string PARAM_RADIUS_CORRECTION = "radius-correction";
const std::string PARAM_COLOR_SCHEME = "color-scheme";
const std::string PARAM_GEOMETRY_QUALITY = "geometry-quality";
const std::string PARAM_NEST_CACHE_FILENAME = "nest-cache-file";
const std::string PARAM_MOLECULAR_SYSTEM_CONFIG = "molecular-system-config";
const std::string PARAM_MEMORY_MODE = "memory-mode";

const std::string GEOMETRY_MEMORY_MODES[2] = {"shared", "replicated"};
const std::string GEOMETRY_QUALITIES[3] = {"low", "medium", "high"};
const std::array<std::string, 12> COLOR_SCHEMES = {
    {"none", "protein-by-id", "protein-atoms", "protein-chains",
     "protein-residues"}};
}

namespace brayns
{
GeometryParameters::GeometryParameters()
    : AbstractParameters("Geometry")
    , _colorScheme(ColorScheme::none)
    , _geometryQuality(GeometryQuality::high)
    , _memoryMode(MemoryMode::shared)
{
    _parameters.add_options() //
        (PARAM_NEST_CIRCUIT.c_str(), po::value<std::string>(),
         "H5 file containing the NEST circuit [string]")
        //
        (PARAM_NEST_REPORT.c_str(), po::value<std::string>(),
         "NEST simulation report file [string]")
        //
        (PARAM_GEOMETRY_QUALITY.c_str(), po::value<std::string>(),
         "Geometry rendering quality [low|medium|high]")
        //
        (PARAM_NEST_CACHE_FILENAME.c_str(), po::value<std::string>(),
         "Cache file containing nest data [string]")
        //
        (PARAM_MOLECULAR_SYSTEM_CONFIG.c_str(), po::value<std::string>(),
         "Molecular system configuration [string]")
        //
        (PARAM_MEMORY_MODE.c_str(), po::value<std::string>(),
         "Defines what memory mode should be used between Brayns and "
         "the "
         "underlying renderer [shared|replicated]");
}

void GeometryParameters::parse(const po::variables_map& vm)
{
    if (vm.count(PARAM_NEST_CIRCUIT))
        _NESTCircuit = vm[PARAM_NEST_CIRCUIT].as<std::string>();
    if (vm.count(PARAM_NEST_REPORT))
        _NESTReport = vm[PARAM_NEST_REPORT].as<std::string>();

    if (vm.count(PARAM_NEST_CACHE_FILENAME))
        _NESTCacheFile = vm[PARAM_NEST_CACHE_FILENAME].as<std::string>();
    if (vm.count(PARAM_MOLECULAR_SYSTEM_CONFIG))
        _molecularSystemConfig =
            vm[PARAM_MOLECULAR_SYSTEM_CONFIG].as<std::string>();

    if (vm.count(PARAM_MEMORY_MODE))
    {
        const auto& memoryMode = vm[PARAM_MEMORY_MODE].as<std::string>();
        for (size_t i = 0; i < sizeof(GEOMETRY_MEMORY_MODES) /
                                   sizeof(GEOMETRY_MEMORY_MODES[0]);
             ++i)
            if (memoryMode == GEOMETRY_MEMORY_MODES[i])
                _memoryMode = static_cast<MemoryMode>(i);
    }

    markModified();
}

void GeometryParameters::print()
{
    AbstractParameters::print();
    BRAYNS_INFO << "NEST circuit file          : " << _NESTCircuit << std::endl;
    BRAYNS_INFO << "NEST simulation report file: " << _NESTReport << std::endl;
    BRAYNS_INFO << "NEST cache file            : " << _NESTCacheFile
                << std::endl;
    BRAYNS_INFO << "Color scheme               : "
                << getColorSchemeAsString(_colorScheme) << std::endl;
    BRAYNS_INFO << "Geometry quality           : "
                << getGeometryQualityAsString(_geometryQuality) << std::endl;
    BRAYNS_INFO << "Molecular system config    : " << _molecularSystemConfig
                << std::endl;
    BRAYNS_INFO << "Memory mode                : "
                << (_memoryMode == MemoryMode::shared ? "Shared" : "Replicated")
                << std::endl;
}

const std::string& GeometryParameters::getColorSchemeAsString(
    const ColorScheme value) const
{
    return COLOR_SCHEMES[static_cast<size_t>(value)];
}

const std::string& GeometryParameters::getGeometryQualityAsString(
    const GeometryQuality value) const
{
    return GEOMETRY_QUALITIES[static_cast<size_t>(value)];
}
}
