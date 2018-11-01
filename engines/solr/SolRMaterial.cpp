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

#include "SolRMaterial.h"

#include <brayns/common/log.h>

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#endif
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif

namespace
{
const bool procedural = false;
const bool wireframe = false;
const float wireframeDepth = 0.f;
const float transparency = 1.f;
const int diffuseTextureId = -1;
const int normalTextureId = -1;
const int bumpTextureId = -1;
const int specularTextureId = -1;
const int reflectionTextureId = -1;
const int transparencyTextureId = -1;
const int ambientOcclusionTextureId = -1;
} // namespace

namespace brayns
{
struct TextureTypeMaterialAttribute
{
    TextureType type;
    std::string attribute;
};

static TextureTypeMaterialAttribute textureTypeMaterialAttribute[8] = {
    {TT_DIFFUSE, "map_kd"},
    {TT_NORMALS, "map_bump"},
    {TT_BUMP, "map_bump"},
    {TT_SPECULAR, "map_ks"},
    {TT_EMISSIVE, "map_ns"},
    {TT_OPACITY, "map_d"},
    {TT_REFLECTION, "map_reflection"},
    {TT_REFRACTION, "map_refraction"}};

SolRMaterial::SolRMaterial(solr::GPUKernel* kernel)
    : _kernel(kernel)
{
    _id = _kernel->addMaterial();
}

SolRMaterial::~SolRMaterial() {}

void SolRMaterial::commit()
{
    if (!isModified())
        return;

    _kernel->setMaterial(_id, _diffuseColor.x(), _diffuseColor.y(),
                         _diffuseColor.z(), _glossiness, _reflectionIndex,
                         _refractionIndex, procedural, wireframe,
                         wireframeDepth, transparency, _opacity,
                         diffuseTextureId, normalTextureId, bumpTextureId,
                         specularTextureId, reflectionTextureId,
                         transparencyTextureId, ambientOcclusionTextureId,
                         _specularColor.x(), _specularColor.y(),
                         _specularColor.z(), _emission, _emission, _emission,
                         false);
    resetModified();
}

} // namespace brayns
