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

#include "OptiXFrameBuffer.h"
#include "OptiXContext.h"
#include "OptiXUtils.h"

#include <brayns/common/log.h>
#include <brayns/parameters/RenderingParameters.h>

#include <optixu/optixu_math_namespace.h>

namespace brayns
{
OptiXFrameBuffer::OptiXFrameBuffer(const Vector2ui& frameSize,
                                   FrameBufferFormat frameBufferFormat,
                                   const RenderingParameters& renderParams)
    : FrameBuffer(frameSize, frameBufferFormat, renderParams.getAccumulation())
    , _renderParams(renderParams)
{
    resize(frameSize);
}

OptiXFrameBuffer::~OptiXFrameBuffer()
{
    auto lock = getScopeLock();
    _cleanup();
}

void OptiXFrameBuffer::_cleanup()
{
    RT_DESTROY(_frameBuffer);
    RT_DESTROY(_accumBuffer);

    // Post processing
    RT_DESTROY(_denoiserStage);
    RT_DESTROY(_tonemapStage);
    RT_DESTROY(_tonemappedBuffer);
    RT_DESTROY(_denoisedBuffer);
    RT_DESTROY(_commandListWithDenoiser);
    RT_DESTROY(_commandListWithoutDenoiser);
}

void OptiXFrameBuffer::resize(const Vector2ui& frameSize)
{
    if (frameSize.product() == 0)
        BRAYNS_THROW(std::runtime_error("Invalid size for framebuffer resize"));

    if (_frameBuffer && getSize() == frameSize)
        return;

    _frameSize = frameSize;

    _recreate();
}

void OptiXFrameBuffer::_recreate()
{
    auto lock = getScopeLock();

    if (_frameBuffer)
    {
        _unmapUnsafe();
        _cleanup();
    }

    RTformat format;
    switch (_frameBufferFormat)
    {
    case FrameBufferFormat::rgb_i8:
        format = RT_FORMAT_UNSIGNED_BYTE3;
        break;
    case FrameBufferFormat::rgba_i8:
    case FrameBufferFormat::bgra_i8:
        format = RT_FORMAT_UNSIGNED_BYTE4;
        break;
    case FrameBufferFormat::rgb_f32:
        format = RT_FORMAT_FLOAT4;
        break;
    default:
        format = RT_FORMAT_UNKNOWN;
    }

    auto context = OptiXContext::get().getOptixContext();
    const auto& size = _frameSize;
    _frameBuffer =
        context->createBuffer(RT_BUFFER_OUTPUT, format, size.x(), size.y());
    context[CUDA_BUFFER_OUTPUT]->set(_frameBuffer);

    _accumBuffer = context->createBuffer(RT_BUFFER_INPUT_OUTPUT,
                                         RT_FORMAT_FLOAT4, size.x(), size.y());
    context[CUDA_BUFFER_ACCUMULATION]->set(_accumBuffer);

    _tonemappedBuffer =
        context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4, size.x(),
                              size.y());
    context[CUDA_BUFFER_TONEMAPPED]->set(_tonemappedBuffer);

    _denoisedBuffer = context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4,
                                            size.x(), size.y());
    context[CUDA_BUFFER_DENOISED]->set(_denoisedBuffer);

    clear();
}

void OptiXFrameBuffer::clear()
{
    _accumulationFrame = 0u;
}

void OptiXFrameBuffer::map()
{
    _mapMutex.lock();
    _mapUnsafe();
}

void OptiXFrameBuffer::_mapUnsafe()
{
    auto context = OptiXContext::get().getOptixContext();
    if (_accumulation)
        context[CUDA_ATTR_FRAME]->setUint(_accumulationFrame);
    else
        context[CUDA_ATTR_FRAME]->setUint(0u);

    if (_renderParams.getPostProcessingFilters())
    {
        if (!_postprocessingStagesInitialized)
            _initializePostProcessingStages();

        const bool useDenoiser = (_accumulationFrame >= _numNonDenoisedFrames);

        if (useDenoiser)
            rtBufferMap(_denoisedBuffer->get(), &_imageData);
        else
            rtBufferMap(_tonemappedBuffer->get(), &_imageData);
    }
    else
        rtBufferMap(_frameBuffer->get(), &_imageData);

    ++_accumulationFrame;
}

void OptiXFrameBuffer::unmap()
{
    _unmapUnsafe();
    _mapMutex.unlock();
}

void OptiXFrameBuffer::_unmapUnsafe()
{
    // Post processing stages
    if (_renderParams.getPostProcessingFilters() && _commandListWithDenoiser &&
        _commandListWithoutDenoiser)
    {
        auto context = OptiXContext::get().getOptixContext();
        context[CUDA_ATTR_BLEND]->setFloat(_denoiseBlend);
        try
        {
            if (_accumulationFrame >= _numNonDenoisedFrames)
                _commandListWithDenoiser->execute();
            else
                _commandListWithoutDenoiser->execute();
        }
        catch (...)
        {
            BRAYNS_THROW(std::runtime_error("Hum... Something went wrong"));
        }
        rtBufferUnmap(_denoisedBuffer->get());
        rtBufferUnmap(_tonemappedBuffer->get());
    }

    rtBufferUnmap(_frameBuffer->get());
}

void OptiXFrameBuffer::setAccumulation(const bool accumulation)
{
    if (_accumulation != accumulation)
    {
        FrameBuffer::setAccumulation(accumulation);
        _recreate();
    }
}

void OptiXFrameBuffer::_initializePostProcessingStages()
{
    if (_postprocessingStagesInitialized)
        return;

    BRAYNS_INFO << "Initializing post processing stages" << std::endl;
    auto context = OptiXContext::get().getOptixContext();
    _tonemapStage =
        context->createBuiltinPostProcessingStage(CUDA_STAGE_TONE_MAPPER);
    _tonemapStage->declareVariable(CUDA_ATTR_BUFFER_INPUT)->set(_accumBuffer);
    _tonemapStage->declareVariable(CUDA_ATTR_BUFFER_OUTPUT)
        ->set(_tonemappedBuffer);
    _tonemapStage->declareVariable(CUDA_ATTR_EXPOSURE)
        ->setFloat(DEFAULT_EXPOSURE);
    _tonemapStage->declareVariable(CUDA_ATTR_GAMMA)->setFloat(DEFAULT_GAMMA);

    _denoiserStage =
        context->createBuiltinPostProcessingStage(CUDA_STAGE_DENOISER);
    _denoiserStage->declareVariable(CUDA_ATTR_BUFFER_INPUT)
        ->set(_tonemappedBuffer);
    _denoiserStage->declareVariable(CUDA_ATTR_BUFFER_OUTPUT)
        ->set(_denoisedBuffer);
    _denoiserStage->declareVariable(CUDA_ATTR_BLEND)->setFloat(_denoiseBlend);
    _denoiserStage->declareVariable(CUDA_BUFFER_INPUT_ALBEDO);
    _denoiserStage->declareVariable(CUDA_BUFFER_INPUT_NORMAL);

    // With denoiser
    _commandListWithDenoiser = context->createCommandList();
    _commandListWithDenoiser->appendLaunch(0, _frameSize.x(), _frameSize.y());
    _commandListWithDenoiser->appendPostprocessingStage(_tonemapStage,
                                                        _frameSize.x(),
                                                        _frameSize.y());
    _commandListWithDenoiser->appendPostprocessingStage(_denoiserStage,
                                                        _frameSize.x(),
                                                        _frameSize.y());
    _commandListWithDenoiser->finalize();

    // Without denoiser
    _commandListWithoutDenoiser = context->createCommandList();
    _commandListWithoutDenoiser->appendLaunch(0, _frameSize.x(),
                                              _frameSize.y());
    _commandListWithoutDenoiser->appendPostprocessingStage(_tonemapStage,
                                                           _frameSize.x(),
                                                           _frameSize.y());
    _commandListWithoutDenoiser->finalize();

    _postprocessingStagesInitialized = true;
}

} // namespace brayns
