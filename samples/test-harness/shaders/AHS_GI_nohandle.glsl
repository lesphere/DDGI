#version 460 core

#extension GL_EXT_samplerless_texture_functions : require

#include "samples/test-harness/shaders/include/Descriptors_nohandle.glsl"
#include "samples/test-harness/shaders/include/RayTracing_nohandle.glsl"

layout(location = 0) rayPayloadInEXT PackedPayload packedPayload;
hitAttributeEXT Block_Attrib {
    vec2 barycentrics;
} attrib;

void main() {
    // Load the intersected mesh geometry's data
    GeometryData geometry;
    GetGeometryData(gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT, geometry);

    // Load the surface material
    Material material = GetMaterial(geometry);

    float alpha = material.opacity;
    if (material.alphaMode == 2) {
        // Load the vertices
        Vertex vertices[3];
        LoadVerticesPosUV0(gl_InstanceCustomIndexEXT, gl_PrimitiveID, geometry, vertices);

        // Interpolate the triangle's texture coordinates
        vec3 barycentrics = vec3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
        Vertex v = InterpolateVertexUV0(vertices, barycentrics);

        // Sample the texture
        if (material.albedoTexIdx > -1) {
            // Get the number of mip levels
            uint numLevels = uint(textureQueryLevels(GetTex2D(material.albedoTexIdx)));

            // Sample the texture
            alpha *= textureLod(sampler2D(GetTex2D(material.albedoTexIdx), BilinearWrapSampler), v.uv0, numLevels * 0.6667f).a;
        }
    }

    if (alpha < material.alphaCutoff) ignoreIntersectionEXT;
}
