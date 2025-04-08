#ifndef RTXGI_DDGI_PROBE_DATA_COMMON_GLSL
#define RTXGI_DDGI_PROBE_DATA_COMMON_GLSL

#include "rtxgi-sdk/shaders/ddgi/include/Common.glsl"

//------------------------------------------------------------------------
// Probe Data Texture Write Helpers
//------------------------------------------------------------------------

/**
 * Normalizes the world-space offset and writes it to the probe data texture.
 * Probe Relocation limits this range to [0.f, 0.45f).
 */
// Assume that all probeData is Image2DArray_rgba16f in config
void DDGIStoreProbeDataOffset(Image2DArray_rgba16f probeData, uvec3 coords, vec3 wsOffset, DDGIVolumeDescGPU volume) {
    ImageStore(probeData, ivec3(coords), vec4(wsOffset / volume.probeSpacing, 0.f));
}

//------------------------------------------------------------------------
// Probe Data Texture Read Helpers
//------------------------------------------------------------------------

/**
 * Reads the probe's position offset (from a probeDataIdx) and converts it to a world-space offset.
 */
vec3 DDGILoadProbeDataOffset(uint probeDataIdx, uvec3 coords, DDGIVolumeDescGPU volume) {
    return texelFetch(GetTex2DArray(probeDataIdx), ivec3(coords), 0).xyz * volume.probeSpacing;
}

/**
 * Reads the probe's position offset (from a Image2DArray_rgba16f) and converts it to a world-space offset.
 */
vec3 DDGILoadProbeDataOffset(Image2DArray_rgba16f probeData, uvec3 coords, DDGIVolumeDescGPU volume) {
    return ImageLoad(probeData, ivec3(coords)).xyz * volume.probeSpacing;
}

/**
 * Reads the probe's position offset (from a Image2DArray_rgba32f) and converts it to a world-space offset.
 */
vec3 DDGILoadProbeDataOffset(Image2DArray_rgba32f probeData, uvec3 coords, DDGIVolumeDescGPU volume) {
    return ImageLoad(probeData, ivec3(coords)).xyz * volume.probeSpacing;
}

#endif // RTXGI_DDGI_PROBE_DATA_COMMON_GLSL
