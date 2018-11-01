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

#include "OptiXVolume.h"
#include "OptiXContext.h"
#include "OptiXUtils.h"

namespace
{
const float DEFAULT_SAMPLING = 1024.f;
const float DEFAULT_ALPHA_CORRECTION = 1.f;
} // namespace

namespace brayns
{
OptiXVolume::OptiXVolume(const Vector3ui& dimensions, const Vector3f& spacing,
                         const DataType type, VolumeParameters& params)
    : Volume(dimensions, spacing, type)
    , SharedDataVolume(dimensions, spacing, type)
    , _parameters(params)
{
    switch (type)
    {
    case DataType::FLOAT:
        _dataSize = 4;
        _optixDataType = RT_FORMAT_FLOAT;
        break;
    case DataType::UINT8:
        _dataSize = 1;
        _optixDataType = RT_FORMAT_UNSIGNED_BYTE;
        break;
    case DataType::UINT16:
        _dataSize = 2;
        _optixDataType = RT_FORMAT_UNSIGNED_INT;
        break;
    case DataType::INT16:
        _dataSize = 2;
        _optixDataType = RT_FORMAT_INT;
        break;
    case DataType::DOUBLE:
    case DataType::UINT32:
    case DataType::INT8:
    case DataType::INT32:
        throw std::runtime_error("Unsupported voxel type " +
                                 std::to_string(int(type)));
    }

    auto context = OptiXContext::get().getOptixContext();
    context["volumeDataSize"]->setUint(_dataSize);
    context["volumeVoxelType"]->setUint(
        static_cast<unsigned int>(_optixDataType));

    context["volumeDimensions"]->setUint(dimensions.x(), dimensions.y(),
                                         dimensions.z());
    const auto offset = _parameters.getOffset();
    context["volumeOffset"]->setFloat(offset.x(), offset.y(), offset.z());
    context["volumeElementSpacing"]->setFloat(spacing.x(), spacing.y(),
                                              spacing.z());

    context["volumeSamplingStep"]->setFloat(DEFAULT_SAMPLING *
                                            _parameters.getSamplingRate());
    context["volumeGradientShading"]->setUint(
        _parameters.getGradientShading() ? 1 : 0);
    context["volumeSingleShading"]->setUint(_parameters.getSingleShade() ? 1
                                                                         : 0);
    context["volumeAdaptiveSampling"]->setUint(
        _parameters.getAdaptiveSampling() ? 1 : 0);
    context["alpha_correction"]->setFloat(DEFAULT_ALPHA_CORRECTION);
}

OptiXVolume::~OptiXVolume()
{
    if (_volumeBuffer)
        _volumeBuffer->destroy();
}

void OptiXVolume::setVoxels(const void* voxels)
{
    RT_DESTROY(_volumeBuffer);
    RT_DESTROY(_emptyUnsignedByteBuffer);
    RT_DESTROY(_emptyUnsignedIntBuffer);
    RT_DESTROY(_emptyIntBuffer);
    RT_DESTROY(_emptyFloatBuffer);

    const uint64_t nbVoxels =
        _dimensions.x() * _dimensions.y() * _dimensions.z();
    const uint64_t bufferSize = _dataSize * nbVoxels;
    auto context = OptiXContext::get().getOptixContext();
    _volumeBuffer =
        context->createBuffer(RT_BUFFER_INPUT, _optixDataType, nbVoxels);
    memcpy(_volumeBuffer->map(), voxels, bufferSize);

    _emptyUnsignedByteBuffer =
        context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE, 0);
    _emptyUnsignedIntBuffer =
        context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT, 0);
    _emptyIntBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT, 0);
    _emptyFloatBuffer =
        context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT, 0);

    switch (_optixDataType)
    {
    case RT_FORMAT_FLOAT:
        context["volumeVoxelsUnsignedByte"]->setBuffer(
            _emptyUnsignedByteBuffer);
        context["volumeVoxelsUnsignedInt"]->setBuffer(_emptyUnsignedIntBuffer);
        context["volumeVoxelsInt"]->setBuffer(_emptyIntBuffer);
        context["volumeVoxelsFloat"]->setBuffer(_volumeBuffer);
        break;
    case RT_FORMAT_UNSIGNED_BYTE:
        context["volumeVoxelsUnsignedByte"]->setBuffer(_volumeBuffer);
        context["volumeVoxelsUnsignedInt"]->setBuffer(_emptyUnsignedIntBuffer);
        context["volumeVoxelsInt"]->setBuffer(_emptyIntBuffer);
        context["volumeVoxelsFloat"]->setBuffer(_emptyFloatBuffer);
        break;
    case RT_FORMAT_UNSIGNED_INT:
        context["volumeVoxelsUnsignedByte"]->setBuffer(
            _emptyUnsignedByteBuffer);
        context["volumeVoxelsUnsignedInt"]->setBuffer(_volumeBuffer);
        context["volumeVoxelsInt"]->setBuffer(_emptyIntBuffer);
        context["volumeVoxelsFloat"]->setBuffer(_emptyFloatBuffer);
        break;
    case RT_FORMAT_INT:
        context["volumeVoxelsUnsignedByte"]->setBuffer(
            _emptyUnsignedByteBuffer);
        context["volumeVoxelsUnsignedInt"]->setBuffer(_emptyUnsignedIntBuffer);
        context["volumeVoxelsInt"]->setBuffer(_volumeBuffer);
        context["volumeVoxelsFloat"]->setBuffer(_emptyFloatBuffer);
        break;
    default:
        throw std::runtime_error("Unsupported voxel type " +
                                 std::to_string(int(_optixDataType)));
    }
    _volumeBuffer->unmap();
}

void OptiXVolume::setDataRange(const Vector2f& range)
{
    auto context = OptiXContext::get().getOptixContext();
    context["volumeDataRange"]->setUint(range.x(), range.y());
    markModified();
}

void OptiXVolume::commit()
{
    if (_parameters.isModified())
    {
        auto context = OptiXContext::get().getOptixContext();
        context["volumeSamplingStep"]->setFloat(DEFAULT_SAMPLING *
                                                _parameters.getSamplingRate());
        context["volumeGradientShading"]->setUint(
            _parameters.getGradientShading() ? 1 : 0);
        context["volumeSingleShading"]->setUint(
            _parameters.getSingleShade() ? 1 : 0);
        context["volumeAdaptiveSampling"]->setUint(
            _parameters.getAdaptiveSampling() ? 1 : 0);
    }
    resetModified();
}

} // namespace brayns
