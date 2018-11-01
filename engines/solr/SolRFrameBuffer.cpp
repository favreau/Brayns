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

#include "SolRFrameBuffer.h"

#include <brayns/common/log.h>
#include <brayns/parameters/StreamParameters.h>

#ifdef USE_CUDA
#include <solr/engines/CudaKernel.h>
#else
#ifdef USE_OPENCL
#include <solr/engines/OpenCLKernel.h>
#endif // USE_OPENCL
#endif // USE_CUDA

namespace brayns
{
SolRFrameBuffer::SolRFrameBuffer(solr::GPUKernel* kernel,
                                 const Vector2ui& frameSize,
                                 const FrameBufferFormat colorDepth,
                                 const bool accumulation)
    : FrameBuffer(frameSize, colorDepth, accumulation)
    , _colorBuffer(0)
    , _depthBuffer(0)
    , _kernel(kernel)
{
    resize(frameSize);
}

SolRFrameBuffer::~SolRFrameBuffer()
{
    auto lock = getScopeLock();

    _unmapUnsafe();
}

void SolRFrameBuffer::resize(const Vector2ui& frameSize)
{
    if (frameSize.product() == 0)
        throw std::runtime_error("Invalid size for framebuffer resize");

    if (getSize() == frameSize)
        return;

    _frameSize = frameSize;

    _recreate();

    auto& sceneInfo = _kernel->getSceneInfo();
    sceneInfo.size.x = frameSize.x();
    sceneInfo.size.y = frameSize.y();
}

void SolRFrameBuffer::_recreate()
{
    clear();
}

void SolRFrameBuffer::clear()
{
    FrameBuffer::clear();
    _kernel->getSceneInfo().pathTracingIteration = 0;
}

void SolRFrameBuffer::map()
{
    _kernel->getSceneInfo().pathTracingIteration = _accumFrames;
    _mapMutex.lock();
    _mapUnsafe();
}

void SolRFrameBuffer::_mapUnsafe()
{
    if (_frameBufferFormat == FrameBufferFormat::none)
        return;

    _colorBuffer = (uint8_t*)_kernel->getBitmap();
    _depthBuffer = nullptr;
}

void SolRFrameBuffer::unmap()
{
    _unmapUnsafe();
    _mapMutex.unlock();
}

void SolRFrameBuffer::_unmapUnsafe()
{
    if (_frameBufferFormat == FrameBufferFormat::none)
        return;

    if (_colorBuffer)
    {
        _colorBuffer = nullptr;
    }

    if (_depthBuffer)
    {
        _depthBuffer = nullptr;
    }
}

void SolRFrameBuffer::setAccumulation(const bool accumulation)
{
    if (_accumulation != accumulation)
    {
        FrameBuffer::setAccumulation(accumulation);
        _recreate();
    }
}
} // namespace brayns
