/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * Based on OSPRay implementation
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

// ospray
#include "ospray/SDK/math/vec.ih"
#include "ospray/SDK/math/box.ih"
#include "ospray/SDK/common/Ray.ih"
#include "ospray/SDK/common/Model.ih"
#include "ospray/SDK/geometry/Geometry.ih"

// embree
#include "embree2/rtcore.isph"
#include "embree2/rtcore_scene.isph"
#include "embree2/rtcore_geometry_user.isph"

struct Planes
{
    uniform Geometry geometry;

    uniform uint8 *uniform data;
    uniform Material *uniform *materialList;

    float distance; // TODO: Remove!!
    int   materialID;
    int   offset_normal;
    int   offset_distance;
    int   offset_materialID;
    int32 numPlanes;
    int32 bytesPerPlane;
};

typedef uniform float uniform_float;

static void Planes_postIntersect(
    uniform Geometry *uniform geometry,
    uniform Model *uniform model,
    varying DifferentialGeometry &dg,
    const varying Ray &ray,
    uniform int64 flags )
{
    uniform Planes *uniform this = ( uniform Planes *uniform )geometry;
    dg.geometry = geometry;
    dg.material = geometry->material;
    dg.st.x = 0.f;
    dg.st.y = 0.f;

    // Store timestamp as texture coordinate
    uniform uint8 *planePtr = this->data + this->bytesPerPlane * ray.primID;

    vec3f Ng = *(( uniform vec3f *varying )( planePtr + this->offset_normal ));
    vec3f Ns = Ng;

    if( flags & DG_NORMALIZE )
    {
        Ng = normalize( Ng );
        Ns = normalize( Ns );
    }
    if( flags & DG_FACEFORWARD )
    {
        if( dot( ray.dir, Ng ) >= 0.f ) Ng = neg( Ng );
        if( dot( ray.dir, Ns ) >= 0.f ) Ns = neg( Ns );
    }
    if(( flags & DG_MATERIALID ) && ( this->offset_materialID >= 0 ))
        dg.materialID = *(( uniform uint32 *varying )( planePtr + this->offset_materialID ));
    dg.Ng = Ng;
    dg.Ns = Ns;
}

void Planes_bounds(
    uniform Planes *uniform geometry,
    uniform size_t primID,
    uniform box3fa &bbox )
{
    uniform uint8 *uniform planePtr =
        geometry->data + geometry->bytesPerPlane*(( uniform int64 )primID );

    uniform float distance = geometry->distance;
    if (geometry->offset_distance >= 0)
        distance = *((uniform float *)(planePtr+geometry->offset_distance));

    uniform vec3f normal =
        *((uniform vec3f*)( planePtr+geometry->offset_normal ));

    const uniform float i = 1.5f;// distance; //1e5f;
#if 0
    const uniform vec3f up = make_vec3f( 1.f - abs(normalize(normal)) );
    bbox = make_box3fa( neg(up * i) + normal * distance * 0.99f, up * i + normal * distance * 1.01f);
#else
    bbox = make_box3fa( make_vec3f( -i, -i ,-i ), make_vec3f( i, i, i));
#endif
}

void Planes_intersect(
    uniform Planes *uniform geometry,
    varying Ray &ray,
    uniform size_t primID )
{
    uniform uint8 *uniform planePtr =
        geometry->data + geometry->bytesPerPlane * (( uniform int64 )primID );

    uniform float distance = geometry->distance;
    if( geometry->offset_distance >= 0 )
        distance = *((uniform float *)( planePtr + geometry->offset_distance ));

    uniform vec3f normal = *((uniform vec3f*)( planePtr + geometry->offset_normal ));

    float denom = dot( ray.dir, normal );
    if( denom == 0.f )
        denom = 1.e-5f;

    float t = -( dot( ray.org, normal ) + distance ) / denom;

    if( t > 0.f )
    {
        ray.primID = primID;
        ray.geomID = geometry->geometry.geomID;
        ray.t = t;
        ray.Ng = ( denom < 0.f ) ? normal : neg(normal);
    }
}


export void *uniform Planes_create(void *uniform cppEquivalent)
{
    uniform Planes *uniform geom = uniform new uniform Planes;
    Geometry_Constructor(&geom->geometry,cppEquivalent, Planes_postIntersect, 0, 0, 0);
    return geom;
}

export void PlanesGeometry_set(
    void  *uniform _geom,
    void  *uniform _model,
    void  *uniform data,
    int    uniform numPlanes,
    int    uniform bytesPerPlane,
    float  uniform distance,
    int    uniform materialID,
    int    uniform offset_normal,
    int    uniform offset_distance,
    int    uniform offset_materialID)
{
    uniform Planes *uniform geom = ( uniform Planes *uniform )_geom;
    uniform Model *uniform model = ( uniform Model *uniform )_model;

    uniform uint32 geomID = rtcNewUserGeometry( model->embreeSceneHandle, numPlanes );

    geom->geometry.model = model;
    geom->geometry.geomID = geomID;
    geom->numPlanes = numPlanes;
    geom->distance = distance;
    geom->data = (uniform uint8 *uniform) data;
    geom->materialID = materialID;
    geom->bytesPerPlane = bytesPerPlane;

    geom->offset_normal = offset_normal;
    geom->offset_distance = offset_distance;
    geom->offset_materialID = offset_materialID;

    rtcSetUserData( model->embreeSceneHandle, geomID, geom );
    rtcSetBoundsFunction(
                model->embreeSceneHandle, geomID,
                (uniform RTCBoundsFunc)&Planes_bounds );
    rtcSetIntersectFunction(
                model->embreeSceneHandle, geomID,
                ( uniform RTCIntersectFuncVarying )&Planes_intersect );
    rtcSetOccludedFunction(
                model->embreeSceneHandle, geomID,
                ( uniform RTCOccludedFuncVarying )&Planes_intersect );
    rtcEnable( model->embreeSceneHandle, geomID );
}
