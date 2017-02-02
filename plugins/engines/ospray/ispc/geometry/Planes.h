/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
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

#include <brayns/common/types.h>
#include "ospray/SDK/geometry/Geometry.h"

namespace ospray
{

struct Planes : public ospray::Geometry
{
    std::string toString() const final { return "brayns::Planes"; }
    void finalize( ospray::Model *model ) final;

    float distance;
    int32 materialID;

    size_t numPlanes;
    size_t bytesPerPlane;
    int64 offset_normal;
    int64 offset_distance;
    int64 offset_materialID;

    ospray::Ref< ospray::Data > data;

    Planes();
};

} // ::brayns
