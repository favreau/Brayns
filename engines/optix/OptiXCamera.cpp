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

#include "OptiXCamera.h"
#include "OptiXUtils.h"

#include <brayns/common/log.h>

namespace brayns
{
OptiXCamera::OptiXCamera(const bool environmentMap)
{
    _camera = OptiXContext::get().createCamera(environmentMap);
}

OptiXCamera::~OptiXCamera()
{
    if (_camera)
        _camera->destroy();
}

void OptiXCamera::commit()
{
    auto context = OptiXContext::get().getOptixContext();

    Vector3d u, v, w;

    const Vector3d& pos = getPosition();

    _calculateCameraVariables(u, v, w);

    context[CUDA_ATTR_CAMERA_EYE]->setFloat(pos.x(), pos.y(), pos.z());
    context[CUDA_ATTR_CAMERA_U]->setFloat(u.x(), u.y(), u.z());
    context[CUDA_ATTR_CAMERA_V]->setFloat(v.x(), v.y(), v.z());
    context[CUDA_ATTR_CAMERA_W]->setFloat(w.x(), w.y(), w.z());
    context[CUDA_ATTR_CAMERA_APERTURE_RADIUS]->setFloat(
        getProperty<double>("apertureRadius"));
    context[CUDA_ATTR_CAMERA_FOCAL_SCALE]->setFloat(
        getProperty<double>("focusDistance"));
    context[CUDA_ATTR_CAMERA_BAD_COLOR]->setFloat(1.f, 0.f, 1.f);
    context[CUDA_ATTR_CAMERA_OFFSET]->setFloat(0, 0);

    Vector4fs buffer;
    if (!_clipPlanes.empty())
    {
        for (const auto& clipPlane : _clipPlanes)
            buffer.push_back({(float)clipPlane[0], (float)clipPlane[1],
                              (float)clipPlane[2], (float)clipPlane[3]});

        if (_clipPlanesBuffer)
            _clipPlanesBuffer->destroy();
    }
    _clipPlanesBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4,
                                              _clipPlanes.size());
    memcpy(_clipPlanesBuffer->map(), buffer.data(),
           _clipPlanes.size() * sizeof(Vector4f));
    _clipPlanesBuffer->unmap();
    context[CUDA_ATTR_CLIP_PLANES]->setBuffer(_clipPlanesBuffer);
    context[CUDA_ATTR_NB_CLIP_PLANES]->setUint(_clipPlanes.size());
}

void OptiXCamera::_calculateCameraVariables(Vector3d& U, Vector3d& V,
                                            Vector3d& W)
{
    float ulen, vlen, wlen;
    W = getTarget() - getPosition();

    wlen = W.length();
    U = normalize(cross(W, getUp()));
    V = normalize(cross(U, W));

    vlen = wlen * tanf(0.5f * getProperty<double>("fovy") * M_PI / 180.f);
    V *= vlen;
    ulen = vlen * getProperty<double>("aspect");
    U *= ulen;
}

} // namespace brayns
