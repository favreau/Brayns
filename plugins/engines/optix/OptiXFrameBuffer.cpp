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

#include "OptiXFrameBuffer.h"
#include "OptiXTypes.h"
#include "OptiXUtils.h"

#include <brayns/common/log.h>

#include <optixu/optixu_math_namespace.h>

namespace
{
const std::string STAGE_TONE_MAPPER = "TonemapperSimple";
const std::string STAGE_DENOISER = "DLDenoiser";
const std::string VARIABLE_INPUT_BUFFER = "input_buffer";
const std::string VARIABLE_OUTPUT_BUFFER = "output_buffer";
const std::string VARIABLE_INPUT_ALBEDO_BUFFER = "input_albedo_buffer";
const std::string VARIABLE_INPUT_NORMAL_BUFFER = "input_normal_buffer";
const std::string VARIABLE_EXPOSURE = "exposure";
const std::string VARIABLE_GAMMA = "gamma";
const std::string VARIABLE_BLEND = "blend";

const float DEFAULT_EXPOSURE = 2.0f;
const float DEFAULT_GAMMA = 2.2f;
}

namespace brayns
{
OptiXFrameBuffer::OptiXFrameBuffer(const Vector2ui& frameSize,
                                   const FrameBufferFormat colorDepth,
                                   const AccumulationType accumulationType,
                                   optix::Context& context)
    : FrameBuffer(frameSize, colorDepth, accumulationType)
    , _outputBuffer(nullptr)
    , _accumBuffer(nullptr)
    , _tonemappedBuffer(nullptr)
    , _denoisedBuffer(nullptr)
    , _context(context)
    , _colorBuffer(nullptr)
    , _floatBuffer(nullptr)
    , _accumulationFrameNumber(1u)
    , _colorData(nullptr)
    , _floatData(nullptr)
    , _commandListWithDenoiser(nullptr)
    , _commandListWithoutDenoiser(nullptr)
    , _tonemapStage(nullptr)
    , _denoiserStage(nullptr)
{
    resize(frameSize);
}

OptiXFrameBuffer::~OptiXFrameBuffer()
{
    _cleanup();
}

void OptiXFrameBuffer::_cleanup()
{
    unmap();

    RT_DESTROY(_outputBuffer);
    RT_DESTROY(_accumBuffer);

    if (_accumulationType == AccumulationType::ai_denoised)
    {
        // Post processing
        RT_DESTROY(_denoiserStage);
        RT_DESTROY(_tonemapStage);
        RT_DESTROY(_tonemappedBuffer);
        RT_DESTROY(_denoisedBuffer);
        RT_DESTROY(_commandListWithDenoiser);
        RT_DESTROY(_commandListWithoutDenoiser);
        _postprocessingStagesInitialized = false;
    }
}

void OptiXFrameBuffer::resize(const Vector2ui& frameSize)
{
    _frameSize = frameSize;
    _cleanup();

    RTformat format;
    switch (_frameBufferFormat)
    {
    case FrameBufferFormat::rgba_i8:
        format = RT_FORMAT_UNSIGNED_BYTE4;
        break;
    case FrameBufferFormat::rgb_f32:
        format = RT_FORMAT_FLOAT4;
        break;
    default:
        format = RT_FORMAT_UNKNOWN;
    }

    _outputBuffer = _context->createBuffer(RT_BUFFER_OUTPUT, format,
                                           _frameSize.x(), _frameSize.y());
    _context[CUDA_OUTPUT_BUFFER]->set(_outputBuffer);

    _accumBuffer =
        _context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4,
                               _frameSize.x(), _frameSize.y());
    _context[CUDA_ACCUMULATION_BUFFER]->set(_accumBuffer);

    if (_accumulationType == AccumulationType::ai_denoised)
    {
        _tonemappedBuffer =
            _context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4,
                                   _frameSize.x(), _frameSize.y());
        _context[CUDA_TONEMAPPED_BUFFER]->set(_tonemappedBuffer);

        _denoisedBuffer =
            _context->createBuffer(RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4,
                                   _frameSize.x(), _frameSize.y());
        _context[CUDA_DENOISED_BUFFER]->set(_denoisedBuffer);
    }
    clear();
}

void OptiXFrameBuffer::clear()
{
    _accumulationFrameNumber = 1u;
}

void OptiXFrameBuffer::map()
{
    // Post processing
    if (_accumulationType == AccumulationType::ai_denoised)
        if (!_postprocessingStagesInitialized)
            _initializePostProcessingStages();

    // Mapping
    if (!_outputBuffer)
        return;
    rtBufferMap(_outputBuffer->get(), &_colorData);

    if (_accumulationType == AccumulationType::none)
        _context[CUDA_FRAME_NUMBER]->setUint(1u);
    else
        _context[CUDA_FRAME_NUMBER]->setUint(_accumulationFrameNumber++);

    _colorBuffer = (uint8_t*)(_colorData);

    if (_accumulationType == AccumulationType::ai_denoised)
    {
        if (!_denoisedBuffer)
            return;
        const bool useDenoiser =
            (_accumulationFrameNumber > _numNonDenoisedFrames);
        if (useDenoiser)
            rtBufferMap(_denoisedBuffer->get(), &_floatData);
        else
            rtBufferMap(_tonemappedBuffer->get(), &_floatData);
        _floatBuffer = (float*)_floatData;
    }
}

void OptiXFrameBuffer::unmap()
{
    const bool useDenoiser =
        (_accumulationFrameNumber >= _numNonDenoisedFrames);
    if (_accumulationType == AccumulationType::ai_denoised)
        // Post processing stages
        if (_commandListWithDenoiser && _commandListWithoutDenoiser)
        {
            optix::Variable(_denoiserStage->queryVariable(VARIABLE_BLEND))
                ->setFloat(_denoiseBlend);
            try
            {
                if (useDenoiser)
                    _commandListWithDenoiser->execute();
                else
                    _commandListWithoutDenoiser->execute();
            }
            catch (...)
            {
            }
        }

    // Unmap
    if (!_outputBuffer)
        return;
    rtBufferUnmap(_outputBuffer->get());
    _colorBuffer = nullptr;

    if (_accumulationType == AccumulationType::ai_denoised)
    {
        if (!_denoisedBuffer)
            return;
        if (useDenoiser)
            rtBufferUnmap(_denoisedBuffer->get());
        else
            rtBufferUnmap(_tonemappedBuffer->get());
    }
    _floatBuffer = nullptr;
}

void OptiXFrameBuffer::_initializePostProcessingStages()
{
    _tonemapStage =
        _context->createBuiltinPostProcessingStage(STAGE_TONE_MAPPER);
    _tonemapStage->declareVariable(VARIABLE_INPUT_BUFFER)->set(_accumBuffer);
    _tonemapStage->declareVariable(VARIABLE_OUTPUT_BUFFER)
        ->set(_tonemappedBuffer);
    _tonemapStage->declareVariable(VARIABLE_EXPOSURE)
        ->setFloat(DEFAULT_EXPOSURE);
    _tonemapStage->declareVariable(VARIABLE_GAMMA)->setFloat(DEFAULT_GAMMA);

    _denoiserStage = _context->createBuiltinPostProcessingStage(STAGE_DENOISER);
    _denoiserStage->declareVariable(VARIABLE_INPUT_BUFFER)
        ->set(_tonemappedBuffer);
    _denoiserStage->declareVariable(VARIABLE_OUTPUT_BUFFER)
        ->set(_denoisedBuffer);
    _denoiserStage->declareVariable(VARIABLE_BLEND)->setFloat(_denoiseBlend);
    _denoiserStage->declareVariable(VARIABLE_INPUT_ALBEDO_BUFFER);
    _denoiserStage->declareVariable(VARIABLE_INPUT_NORMAL_BUFFER);

    // With denoiser
    _commandListWithDenoiser = _context->createCommandList();
    _commandListWithDenoiser->appendLaunch(0, _frameSize.x(), _frameSize.y());
    _commandListWithDenoiser->appendPostprocessingStage(_tonemapStage,
                                                        _frameSize.x(),
                                                        _frameSize.y());
    _commandListWithDenoiser->appendPostprocessingStage(_denoiserStage,
                                                        _frameSize.x(),
                                                        _frameSize.y());
    _commandListWithDenoiser->finalize();

    // Without denoiser
    _commandListWithoutDenoiser = _context->createCommandList();
    _commandListWithoutDenoiser->appendLaunch(0, _frameSize.x(),
                                              _frameSize.y());
    _commandListWithoutDenoiser->appendPostprocessingStage(_tonemapStage,
                                                           _frameSize.x(),
                                                           _frameSize.y());
    _commandListWithoutDenoiser->finalize();

    _postprocessingStagesInitialized = true;
}
}
