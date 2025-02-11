#ifndef DESCRIPTORS_GLSL
#define DESCRIPTORS_GLSL

// need to reallocate the file to the correct path
#include "Framework.glsl"

#include "../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.glsl"
#include "../../include/graphics/Types.glsl"

#define RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS 0
#define RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP 1

#ifndef RTXGI_BINDLESS_TYPE
#error Required define RTXGI_BINDLESS_TYPE is not defined!
#endif

struct TLASInstance
{
#pragma pack_matrix(row_major)
    mat3x4 transform;
#pragma pack_matrix(column_major)
    uint     instanceID24_Mask8;
    uint     instanceContributionToHitGroupIndex24_Flags8;
    uvec2    blasAddress;
};

#define DEFINE_ARRAY_REF(type) VIWO_BUFFER_REF(Array_##type) { type v[]; }

// Global Root / Push Constants ------------------------------------------------------------------------------------

/* Params definition example
VIWO_BUFFER_REF(Params) {
    Buffer globalConst;
    Buffer camera;

    // TODO: use viwo's lights and materials
    // Buffer lights;
    // Buffer materials;

    // StructuredBuffer<TLASInstance> TLASInstances seems not used (check twice when implementing)
    // Buffer tlasInstances;

    Buffer ddgiVolumes;
    Buffer ddgiVolumeBindless;

    // TODO: manage DDGI Volume TLAS in viwo
    Buffer rwTLASInstances;

    // for RWTexture2D in HLSL
    // Image2D_rgba8 GBufferA;
    // Image2D_rgba32f GBufferB;
    // use ivec2 ImageSize(Image2D_rgba8 img), void ImageStore(Image2D_rgba8 img, ivec2 p, vec4 data) and vec4 ImageLoad(Image2D_rgba8 img, ivec2 p);

    // for RWTexture2DArray in HLSL, see format in GetDDGIVolumeTextureFormat() and in config with ddgi.volume.0.textures.***.format
    // Image2DArray_rgba32f ***;

    // for scene TLAS, get handle, maybe refer to below
    // struct GPUScene {
    //     AccelerationStructure acc;
    //     Buffer vertex_layouts;
    //     Buffer instances;
    //     Buffer materials;
    // };

    // usage of texture2D in GLSL:
    // uniform sampler mySampler;
    // uniform texture2D myTexture;
    // sampler2D combined = sampler2D(myTexture, mySampler);
    // vec4 color = texture(combined, texCoord);

    // for Texture2D in HLSL
    // TextureRaw2D sceneTexture;
    // in Framework.glsl, define:
    // #define VIWO_ACCESS_BINDLESS_TEXTURERAW(textureType, textureFormat, handle) viwo_##textureType##_##textureFormat[nonuniformEXT(handle)]
    // in Framework.gen.glsl, define:
    // layout(set = 5, binding = 0) uniform texture2D viwo_texture2D[];
    // struct TextureRaw2D { uint handle; };
    // vec4 Texture(TextureRaw2D tex, sampler s, vec2 p) { return sampler2D(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2D, tex.handle), s), p); }
    // vec4 TexelFetch(TextureRaw2D tex, ivec2 p, int lod) { return texelFetch(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2D, tex.handle), p, lod); }

    // for Texture2DArray in HLSL
    // TextureRaw2DArray ddgiTexture;
    // in Framework.gen.glsl, define:
    // layout(set = 5, binding = 0) uniform texture2DArray viwo_texture2DArray[];
    // struct TextureRaw2DArray { uint handle; };
    // vec4 Texture(sampler s, TextureRaw2DArray tex, vec3 p) { return sampler2DArray(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2DArray, tex.handle), s), p); }

    // for ByteAddressBuffer in HLSL
    // Buffer meshOffsets;
    // Buffer geometryData;
    // VIWO_BUFFER_REF(Array_GeometryBuffer) {
    //    Buffer v[];
    // }
    // Array_GeometryBuffer geometryBuffers;
};
*/

// VK_PUSH_CONST ConstantBuffer<GlobalConstants> GlobalConst : register(b0, space0);
VIWO_BUFFER_REF(Block_GlobalConstants) {
    GlobalConstants v;
};
// use VIWO_LOAD_PARAMS(Params).globalConst to get the handle of the globalConst buffer
#define GetGlobalConst(globalConst, x, y) (VIWO_GET_BUFFER_REF(globalConst, Block_GlobalConstants).v.x##_##y)

uint GetPTNumBounces() { return (GetGlobalConst(pt, numBounces) &  0x7FFFFFFF); }
uint GetPTProgressive() { return (GetGlobalConst(pt, numBounces) & 0x80000000); }

uint GetPTSamplesPerPixel() { return (GetGlobalConst(pt, samplesPerPixel) & 0x3FFFFFFF); }
uint GetPTAntialiasing() { return (GetGlobalConst(pt, samplesPerPixel) & 0x80000000); }
uint GetPTShaderExecutionReordering() { return GetGlobalConst(pt, samplesPerPixel) & 0x40000000; }

uint HasDirectionalLight() { return GetGlobalConst(lighting, hasDirectionalLight); }
uint GetNumPointLights() { return GetGlobalConst(lighting, numPointLights); }
uint GetNumSpotLights() { return GetGlobalConst(lighting, numSpotLights); }

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

DEFINE_ARRAY_REF(DDGIVolumeDescGPUPacked);
Array_DDGIVolumeDescGPUPacked GetDDGIVolumeConstants(Buffer ddgiVolumes, uint index) { return VIWO_GET_BUFFER_REF(ddgiVolumes, Block_DDGIVolumeDescGPUPacked); }
DEFINE_ARRAY_REF(DDGIVolumeResourceIndices);
Array_DDGIVolumeResourceIndices GetDDGIVolumeResourceIndices(Buffer ddgiVolumeBindless, uint index) { return VIWO_GET_BUFFER_REF(ddgiVolumeBindless, Block_DDGIVolumeResourceIndices); }

DEFINE_ARRAY_REF(TLASInstance v);
Array_TLASInstance GetDDGIProbeVisTLASInstances(Buffer rwTLASInstances) { return VIWO_GET_BUFFER_REF(rwTLASInstances, Block_TLASInstance); }

VIWO_BUFFER_REF(Block_ByteAddressBuffer) {
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

sampler GetBilinearWrapSampler() { return Samplers[0]; }
sampler GetPointClampSampler() { return Samplers[1]; }
sampler GetAnisoWrapSampler() { return Samplers[2]; }

// Resource Accessor Functions ------------------------------------------------------------------------------

uint ReadUInt(Buffer ByteAddrBuffer, uint byteOffset) {
    return VIWO_GET_BUFFER_REF(ByteAddrBuffer, Block_ByteAddressBuffer).data[byteOffset / 4]; // Divide by 4 to get 32-bit aligned offset
}
uvec2 ReadUInt2(Buffer ByteAddrBuffer, uint byteOffset) {
    return uvec2(ReadUInt(ByteAddrBuffer, byteOffset), ReadUInt(ByteAddrBuffer, byteOffset + 4));
}
uvec3 ReadUInt3(Buffer ByteAddrBuffer, uint byteOffset) {
    return uvec3(ReadUInt(ByteAddrBuffer, byteOffset), ReadUInt(ByteAddrBuffer, byteOffset + 4), ReadUInt(ByteAddrBuffer, byteOffset + 8));
}

void GetGeometryData(Buffer meshOffsets, Buffer geometryData, uint meshIndex, uint geometryIndex, out GeometryData geometry) {
    uint address = ReadUInt(meshOffsets, meshIndex * 4); // address of the Mesh in the GeometryData buffer
    address += geometryIndex * 12; // offset to mesh primitive geometry, GeometryData stride is 12 bytes

    geometry.materialIndex = ReadUInt(geometryData, address);
    geometry.indexByteAddress = ReadUInt(geometryData, address + 4);
    geometry.vertexByteAddress = ReadUInt(geometryData, address + 8);
}

Buffer GetIndexBuffer(Array_GeometryBuffer geometryBuffers, uint meshIndex) { return geometryBuffers.v[meshIndex * 2]; }
Buffer GetVertexBuffer(Array_GeometryBuffer geometryBuffers, uint meshIndex) { return geometryBuffers.v[meshIndex * 2 + 1]; }

#elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP

#error Descriptor Heap Bindless Type is not supported!

#endif // RTXGI_BINDLESS_TYPE

#endif // DESCRIPTORS_GLSL
