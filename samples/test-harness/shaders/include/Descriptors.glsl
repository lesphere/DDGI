#ifndef DESCRIPTORS_GLSL
#define DESCRIPTORS_GLSL

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : enable

// need to add the viwo shader include dir into shaderc include path
#include "Draw/Framework.glsl"

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

#define DEFINE_ARRAY_REF(type) VIWO_BUFFER_REF(Array_##type) { type v[]; }

// Global Root / Push Constants ------------------------------------------------------------------------------------

// VK_PUSH_CONST ConstantBuffer<GlobalConstants> GlobalConst : register(b0, space0);
VIWO_BUFFER_REF(Block_GlobalConstants) {
    GlobalConstants v;
};
// use VIWO_LOAD_PARAMS(Params).globalConst to get the handle of the globalConst buffer
#define GetGlobalConst(globalConst, x, y) (VIWO_GET_BUFFER_REF(globalConst, Block_GlobalConstants).v.x##_##y)

/* for PT
uint GetPTNumBounces() { return (GetGlobalConst(pt, numBounces) &  0x7FFFFFFF); }
uint GetPTProgressive() { return (GetGlobalConst(pt, numBounces) & 0x80000000); }

uint GetPTSamplesPerPixel() { return (GetGlobalConst(pt, samplesPerPixel) & 0x3FFFFFFF); }
uint GetPTAntialiasing() { return (GetGlobalConst(pt, samplesPerPixel) & 0x80000000); }
uint GetPTShaderExecutionReordering() { return GetGlobalConst(pt, samplesPerPixel) & 0x40000000; }
*/

// TODO: for lights
uint HasDirectionalLight(Buffer globalConst) { return GetGlobalConst(globalConst, lighting, hasDirectionalLight); }
uint GetNumPointLights(Buffer globalConst) { return GetGlobalConst(globalConst, lighting, numPointLights); }
uint GetNumSpotLights(Buffer globalConst) { return GetGlobalConst(globalConst, lighting, numSpotLights); }

//----------------------------------------------------------------------------------------------------------------
// Root Signature Descriptors and Mappings
// ---------------------------------------------------------------------------------------------------------------

#if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

// Samplers -------------------------------------------------------------------------------------------------

layout(set = 4, binding = 0) uniform sampler Samplers[];

// Constant Buffers -----------------------------------------------------------------------------------------

VIWO_BUFFER_REF(Block_Camera) {
    Camera v;
};
// use VIWO_LOAD_PARAMS(Params).camera to get the handle of the camera buffer
Camera GetCamera(Buffer camera) { return VIWO_GET_BUFFER_REF(camera, Block_Camera).v; }

// Bindless Resources ---------------------------------------------------------------------------------------

layout(buffer_reference, scalar) buffer Array_Lights {
    Light v[];
};
Light GetLight(Buffer lights, uint index) { return VIWO_GET_BUFFER_REF(lights, Array_Lights).v[index]; }

DEFINE_ARRAY_REF(Material);
Material GetMaterial(Buffer materials, GeometryData geometry) { return VIWO_GET_BUFFER_REF(materials, Array_Material).v[geometry.materialIndex]; }

DEFINE_ARRAY_REF(DDGIVolumeDescGPUPacked);
Array_DDGIVolumeDescGPUPacked GetDDGIVolumeConstants(Buffer ddgiVolumes, uint index) { return VIWO_GET_BUFFER_REF(ddgiVolumes, Array_DDGIVolumeDescGPUPacked); }
DEFINE_ARRAY_REF(DDGIVolumeResourceIndices);
Array_DDGIVolumeResourceIndices GetDDGIVolumeResourceIndices(Buffer ddgiVolumeBindless, uint index) { return VIWO_GET_BUFFER_REF(ddgiVolumeBindless, Array_DDGIVolumeResourceIndices); }

// DEFINE_ARRAY_REF(TLASInstance);
// Array_TLASInstance GetDDGIProbeVisTLASInstances(Buffer rwTLASInstances) { return VIWO_GET_BUFFER_REF(rwTLASInstances, Array_TLASInstance); }

layout(set = 5, binding = 0) uniform texture2D Tex2D[];
layout(set = 5, binding = 1) uniform texture2DArray Tex2DArray[];

VIWO_BUFFER_REF(ByteAddressBuffer) {
    uint data[];
};

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

uint ReadUInt(Buffer ByteAddrBuffer, uint byteOffset) {
    return VIWO_GET_BUFFER_REF(ByteAddrBuffer, ByteAddressBuffer).data[byteOffset / 4]; // Divide by 4 to get 32-bit aligned offset
}
uvec2 ReadUInt2(Buffer ByteAddrBuffer, uint byteOffset) {
    return uvec2(ReadUInt(ByteAddrBuffer, byteOffset), ReadUInt(ByteAddrBuffer, byteOffset + 4));
}
uvec3 ReadUInt3(Buffer ByteAddrBuffer, uint byteOffset) {
    return uvec3(ReadUInt(ByteAddrBuffer, byteOffset), ReadUInt(ByteAddrBuffer, byteOffset + 4), ReadUInt(ByteAddrBuffer, byteOffset + 8));
}
uvec4 ReadUInt4(Buffer ByteAddrBuffer, uint byteOffset) {
    return uvec4(ReadUInt(ByteAddrBuffer, byteOffset), ReadUInt(ByteAddrBuffer, byteOffset + 4), ReadUInt(ByteAddrBuffer, byteOffset + 8), ReadUInt(ByteAddrBuffer, byteOffset + 12));
}

void GetGeometryData(Buffer meshOffsets, Buffer geometryData, uint meshIndex, uint geometryIndex, out GeometryData geometry) {
    uint address = ReadUInt(meshOffsets, meshIndex * 4); // address of the Mesh in the GeometryData buffer
    address += geometryIndex * 12; // offset to mesh primitive geometry, GeometryData stride is 12 bytes

    geometry.materialIndex = ReadUInt(geometryData, address);
    geometry.indexByteAddress = ReadUInt(geometryData, address + 4);
    geometry.vertexByteAddress = ReadUInt(geometryData, address + 8);
}

DEFINE_ARRAY_REF(Buffer);
Buffer GetIndexBuffer(Buffer sceneIBH, uint meshIndex) { return VIWO_GET_BUFFER_REF(sceneIBH, Array_Buffer).v[meshIndex]; }
Buffer GetVertexBuffer(Buffer sceneVBH, uint meshIndex) { return VIWO_GET_BUFFER_REF(sceneVBH, Array_Buffer).v[meshIndex]; }

// Bindless Resource Array Accessors ------------------------------------------------------------------------

// #define GetRWTex2D(index) RWTex2D[index]
#define GetTex2D(index) Tex2D[index]

// #define GetRWTex2DArray(index) RWTex2DArray[index]
#define GetTex2DArray(index) Tex2DArray[index]

// Push Constants Params ------------------------------------------------------------------------------------

// struct GPUScene {
//     AccelerationStructure acc;
//     Buffer vertex_layouts;
//     Buffer instances;
//     Buffer materials;
// };

VIWO_BUFFER_REF(Params) {
    Buffer globalConst;

    // TODO: use viwo's lights and materials
    Buffer lights;
    Buffer materials;

    AccelerationStructure tlas;

    // Buffer camera; // TODO

    // StructuredBuffer<TLASInstance> TLASInstances seems not used (check twice when implementing)
    // Buffer tlasInstances;

    Buffer ddgiVolumes;
    Buffer ddgiVolumeBindless;

    Buffer meshOffsets;
    Buffer geometryData;
    Buffer sceneIBH;
    Buffer sceneVBH;
};

#elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP

#error Descriptor Heap Bindless Type is not supported!

#endif // RTXGI_BINDLESS_TYPE

#endif // DESCRIPTORS_GLSL
