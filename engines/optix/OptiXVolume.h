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

#pragma once

#include <brayns/common/volume/BrickedVolume.h>
#include <brayns/common/volume/SharedDataVolume.h>
#include <brayns/parameters/VolumeParameters.h>

#include <optixu/optixpp_namespace.h>

namespace brayns
{
class OptiXVolume : public SharedDataVolume, virtual public Volume
{
public:
    OptiXVolume(const Vector3ui& dimensions, const Vector3f& spacing,
                const DataType type, VolumeParameters& params);
    ~OptiXVolume();

    void setDataRange(const Vector2f& range) final;
    void commit() final;
    void setVoxels(const void* voxels) final;

protected:
    VolumeParameters& _parameters;

    ::optix::Buffer _volumeBuffer{nullptr};
    ::optix::Buffer _emptyUnsignedByteBuffer{nullptr};
    ::optix::Buffer _emptyUnsignedIntBuffer{nullptr};
    ::optix::Buffer _emptyIntBuffer{nullptr};
    ::optix::Buffer _emptyFloatBuffer{nullptr};

    ::optix::Buffer _emptyBuffer{nullptr};
    size_t _dataSize{1};
    RTformat _optixDataType;
};

using OptiXVolumePtr = std::shared_ptr<OptiXVolume>;

} // namespace brayns
