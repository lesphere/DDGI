#version 450 // Ensure compatibility with Vulkan and modern GLSL features

#ifndef RAYTRACING_GLSL
#define RAYTRACING_GLSL

// Note: you must include Descriptors.hlsl before this file
// which defines structures like Payload and PackedPayload

/**
 * Pack the payload into a compressed format.
 * Complement of UnpackPayload().
 */
PackedPayload PackPayload(Payload input) {
    PackedPayload output;
    output.hitT = input.hitT;
    output.worldPosition = input.worldPosition;

    output.packed0.x = packHalf2x16(vec2(input.albedo.r, input.albedo.g));
    output.packed0.y = packHalf2x16(vec2(input.albedo.b, input.normal.x));
    output.packed0.z = packHalf2x16(vec2(input.normal.y, input.normal.z));
    output.packed0.w = packHalf2x16(vec2(input.metallic, input.roughness));

    output.packed1.x = packHalf2x16(vec2(input.shadingNormal.x, input.shadingNormal.y));
    output.packed1.y = packHalf2x16(vec2(input.shadingNormal.z, input.opacity));
    output.packed1.z = packHalf2x16(vec2(input.hitKind, 0.0));

    return output;
}

/**
 * Unpack the compressed payload into the full-sized payload format.
 * Complement of PackPayload().
 */
Payload UnpackPayload(PackedPayload input) {
    Payload output;
    output.hitT = input.hitT;
    output.worldPosition = input.worldPosition;

    output.albedo.r = unpackHalf2x16(input.packed0.x).x;
    output.albedo.g = unpackHalf2x16(input.packed0.x).y;
    output.albedo.b = unpackHalf2x16(input.packed0.y).x;
    output.normal.x = unpackHalf2x16(input.packed0.y).y;
    output.normal.y = unpackHalf2x16(input.packed0.z).x;
    output.normal.z = unpackHalf2x16(input.packed0.z).y;
    output.metallic = unpackHalf2x16(input.packed0.w).x;
    output.roughness = unpackHalf2x16(input.packed0.w).y;

    output.shadingNormal.x = unpackHalf2x16(input.packed1.x).x;
    output.shadingNormal.y = unpackHalf2x16(input.packed1.x).y;
    output.shadingNormal.z = unpackHalf2x16(input.packed1.y).x;
    output.opacity = unpackHalf2x16(input.packed1.y).y;
    output.hitKind = unpackHalf2x16(input.packed1.z).x;

    return output;
}

/**
 * Load a triangle's indices.
 */
uvec3 LoadIndices(Array_GeometryBuffer geometryBuffers, uint meshIndex, uint primitiveIndex, GeometryData geometry) {
    uint address = geometry.indexByteAddress + (primitiveIndex * 3) * 4; // 3 indices per primitive, 4 bytes for each index
    return ReadUInt3(GetIndexBuffer(geometryBuffers, meshIndex), address);
}

// Function to load vertices
void LoadVertices(Array_GeometryBuffer geometryBuffers, uint meshIndex, uint primitiveIndex, GeometryData geometry, out vec4 vertices[3]) {
    // Get the indices
    uvec3 indices = LoadIndices(geometryBuffers, meshIndex, primitiveIndex, geometry);

    // Load the vertices
    for (int i = 0; i < 3; i++) {
        uint address = geometry.vertexByteAddress + indices[i] * 48; // Vertices contain 12 floats / 48 bytes
        
        // Load the position
        vertices[i].position = uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load3(address));
        address += 12;

        // Load the normal
        vertices[i].normal = uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load3(address));
        address += 12;

        // Load the tangent
        vertices[i].tangent = uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load4(address));
        address += 16;

        // Load the texture coordinates
        vertices[i].uv0 = uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load2(address));
    }
}

/**
 * Load a triangle's vertex data (only position and uv0).
 */
void LoadVerticesPosUV0(Array_GeometryBuffer geometryBuffers, uint meshIndex, uint primitiveIndex, GeometryData geometry, out Vertex vertices[3])
{
    // Get the indices
    uvec3 indices = LoadIndices(geometryBuffers, meshIndex, primitiveIndex, geometry);

    // Load the vertices
    uint address;
    for (uint i = 0; i < 3; i++) {
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // Vertices contain 12 floats / 48 bytes

        // Load the position
        vertices[i].position = uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load3(address));
        address += 40; // skip normal and tangent

        // Load the texture coordinates
        vertices[i].uv0 = uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load2(address));
    }
}

/**
 * Load (only) a triangle's texture coordinates and return the barycentric interpolated texture coordinates.
 */
vec2 LoadAndInterpolateUV0(Array_GeometryBuffer geometryBuffers, uint meshIndex, uint primitiveIndex, GeometryData geometry, vec3 barycentrics)
{
    // Get the triangle indices
    uvec3 indices = LoadIndices(geometryBuffers, meshIndex, primitiveIndex, geometry);

    // Interpolate the texture coordinates
    int address;
    vec2 uv0 = vec2(0.f, 0.f);
    for (uint i = 0; i < 3; i++)
    {
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // 12 floats (3: pos, 3: normals, 4:tangent, 2:uv0)
        address += 40;                                                // 40 bytes (10 * 4): skip position, normal, and tangent
        uv0 += uintBitsToFloat(GetVertexBuffer(geometryBuffers, meshIndex).Load2(address)) * barycentrics[i];
    }

    return uv0;
}

/**
 * Return interpolated vertex attributes (all).
 */
Vertex InterpolateVertex(Vertex vertices[3], vec3 barycentrics)
{
    // Interpolate the vertex attributes
    Vertex v;
    for (uint i = 0; i < 3; i++)
    {
        v.position += vertices[i].position * barycentrics[i];
        v.normal += vertices[i].normal * barycentrics[i];
        v.tangent.xyz += vertices[i].tangent.xyz * barycentrics[i];
        v.uv0 += vertices[i].uv0 * barycentrics[i];
    }

    // Normalize normal and tangent vectors, set tangent direction component
    v.normal = normalize(v.normal);
    v.tangent.xyz = normalize(v.tangent.xyz);
    v.tangent.w = vertices[0].tangent.w;

    return v;
}

/**
 * Return interpolated vertex attributes (uv0 only)
 */
Vertex InterpolateVertexUV0(Vertex vertices[3], vec3 barycentrics)
{
    // Interpolate the vertex attributes
    Vertex v;
    for (uint i = 0; i < 3; i++)
    {
        v.uv0 += vertices[i].uv0 * barycentrics[i];
    }

    return v;
}

// --- Ray Differentials ---

struct RayDiff
{
    vec3 dOdx;
    vec3 dOdy;
    vec3 dDdx;
    vec3 dDdy;
};

/**
 * Get the ray direction differentials.
 */
void ComputeRayDirectionDifferentials(vec3 nonNormalizedCameraRaydir, vec3 right, vec3 up, vec2 viewportDims, out vec3 dDdx, out vec3 dDdy)
{
    // Igehy Equation 8
    float dd = dot(nonNormalizedCameraRaydir, nonNormalizedCameraRaydir);
    float divd = 2.f / (dd * sqrt(dd));
    float dr = dot(nonNormalizedCameraRaydir, right);
    float du = dot(nonNormalizedCameraRaydir, up);
    dDdx = ((dd * right) - (dr * nonNormalizedCameraRaydir)) * divd / viewportDims.x;
    dDdy = -((dd * up) - (du * nonNormalizedCameraRaydir)) * divd / viewportDims.y;
}

/**
 * Propogate the ray differential to the current hit point.
 */
void PropagateRayDiff(vec3 D, float t, vec3 N, inout RayDiff rd)
{
    // Part of Igehy Equation 10
    vec3 dodx = rd.dOdx + t * rd.dDdx;
    vec3 dody = rd.dOdy + t * rd.dDdy;

    // Igehy Equations 10 and 12
    float rcpDN = 1.f / dot(D, N);
    float dtdx = -dot(dodx, N) * rcpDN;
    float dtdy = -dot(dody, N) * rcpDN;
    dodx += D * dtdx;
    dody += D * dtdy;

    // Store differential origins
    rd.dOdx = dodx;
    rd.dOdy = dody;
}

/**
 * Apply instance transforms to geometry, compute triangle edges and normal.
 */
void PrepVerticesForRayDiffs(Vertex vertices[3], out vec3 edge01, out vec3 edge02, out vec3 faceNormal)
{
    // Apply instance transforms
    vertices[0].position = (gl_ObjectToWorld3x4EXT * vec4(vertices[0].position, 1.f)).xyz;
    vertices[1].position = (gl_ObjectToWorld3x4EXT * vec4(vertices[1].position, 1.f)).xyz;
    vertices[2].position = (gl_ObjectToWorld3x4EXT * vec4(vertices[2].position, 1.f)).xyz;

    // Find edges and face normal
    edge01 = vertices[1].position - vertices[0].position;
    edge02 = vertices[2].position - vertices[0].position;
    faceNormal = cross(edge01, edge02);
}

/**
 * Get the barycentric differentials.
 */
void ComputeBarycentricDifferentials(RayDiff rd, vec3 rayDir, vec3 edge01, vec3 edge02, vec3 faceNormalW, out vec2 dBarydx, out vec2 dBarydy)
{
    // Igehy "Normal-Interpolated Triangles"
    vec3 Nu = cross(edge02, faceNormalW);
    vec3 Nv = cross(edge01, faceNormalW);

    // Plane equations for the triangle edges, scaled in order to make the dot with the opposite vertex equal to 1
    vec3 Lu = Nu / (dot(Nu, edge01));
    vec3 Lv = Nv / (dot(Nv, edge02));

    dBarydx.x = dot(Lu, rd.dOdx);     // du / dx
    dBarydx.y = dot(Lv, rd.dOdx);     // dv / dx
    dBarydy.x = dot(Lu, rd.dOdy);     // du / dy
    dBarydy.y = dot(Lv, rd.dOdy);     // dv / dy
}

/**
 * Get the interpolated texture coordinate differentials.
 */
void InterpolateTexCoordDifferentials(vec2 dBarydx, vec2 dBarydy, Vertex vertices[3], out vec2 dx, out vec2 dy)
{
    vec2 delta1 = vertices[1].uv0 - vertices[0].uv0;
    vec2 delta2 = vertices[2].uv0 - vertices[0].uv0;
    dx = dBarydx.x * delta1 + dBarydx.y * delta2;
    dy = dBarydy.x * delta1 + dBarydy.y * delta2;
}

/**
 * Get the texture coordinate differentials using ray differentials.
 */
//void ComputeUV0Differentials(Vertex vertices[3], ConstantBuffer<Camera> camera, vec3 rayDirection, float hitT, out vec2 dUVdx, out vec2 dUVdy)
void ComputeUV0Differentials(Vertex vertices[3], vec3 rayDirection, float hitT, out vec2 dUVdx, out vec2 dUVdy)
{
    // Initialize a ray differential
    RayDiff rd = (RayDiff)0;

    // Get ray direction differentials
    //ComputeRayDirectionDifferentials(rayDirection, camera.right, camera.up, camera.resolution, rd.dDdx, rd.dDdy);
    ComputeRayDirectionDifferentials(rayDirection, GetCamera().right, GetCamera().up, GetCamera().resolution, rd.dDdx, rd.dDdy);

    // Get the triangle edges and face normal
    vec3 edge01, edge02, faceNormal;
    PrepVerticesForRayDiffs(vertices, edge01, edge02, faceNormal);

    // Propagate the ray differential to the current hit point
    PropagateRayDiff(rayDirection, hitT, faceNormal, rd);

    // Get the barycentric differentials
    vec2 dBarydx, dBarydy;
    ComputeBarycentricDifferentials(rd, rayDirection, edge01, edge02, faceNormal, dBarydx, dBarydy);

    // Interpolate the texture coordinate differentials
    InterpolateTexCoordDifferentials(dBarydx, dBarydy, vertices, dUVdx, dUVdy);
}

#endif // RAYTRACING_GLSL
