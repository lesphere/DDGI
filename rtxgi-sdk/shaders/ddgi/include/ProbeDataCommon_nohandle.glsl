#ifndef RTXGI_DDGI_PROBE_DATA_COMMON_GLSL
#define RTXGI_DDGI_PROBE_DATA_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : require

#include "rtxgi-sdk/shaders/ddgi/include/Common.glsl"

//------------------------------------------------------------------------
// Probe Data Texture Write Helpers
//------------------------------------------------------------------------

/**
 * Normalizes the world-space offset and writes it to the probe data texture.
 * Probe Relocation limits this range to [0.f, 0.45f).
 */
// Assume that all probeData is Image2DArray_rgba32f in config
void DDGIStoreProbeDataOffset(uint probeDataIdx, uvec3 coords, vec3 wsOffset, DDGIVolumeDescGPU volume) {
    imageStore(Image2DArray_rgba32f[probeDataIdx], ivec3(coords), vec4(wsOffset / volume.probeSpacing, 0.f));
}

//------------------------------------------------------------------------
// Probe Data Texture Read Helpers
//------------------------------------------------------------------------

/**
 * Reads the probe's position offset (from a texture2DArray) and converts it to a world-space offset.
 */
vec3 DDGILoadProbeDataOffsetFromTex(uint probeDataIdx, uvec3 coords, DDGIVolumeDescGPU volume) {
    return texelFetch(GetTex2DArray(probeDataIdx), ivec3(coords), 0).xyz * volume.probeSpacing;
}

/**
 * Reads the probe's position offset (from a Image2DArray_rgba32f) and converts it to a world-space offset.
 */
vec3 DDGILoadProbeDataOffsetFromImage(uint probeDataIdx, uvec3 coords, DDGIVolumeDescGPU volume) {
    return imageLoad(Image2DArray_rgba32f[probeDataIdx], ivec3(coords)).xyz * volume.probeSpacing;
}

#endif // RTXGI_DDGI_PROBE_DATA_COMMON_GLSL
