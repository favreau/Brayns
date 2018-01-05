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

#ifndef OPTIXFRAMEBUFFER_H
#define OPTIXFRAMEBUFFER_H

#include <brayns/common/renderer/FrameBuffer.h>
#include <optixu/optixpp_namespace.h>

namespace brayns
{
/**
   OptiX specific frame buffer

   This object is the OptiX specific implementation of a frame buffer
*/
class OptiXFrameBuffer : public brayns::FrameBuffer
{
public:
    OptiXFrameBuffer(const Vector2ui& frameSize,
                     const FrameBufferFormat colorDepth,
                     const AccumulationType accumulationType,
                     optix::Context& context);

    ~OptiXFrameBuffer();

    void clear() final;

    void resize(const Vector2ui& frameSize) final;

    void map() final;

    void unmap() final;

    uint8_t* getColorBuffer() final { return _colorBuffer; }
    float* getFloatBuffer() final { return _floatBuffer; }
    size_t getFloatDepth() final { return 4; }
private:
    void _cleanup();
    optix::Buffer _outputBuffer;
    optix::Buffer _accumBuffer;
    optix::Buffer _tonemappedBuffer;
    optix::Buffer _denoisedBuffer;
    optix::Context& _context;
    uint8_t* _colorBuffer;
    float* _floatBuffer;
    uint16_t _accumulationFrameNumber;
    void* _colorData;
    void* _floatData;

    // Post processing
    void _initializePostProcessingStages();
    optix::CommandList _commandListWithDenoiser{nullptr};
    optix::CommandList _commandListWithoutDenoiser{nullptr};
    optix::PostprocessingStage _tonemapStage{nullptr};
    optix::PostprocessingStage _denoiserStage{nullptr};

    size_t _numNonDenoisedFrames{4}; // number of frames that show the original
                                     // image before switching on denoising
    float _denoiseBlend{
        0.f}; // Defines the amount of the original image that is
              // blended with the denoised result ranging from
              // 0.0 to 1.0

    bool _postprocessingStagesInitialized{false};
};
}

#endif // OPTIXFRAMEBUFFER_H
