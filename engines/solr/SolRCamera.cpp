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

#include "SolRCamera.h"

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#else
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif // USE_OPENCL
#endif // USE_CUDA

namespace brayns
{
SolRCamera::SolRCamera(solr::GPUKernel* kernel)
    : _kernel(kernel)
{
}

SolRCamera::~SolRCamera() {}

void SolRCamera::commit()
{
    if (!isModified())
        return;

    const auto p = getPosition();
    const auto t = getTarget();
    const auto d = t - p;

    const float yaw = atan2(d.x(), d.z());
    const float padj = sqrt(pow(d.x(), 2) + pow(d.z(), 2));
    const float pitch = atan2(padj, d.y());
    solr::vec4f angles = {pitch, yaw, 0, 1.f};

    _kernel->setCamera({0.5f, 0.5f, -1.5f - (float)d.length()}, {0.f, 0.f, 1.f},
                       angles);
}

void SolRCamera::setEnvironmentMap(const bool /*environmentMap*/) {}

void SolRCamera::setClipPlanes(const ClipPlanes& clipPlanes)
{
    if (_clipPlanes == clipPlanes)
        return;
    _clipPlanes = clipPlanes;
    markModified(false);
}

bool SolRCamera::isSideBySideStereo() const
{
    return hasProperty("stereoMode") && getProperty<int>("stereoMode") == 3;
}

} // namespace brayns
