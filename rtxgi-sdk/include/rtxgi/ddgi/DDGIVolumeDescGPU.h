#ifndef RTXGI_DDGI_VOLUME_DESC_GPU_H
#define RTXGI_DDGI_VOLUME_DESC_GPU_H

#ifdef GLSL
    #define float3 vec3
    #define float4 vec4
    #define int3 ivec3
    #define uint3 uvec3
    #define uint4 uvec4
    // need include "Framework.glsl" before this file
#elif !defined(HLSL)
    #include <rtxgi/Math.h>
    #include <rtxgi/Types.h>
    using namespace rtxgi;
#endif

/**
 * Describes the location (i.e. index) of DDGIVolume resources
 * on the D3D descriptor heap or in bindless resource arrays.
 */
struct DDGIVolumeResourceIndices
{
    uint     rayDataUAVIndex;                    // Index of the ray data UAV on the descriptor heap or in a RWTexture2D resource array
    uint     rayDataSRVIndex;                    // Index of the ray data SRV on the descriptor heap or in a Texture2D resource array
    uint     probeIrradianceUAVIndex;            // Index of the probe irradiance UAV on the descriptor heap or in a RWTexture2DArray resource array
    uint     probeIrradianceSRVIndex;            // Index of the probe irradiance SRV on the descriptor heap or in a Texture2DArray resource array
    //------------------------------------------------- 16B
    uint     probeDistanceUAVIndex;              // Index of the probe distance UAV on the descriptor heap or in a RWTexture2DArray resource array
    uint     probeDistanceSRVIndex;              // Index of the probe distance SRV on the descriptor heap or in a Texture2DArray resource array
    uint     probeDataUAVIndex;                  // Index of the probe data UAV on the descriptor heap or in a RWTexture2DArray resource array
    uint     probeDataSRVIndex;                  // Index of the probe data SRV on the descriptor heap or in a Texture2DArray resource array
    //------------------------------------------------- 32B
    uint     probeVariabilityUAVIndex;           // Index of the probe variability UAV on the descriptor heap or in a RWTexture2DArray resource Array
    uint     probeVariabilitySRVIndex;           // Index of the probe variability SRV on the descriptor heap or in a Texture2DArray resource array
    uint     probeVariabilityAverageUAVIndex;    // Index of the probe variability average UAV on the descriptor heap or in a RWTexture2DArray resource Array
    uint     probeVariabilityAverageSRVIndex;    // Index of the probe variability average SRV on the descriptor heap or in a Texture2DArray resource array
    //------------------------------------------------- 48B
#if defined(GLSL) || defined(HLSL)
    #define uint32_t uint
#endif
    uint32_t rayDataHandleStorage;                      // Handle of the ray data texture for storage descriptor
    uint32_t probeIrradianceHandleStorage;              // Handle of the probe irradiance texture for storage descriptor
    uint32_t probeDistanceHandleStorage;                // Handle of the probe distance texture for storage descriptor
    uint32_t probeDataHandleStorage;                    // Handle of the probe data texture for storage descriptor
    uint32_t probeVariabilityHandleStorage;             // Handle of the probe variability texture for storage descriptor
    uint32_t probeVariabilityAverageHandleStorage;      // Handle of the probe variability average texture for storage descriptor
    //------------------------------------------------- 72B
};

/**
 * Describes the properties of a DDGIVolume, with values packed to compact formats.
 * This version of the struct uses 128B to store some values at full precision.
 */
struct DDGIVolumeDescGPUPacked
{
    float3   origin;
    float    probeHysteresis;
    //------------------------------------------------- 16B
    float4   rotation;
    //------------------------------------------------- 32B
    float4   probeRayRotation;
    //------------------------------------------------- 48B
    float    probeMaxRayDistance;
    float    probeNormalBias;
    float    probeViewBias;
    float    probeDistanceExponent;
    //------------------------------------------------- 64B
    float    probeIrradianceEncodingGamma;
    float    probeIrradianceThreshold;
    float    probeBrightnessThreshold;
    float    probeMinFrontfaceDistance;
    //------------------------------------------------- 80B
    float3   probeSpacing;
    uint     packed0;       // probeCounts.x (10), probeCounts.y (10), probeCounts.z (10), unused (2)
    //------------------------------------------------- 96B
    uint     packed1;       // probeRandomRayBackfaceThreshold (16), probeFixedRayBackfaceThreshold (16)
    uint     packed2;       // probeNumRays (16), probeNumIrradianceInteriorTexels (8), probeNumDistanceInteriorTexels (8)
    uint     packed3;       // probeScrollOffsets.x (15) sign bit (1), probeScrollOffsets.y (15) sign bit (1)
    uint     packed4;       // probeScrollOffsets.z (15) sign bit (1)
                            // movementType (1), probeRayDataFormat (3), probeIrradianceFormat (3), probeRelocationEnabled (1)
                            // probeClassificationEnabled (1), probeVariabilityEnabled (1)
                            // probeScrollClear Y-Z plane (1), probeScrollClear X-Z plane (1), probeScrollClear X-Y plane (1)
                            // probeScrollDirection Y-Z plane (1), probeScrollDirection X-Z plane (1), probeScrollDirection X-Y plane (1)
    //------------------------------------------------- 112B
    uint4    reserved;      // 16B reserved for future use
    //------------------------------------------------- 128B
};

/**
 * Describes the properties of a DDGIVolume.
 */
struct DDGIVolumeDescGPU
{
    float3   origin;                             // world-space location of the volume center

    float4   rotation;                           // rotation quaternion for the volume
    float4   probeRayRotation;                   // rotation quaternion for probe rays

    uint     movementType;                       // type of movement the volume allows. 0: default, 1: infinite scrolling

    float3   probeSpacing;                       // world-space distance between probes
    int3     probeCounts;                        // number of probes on each axis of the volume

    int      probeNumRays;                       // number of rays traced per probe
    int      probeNumIrradianceInteriorTexels;   // number of texels in one dimension of a probe's irradiance texture (does not include 1-texel border)
    int      probeNumDistanceInteriorTexels;     // number of texels in one dimension of a probe's distance texture (does not include 1-texel border)

    float    probeHysteresis;                    // weight of the previous irradiance and distance data store in probes
    float    probeMaxRayDistance;                // maximum world-space distance a probe ray can travel
    float    probeNormalBias;                    // offset along the surface normal, applied during lighting to avoid numerical instabilities when determining visibility
    float    probeViewBias;                      // offset along the camera view ray, applied during lighting to avoid numerical instabilities when determining visibility
    float    probeDistanceExponent;              // exponent used during visibility testing. High values react rapidly to depth discontinuities, but may cause banding
    float    probeIrradianceEncodingGamma;       // exponent that perceptually encodes irradiance for faster light-to-dark convergence

    float    probeIrradianceThreshold;           // threshold to identify when large lighting changes occur
    float    probeBrightnessThreshold;           // threshold that specifies the maximum allowed difference in brightness between the previous and current irradiance values
    float    probeRandomRayBackfaceThreshold;    // threshold that specifies the ratio of *random* rays traced for a probe that may hit back facing triangles before the probe is considered inside geometry (used in blending)

    // Probe Relocation, Probe Classification
    float    probeFixedRayBackfaceThreshold;     // threshold that specifies the ratio of *fixed* rays traced for a probe that may hit back facing triangles before the probe is considered inside geometry (used in relocation & classification)
    float    probeMinFrontfaceDistance;          // minimum world-space distance to a front facing triangle allowed before a probe is relocated

    // Infinite Scrolling Volumes
    int3     probeScrollOffsets;                 // grid-space offsets used for scrolling movement
    bool     probeScrollClear[3];                // whether probes of a plane need to be cleared due to scrolling movement
    bool     probeScrollDirections[3];           // direction of scrolling movement (0: negative, 1: positive)

    // Feature Options
    uint     probeRayDataFormat;                 // texture format of the ray data texture (EDDGIVolumeTextureFormat)
    uint     probeIrradianceFormat;              // texture format of the irradiance texture (EDDGIVolumeTextureFormat)
    bool     probeRelocationEnabled;             // whether probe relocation is enabled for this volume
    bool     probeClassificationEnabled;         // whether probe classification is enabled for this volume
    bool     probeVariabilityEnabled;            // whether probe variability is enabled for this volume
};

#if !defined(GLSL) && !defined(HLSL) // CPU only
static inline rtxgi::DDGIVolumeDescGPUPacked PackDDGIVolumeDescGPU(const rtxgi::DDGIVolumeDescGPU unpacked)
{
    rtxgi::DDGIVolumeDescGPUPacked packed = {};

    packed.origin = unpacked.origin;
    packed.probeHysteresis = unpacked.probeHysteresis;
    packed.rotation = unpacked.rotation;
    packed.probeRayRotation = unpacked.probeRayRotation;
    packed.probeMaxRayDistance = unpacked.probeMaxRayDistance;
    packed.probeNormalBias = unpacked.probeNormalBias;
    packed.probeViewBias = unpacked.probeViewBias;
    packed.probeDistanceExponent = unpacked.probeDistanceExponent;
    packed.probeIrradianceEncodingGamma = unpacked.probeIrradianceEncodingGamma;
    packed.probeIrradianceThreshold = unpacked.probeIrradianceThreshold;
    packed.probeBrightnessThreshold = unpacked.probeBrightnessThreshold;
    packed.probeMinFrontfaceDistance = unpacked.probeMinFrontfaceDistance;
    packed.probeSpacing = unpacked.probeSpacing;

    packed.packed0  = (uint32_t)unpacked.probeCounts.x;
    packed.packed0 |= (uint32_t)unpacked.probeCounts.y << 10;
    packed.packed0 |= (uint32_t)unpacked.probeCounts.z << 20;

    packed.packed1  = (uint32_t)(unpacked.probeRandomRayBackfaceThreshold * 65535);
    packed.packed1 |= (uint32_t)(unpacked.probeFixedRayBackfaceThreshold * 65535) << 16;

    packed.packed2  = (uint32_t)unpacked.probeNumRays;
    packed.packed2 |= (uint32_t)unpacked.probeNumIrradianceInteriorTexels << 16;
    packed.packed2 |= (uint32_t)unpacked.probeNumDistanceInteriorTexels << 24;

    // Probe Scroll Offsets
    packed.packed3 = (packed.packed3 & ~0x7FFF)     | abs(unpacked.probeScrollOffsets.x);
    packed.packed3 = (packed.packed3 & ~0x8000)     | ((unpacked.probeScrollOffsets.x < 0) << 15);
    packed.packed3 = (packed.packed3 & ~0x10000)    | abs(unpacked.probeScrollOffsets.y) << 16;
    packed.packed3 = (packed.packed3 & ~0x80000000) | ((unpacked.probeScrollOffsets.y < 0) << 31);
    packed.packed4 = (packed.packed4 & ~0x7FFF)     | abs(unpacked.probeScrollOffsets.z);
    packed.packed4 = (packed.packed4 & ~0x8000)     | ((unpacked.probeScrollOffsets.z < 0) << 15);

    // Feature Bits
    packed.packed4 = (packed.packed4 & ~0x10000)    | (unpacked.movementType << 16);
    packed.packed4 = (packed.packed4 & ~0xE0000)    | (unpacked.probeRayDataFormat << 17);
    packed.packed4 = (packed.packed4 & ~0x700000)   | (unpacked.probeIrradianceFormat << 20);
    packed.packed4 = (packed.packed4 & ~0x800000)   | (unpacked.probeRelocationEnabled << 23);
    packed.packed4 = (packed.packed4 & ~0x1000000)  | (unpacked.probeClassificationEnabled << 24);
    packed.packed4 = (packed.packed4 & ~0x2000000)  | (unpacked.probeVariabilityEnabled << 25);
    packed.packed4 = (packed.packed4 & ~0x4000000)  | (unpacked.probeScrollClear[0] << 26);
    packed.packed4 = (packed.packed4 & ~0x8000000)  | (unpacked.probeScrollClear[1] << 27);
    packed.packed4 = (packed.packed4 & ~0x10000000) | (unpacked.probeScrollClear[2] << 28);
    packed.packed4 = (packed.packed4 & ~0x20000000) | (unpacked.probeScrollDirections[0] << 29);
    packed.packed4 = (packed.packed4 & ~0x40000000) | (unpacked.probeScrollDirections[1] << 30);
    packed.packed4 = (packed.packed4 & ~0x80000000) | (unpacked.probeScrollDirections[2] << 31);

    return packed;
}
#endif // if !defined(GLSL) && !defined(HLSL)

#if !defined(GLSL) && !defined(HLSL) // CPU
static inline rtxgi::DDGIVolumeDescGPU UnpackDDGIVolumeDescGPU(const rtxgi::DDGIVolumeDescGPUPacked packed)
{
    rtxgi::DDGIVolumeDescGPU unpacked;
#else // GPU
DDGIVolumeDescGPU UnpackDDGIVolumeDescGPU(DDGIVolumeDescGPUPacked packed)
{
    DDGIVolumeDescGPU unpacked;
#endif // if !defined(GLSL) && !defined(HLSL)
    unpacked.origin = packed.origin;
    unpacked.probeHysteresis = packed.probeHysteresis;
    unpacked.rotation = packed.rotation;
    unpacked.probeRayRotation = packed.probeRayRotation;
    unpacked.probeMaxRayDistance = packed.probeMaxRayDistance;
    unpacked.probeNormalBias = packed.probeNormalBias;
    unpacked.probeViewBias = packed.probeViewBias;
    unpacked.probeDistanceExponent = packed.probeDistanceExponent;
    unpacked.probeIrradianceEncodingGamma = packed.probeIrradianceEncodingGamma;
    unpacked.probeIrradianceThreshold = packed.probeIrradianceThreshold;
    unpacked.probeBrightnessThreshold = packed.probeBrightnessThreshold;
    unpacked.probeMinFrontfaceDistance = packed.probeMinFrontfaceDistance;
    unpacked.probeSpacing = packed.probeSpacing;

    // Probe Counts
    unpacked.probeCounts.x = int(packed.packed0 & 0x000003FFu);
    unpacked.probeCounts.y = int((packed.packed0 >> 10) & 0x000003FFu);
    unpacked.probeCounts.z = int((packed.packed0 >> 20) & 0x000003FFu);

    // Thresholds
    unpacked.probeRandomRayBackfaceThreshold = float(packed.packed1 & 0x0000FFFF) / 65535.f;
    unpacked.probeFixedRayBackfaceThreshold = float((packed.packed1 >> 16) & 0x0000FFFF) / 65535.f;

    // Counts
    unpacked.probeNumRays = int(packed.packed2 & 0x0000FFFFu);
    unpacked.probeNumIrradianceInteriorTexels = int((packed.packed2 >> 16) & 0x000000FFu);
    unpacked.probeNumDistanceInteriorTexels = int((packed.packed2 >> 24) & 0x000000FFu);

    // Probe Scroll Offsets
    unpacked.probeScrollOffsets.x = int(packed.packed3 & 0x00007FFFu);
    if (((packed.packed3 >> 15) & 0x00000001) != 0) unpacked.probeScrollOffsets.x *= -1;
    unpacked.probeScrollOffsets.y = int((packed.packed3 >> 16) & 0x00007FFFu);
    if (((packed.packed3 >> 31) & 0x00000001) != 0) unpacked.probeScrollOffsets.y *= -1;
    unpacked.probeScrollOffsets.z = int((packed.packed4) & 0x00007FFFu);
    if (((packed.packed4 >> 15) & 0x00000001) != 0) unpacked.probeScrollOffsets.z *= -1;

    // Feature Bits
    unpacked.movementType = (packed.packed4 >> 16) & 0x00000001;
    unpacked.probeRayDataFormat = (packed.packed4 >> 17) & 0x00000007;
    unpacked.probeIrradianceFormat = (packed.packed4 >> 20) & 0x00000007;
    unpacked.probeRelocationEnabled = bool((packed.packed4 >> 23) & 0x00000001);
    unpacked.probeClassificationEnabled = bool((packed.packed4 >> 24) & 0x00000001);
    unpacked.probeVariabilityEnabled = bool((packed.packed4 >> 25) & 0x00000001);
    unpacked.probeScrollClear[0] = bool((packed.packed4 >> 26) & 0x00000001);
    unpacked.probeScrollClear[1] = bool((packed.packed4 >> 27) & 0x00000001);
    unpacked.probeScrollClear[2] = bool((packed.packed4 >> 28) & 0x00000001);
    unpacked.probeScrollDirections[0] = bool((packed.packed4 >> 29) & 0x00000001);
    unpacked.probeScrollDirections[1] = bool((packed.packed4 >> 30) & 0x00000001);
    unpacked.probeScrollDirections[2] = bool((packed.packed4 >> 31) & 0x00000001);

    return unpacked;
}

#endif // RTXGI_DDGI_VOLUME_DESC_GPU_H
