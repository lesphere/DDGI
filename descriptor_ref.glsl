// Params definition example
VIWO_BUFFER_REF(Params) {
    Buffer globalConst;
    Buffer camera;

    // TODO: use viwo's lights and materials
    Buffer lights;
    Buffer materials;

    // StructuredBuffer<TLASInstance> TLASInstances seems not used (check twice when implementing)
    Buffer tlasInstances;

    Buffer ddgiVolumes;
    Buffer ddgiVolumeBindless;

    // TODO: manage DDGI Volume TLAS in viwo
    Buffer rwTLASInstances;

    // for RWTexture2D in HLSL, viwo's bindless way
    // make sure the formats defined in Framework.gen.glsl
    Image2D_bgra8 PTOutput;         // refer to Graphics::Vulkan::PathTracing::CreateTextures()
    Image2D_rgba32f PTAccumulation; // refer to Graphics::Vulkan::PathTracing::CreateTextures()
    Image2D_bgra8 GBufferA;         // refer to Graphics::Vulkan::CreateRenderTargets()
    Image2D_rgba32f GBufferB;       // refer to Graphics::Vulkan::CreateRenderTargets()
    Image2D_rgba32f GBufferC;       // refer to Graphics::Vulkan::CreateRenderTargets()
    Image2D_rgba32f GBufferD;       // refer to Graphics::Vulkan::CreateRenderTargets()
    Image2D_r8 RTAOOutput;          // refer to Graphics::Vulkan::RTAO::CreateTextures()
    Image2D_r8 RTAORaw;             // refer to Graphics::Vulkan::RTAO::CreateTextures()
    Image2D_rgba16f DDGIOutput;     // refer to Graphics::Vulkan::DDGI::CreateTextures()
    // usage: ivec2 ImageSize(Image2D_rgba8 img), void ImageStore(Image2D_rgba8 img, ivec2 p, vec4 data) and vec4 ImageLoad(Image2D_rgba8 img, ivec2 p);

    // // for RWTexture2D in HLSL, try to do in HLSL's way, but how ?
    // // in Descriptors.glsl, define all the possible formats in the same binding:
    // layout(set = 6, binding = 0, bgra8) uniform image2D ddgi_image2D_bgra8[];
    // layout(set = 6, binding = 0, rgba32f) uniform image2D ddgi_image2D_rgba32f[];
    // layout(set = 6, binding = 0, r8) uniform image2D ddgi_image2D_r8[];
    // layout(set = 6, binding = 0, rgba16f) uniform image2D ddgi_image2D_rgba16f[];

    // for RWTexture2DArray in HLSL, viwo's bindless way
    // make sure the formats defined in Framework.gen.glsl
    // see format in GetDDGIVolumeTextureFormat() and in config with ddgi.volume.0.textures.***.format
    // all the possible formats in GetDDGIVolumeTextureFormat():
    // Image2DArray_rg32f RayData;              // VK_FORMAT_R32G32_SFLOAT
    Image2DArray_rgba32f RayData;            // VK_FORMAT_R32G32B32A32_SFLOAT
    // Image2DArray_rgb10_a2 Irradiance;        // VK_FORMAT_A2B10G10R10_UNORM_PACK32, is this respond to rgba10_a2?
    // Image2DArray_rgba16f Irradiance;         // VK_FORMAT_R16G16B16A16_SFLOAT
    Image2DArray_rgba32f Irradiance;         // VK_FORMAT_R32G32B32A32_SFLOAT
    // Image2DArray_rg16f Distance;             // VK_FORMAT_R16G16_SFLOAT
    Image2DArray_rg32f Distance;             // VK_FORMAT_R32G32_SFLOAT
    // Image2DArray_rgba16f Data;               // VK_FORMAT_R16G16B16A16_SFLOAT
    Image2DArray_rgba32f Data;               // VK_FORMAT_R32G32B32A32_SFLOAT
    // Image2DArray_r16f Variability;           // VK_FORMAT_R16_SFLOAT
    Image2DArray_r32f Variability;           // VK_FORMAT_R32_SFLOAT
    Image2DArray_rg32f VariabilityAverage;   // VK_FORMAT_R32G32_SFLOAT

    // for RWTexture2DArray in HLSL, try to do in HLSL's way, but how ?
    // in Descriptors.glsl, define all the possible formats in the same binding:
    // layout(set = 6, binding = 1, rg32f) uniform image2DArray ddgi_image2DArray_rg32f[];
    // layout(set = 6, binding = 1, rgba32f) uniform image2DArray ddgi_image2DArray_rgba32f[];
    // layout(set = 6, binding = 1, rgb10_a2) uniform image2DArray ddgi_image2DArray_rgb10_a2[];
    // layout(set = 6, binding = 1, rgba16f) uniform image2DArray ddgi_image2DArray_rgba16f[];
    // layout(set = 6, binding = 1, rg16f) uniform image2DArray ddgi_image2DArray_rg16f[];
    // layout(set = 6, binding = 1, r16f) uniform image2DArray ddgi_image2DArray_r16f[];
    // layout(set = 6, binding = 1, r32f) uniform image2DArray ddgi_image2DArray_r32f[];

    // for scene TLAS, get handle, maybe refer to below
    struct GPUScene {
        AccelerationStructure acc;
        Buffer vertex_layouts;
        Buffer instances;
        Buffer materials;
    };

    // usage of texture2D in GLSL:
    uniform sampler mySampler;
    uniform texture2D myTexture;
    sampler2D combined = sampler2D(myTexture, mySampler);
    vec4 color = texture(combined, texCoord);

    // deprecated
    // // for Texture2D in HLSL
    // TextureRaw2D sceneTexture;
    // // in Framework.glsl, define:
    // #define VIWO_ACCESS_BINDLESS_TEXTURERAW(ty, hd) viwo_##ty[nonuniformEXT(hd)]
    // // in Framework.gen.glsl, define:
    // layout(set = 5, binding = 0) uniform texture2D viwo_texture2D[];
    // struct TextureRaw2D { uint handle; };
    // vec4 Texture(TextureRaw2D tex, sampler s, vec2 p) { return texture(sampler2D(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2D, tex.handle), s), p); }
    // vec4 TexelFetch(TextureRaw2D tex, ivec2 p, int lod) { return texelFetch(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2D, tex.handle), p, lod); }

    // for Texture2D in HLSL, not use the viwo's bindless way, use the way like in HLSL
    // in Descriptors.glsl, define:
    layout(set = 5, binding = 0) uniform texture2D viwo_texture2D[];

    // deprecated
    // // for Texture2DArray in HLSL
    // TextureRaw2DArray ddgiTexture;
    // // in Framework.gen.glsl, define:
    // layout(set = 5, binding = 0) uniform texture2DArray viwo_texture2DArray[];
    // struct TextureRaw2DArray { uint handle; };
    // vec4 Texture(TextureRaw2DArray tex, sampler s, vec3 p) { return texture(sampler2DArray(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2DArray, tex.handle), s), p); }
    // vec4 TextureLod(TextureRaw2DArray tex, sampler s, vec3 p, float bias) { return textureLod(sampler2DArray(VIWO_ACCESS_BINDLESS_TEXTURERAW(texture2DArray, tex.handle), s), p, bias); }

    // for Texture2DArray in HLSL, not use the viwo's bindless way, use the way like in HLSL
    // in Descriptors.glsl, define:
    layout(set = 6, binding = 0) uniform texture2DArray viwo_texture2DArray[];

    // for ByteAddressBuffer in HLSL
    Buffer meshOffsets;
    Buffer geometryData;

    VIWO_BUFFER_REF(Array_Buffer) {
       Buffer v[];
    }
    Buffer sceneIBH;
    Buffer sceneVBH;
    // get index/vertex buffer from sceneIBH/sceneVBH
    Buffer GetIndexBuffer(Buffer sceneIBH, uint meshIndex) { return VIWO_GET_BUFFER_REF(sceneIBH, Array_Buffer).v[meshIndex]; }
    Buffer GetVertexBuffer(Buffer sceneVBH, uint meshIndex) { return VIWO_GET_BUFFER_REF(sceneVBH, Array_Buffer).v[meshIndex]; }
};
