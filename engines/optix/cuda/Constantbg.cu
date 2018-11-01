/* 
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <optix_world.h>

#include "renderer/VolumeRenderer.h"
#include "CommonStructs.h"

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(float3, bg_color, , );
rtTextureSampler<float4, 2> envmap;
rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );

// Constant background
RT_PROGRAM void miss()
{
    // Volume
    const float4 volumeContribution = getVolumeContribution(ray, INFINITY);
    float4 result = make_float4(bg_color, 0.f);
    if(volumeContribution.w > 0.f)
        result = volumeContribution;
    prd_radiance.result = make_float3(result);
}

// Environment map background
RT_PROGRAM void envmap_miss()
{
    float4 result = getVolumeContribution(ray, INFINITY);
    if(result.w == 0.f)
    {
        float theta = atan2f( ray.direction.x, ray.direction.z );
        float phi   = M_PIf * 0.5f -  acosf( ray.direction.y );
        float u     = (theta + M_PIf) * (0.5f * M_1_PIf);
        float v     = -0.5f * ( 1.0f + sin(phi) );
        result = tex2D(envmap, u, v);
    }
    prd_radiance.result = make_float3(result);
}
