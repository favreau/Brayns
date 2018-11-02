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

#include "CircuitExplorerParams.h"
#include "json.hpp"

#define FROM_JSON(PARAM, JSON, NAME) \
    PARAM.NAME = JSON[#NAME].get<decltype(PARAM.NAME)>()

bool from_json(LoadModelFromCache &param, const std::string &payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, name);
        FROM_JSON(param, js, filename);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(SaveModelToCache &param, const std::string &payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, modelId);
        FROM_JSON(param, js, filename);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(MaterialDescriptor &param, const std::string &payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, modelId);
        FROM_JSON(param, js, materialId);
        FROM_JSON(param, js, diffuseColor);
        FROM_JSON(param, js, specularColor);
        FROM_JSON(param, js, specularExponent);
        FROM_JSON(param, js, reflectionIndex);
        FROM_JSON(param, js, opacity);
        FROM_JSON(param, js, refractionIndex);
        FROM_JSON(param, js, emission);
        FROM_JSON(param, js, glossiness);
        FROM_JSON(param, js, castSimulationData);
        FROM_JSON(param, js, shadingMode);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(SynapsesDescriptor &param, const std::string &payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, circuitConfiguration);
        FROM_JSON(param, js, gid);
        FROM_JSON(param, js, htmlColors);
        FROM_JSON(param, js, lightEmission);
    }
    catch (...)
    {
        return false;
    }
    return true;
}
