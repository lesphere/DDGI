#ifndef RTXGI_DDGI_PROBE_INDEXING_GLSL
#define RTXGI_DDGI_PROBE_INDEXING_GLSL

#include "Common.glsl"

//------------------------------------------------------------------------
// Probe Indexing Helpers
//------------------------------------------------------------------------

/**
 * Get the number of probes on a horizontal plane, in the active coordinate system.
 */
int DDGIGetProbesPerPlane(ivec3 probeCounts)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return (probeCounts.x * probeCounts.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return (probeCounts.x * probeCounts.y);
#endif
}

/**
 * Get the index of the horizontal plane, in the active coordinate system.
 */
int DDGIGetPlaneIndex(ivec3 probeCoords)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return probeCoords.z;
#else
    return probeCoords.y;
#endif
}

/**
 * Get the index of a probe within a horizontal plane that the probe coordinates map to, in the active coordinate system.
 */
int DDGIGetProbeIndexInPlane(ivec3 probeCoords, ivec3 probeCounts)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return probeCoords.x + (probeCounts.x * probeCoords.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    return probeCoords.y + (probeCounts.y * probeCoords.x);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return probeCoords.x + (probeCounts.x * probeCoords.y);
#endif
}

/**
 * Get the index of a probe within a horizontal plane (i.e. Texture2DArray slice) that the
 * given texel coordinates map to, in the active coordinate system. Provided 2D texel coordinates
 * should *not* include the octahedral texture's 1-texel border.
 */
int DDGIGetProbeIndexInPlane(uvec3 texCoords, ivec3 probeCounts, int probeNumTexels)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return int(texCoords.x / probeNumTexels) + (probeCounts.x * int(texCoords.y / probeNumTexels));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    return int(texCoords.x / probeNumTexels) + (probeCounts.y * int(texCoords.y / probeNumTexels));
#endif
}

//------------------------------------------------------------------------
// Probe Indices
//------------------------------------------------------------------------

/**
 * Computes the probe index from 3D grid coordinates.
 * The opposite of DDGIGetProbeCoords(probeIndex,...).
 */
int DDGIGetProbeIndex(ivec3 probeCoords, DDGIVolumeDescGPU volume)
{
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int planeIndex = DDGIGetPlaneIndex(probeCoords);
    int probeIndexInPlane = DDGIGetProbeIndexInPlane(probeCoords, volume.probeCounts);

    return (planeIndex * probesPerPlane) + probeIndexInPlane;
}

/**
 * Computes the probe index from 3D (Texture2DArray) texture coordinates.
 */
int DDGIGetProbeIndex(uvec3 texCoords, int probeNumTexels, DDGIVolumeDescGPU volume)
{
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int probeIndexInPlane = DDGIGetProbeIndexInPlane(texCoords, volume.probeCounts, probeNumTexels);

    return (texCoords.z * probesPerPlane) + probeIndexInPlane;
}

//------------------------------------------------------------------------
// Probe Grid Coordinates
//------------------------------------------------------------------------

/**
 * Computes the 3D grid-space coordinates for the probe at the given probe index in the range [0, numProbes-1].
 * The opposite of DDGIGetProbeIndex(probeCoords,...).
 */
ivec3 DDGIGetProbeCoords(int probeIndex, DDGIVolumeDescGPU volume)
{
    ivec3 probeCoords;

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    probeCoords.x = probeIndex % volume.probeCounts.x;
    probeCoords.y = probeIndex / (volume.probeCounts.x * volume.probeCounts.z);
    probeCoords.z = (probeIndex / volume.probeCounts.x) % volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    probeCoords.x = (probeIndex / volume.probeCounts.y) % volume.probeCounts.x;
    probeCoords.y = probeIndex % volume.probeCounts.y;
    probeCoords.z = probeIndex / (volume.probeCounts.x * volume.probeCounts.y);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    probeCoords.x = probeIndex % volume.probeCounts.x;
    probeCoords.y = (probeIndex / volume.probeCounts.x) % volume.probeCounts.y;
    probeCoords.z = probeIndex / (volume.probeCounts.y * volume.probeCounts.x);
#endif

    return probeCoords;
}

/**
 * Computes the 3D grid-space coordinates of the "base" probe (i.e. floor of xyz) of the 8-probe
 * cube that surrounds the given world space position. The other seven probes of the cube
 * are offset by 0 or 1 in grid space along each axis.
 *
 * This function accounts for scroll offsets to adjust the volume's origin.
 */
ivec3 DDGIGetBaseProbeGridCoords(vec3 worldPosition, DDGIVolumeDescGPU volume)
{
    // Get the vector from the volume origin to the surface point
    vec3 position = worldPosition - (volume.origin + (volume.probeScrollOffsets * volume.probeSpacing));

    // Rotate the world position into the volume's space
    if(!IsVolumeMovementScrolling(volume)) position = RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation));

    // Shift from [-n/2, n/2] to [0, n] (grid space)
    position += (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;

    // Quantize the position to grid space
    ivec3 probeCoords = int3(position / volume.probeSpacing);

    // Clamp to [0, probeCounts - 1]
    // Snaps positions outside of grid to the grid edge
    probeCoords = clamp(probeCoords, ivec3(0, 0, 0), (volume.probeCounts - ivec3(1, 1, 1)));

    return probeCoords;
}

//------------------------------------------------------------------------
// Texture Coordinates
//------------------------------------------------------------------------

/**
 * Computes the RayData Texture2DArray coordinates of the probe at the given probe index.
 *
 * When infinite scrolling is enabled, probeIndex is expected to be the scroll adjusted probe index.
 * Obtain the adjusted index with DDGIGetScrollingProbeIndex().
 */
uvec3 DDGIGetRayDataTexelCoords(int rayIndex, int probeIndex, DDGIVolumeDescGPU volume)
{
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);

    uvec3 coords;
    coords.x = rayIndex;
    coords.z = probeIndex / probesPerPlane;
    coords.y = probeIndex - (coords.z * probesPerPlane);

    return coords;
}

/**
 * Computes the Texture2DArray coordinates of the probe at the given probe index.
 *
 * When infinite scrolling is enabled, probeIndex is expected to be the scroll adjusted probe index.
 * Obtain the adjusted index with DDGIGetScrollingProbeIndex().
 */
uvec3 DDGIGetProbeTexelCoords(int probeIndex, DDGIVolumeDescGPU volume)
{
    // Find the probe's plane index
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int planeIndex = int(probeIndex / probesPerPlane);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    int x = (probeIndex % volume.probeCounts.x);
    int y = (probeIndex / volume.probeCounts.x) % volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    int x = (probeIndex % volume.probeCounts.y);
    int y = (probeIndex / volume.probeCounts.y) % volume.probeCounts.x;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    int x = (probeIndex % volume.probeCounts.x);
    int y = (probeIndex / volume.probeCounts.x) % volume.probeCounts.y;
#endif

    return uvec3(x, y, planeIndex);
}

/**
 * Computes the normalized texture UVs within the Probe Irradiance and Probe Distance texture arrays
 * given the probe index and 2D normalized octant coordinates [-1, 1]. Used when sampling the texture arrays.
 * 
 * When infinite scrolling is enabled, probeIndex is expected to be the scroll adjusted probe index.
 * Obtain the adjusted index with DDGIGetScrollingProbeIndex().
 */
vec3 DDGIGetProbeUV(int probeIndex, vec2 octantCoordinates, int numProbeInteriorTexels, DDGIVolumeDescGPU volume)
{
    // Get the probe's texel coordinates, assuming one texel per probe
    uvec3 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

    // Add the border texels to get the total texels per probe
    float numProbeTexels = (numProbeInteriorTexels + 2.f);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    float textureWidth = numProbeTexels * volume.probeCounts.x;
    float textureHeight = numProbeTexels * volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    float textureWidth = numProbeTexels * volume.probeCounts.y;
    float textureHeight = numProbeTexels * volume.probeCounts.x;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    float textureWidth = numProbeTexels * volume.probeCounts.x;
    float textureHeight = numProbeTexels * volume.probeCounts.y;
#endif

    // Move to the center of the probe and move to the octant texel before normalizing
    vec2 uv = vec2(coords.x * numProbeTexels, coords.y * numProbeTexels) + (numProbeTexels * 0.5f);
    uv += octantCoordinates.xy * ((float)numProbeInteriorTexels * 0.5f);
    uv /= vec2(textureWidth, textureHeight);
    return vec3(uv, coords.z);
}

//------------------------------------------------------------------------
// Probe Classification
//------------------------------------------------------------------------

/**
 * Loads and returns the probe's classification state (from a Image2DArray_rgba16f).
 */
float DDGILoadProbeState(int probeIndex, Image2DArray_rgba16f probeData, DDGIVolumeDescGPU volume)
{
    float state = RTXGI_DDGI_PROBE_STATE_ACTIVE;
    if (volume.probeClassificationEnabled)
    {
        // Get the probe's texel coordinates in the Probe Data texture
        ivec3 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Get the probe's classification state
        state = ImageLoad(probeData, probeDataCoords).w;
    }

    return state;
}

/**
 * Loads and returns the probe's classification state (from a Image2DArray_rgba32f).
 */
float DDGILoadProbeState(int probeIndex, Image2DArray_rgba32f probeData, DDGIVolumeDescGPU volume)
{
    float state = RTXGI_DDGI_PROBE_STATE_ACTIVE;
    if (volume.probeClassificationEnabled)
    {
        // Get the probe's texel coordinates in the Probe Data texture
        ivec3 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Get the probe's classification state
        state = ImageLoad(probeData, probeDataCoords).w;
    }

    return state;
}

/**
 * Loads and returns the probe's classification state (from a TextureRaw2DArray).
 */
float DDGILoadProbeState(int probeIndex, TextureRaw2DArray probeData, DDGIVolumeDescGPU volume)
{
    float state = RTXGI_DDGI_PROBE_STATE_ACTIVE;
    if (volume.probeClassificationEnabled)
    {
        // Get the probe's texel coordinates in the Probe Data texture
        ivec3 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Get the probe's classification state
        state = TexelFetch(probeData, probeDataCoords, 0).w;
    }

    return state;
}

//------------------------------------------------------------------------
// Infinite Scrolling
//------------------------------------------------------------------------

/**
 * Adjusts the probe index for when infinite scrolling is enabled.
 * This can run when scrolling is disabled since zero offsets result
 * in the same probe index.
 */
int DDGIGetScrollingProbeIndex(ivec3 probeCoords, DDGIVolumeDescGPU volume)
{
    return DDGIGetProbeIndex(((probeCoords + volume.probeScrollOffsets + volume.probeCounts) % volume.probeCounts), volume);
}

/**
 * Clears probe irradiance and distance data for a plane of probes that have been scrolled to new positions.
 */
bool DDGIClearScrolledPlane(ivec3 probeCoords, int planeIndex, DDGIVolumeDescGPU volume)
{
    if (volume.probeScrollClear[planeIndex])
    {
        int offset = volume.probeScrollOffsets[planeIndex];
        int probeCount = volume.probeCounts[planeIndex];
        int direction = volume.probeScrollDirections[planeIndex];

        int coord = 0;
        if(direction) coord = (probeCount + (offset - 1)) % probeCount; // scrolling in positive direction
        else coord = (probeCount + (offset % probeCount)) % probeCount; // scrolling in negative direction

        // Probe has scrolled and needs to be cleared
        if (probeCoords[planeIndex] == coord) return true;
    }
    return false;
}

#endif // RTXGI_DDGI_PROBE_INDEXING_GLSL