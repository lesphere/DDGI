#version 460 core

#include "samples/test-harness/shaders/include/Descriptors_nohandle.glsl"
#include "samples/test-harness/shaders/include/RayTracing_nohandle.glsl"

layout(location = 0) rayPayloadInEXT PackedPayload packedPayload;

// ---[ Miss Shader ]---

void main() {
    packedPayload.hitT = -1.f;
}
