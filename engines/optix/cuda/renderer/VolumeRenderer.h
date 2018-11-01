#include "../CommonStructs.h"
#include "../Random.h"

//#define TEST_NORMALS

// Scene
rtDeclareVariable(unsigned int, frame, , );
rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );

// Rendering
rtDeclareVariable(float, alpha_correction, , );
rtDeclareVariable(float, shadows, , );
rtDeclareVariable(float, softShadows, , );

// Lights
rtBuffer<BasicLight> lights;

// Volume
rtBuffer<unsigned char> volumeVoxelsUnsignedByte;
rtBuffer<float> volumeVoxelsFloat;
rtBuffer<unsigned int> volumeVoxelsUnsignedInt;
rtBuffer<int> volumeVoxelsInt;

rtDeclareVariable(uint3, volumeDimensions, , );
rtDeclareVariable(float3, volumeOffset, , );
rtDeclareVariable(float3, volumeElementSpacing, , );
rtDeclareVariable(float, volumeSamplingStep, , );
rtDeclareVariable(uint, volumeGradientShading, , );
rtDeclareVariable(uint, volumeSingleShading, , );
rtDeclareVariable(uint, volumeAdaptiveSampling, , );
rtDeclareVariable(uint, volumeVoxelType, , );
rtDeclareVariable(uint, volumeDataSize, , );

// Color map
rtBuffer<float4> colorMap;
rtBuffer<float3> emissionIntensityMap;
rtDeclareVariable(float, colorMapMinValue, , );
rtDeclareVariable(float, colorMapRange, , );
rtDeclareVariable(uint, colorMapSize, , );
rtBuffer<uchar4, 2> output_buffer;

static __device__ bool intersectBox(const optix::Ray& ray, const float3 aabbMin,
                                    const float3 aabbMax, float& t0, float& t1)
{
    optix::Ray r = ray;
    // We need to avoid division by zero in "vec3 invR = 1.0 / r.Dir;"
    if (r.direction.x == 0)
        r.direction.x = DEFAULT_EPSILON;

    if (r.direction.y == 0)
        r.direction.y = DEFAULT_EPSILON;

    if (r.direction.z == 0)
        r.direction.z = DEFAULT_EPSILON;

    const float3 invR = 1.f / r.direction;
    const float3 tbot = invR * (aabbMin - r.origin);
    const float3 ttop = invR * (aabbMax - r.origin);
    const float3 tmin = fminf(ttop, tbot);
    const float3 tmax = fmaxf(ttop, tbot);
    float2 t = make_float2(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    t0 = max(t.x, t.y);
    t = make_float2(min(tmax.x, tmax.y), min(tmax.x, tmax.z));
    t1 = min(t.x, t.y);
    return (t0 <= t1);
}

static __device__ void composite(const float4& src, float4& dst,
                                 float alphaCorrection = 1.f)
{
    const float alpha =
        1.f - pow(1.f - min(src.w, 1.f - 1.f / 1024.f), alphaCorrection);
    const float a = alpha * (1.f - dst.w);
    dst.x = dst.x + src.x * a;
    dst.y = dst.y + src.y * a;
    dst.z = dst.z + src.z * a;
    dst.w += (alpha * (1.f - dst.w));
}

static __device__ bool getVoxelColor(const float3& point, float4& voxelColor)
{
    if (point.x > 0.f && point.x < volumeDimensions.x && point.y > 0.f &&
        point.y < volumeDimensions.y && point.z > 0.f &&
        point.z < volumeDimensions.z)
    {
#ifdef TEST_RGB
        voxelColor = make_float4(point.x / volumeDimensions.x,
                                 point.y / volumeDimensions.y,
                                 point.z / volumeDimensions.z, 0.1f);
#else
        const ulong index = (ulong)(
            (ulong)floor(point.x) + (ulong)floor(point.y) * volumeDimensions.x +
            (ulong)floor(point.z) * volumeDimensions.x * volumeDimensions.y);

        float voxelValue;
        switch (volumeVoxelType)
        {
        case RT_FORMAT_UNSIGNED_BYTE:
        {
            voxelValue = volumeVoxelsUnsignedByte[index];
            break;
        }
        case RT_FORMAT_FLOAT:
        {
            voxelValue = volumeVoxelsFloat[index];
            break;
        }
        case RT_FORMAT_UNSIGNED_INT:
        {
            voxelValue = volumeVoxelsUnsignedInt[index * volumeDataSize];
            break;
        }
        case RT_FORMAT_INT:
        {
            voxelValue = volumeVoxelsInt[index];
            break;
        }
        }

        const float normalizedValue = (float)colorMapSize *
                                      (voxelValue - colorMapMinValue) /
                                      colorMapRange;
        voxelColor = colorMap[normalizedValue];
#endif
        return true;
    }
    return false;
}

static __device__ float getVolumeShadowContribution(const optix::Ray& volumeRay)
{
    // Volume bounding box
    float t0, t1;
    const float3 aabbMin = volumeOffset;
    const float3 aabbMax = volumeOffset + make_float3(volumeDimensions);
    if (!intersectBox(volumeRay, aabbMin, aabbMax, t0, t1))
        return 0.f;

    // Ray marching
    float t = t1;
    float shadowIntensity = 0.f;

    while (t > max(t0, volumeSamplingStep) && shadowIntensity < 1.f)
    {
        const float3 point = volumeRay.origin - volumeRay.direction * t;
        float4 voxelColor;
        if (getVoxelColor(point, voxelColor))
            if (voxelColor.w >= DEFAULT_VOLUME_SHADING_THRESHOLD)
                shadowIntensity += voxelColor.w;
        t -= volumeSamplingStep;
    }
    return shadowIntensity;
}

static __device__ float4 getVolumeContribution(const optix::Ray& volumeRay,
                                               const float hit)
{
    if (colorMapSize == 0)
        return make_float4(0.f, 1.f, 0.f, 0.f);

    float4 pathColor = make_float4(0.f);

    // Volume bounding box
    float t0, t1;
    const float3 aabbMin = volumeOffset;
    const float3 aabbMax = volumeOffset + make_float3(volumeDimensions);
    if (!intersectBox(volumeRay, aabbMin, aabbMax, t0, t1))
        return make_float4(0.f);

    // Ray marching
    float samplingStep = volumeSamplingStep;
    if (volumeAdaptiveSampling == 1)
        samplingStep = max(1.f, volumeSamplingStep - frame);

    optix::size_t2 screen = output_buffer.size();
    unsigned int seed =
        tea<16>(screen.x * launch_index.y + launch_index.x, frame);
    const float delta = rnd(seed) * samplingStep;

    float t = max(samplingStep, t0) + samplingStep - delta;
    const float tMax = min(hit, t1);

    while (t < tMax && pathColor.w < 1.f)
    {
        const float3 point =
            ((volumeRay.origin + volumeRay.direction * t) - volumeOffset) /
            volumeElementSpacing;

        const float3 neighbours[7] = {{0.f, 0.f, 0.f},  {-1.f, 0.f, 0.f},
                                      {1.f, 0.f, 0.f},  {0.f, 1.f, 0.f},
                                      {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f},
                                      {0.f, 0.f, -1.f}};

        float4 colorMapColor = make_float4(0.f);

        const unsigned int nbIterations = (volumeGradientShading == 1 ? 7 : 1);
        float3 normal = make_float3(0.f, 0.f, 0.f);
        const float3 s = volumeElementSpacing;
        float voxelOpacity = 0.f;

        unsigned int count = 0;
        float4 voxelColor;
        for (unsigned int i = 0; i < nbIterations; ++i)
            if (getVoxelColor(point + s * neighbours[i], voxelColor))
            {
                if (count == 0)
                    voxelOpacity = voxelColor.w;

                normal += neighbours[i] * voxelColor.w;
                colorMapColor += voxelColor;
                ++count;
            }

        colorMapColor /= (float)count;
        colorMapColor.w = voxelOpacity;
        normal = optix::normalize(normal);

        if (volumeGradientShading == 1 &&
            voxelOpacity > DEFAULT_VOLUME_SHADING_THRESHOLD)
        {
#ifdef TEST_NORMALS
            const float3 n = 0.5f + 0.5f * normal;
            colorMapColor = make_float4(n.x, n.y, n.z, voxelOpacity);
#else
            for (int i = 0; i < lights.size(); ++i)
            {
                BasicLight light = lights[i];
                float3 direction;
                const float3 random =
                    softShadows > 0.f
                        ? softShadows * make_float3(rnd(seed) - 0.5f,
                                                    rnd(seed) - 0.5f,
                                                    rnd(seed) - 0.5f)
                        : make_float3(0.f);

                if (light.type == 0)
                {
                    // Point light
                    float3 pos = light.pos;
                    direction = optix::normalize(pos + random - point);
                }
                else
                {
                    // Directional light
                    float3 pos = point - light.dir;
                    direction =
                        optix::normalize((pos + 1000.f * random) - point);
                }

                // Light ray
                optix::Ray lightRay;
                lightRay.origin = point;
                lightRay.direction = direction;

                // Phong shading
                float cosNL =
                    max(0.f, optix::dot(normal, -1.f * lightRay.direction));

                // Shadows
                float shadowIntensity = 1.f;
                if (shadows > 0.f)
                {
                    shadowIntensity = getVolumeShadowContribution(lightRay);
                    shadowIntensity = 1.f - shadows * shadowIntensity;
                }

                // Combine shadows and shading
                const float diffuse = DEFAULT_SHADING_ALPHA *
                                      (0.5f + 0.5f * cosNL) * shadowIntensity;
                colorMapColor.x = colorMapColor.x * diffuse;
                colorMapColor.y = colorMapColor.y * diffuse;
                colorMapColor.z = colorMapColor.z * diffuse;
            }
#endif
        }

        composite(colorMapColor, pathColor, samplingStep);
        t += samplingStep;
    }

    return make_float4(max(0.f, min(1.f, pathColor.z)),
                       max(0.f, min(1.f, pathColor.y)),
                       max(0.f, min(1.f, pathColor.x)),
                       max(0.f, min(1.f, pathColor.w)));
}
