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

#include <brayns/common/types.h>

struct LoadModelFromCache
{
    std::string name;
    std::string filename;
};

bool from_json(LoadModelFromCache &modelLoad, const std::string &payload);

struct SaveModelToCache
{
    size_t modelId;
    std::string filename;
};

bool from_json(SaveModelToCache &modelSave, const std::string &payload);

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

bool from_json(MaterialDescriptor &materialDescriptor,
               const std::string &payload);

struct SynapsesDescriptor
{
    std::string circuitConfiguration;
    uint32_t gid;
    std::vector<std::string> htmlColors;
    float lightEmission;
};

bool from_json(SynapsesDescriptor &synapsesDescriptor,
               const std::string &payload);

#endif // CIRCUITVIEWERPARAMS_H
