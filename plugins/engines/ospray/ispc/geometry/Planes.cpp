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

#include <vector>

// ospray
#include "Planes.h"
#include "ospray/SDK/common/Data.h"
#include "ospray/SDK/common/Model.h"
// ispc-generated files
#include "Planes_ispc.h"

namespace ospray
{

Planes::Planes()
{
    this->ispcEquivalent = ispc::Planes_create(this);
}

void Planes::finalize(ospray::Model *model)
{
    distance = getParam1f( "distance", 0.01f );
    materialID = getParam1i( "materialID", 0 );
    bytesPerPlane = getParam1i( "bytes_per_plane", 4 * sizeof( float ));
    offset_normal = getParam1i( "offset_normal", 0 );
    offset_distance = getParam1i( "offset_distance", -1 );
    offset_materialID = getParam1i( "offset_materialID", -1 );
    data = getParamData( "planes", nullptr );

    if (data.ptr == nullptr)
        throw std::runtime_error("#ospray:geometry/planes: " \
                                 "no 'planes' data specified");
    numPlanes = data->numBytes / bytesPerPlane;

    if (numPlanes >= (1ULL << 30))
    {
        throw std::runtime_error("#brayns::Planes: too many "\
                                 "planes in this geometry. Consider "\
                                 "splitting this geometry in multiple "\
                                 "geometries with fewer planes (you "\
                                 "can still put all those geometries into a "\
                                 "single model, but you can't put that many "\
                                 "planes into a single geometry "\
                                 "without causing address overflows)");
    }

    ispc::PlanesGeometry_set(
        getIE(),model->getIE(), data->data, numPlanes, bytesPerPlane,
        distance, materialID, offset_normal, offset_distance, offset_materialID );
}

OSP_REGISTER_GEOMETRY( Planes, planes );

} // ::brayns
