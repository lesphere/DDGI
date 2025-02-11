#ifndef RTXGI_DDGI_PROBE_COMMON_GLSL
#define RTXGI_DDGI_PROBE_COMMON_GLSL

#include "Common.glsl"
#include "ProbeDataCommon.glsl"
#include "ProbeRayCommon.glsl"
#include "ProbeIndexing.glsl"
#include "ProbeOctahedral.glsl"

//------------------------------------------------------------------------
// Probe World Position
//------------------------------------------------------------------------

/**
 * Computes the world-space position of a probe from the probe's 3D grid-space coordinates.
 * Probe relocation is not considered.
 */
vec3 DDGIGetProbeWorldPosition(ivec3 probeCoords, DDGIVolumeDescGPU volume)
{
    // Multiply the grid coordinates by the probe spacing
    vec3 probeGridWorldPosition = probeCoords * volume.probeSpacing;

    // Shift the grid of probes by half of each axis extent to center the volume about its origin
    vec3 probeGridShift = (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;

    // Center the probe grid about the origin
    vec3 probeWorldPosition = (probeGridWorldPosition - probeGridShift);

    // Rotate the probe grid if infinite scrolling is not enabled
    if (!IsVolumeMovementScrolling(volume)) probeWorldPosition = RTXGIQuaternionRotate(probeWorldPosition, volume.rotation);

    // Translate the grid to the volume's center
    probeWorldPosition += volume.origin + (volume.probeScrollOffsets * volume.probeSpacing);

    return probeWorldPosition;
}

/**
 * Computes the world-space position of a probe from the probe's 3D grid-space coordinates.
 * When probe relocation is enabled, offsets are loaded from the probe data
 * Texture2D and used to adjust the final world position.
 */
vec3 DDGIGetProbeWorldPosition(ivec3 probeCoords, DDGIVolumeDescGPU volume, TextureRaw2DArray probeData)
{
    // Get the probe's world-space position
    vec3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume);

    // If the volume has probe relocation enabled, account for the probe offsets
    if (volume.probeRelocationEnabled)
    {
        // Get the scroll adjusted probe index
        int probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

        // Find the texture coordinates of the probe in the Probe Data texture
        uvec3 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Load the probe's world-space position offset and add it to the current world position
        probeWorldPosition += DDGILoadProbeDataOffset(probeData, coords, volume);
    }

    return probeWorldPosition;
}

/**
 * Computes the world-space position of a probe from the probe's 3D grid-space coordinates.
 * When probe relocation is enabled, offsets are loaded from the probe data
 * Image2DArray_rgba16f and used to adjust the final world position.
 */
vec3 DDGIGetProbeWorldPosition(ivec3 probeCoords, DDGIVolumeDescGPU volume, Image2DArray_rgba16f probeData)
{
    // Get the probe's world-space position
    vec3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume);

    // If the volume has probe relocation enabled, account for the probe offsets
    if (volume.probeRelocationEnabled)
    {
        // Get the scroll adjusted probe index
        int probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

        // Find the texture coordinates of the probe in the Probe Data texture
        uvec3 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Load the probe's world-space position offset and add it to the current world position
        probeWorldPosition += DDGILoadProbeDataOffset(probeData, coords, volume);
    }

    return probeWorldPosition;
}

/**
 * Computes the world-space position of a probe from the probe's 3D grid-space coordinates.
 * When probe relocation is enabled, offsets are loaded from the probe data
 * Image2DArray_rgba32f and used to adjust the final world position.
 */
vec3 DDGIGetProbeWorldPosition(ivec3 probeCoords, DDGIVolumeDescGPU volume, Image2DArray_rgba32f probeData)
{
    // Get the probe's world-space position
    vec3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume);

    // If the volume has probe relocation enabled, account for the probe offsets
    if (volume.probeRelocationEnabled)
    {
        // Get the scroll adjusted probe index
        int probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

        // Find the texture coordinates of the probe in the Probe Data texture
        uvec3 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Load the probe's world-space position offset and add it to the current world position
        probeWorldPosition += DDGILoadProbeDataOffset(probeData, coords, volume);
    }

    return probeWorldPosition;
}

#endif // RTXGI_DDGI_PROBE_COMMON_GLSL
