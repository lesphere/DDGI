#version 460 core

#include "samples/test-harness/shaders/include/Descriptors.glsl"
#include "samples/test-harness/shaders/include/RayTracing.glsl"

layout(location = 0) rayPayloadInEXT PackedPayload packedPayload;
hitAttributeEXT Block_Attrib {
    vec2 barycentrics;
} attrib;

void main() {
    Params params = VIWO_LOAD_PARAMS(Params);

    Payload payload = Payload(vec3(0.f), 0.f, vec3(0.f), 0.f, vec3(0.f), 0.f, vec3(0.f), 0.f, 0u);
    payload.hitT = gl_HitTEXT;
    payload.hitKind = gl_HitKindEXT;

    // Load the intersected mesh geometry's data
    GeometryData geometry;
    GetGeometryData(params.meshOffsets, params.geometryData, gl_InstanceCustomIndexEXT, gl_GeometryIndexEXT, geometry);

    // Load the triangle's vertices
    Vertex vertices[3];
    LoadVertices(params.sceneIBH, params.sceneVBH, gl_InstanceCustomIndexEXT, gl_PrimitiveID, geometry, vertices);

    // Interpolate the triangle's attributes for the hit location (position, normal, tangent, texture coordinates)
    vec3 barycentrics = vec3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
    Vertex v = InterpolateVertex(vertices, barycentrics);

    // World position
    payload.worldPosition = v.position;
    payload.worldPosition = (gl_ObjectToWorldEXT * vec4(payload.worldPosition, 1.f)).xyz; // instance transform

    // Geometric normal
    payload.normal = v.normal;
    payload.normal = normalize((gl_ObjectToWorldEXT * vec4(payload.normal, 0.f)).xyz);
    payload.shadingNormal = payload.normal;

    // Load the surface material
    Material material = GetMaterial(params.materials, geometry);
    payload.albedo = material.albedo;
    payload.opacity = material.opacity;

    // Albedo and Opacity
    if (material.albedoTexIdx > -1) {
        // Get the number of mip levels
        uint numLevels = uint(textureQueryLevels(GetTex2D(material.albedoTexIdx)));

        // Sample the albedo texture
        vec4 bco = textureLod(sampler2D(GetTex2D(material.albedoTexIdx), BilinearWrapSampler), v.uv0, float(numLevels) / 2.f);
        payload.albedo *= bco.rgb;
        payload.opacity *= bco.a;
    }

    // Shading normal
    if (material.normalTexIdx > -1) {
        // Get the number of mip levels
        uint numLevels = uint(textureQueryLevels(GetTex2D(material.normalTexIdx)));

        vec3 tangent = normalize((gl_ObjectToWorldEXT * vec4(v.tangent.xyz, 0.f)).xyz);
        vec3 bitangent = cross(payload.normal, tangent) * vec3(v.tangent.w); // why need to multiply by the tangent's w component?
        mat3 TBN = { tangent, bitangent, payload.normal };
        payload.shadingNormal = textureLod(sampler2D(GetTex2D(material.normalTexIdx), BilinearWrapSampler), v.uv0, float(numLevels) / 2.f).xyz;
        payload.shadingNormal = (payload.shadingNormal * 2.f) - 1.f;    // Transform to [-1, 1]
        payload.shadingNormal = TBN * payload.shadingNormal;            // Transform tangent-space normal to world-space
    }

    // Pack the payload
    packedPayload = PackPayload(payload);
}
