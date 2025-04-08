#ifndef DESCRIPTORS_GLSL
#define DESCRIPTORS_GLSL

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : require

#include "rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "samples/test-harness/include/graphics/Types.h"

#define RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS 0
#define RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP 1

#ifndef RTXGI_BINDLESS_TYPE
#error Required define RTXGI_BINDLESS_TYPE is not defined!
#endif

// struct TLASInstance
// {
// #pragma pack_matrix(row_major)
//     mat3x4 transform;
// #pragma pack_matrix(column_major)
//     uint     instanceID24_Mask8;
//     uint     instanceContributionToHitGroupIndex24_Flags8;
//     uvec2    blasAddress;
// };

struct RayDesc {
    vec3 origin;
    float tmin;
    vec3 direction;
    float tmax;
};

// Global Root / Push Constants ------------------------------------------------------------------------------------

layout(push_constant) uniform Block_GlobalConstants {
    GlobalConstants GlobalConst;
};

#define GetGlobalConst(x, y) (GlobalConst.x##_##y)

/* for PT
uint GetPTNumBounces() { return (GetGlobalConst(pt, numBounces) &  0x7FFFFFFF); }
uint GetPTProgressive() { return (GetGlobalConst(pt, numBounces) & 0x80000000); }

uint GetPTSamplesPerPixel() { return (GetGlobalConst(pt, samplesPerPixel) & 0x3FFFFFFF); }
uint GetPTAntialiasing() { return (GetGlobalConst(pt, samplesPerPixel) & 0x80000000); }
uint GetPTShaderExecutionReordering() { return GetGlobalConst(pt, samplesPerPixel) & 0x40000000; }
*/

uint HasDirectionalLight() { return GetGlobalConst(lighting, hasDirectionalLight); }
uint GetNumPointLights() { return GetGlobalConst(lighting, numPointLights); }
uint GetNumSpotLights() { return GetGlobalConst(lighting, numSpotLights); }

//----------------------------------------------------------------------------------------------------------------
// Root Signature Descriptors and Mappings
// ---------------------------------------------------------------------------------------------------------------

#if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

// Samplers -------------------------------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform sampler Samplers[];

// Uniform Buffers -----------------------------------------------------------------------------------------

layout(set = 0, binding = 1, std430) uniform Block_Camera {
    Camera CameraCB;
};

// Shader Storage Buffers ----------------------------------------------------------------------------------

layout(set = 0, binding = 2, scalar) buffer Block_Lights {
    Light Lights[];
};

layout(set = 0, binding = 3, std430) buffer Block_Materials {
    Material Materials[];
};

layout(set = 0, binding = 5, std430) buffer Block_DDGIVolumeDescGPUPacked {
    DDGIVolumeDescGPUPacked DDGIVolumes[];
};

layout(set = 0, binding = 6, std430) buffer Block_DDGIVolumeResourceIndices {
    DDGIVolumeResourceIndices DDGIVolumeBindless[];
};

// Bindless Resources ---------------------------------------------------------------------------------------

layout(set = 0, binding = 8, rgba8) uniform image2D Image2D_rgba8[];
layout(set = 0, binding = 8, rgba32f) uniform image2D Image2D_rgba32f[];
layout(set = 0, binding = 8, r8) uniform image2D Image2D_r8[];
layout(set = 0, binding = 8, rgba16f) uniform image2D Image2D_rgba16f[];

layout(set = 0, binding = 9, rg32f) uniform image2DArray Image2DArray_rg32f[];
layout(set = 0, binding = 9, rgba32f) uniform image2DArray Image2DArray_rgba32f[];
layout(set = 0, binding = 9, r32f) uniform image2DArray Image2DArray_r32f[];

layout(set = 0, binding = 10) uniform accelerationStructureEXT TLAS[];

layout(set = 0, binding = 11) uniform texture2D Tex2D[];
layout(set = 0, binding = 12) uniform texture2DArray Tex2DArray[];

layout(set = 0, binding = 13) buffer Block_ByteAddressBuffer {
    uint data[];
} ByteAddrBuffer[];

// Defines for Convenience ----------------------------------------------------------------------------------

#define PT_OUTPUT_INDEX 0
#define PT_ACCUMULATION_INDEX 1
#define GBUFFERA_INDEX 2
#define GBUFFERB_INDEX 3
#define GBUFFERC_INDEX 4
#define GBUFFERD_INDEX 5
#define RTAO_OUTPUT_INDEX 6
#define RTAO_RAW_INDEX 7
#define DDGI_OUTPUT_INDEX 8

#define SCENE_TLAS_INDEX 0
#define DDGIPROBEVIS_TLAS_INDEX 1

#define BLUE_NOISE_INDEX 0

#define SPHERE_INDEX_BUFFER_INDEX 0
#define SPHERE_VERTEX_BUFFER_INDEX 1
#define MESH_OFFSETS_INDEX 2
#define GEOMETRY_DATA_INDEX 3
#define GEOMETRY_BUFFERS_INDEX 4

// Sampler Accessor Functions ------------------------------------------------------------------------------

#define BilinearWrapSampler Samplers[0]
#define PointClampSampler Samplers[1]
#define AnisoWrapSampler Samplers[2]

// Resource Accessor Functions ------------------------------------------------------------------------------

// GetCamera

Material GetMaterial(GeometryData geometry) { return Materials[geometry.materialIndex]; }

#define GetAccelerationStructure(index) TLAS[index]

uint ReadUInt(uint index, uint byteOffset) {
    return ByteAddrBuffer[nonuniformEXT(index)].data[byteOffset / 4]; // Divide by 4 to get 32-bit aligned offset
}
uvec2 ReadUInt2(uint index, uint byteOffset) {
    return uvec2(ReadUInt(index, byteOffset), ReadUInt(index, byteOffset + 4));
}
uvec3 ReadUInt3(uint index, uint byteOffset) {
    return uvec3(ReadUInt(index, byteOffset), ReadUInt(index, byteOffset + 4), ReadUInt(index, byteOffset + 8));
}
uvec4 ReadUInt4(uint index, uint byteOffset) {
    return uvec4(ReadUInt(index, byteOffset), ReadUInt(index, byteOffset + 4), ReadUInt(index, byteOffset + 8), ReadUInt(index, byteOffset + 12));
}

void GetGeometryData(uint meshIndex, uint geometryIndex, out GeometryData geometry) {
    uint address = ReadUInt(MESH_OFFSETS_INDEX, meshIndex * 4); // address of the Mesh in the GeometryData buffer
    address += geometryIndex * 12; // offset to mesh primitive geometry, GeometryData stride is 12 bytes

    geometry.materialIndex = ReadUInt(GEOMETRY_DATA_INDEX, address);
    geometry.indexByteAddress = ReadUInt(GEOMETRY_DATA_INDEX, address + 4);
    geometry.vertexByteAddress = ReadUInt(GEOMETRY_DATA_INDEX, address + 8);
}

#define GetSphereIndexBuffer ByteAddrBuffer[SPHERE_INDEX_BUFFER_INDEX]
#define GetSphereVertexBuffer ByteAddrBuffer[SPHERE_VERTEX_BUFFER_INDEX]

uint GetIndexBufferGlobalIndex(uint meshIndex) { return GEOMETRY_BUFFERS_INDEX + (meshIndex * 2); }
uint GetVertexBufferGlobalIndex(uint meshIndex) { return GEOMETRY_BUFFERS_INDEX + (meshIndex * 2) + 1;}

// Bindless Resource Array Accessors ------------------------------------------------------------------------

// #define GetRWTex2D(index) RWTex2D[index]
// convert GetRWTex2D(A_INDEX) in hlsl to Image2D_AFormat[A_INDEX] in glsl according to the format of A
#define GetTex2D(index) Tex2D[index]

// #define GetRWTex2DArray(index) RWTex2DArray[index]
// similar to GetRWTex2D above
#define GetTex2DArray(index) Tex2DArray[index]

#elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP

#error Descriptor Heap Bindless Type is not supported!

#endif // RTXGI_BINDLESS_TYPE

#endif // DESCRIPTORS_GLSL
