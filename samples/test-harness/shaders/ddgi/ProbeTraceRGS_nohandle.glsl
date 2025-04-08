#version 460 core

#include "samples/test-harness/shaders/include/Descriptors_nohandle.glsl"

// Setup the ray payload
layout(location = 0) rayPayloadEXT PackedPayload packedPayload;

#include "samples/test-harness/shaders/include/Common.glsl"
#include "samples/test-harness/shaders/include/Lighting_nohandle.glsl"
#include "samples/test-harness/shaders/include/RayTracing_nohandle.glsl"

#include "rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants_nohandle.glsl"
#include "rtxgi-sdk/shaders/ddgi/Irradiance_nohandle.glsl"

// ---[ Ray Generation Shader ]---

void main() {
    // Get the DDGIVolume's index (from root/push constants)
    uint volumeIndex = GetDDGIVolumeIndex();

    // Get the DDGIVolume's bindless resource indices
    DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

    // Get the DDGIVolume's constants from the structured buffer
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Compute the probe index for this thread
    int rayIndex = int(gl_LaunchIDEXT.x);                    // index of the ray to trace for this probe
    int probePlaneIndex = int(gl_LaunchIDEXT.y);             // index of this probe within the plane of probes
    int planeIndex = int(gl_LaunchIDEXT.z);                  // index of the plane this probe is part of
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);

    int probeIndex = (planeIndex * probesPerPlane) + probePlaneIndex;

    // Get the probe's grid coordinates
    ivec3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

    // Adjust the probe index for the scroll offsets
    probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

    // Get the probe data texture array index
    uint ProbeDataTexIdx = resourceIndices.probeDataSRVIndex;

    // Get the probe's state
    float probeState = DDGILoadProbeStateFromTex(probeIndex, ProbeDataTexIdx, volume);

    // Early out: do not shoot rays when the probe is inactive *unless* it is one of the "fixed" rays used by probe classification
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS) return;

    // Get the probe's world position
    // Note: world positions are computed from probe coordinates *not* adjusted for infinite scrolling
    vec3 probeWorldPosition = DDGIGetProbeWorldPositionFromTex(probeCoords, volume, ProbeDataTexIdx);

    // Get a random normalized ray direction to use for a probe ray
    vec3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, volume);

    // Get the coordinates for the probe ray in the RayData texture array
    // Note: probe index is the scroll adjusted index (if scrolling is enabled)
    uvec3 outputCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

    // Setup the probe ray
    RayDesc ray;
    ray.origin = probeWorldPosition;
    ray.direction = probeRayDirection;
    ray.tmin = 0.f;
    ray.tmax = volume.probeMaxRayDistance;

    // Initialize the ray payload
    packedPayload = PackedPayload(0.f, vec3(0.f), uvec4(0u), uvec3(0u));

    // If classification is enabled, pass the probe's state to hit shaders through the payload
    // TODO: seems not used by any hit shaders, double check
    // if(volume.probeClassificationEnabled) packedPayload.packed0.x = probeState;

    // Get the acceleration structure
    // accelerationStructureEXT SceneTLAS = viwo_acceleration_structures[params.scene.acc.handle];

    // NV_API not used in Vulkan
    // Trace the Probe Ray
    traceRayEXT(
        GetAccelerationStructure(SCENE_TLAS_INDEX),
        gl_RayFlagsNoneEXT,
        0xFFu,
        0u,
        0u,
        0u,
        ray.origin,
        ray.tmin,
        ray.direction,
        ray.tmax,
        0);

    // Get the ray data texture array
    // RWTexture2DArray<float4> RayData = GetRWTex2DArray(resourceIndices.rayDataUAVIndex);
    // How to deal with the different possible formats of RayData, Image2DArray_rgba32f or Image2DArray_rg32f ?
    // unify the format to get the best performance, assume it is Image2DArray_rgba32f
    // why need to use two different formats in the original code?
    uint rayDataIndex = resourceIndices.rayDataUAVIndex;

    // The ray missed. Store the miss radiance, set the hit distance to a large value, and exit early.
    if (packedPayload.hitT < 0.f)
    {
        // Store the ray miss
        DDGIStoreProbeRayMiss(rayDataIndex, outputCoords, volume, GetGlobalConst(app, skyRadiance));
        return;
    }

    // Unpack the payload
    Payload payload = UnpackPayload(packedPayload);

    // The ray hit a surface backface
    if (payload.hitKind == gl_HitKindBackFacingTriangleEXT)
    {
        // Store the ray backface hit
        DDGIStoreProbeRayBackfaceHit(rayDataIndex, outputCoords, volume, payload.hitT);
        return;
    }

    // Early out: a "fixed" ray hit a front facing surface. Fixed rays are not blended since their direction
    // is not random and they would bias the irradiance estimate. Don't perform lighting for these rays.
    if((volume.probeRelocationEnabled || volume.probeClassificationEnabled) && rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS)
    {
        // Store the ray front face hit distance (only)
        DDGIStoreProbeRayFrontfaceHit(rayDataIndex, outputCoords, volume, payload.hitT);
        return;
    }

    // Direct Lighting and Shadowing
    // Get the (dynamic) lights through global SSBO directly
    vec3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), GetAccelerationStructure(SCENE_TLAS_INDEX));
    // vec3 diffuse = vec3(0.f);

    // Indirect Lighting (recursive)
    vec3 irradiance = vec3(0.f);
    vec3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.direction, volume);

    // Get the volume resources needed for the irradiance query
    DDGIVolumeResources resources;
    resources.probeIrradianceTexIdx = resourceIndices.probeIrradianceSRVIndex;
    resources.probeDistanceTexIdx = resourceIndices.probeDistanceSRVIndex;
    resources.probeDataTexIdx = resourceIndices.probeDataSRVIndex;
    // resources.bilinearSampler = GetBilinearWrapSampler();

    // Compute volume blending weight
    float volumeBlendWeight = DDGIGetVolumeBlendWeight(payload.worldPosition, volume);

    // Don't evaluate irradiance when the surface is outside the volume
    if (volumeBlendWeight > 0)
    {
        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(
            payload.worldPosition,
            surfaceBias,
            payload.normal,
            volume,
            resources);

        // Attenuate irradiance by the blend weight
        irradiance *= volumeBlendWeight;
    }

    // Perfectly diffuse reflectors don't exist in the real world.
    // Limit the BRDF albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9f;

    // Store the final ray radiance and hit distance
    vec3 radiance = diffuse + ((min(payload.albedo, vec3(maxAlbedo, maxAlbedo, maxAlbedo)) / PI) * irradiance);

    DDGIStoreProbeRayFrontfaceHit(rayDataIndex, outputCoords, volume, clamp(radiance, 0.0, 1.0), payload.hitT);
}
