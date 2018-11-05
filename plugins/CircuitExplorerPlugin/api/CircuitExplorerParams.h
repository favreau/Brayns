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

#ifndef CIRCUITVIEWERPARAMS_H
#define CIRCUITVIEWERPARAMS_H

#include "../types.h"
#include <brayns/common/types.h>

struct Result
{
    bool success = false;
    std::string error;
};

std::string to_json(const Result& param);

/** Load model from cache */
struct LoadModelFromCache
{
    std::string name;
    std::string path;
};

bool from_json(LoadModelFromCache& modelLoad, const std::string& payload);

/** Save model to cache */
struct SaveModelToCache
{
    size_t modelId;
    std::string path;
};

bool from_json(SaveModelToCache& modelSave, const std::string& payload);

struct MaterialDescriptor
{
    size_t modelId;
    size_t materialId;
    std::vector<float> diffuseColor;
    std::vector<float> specularColor;
    float specularExponent;
    float reflectionIndex;
    float opacity;
    float refractionIndex;
    float emission;
    float glossiness;
    bool castSimulationData;
    std::string shadingMode;
};

bool from_json(MaterialDescriptor& materialDescriptor,
               const std::string& payload);

struct SynapseAttributes
{
    std::string circuitConfiguration;
    uint32_t gid;
    std::vector<std::string> htmlColors;
    float lightEmission;
    float radius;
};

bool from_json(SynapseAttributes& synapseAttributes,
               const std::string& payload);

/** Morphology attributes */
struct MorphologyAttributes
{
    float radiusMultiplier{1.f};
    float radiusCorrection{1.f};
    size_t sectionTypes{255};
    bool realisticSoma{false};
    size_t metaballsSamplesFromSoma{5};
    size_t metaballsGridSize{20};
    float metaballsThreshold{1.f};
    bool dampenBranchThicknessChangerate{true};
    bool useSDFGeometries{true};
    GeometryQuality geometryQuality{GeometryQuality::high};
    MorphologyColorScheme colorScheme{MorphologyColorScheme::none};
    bool useSimulationModel{false};
};

bool from_json(MorphologyAttributes& morphologyAttributes,
               const std::string& payload);

/** Circuit attributes */
struct CircuitAttributes
{
    std::vector<double> aabb{0, 0, 0, 0, 0, 0};
    double density{100};
    std::string meshFilenamePattern;
    std::string meshFolder;
    bool meshTransformation{false};
    std::string targets;
    std::string report;
    double startSimulationTime{0};
    double endSimulationTime{std::numeric_limits<float>::max()};
    double simulationStep{0};
    std::vector<double> simulationValueRange{
        std::numeric_limits<double>::max(), std::numeric_limits<double>::min()};
    size_t simulationHistogramSize{128};
    size_t randomSeed = 0;
    CircuitColorScheme colorScheme{CircuitColorScheme::none};
    bool useSimulationModel{false};
};

bool from_json(CircuitAttributes& circuitAttributes,
               const std::string& payload);

#endif // CIRCUITVIEWERPARAMS_H
