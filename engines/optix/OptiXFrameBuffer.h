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

#ifndef OPTIXFRAMEBUFFER_H
#define OPTIXFRAMEBUFFER_H

#include <brayns/common/renderer/FrameBuffer.h>
#include <optixu/optixpp_namespace.h>

#include <mutex>

namespace brayns
{
/**
   OptiX specific frame buffer

   This object is the OptiX specific implementation of a frame buffer
*/
class OptiXFrameBuffer : public FrameBuffer
{
public:
    OptiXFrameBuffer(const Vector2ui& size, FrameBufferFormat frameBufferFormat,
                     const RenderingParameters& renderParams);
    ~OptiXFrameBuffer();

    void clear() final;
    void resize(const Vector2ui& size) final;
    void map() final;
    void unmap() final;
    void setAccumulation(const bool accumulation) final;

    std::unique_lock<std::mutex> getScopeLock()
    {
        return std::unique_lock<std::mutex>(_mapMutex);
    }
    uint8_t* getByteBuffer() final { return (uint8_t*)_imageData; }
    size_t getByteDepth() const final;

    float* getFloatBuffer() final { return (float*)_imageData; }
    size_t getFloatDepth() const final;

private:
    const RenderingParameters& _renderParams;

    void _cleanup();
    void _recreate();

    optix::Buffer _frameBuffer{nullptr};
    optix::Buffer _accumBuffer{nullptr};
    uint16_t _accumulationFrame{0};
    void* _imageData{nullptr};

    // Mapping
    void _mapUnsafe();
    void _unmapUnsafe();
    std::mutex _mapMutex;

    // Post processing
    void _initializePostProcessingStages();

    optix::Buffer _tonemappedBuffer{nullptr};
    optix::Buffer _denoisedBuffer{nullptr};
    optix::CommandList _commandListWithDenoiser{nullptr};
    optix::CommandList _commandListWithoutDenoiser{nullptr};
    optix::PostprocessingStage _tonemapStage{nullptr};
    optix::PostprocessingStage _denoiserStage{nullptr};

    size_t _numNonDenoisedFrames{4}; // number of frames that show the original
                                     // image before switching on denoising
    float _denoiseBlend{0.1f}; // Defines the amount of the original image that
                               // is blended with the denoised result ranging
                               // from 0.0 to 1.0
    bool _postprocessingStagesInitialized{false};
};
} // namespace brayns

#endif // OPTIXFRAMEBUFFER_H
