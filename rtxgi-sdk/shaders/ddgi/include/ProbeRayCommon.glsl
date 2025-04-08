#ifndef RTXGI_DDGI_PROBE_RAY_COMMON_GLSL
#define RTXGI_DDGI_PROBE_RAY_COMMON_GLSL

#include "rtxgi-sdk/shaders/ddgi/include/Common.glsl"

//------------------------------------------------------------------------
// Probe Ray Data Texture Write Helpers
//------------------------------------------------------------------------

void DDGIStoreProbeRayMiss(Image2DArray_rgba32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, vec3 radiance) {
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        ImageStore(RayData, ivec3(coords), vec4(radiance, 1e27f));
    }
}

// void DDGIStoreProbeRayMiss(Image2DArray_rg32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, vec3 radiance) {
//     if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2) {
//         ImageStore(RayData, ivec3(coords), vec4(uintBitsToFloat(RTXGIFloat3ToUint(radiance)), 1e27f, 0.f, 0.f));
//     }
// }

void DDGIStoreProbeRayFrontfaceHit(Image2DArray_rgba32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, vec3 radiance, float hitT) {
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        // Store color components and hit distance as 32-bit float values.
        ImageStore(RayData, ivec3(coords), vec4(radiance, hitT));
    }
}

// void DDGIStoreProbeRayFrontfaceHit(Image2DArray_rg32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, vec3 radiance, float hitT) {
//     if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2) {
//         // Use R32G32_FLOAT format (don't use R32G32_UINT since hit distance needs to be negative sometimes).
//         // Pack color as R10G10B10 in R32 and store hit distance in G32.
//         const float c_threshold = 1.f / 255.f;
//         if (RTXGIMaxComponent(radiance.rgb) <= c_threshold) radiance.rgb = vec3(0.f, 0.f, 0.f);
//         ImageStore(RayData, ivec3(coords), vec4(uintBitsToFloat(RTXGIFloat3ToUint(radiance.rgb)), hitT, 0.f, 0.f));
//     }
// }

void DDGIStoreProbeRayFrontfaceHit(Image2DArray_rgba32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, float hitT) {
    vec4 data = ImageLoad(RayData, ivec3(coords));
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        data.w = hitT;
    }
    ImageStore(RayData, ivec3(coords), data);
}

// void DDGIStoreProbeRayFrontfaceHit(Image2DArray_rg32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, float hitT) {
//     vec4 data = ImageLoad(RayData, ivec3(coords));
//     if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2) {
//         data.g = hitT;
//     }
//     ImageStore(RayData, ivec3(coords), data);
// }

void DDGIStoreProbeRayBackfaceHit(Image2DArray_rgba32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, float hitT) {
    // Make the hit distance negative to mark a backface hit for blending, probe relocation, and probe classification.
    // Shorten the hit distance on a backface hit by 80% to decrease the influence of the probe during irradiance sampling.
    vec4 data = ImageLoad(RayData, ivec3(coords));
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        data.w = -hitT * 0.2f;
    }
    ImageStore(RayData, ivec3(coords), data);
}

// void DDGIStoreProbeRayBackfaceHit(Image2DArray_rg32f RayData, uvec3 coords, DDGIVolumeDescGPU volume, float hitT) {
//     // Make the hit distance negative to mark a backface hit for blending, probe relocation, and probe classification.
//     // Shorten the hit distance on a backface hit by 80% to decrease the influence of the probe during irradiance sampling.
//     vec4 data = ImageLoad(RayData, ivec3(coords));
//     if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2) {
//         data.g = -hitT * 0.2f;
//     }
//     ImageStore(RayData, ivec3(coords), data);
// }

//------------------------------------------------------------------------
// Probe Ray Data Texture Read Helpers
//------------------------------------------------------------------------

vec3 DDGILoadProbeRayRadiance(Image2DArray_rgba32f RayData, uvec3 coords, DDGIVolumeDescGPU volume)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        return ImageLoad(RayData, ivec3(coords)).rgb;
    }
    return vec3(0.f, 0.f, 0.f);
}

// vec3 DDGILoadProbeRayRadiance(Image2DArray_rg32f RayData, uvec3 coords, DDGIVolumeDescGPU volume)
// {
//     if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
//     {
//         return RTXGIUintToFloat3(floatBitsToUint(ImageLoad(RayData, ivec3(coords)).r));
//     }
//     return vec3(0.f, 0.f, 0.f);
// }

float DDGILoadProbeRayDistance(Image2DArray_rgba32f RayData, uvec3 coords, DDGIVolumeDescGPU volume)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        return ImageLoad(RayData, ivec3(coords)).a;
    }
    return 0.f;
}

// float DDGILoadProbeRayDistance(Image2DArray_rg32f RayData, uvec3 coords, DDGIVolumeDescGPU volume)
// {
//     if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
//     {
//         return ImageLoad(RayData, ivec3(coords)).g;
//     }
//     return 0.f;
// }

//------------------------------------------------------------------------
// Probe Ray Direction
//------------------------------------------------------------------------

/**
 * Computes a spherically distributed, normalized ray direction for the given ray index in a set of ray samples.
 * Applies the volume's random probe ray rotation transformation to "non-fixed" ray direction samples.
 */
vec3 DDGIGetProbeRayDirection(int rayIndex, DDGIVolumeDescGPU volume)
{
    bool isFixedRay = false;
    int sampleIndex = rayIndex;
    int numRays = volume.probeNumRays;

    if (volume.probeRelocationEnabled || volume.probeClassificationEnabled)
    {
        isFixedRay = (rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS);
        sampleIndex = isFixedRay ? rayIndex : (rayIndex - RTXGI_DDGI_NUM_FIXED_RAYS);
        numRays = isFixedRay ? RTXGI_DDGI_NUM_FIXED_RAYS : (numRays - RTXGI_DDGI_NUM_FIXED_RAYS);
    }

    // Get a ray direction on the sphere
    vec3 direction = RTXGISphericalFibonacci(sampleIndex, numRays);

    // Don't rotate fixed rays so relocation/classification are temporally stable
    if (isFixedRay) return normalize(direction);

    // Apply a random rotation and normalize the direction
    return normalize(RTXGIQuaternionRotate(direction, RTXGIQuaternionConjugate(volume.probeRayRotation)));
}


#endif // RTXGI_DDGI_PROBE_RAY_COMMON_GLSL
