#ifndef RAYTRACING_GLSL
#define RAYTRACING_GLSL

// Note: you must include Descriptors.glsl before this file
// which defines structures like Payload and PackedPayload

/**
 * Pack the payload into a compressed format.
 * Complement of UnpackPayload().
 */
PackedPayload PackPayload(Payload unpacked) {
    PackedPayload packed = PackedPayload(0.f, vec3(0.f), uvec4(0u), uvec3(0u));
    packed.hitT = unpacked.hitT;
    packed.worldPosition = unpacked.worldPosition;

    packed.packed0.x = packHalf2x16(vec2(unpacked.albedo.r, unpacked.albedo.g));
    packed.packed0.y = packHalf2x16(vec2(unpacked.albedo.b, unpacked.normal.x));
    packed.packed0.z = packHalf2x16(vec2(unpacked.normal.y, unpacked.normal.z));
    packed.packed0.w = packHalf2x16(vec2(unpacked.metallic, unpacked.roughness));

    packed.packed1.x = packHalf2x16(vec2(unpacked.shadingNormal.x, unpacked.shadingNormal.y));
    packed.packed1.y = packHalf2x16(vec2(unpacked.shadingNormal.z, unpacked.opacity));
    packed.packed1.z = packHalf2x16(vec2(unpacked.hitKind, 0.0));

    return packed;
}

/**
 * Unpack the compressed payload into the full-sized payload format.
 * Complement of PackPayload().
 */
Payload UnpackPayload(PackedPayload packed) {
    Payload unpacked = Payload(vec3(0.f), 0.f, vec3(0.f), 0.f, vec3(0.f), 0.f, vec3(0.f), 0.f, 0u);
    unpacked.hitT = packed.hitT;
    unpacked.worldPosition = packed.worldPosition;

    unpacked.albedo.r = unpackHalf2x16(packed.packed0.x).x;
    unpacked.albedo.g = unpackHalf2x16(packed.packed0.x).y;
    unpacked.albedo.b = unpackHalf2x16(packed.packed0.y).x;
    unpacked.normal.x = unpackHalf2x16(packed.packed0.y).y;
    unpacked.normal.y = unpackHalf2x16(packed.packed0.z).x;
    unpacked.normal.z = unpackHalf2x16(packed.packed0.z).y;
    unpacked.metallic = unpackHalf2x16(packed.packed0.w).x;
    unpacked.roughness = unpackHalf2x16(packed.packed0.w).y;

    unpacked.shadingNormal.x = unpackHalf2x16(packed.packed1.x).x;
    unpacked.shadingNormal.y = unpackHalf2x16(packed.packed1.x).y;
    unpacked.shadingNormal.z = unpackHalf2x16(packed.packed1.y).x;
    unpacked.opacity = unpackHalf2x16(packed.packed1.y).y;
    unpacked.hitKind = uint(unpackHalf2x16(packed.packed1.z).x);

    return unpacked;
}

/**
 * Load a triangle's indices.
 */
uvec3 LoadIndices(uint meshIndex, uint primitiveIndex, GeometryData geometry) {
    uint address = geometry.indexByteAddress + (primitiveIndex * 3) * 4; // 3 indices per primitive, 4 bytes for each index
    return ReadUInt3(GetIndexBufferGlobalIndex(meshIndex), address);
}

// Function to load vertices
void LoadVertices(uint meshIndex, uint primitiveIndex, GeometryData geometry, out Vertex vertices[3]) {
    // Get the indices
    uvec3 indices = LoadIndices(meshIndex, primitiveIndex, geometry);

    // Load the vertices
    for (int i = 0; i < 3; i++) {
        vertices[i] = Vertex(vec3(0.f), vec3(0.f), vec4(0.f), vec2(0.f));
        uint address = geometry.vertexByteAddress + indices[i] * 48; // Vertices contain 12 floats / 48 bytes
        
        // Load the position
        vertices[i].position = uintBitsToFloat(ReadUInt3(GetVertexBufferGlobalIndex(meshIndex), address));
        address += 12;

        // Load the normal
        vertices[i].normal = uintBitsToFloat(ReadUInt3(GetVertexBufferGlobalIndex(meshIndex), address));
        address += 12;

        // Load the tangent
        vertices[i].tangent = uintBitsToFloat(ReadUInt4(GetVertexBufferGlobalIndex(meshIndex), address));
        address += 16;

        // Load the texture coordinates
        vertices[i].uv0 = uintBitsToFloat(ReadUInt2(GetVertexBufferGlobalIndex(meshIndex), address));
    }
}

/**
 * Load a triangle's vertex data (only position and uv0).
 */
void LoadVerticesPosUV0(uint meshIndex, uint primitiveIndex, GeometryData geometry, out Vertex vertices[3])
{
    // Get the indices
    uvec3 indices = LoadIndices(meshIndex, primitiveIndex, geometry);

    // Load the vertices
    uint address;
    for (uint i = 0; i < 3; i++) {
        vertices[i] = Vertex(vec3(0.f), vec3(0.f), vec4(0.f), vec2(0.f));
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // Vertices contain 12 floats / 48 bytes

        // Load the position
        vertices[i].position = uintBitsToFloat(ReadUInt3(GetVertexBufferGlobalIndex(meshIndex), address));
        address += 40; // skip normal and tangent

        // Load the texture coordinates
        vertices[i].uv0 = uintBitsToFloat(ReadUInt2(GetVertexBufferGlobalIndex(meshIndex), address));
    }
}

/**
 * Load (only) a triangle's texture coordinates and return the barycentric interpolated texture coordinates.
 */
vec2 LoadAndInterpolateUV0(uint meshIndex, uint primitiveIndex, GeometryData geometry, vec3 barycentrics)
{
    // Get the triangle indices
    uvec3 indices = LoadIndices(meshIndex, primitiveIndex, geometry);

    // Interpolate the texture coordinates
    uint address;
    vec2 uv0 = vec2(0.f, 0.f);
    for (uint i = 0; i < 3; i++)
    {
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // 12 floats (3: pos, 3: normals, 4:tangent, 2:uv0)
        address += 40;                                                // 40 bytes (10 * 4): skip position, normal, and tangent
        uv0 += uintBitsToFloat(ReadUInt2(GetVertexBufferGlobalIndex(meshIndex), address)) * barycentrics[i];
    }

    return uv0;
}

/**
 * Return interpolated vertex attributes (all).
 */
Vertex InterpolateVertex(Vertex vertices[3], vec3 barycentrics)
{
    // Interpolate the vertex attributes
    Vertex v = Vertex(vec3(0.f), vec3(0.f), vec4(0.f), vec2(0.f));
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
    Vertex v = Vertex(vec3(0.f), vec3(0.f), vec4(0.f), vec2(0.f));
    for (uint i = 0; i < 3; i++)
    {
        v.uv0 += vertices[i].uv0 * barycentrics[i];
    }

    return v;
}

// --- Ray Differentials ---
// for only AHS and CHS
#if defined(AHS) || defined(CHS)

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

    uvec3 launchid = gl_LaunchIDEXT;
    float hitt = gl_HitTEXT;
    mat3x4 transform = gl_ObjectToWorld3x4EXT;
    // Apply instance transforms
    // is 3x4(gl_ObjectToWorld3x4EXT) or 4x3(gl_ObjectToWorldEXT) ?
    vertices[0].position = (gl_ObjectToWorldEXT * vec4(vertices[0].position, 1.f)).xyz;
    vertices[1].position = (gl_ObjectToWorldEXT * vec4(vertices[1].position, 1.f)).xyz;
    vertices[2].position = (gl_ObjectToWorldEXT * vec4(vertices[2].position, 1.f)).xyz;

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
void ComputeUV0Differentials(Vertex vertices[3], Camera camera, vec3 rayDirection, float hitT, out vec2 dUVdx, out vec2 dUVdy)
{
    // Initialize a ray differential
    RayDiff rd = RayDiff(vec3(0.f), vec3(0.f), vec3(0.f), vec3(0.f));

    // Get ray direction differentials
    //ComputeRayDirectionDifferentials(rayDirection, camera.right, camera.up, camera.resolution, rd.dDdx, rd.dDdy);
    ComputeRayDirectionDifferentials(rayDirection, camera.right, camera.up, camera.resolution, rd.dDdx, rd.dDdy);

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

#endif // AHS || CHS

#endif // RAYTRACING_GLSL
