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

#include "OptiXContext.h"
#include "OptiXUtils.h"

#include <brayns/common/log.h>
#include <brayns/common/material/Texture2D.h>

namespace brayns
{
std::unique_ptr<OptiXContext> OptiXContext::_context;

OptiXContext::OptiXContext()
{
    _printSystemInformation();
    _initialize();
}

OptiXContext::~OptiXContext()
{
    RT_DESTROY(_anyHit);
    RT_DESTROY(_closestHit);
    RT_DESTROY(_closestHitTextured);
    RT_DESTROY(_optixContext);
}

OptiXContext& OptiXContext::get()
{
    if (!_context)
        _context.reset(new OptiXContext());

    return *_context;
}

::optix::Material OptiXContext::createMaterial(const bool textured)
{
    const auto& ptxName = CUDA_ADVANCED_SIMULATION_RENDERER;
    if (!_closestHit)
        _closestHit = _optixContext->createProgramFromPTXString(
            ptxName, CUDA_FUNC_CLOSEST_HIT_RADIANCE);

    if (!_closestHitTextured)
        _closestHitTextured = _optixContext->createProgramFromPTXString(
            ptxName, CUDA_FUNC_CLOSEST_HIT_RADIANCE_TEXTURED);

    if (!_anyHit)
        _anyHit = _optixContext->createProgramFromPTXString(ptxName,
                                                            CUDA_FUNC_ANY_HIT);

    auto optixMaterial = _optixContext->createMaterial();
    optixMaterial->setClosestHitProgram(0, textured ? _closestHitTextured
                                                    : _closestHit);
    optixMaterial->setAnyHitProgram(1, _anyHit);
    return optixMaterial;
}

::optix::Program OptiXContext::createCamera(const bool environmentMap)
{
    ::optix::Program camera;
    // Ray generation program
    camera =
        _optixContext->createProgramFromPTXString(CUDA_PERSPECTIVE_CAMERA,
                                                  CUDA_FUNC_PERSPECTIVE_CAMERA);
    _optixContext->setRayGenerationProgram(0, camera);

    // Miss programs
    ::optix::Program missProgram;
    if (environmentMap)
        missProgram = _optixContext->createProgramFromPTXString(
            CUDA_MISS, CUDA_FUNC_CAMERA_ENVMAP_MISS);
    else
        missProgram =
            _optixContext->createProgramFromPTXString(CUDA_MISS,
                                                      CUDA_FUNC_CAMERA_MISS);
    _optixContext->setMissProgram(0, missProgram);

    // Exception program
    _optixContext->setExceptionProgram(
        0,
        _optixContext->createProgramFromPTXString(CUDA_PERSPECTIVE_CAMERA,
                                                  CUDA_FUNC_CAMERA_EXCEPTION));
    BRAYNS_DEBUG << "Camera created" << std::endl;
    return camera;
}

::optix::TextureSampler OptiXContext::createTextureSampler(Texture2DPtr texture)
{
    const uint16_t nx = texture->getWidth();
    const uint16_t ny = texture->getHeight();
    const uint16_t channels = texture->getNbChannels();
    const uint16_t optixChannels = 4;

    // Create texture sampler
    ::optix::TextureSampler sampler = _optixContext->createTextureSampler();
    sampler->setWrapMode(0, RT_WRAP_REPEAT);
    sampler->setWrapMode(1, RT_WRAP_REPEAT);
    sampler->setWrapMode(2, RT_WRAP_REPEAT);
    sampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
    sampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
    sampler->setMaxAnisotropy(1.0f);
    sampler->setMipLevelCount(1u);
    sampler->setArraySize(1u);

    // Create buffer and populate with texture data
    optix::Buffer buffer =
        _optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, nx, ny);
    float* buffer_data = static_cast<float*>(buffer->map());

    size_t idx_src = 0;
    size_t idx_dst = 0;
    for (uint16_t y = 0; y < ny; ++y)
        for (uint16_t x = 0; x < nx; ++x)
        {
            buffer_data[idx_dst + 0] =
                float(texture->getRawData()[idx_src + 2]) / 255.f;
            buffer_data[idx_dst + 1] =
                float(texture->getRawData()[idx_src + 1]) / 255.f;
            buffer_data[idx_dst + 2] =
                float(texture->getRawData()[idx_src + 0]) / 255.f;
            buffer_data[idx_dst + 3] =
                (channels == optixChannels)
                    ? float(texture->getRawData()[idx_src + 4]) / 255.f
                    : 1.f;
            idx_dst += 4;
            idx_src += (channels == optixChannels) ? 4 : 3;
        }
    buffer->unmap();

    // Assign buffer to sampler
    sampler->setBuffer(buffer);
    sampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR,
                               RT_FILTER_NONE);
    return sampler;
}

void OptiXContext::_initialize()
{
    BRAYNS_DEBUG << "Creating context..." << std::endl;
    _optixContext = ::optix::Context::create();

    if (!_optixContext)
        throw(std::runtime_error("Failed to initialize OptiX"));

    _optixContext->setRayTypeCount(2);
    _optixContext->setEntryPointCount(1);
    _optixContext->setStackSize(2800);

    _bounds[OptixGeometryType::cone] =
        _optixContext->createProgramFromPTXString(CUDA_CONES, CUDA_FUNC_BOUNDS);
    _intersects[OptixGeometryType::cone] =
        _optixContext->createProgramFromPTXString(CUDA_CONES,
                                                  CUDA_FUNC_INTERSECTION);

    _bounds[OptixGeometryType::cylinder] =
        _optixContext->createProgramFromPTXString(CUDA_CYLINDERS,
                                                  CUDA_FUNC_BOUNDS);
    _intersects[OptixGeometryType::cylinder] =
        _optixContext->createProgramFromPTXString(CUDA_CYLINDERS,
                                                  CUDA_FUNC_INTERSECTION);

    _bounds[OptixGeometryType::sphere] =
        _optixContext->createProgramFromPTXString(CUDA_SPHERES,
                                                  CUDA_FUNC_BOUNDS);
    _intersects[OptixGeometryType::sphere] =
        _optixContext->createProgramFromPTXString(CUDA_SPHERES,
                                                  CUDA_FUNC_INTERSECTION);

    _bounds[OptixGeometryType::trianglesMesh] =
        _optixContext->createProgramFromPTXString(CUDA_TRIANGLES_MESH,
                                                  CUDA_FUNC_BOUNDS);
    _intersects[OptixGeometryType::trianglesMesh] =
        _optixContext->createProgramFromPTXString(CUDA_TRIANGLES_MESH,
                                                  CUDA_FUNC_INTERSECTION);
    BRAYNS_DEBUG << "Context created" << std::endl;
}

void OptiXContext::_printSystemInformation() const
{
    unsigned int optixVersion;
    RT_CHECK_ERROR_NO_CONTEXT(rtGetVersion(&optixVersion));

    unsigned int major = optixVersion / 1000; // Check major with old formula.
    unsigned int minor;
    unsigned int micro;
    if (3 < major) // New encoding since OptiX 4.0.0 to get two digits micro
                   // numbers?
    {
        major = optixVersion / 10000;
        minor = (optixVersion % 10000) / 100;
        micro = optixVersion % 100;
    }
    else // Old encoding with only one digit for the micro number.
    {
        minor = (optixVersion % 1000) / 10;
        micro = optixVersion % 10;
    }
    BRAYNS_INFO << "OptiX " << major << "." << minor << "." << micro
                << std::endl;

    unsigned int numberOfDevices = 0;
    RT_CHECK_ERROR_NO_CONTEXT(rtDeviceGetDeviceCount(&numberOfDevices));
    BRAYNS_INFO << "Number of Devices = " << numberOfDevices << std::endl;

    for (unsigned int i = 0; i < numberOfDevices; ++i)
    {
        char name[256];
        RT_CHECK_ERROR_NO_CONTEXT(rtDeviceGetAttribute(i,
                                                       RT_DEVICE_ATTRIBUTE_NAME,
                                                       sizeof(name), name));
        BRAYNS_INFO << "Device " << i << ": " << name << std::endl;

        int computeCapability[2] = {0, 0};
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY,
                                 sizeof(computeCapability),
                                 &computeCapability));
        BRAYNS_INFO << "  Compute Support: " << computeCapability[0] << "."
                    << computeCapability[1] << std::endl;

        RTsize totalMemory = 0;
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_TOTAL_MEMORY,
                                 sizeof(totalMemory), &totalMemory));
        BRAYNS_INFO << "  Total Memory: "
                    << (unsigned long long)(totalMemory / 1024 / 1024) << " MB"
                    << std::endl;

        int clockRate = 0;
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_CLOCK_RATE,
                                 sizeof(clockRate), &clockRate));
        BRAYNS_INFO << "  Clock Rate: " << (clockRate / 1000) << " MHz"
                    << std::endl;

        int maxThreadsPerBlock = 0;
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK,
                                 sizeof(maxThreadsPerBlock),
                                 &maxThreadsPerBlock));
        BRAYNS_INFO << "  Max. Threads per Block: " << maxThreadsPerBlock
                    << std::endl;

        int smCount = 0;
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,
                                 sizeof(smCount), &smCount));
        BRAYNS_INFO << "  Streaming Multiprocessor Count: " << smCount
                    << std::endl;

        int executionTimeoutEnabled = 0;
        RT_CHECK_ERROR_NO_CONTEXT(rtDeviceGetAttribute(
            i, RT_DEVICE_ATTRIBUTE_EXECUTION_TIMEOUT_ENABLED,
            sizeof(executionTimeoutEnabled), &executionTimeoutEnabled));
        BRAYNS_INFO << "  Execution Timeout Enabled: "
                    << executionTimeoutEnabled << std::endl;

        int maxHardwareTextureCount = 0;
        RT_CHECK_ERROR_NO_CONTEXT(rtDeviceGetAttribute(
            i, RT_DEVICE_ATTRIBUTE_MAX_HARDWARE_TEXTURE_COUNT,
            sizeof(maxHardwareTextureCount), &maxHardwareTextureCount));
        BRAYNS_INFO << "  Max. Hardware Texture Count: "
                    << maxHardwareTextureCount << std::endl;

        int tccDriver = 0;
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_TCC_DRIVER,
                                 sizeof(tccDriver), &tccDriver));
        BRAYNS_INFO << "  TCC Driver enabled: " << tccDriver << std::endl;

        int cudaDeviceOrdinal = 0;
        RT_CHECK_ERROR_NO_CONTEXT(
            rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_CUDA_DEVICE_ORDINAL,
                                 sizeof(cudaDeviceOrdinal),
                                 &cudaDeviceOrdinal));
        BRAYNS_INFO << "  CUDA Device Ordinal: " << cudaDeviceOrdinal
                    << std::endl;
    }
}

::optix::Geometry OptiXContext::createGeometry(const OptixGeometryType type)
{
    ::optix::Geometry geometry = _optixContext->createGeometry();
    geometry->setBoundingBoxProgram(_bounds[type]);
    geometry->setIntersectionProgram(_intersects[type]);
    return geometry;
}

::optix::GeometryGroup OptiXContext::createGeometryGroup()
{
    auto group = _optixContext->createGeometryGroup();
    group->setAcceleration(
        _optixContext->createAcceleration(DEFAULT_ACCELERATION_STRUCTURE));
    return group;
}

::optix::Group OptiXContext::createGroup()
{
    auto group = _optixContext->createGroup();
    group->setAcceleration(
        _optixContext->createAcceleration(DEFAULT_ACCELERATION_STRUCTURE));
    return group;
}
} // namespace brayns
