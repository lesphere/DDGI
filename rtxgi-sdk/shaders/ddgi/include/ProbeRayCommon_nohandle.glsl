#ifndef RTXGI_DDGI_PROBE_RAY_COMMON_GLSL
#define RTXGI_DDGI_PROBE_RAY_COMMON_GLSL

#include "rtxgi-sdk/shaders/ddgi/include/Common.glsl"

//------------------------------------------------------------------------
// Probe Ray Data Texture Write Helpers
//------------------------------------------------------------------------

void DDGIStoreProbeRayMiss(uint RayDataIndex, uvec3 coords, DDGIVolumeDescGPU volume, vec3 radiance) {
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        imageStore(Image2DArray_rgba32f[RayDataIndex], ivec3(coords), vec4(radiance, 1e27f));
    }
}

void DDGIStoreProbeRayFrontfaceHit(uint RayDataIndex, uvec3 coords, DDGIVolumeDescGPU volume, vec3 radiance, float hitT) {
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        // Store color components and hit distance as 32-bit float values.
        imageStore(Image2DArray_rgba32f[RayDataIndex], ivec3(coords), vec4(radiance, hitT));
    }
}

void DDGIStoreProbeRayFrontfaceHit(uint RayDataIndex, uvec3 coords, DDGIVolumeDescGPU volume, float hitT) {
    vec4 data = imageLoad(Image2DArray_rgba32f[RayDataIndex], ivec3(coords));
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        data.w = hitT;
    }
    imageStore(Image2DArray_rgba32f[RayDataIndex], ivec3(coords), data);
}



void DDGIStoreProbeRayBackfaceHit(uint RayDataIndex, uvec3 coords, DDGIVolumeDescGPU volume, float hitT) {
    // Make the hit distance negative to mark a backface hit for blending, probe relocation, and probe classification.
    // Shorten the hit distance on a backface hit by 80% to decrease the influence of the probe during irradiance sampling.
    vec4 data = imageLoad(Image2DArray_rgba32f[RayDataIndex], ivec3(coords));
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4) {
        data.w = -hitT * 0.2f;
    }
    imageStore(Image2DArray_rgba32f[RayDataIndex], ivec3(coords), data);
}

//------------------------------------------------------------------------
// Probe Ray Data Texture Read Helpers
//------------------------------------------------------------------------

vec3 DDGILoadProbeRayRadiance(uint RayDataIndex, uvec3 coords, DDGIVolumeDescGPU volume)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        return imageLoad(Image2DArray_rgba32f[RayDataIndex], ivec3(coords)).rgb;
    }
    return vec3(0.f, 0.f, 0.f);
}

float DDGILoadProbeRayDistance(uint RayDataIndex, uvec3 coords, DDGIVolumeDescGPU volume)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        return imageLoad(Image2DArray_rgba32f[RayDataIndex], ivec3(coords)).a;
    }
    return 0.f;
}

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
