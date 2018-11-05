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
#define TO_JSON(PARAM, JSON, NAME) JSON[#NAME] = PARAM.NAME;

bool from_json(Result& param, const std::string& payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);

        FROM_JSON(param, js, success);
        FROM_JSON(param, js, error);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

std::string to_json(const Result& param)
{
    try
    {
        nlohmann::json js;

        TO_JSON(param, js, success);
        TO_JSON(param, js, error);
        return js.dump();
    }
    catch (...)
    {
        return "";
    }
    return "";
}

bool from_json(LoadModelFromCache& param, const std::string& payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, name);
        FROM_JSON(param, js, path);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(SaveModelToCache& param, const std::string& payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, modelId);
        FROM_JSON(param, js, path);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(MaterialDescriptor& param, const std::string& payload)
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

bool from_json(SynapseAttributes& param, const std::string& payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, circuitConfiguration);
        FROM_JSON(param, js, gid);
        FROM_JSON(param, js, htmlColors);
        FROM_JSON(param, js, lightEmission);
        FROM_JSON(param, js, radius);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(MorphologyAttributes& param, const std::string& payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, radiusMultiplier);
        FROM_JSON(param, js, radiusCorrection);
        FROM_JSON(param, js, sectionTypes);
        FROM_JSON(param, js, realisticSoma);
        FROM_JSON(param, js, metaballsSamplesFromSoma);
        FROM_JSON(param, js, metaballsGridSize);
        FROM_JSON(param, js, metaballsThreshold);
        FROM_JSON(param, js, dampenBranchThicknessChangerate);
        FROM_JSON(param, js, useSDFGeometries);
        FROM_JSON(param, js, geometryQuality);
        FROM_JSON(param, js, colorScheme);
        FROM_JSON(param, js, useSimulationModel);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool from_json(CircuitAttributes& param, const std::string& payload)
{
    try
    {
        auto js = nlohmann::json::parse(payload);
        FROM_JSON(param, js, aabb);
        FROM_JSON(param, js, density);
        FROM_JSON(param, js, meshFilenamePattern);
        FROM_JSON(param, js, meshFolder);
        FROM_JSON(param, js, meshTransformation);
        FROM_JSON(param, js, targets);
        FROM_JSON(param, js, report);
        FROM_JSON(param, js, startSimulationTime);
        FROM_JSON(param, js, endSimulationTime);
        FROM_JSON(param, js, simulationStep);
        FROM_JSON(param, js, simulationValueRange);
        FROM_JSON(param, js, simulationHistogramSize);
        FROM_JSON(param, js, randomSeed);
        FROM_JSON(param, js, colorScheme);
        FROM_JSON(param, js, useSimulationModel);
    }
    catch (...)
    {
        return false;
    }
    return true;
}
