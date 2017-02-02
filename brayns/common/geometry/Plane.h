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

#ifndef PLANE_H
#define PLANE_H

#include "Primitive.h"

#include <brayns/api.h>
#include <brayns/common/types.h>

namespace brayns
{

class Plane : public Primitive
{

public:

    BRAYNS_API Plane(
        size_t materialId,
        const Vector3f& normal,
        float distance);

    BRAYNS_API const Vector3f& getNormal() const { return _normal; }
    BRAYNS_API float getDistance() const { return _distance; }

    BRAYNS_API virtual size_t serializeData(floats& serializedData);
    BRAYNS_API static size_t getSerializationSize();

protected:

    Vector3f _normal;
    float _distance;

};

}

#endif // PLANE_H
