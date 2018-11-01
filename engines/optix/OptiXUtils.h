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

#ifndef OPTIXUTILS_H
#define OPTIXUTILS_H

#include <engines/optix/brayns_optix_engine_generated_AdvancedSimulationRenderer.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_BasicRenderer.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_Cones.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_Constantbg.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_Cylinders.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_PerspectiveCamera.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_Spheres.cu.ptx.h>
#include <engines/optix/brayns_optix_engine_generated_TrianglesMesh.cu.ptx.h>

#include <optixu/optixpp_namespace.h>

#include <brayns/common/PropertyMap.h>
#include <brayns/common/types.h>

#define RT_DESTROY(__object) \
    if (__object)            \
        __object->destroy(); \
    __object = nullptr;

// Error check/report helper for users of the C API
#define RT_CHECK_ERROR(func)                             \
    do                                                   \
    {                                                    \
        RTresult code = func;                            \
        if (code != RT_SUCCESS)                          \
            throw std::runtime_error("OptiX exception"); \
    } while (0)

#define RT_CHECK_ERROR_NO_CONTEXT(func)                            \
    do                                                             \
    {                                                              \
        RTresult code = func;                                      \
        if (code != RT_SUCCESS)                                    \
            throw std::runtime_error("Optix error in function '" + \
                                     std::string(#func) + "'");    \
    } while (0)

namespace
{
// Defaults
const float DEFAULT_EXPOSURE = 1.5f;
const float DEFAULT_GAMMA = 1.0f;

// Programs
const std::string DEFAULT_ACCELERATION_STRUCTURE = "Trbvh";
const std::string CUDA_SPHERES = brayns_optix_engine_generated_Spheres_cu_ptx;
const std::string CUDA_CYLINDERS =
    brayns_optix_engine_generated_Cylinders_cu_ptx;
const std::string CUDA_CONES = brayns_optix_engine_generated_Cones_cu_ptx;
const std::string CUDA_TRIANGLES_MESH =
    brayns_optix_engine_generated_TrianglesMesh_cu_ptx;
const std::string CUDA_ADVANCED_SIMULATION_RENDERER =
    brayns_optix_engine_generated_AdvancedSimulationRenderer_cu_ptx;
const std::string CUDA_BASIC_RENDERER =
    brayns_optix_engine_generated_BasicRenderer_cu_ptx;
const std::string CUDA_PERSPECTIVE_CAMERA =
    brayns_optix_engine_generated_PerspectiveCamera_cu_ptx;
const std::string CUDA_MISS = brayns_optix_engine_generated_Constantbg_cu_ptx;

// Buffers
const std::string CUDA_BUFFER_ACCUMULATION = "accum_buffer";
const std::string CUDA_BUFFER_OUTPUT = "output_buffer";
const std::string CUDA_BUFFER_DENOISED = "denoised_buffer";
const std::string CUDA_BUFFER_TONEMAPPED = "tonemapped_buffer";
const std::string CUDA_BUFFER_INPUT_ALBEDO = "input_albedo_buffer";
const std::string CUDA_BUFFER_INPUT_NORMAL = "input_normal_buffer";

// Functions
const std::string CUDA_FUNC_BOUNDS = "bounds";
const std::string CUDA_FUNC_INTERSECTION = "intersect";
const std::string CUDA_FUNC_ROBUST_INTERSECTION = "robust_intersect";
const std::string CUDA_FUNC_EXCEPTION = "exception";
const std::string CUDA_FUNC_PERSPECTIVE_CAMERA = "perpectiveCamera";
const std::string CUDA_FUNC_CAMERA_EXCEPTION = "exception";
const std::string CUDA_FUNC_CAMERA_ENVMAP_MISS = "envmap_miss";
const std::string CUDA_FUNC_CAMERA_MISS = "miss";

const std::string CUDA_FUNC_CLOSEST_HIT_RADIANCE = "closest_hit_radiance";
const std::string CUDA_FUNC_CLOSEST_HIT_RADIANCE_TEXTURED =
    "closest_hit_radiance_textured";
const std::string CUDA_FUNC_ANY_HIT = "any_hit";

// Stages
const std::string CUDA_STAGE_TONE_MAPPER = "TonemapperSimple";
const std::string CUDA_STAGE_DENOISER = "DLDenoiser";

// Attributes
const std::string CUDA_ATTR_EXPOSURE = "exposure";
const std::string CUDA_ATTR_GAMMA = "gamma";
const std::string CUDA_ATTR_BLEND = "blend";

const std::string CUDA_ATTR_BUFFER_INPUT = "input_buffer";
const std::string CUDA_ATTR_BUFFER_OUTPUT = "output_buffer";
const std::string CUDA_ATTR_FRAME = "frame";
const std::string CUDA_ATTR_CAMERA_BAD_COLOR = "bad_color";
const std::string CUDA_ATTR_CAMERA_OFFSET = "offset";
const std::string CUDA_ATTR_CAMERA_EYE = "eye";
const std::string CUDA_ATTR_CAMERA_U = "U";
const std::string CUDA_ATTR_CAMERA_V = "V";
const std::string CUDA_ATTR_CAMERA_W = "W";
const std::string CUDA_ATTR_CAMERA_APERTURE_RADIUS = "aperture_radius";
const std::string CUDA_ATTR_CAMERA_FOCAL_SCALE = "focal_scale";
const std::string CUDA_ATTR_CLIP_PLANES = "clip_planes";
const std::string CUDA_ATTR_NB_CLIP_PLANES = "nb_clip_planes";

} // namespace

namespace brayns
{
void setOptiXProperties(::optix::Context context,
                        const PropertyMap& properties);
}

#endif // OPTIXUTILS_H
