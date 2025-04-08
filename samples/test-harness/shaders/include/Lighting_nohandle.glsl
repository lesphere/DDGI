#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "samples/test-harness/shaders/include/Common.glsl"
#include "samples/test-harness/shaders/include/Descriptors_nohandle.glsl"

// Setup the ray payload in shader entry point file before including this file

float SpotAttenuation(vec3 spotDirection, vec3 lightDirection, float umbra, float penumbra) {
    // Spot attenuation function from Frostbite, pg 115 in RTR4
    float cosTheta = clamp(dot(spotDirection, lightDirection), 0.0, 1.0);
    float t = clamp((cosTheta - cos(umbra)) / (cos(penumbra) - cos(umbra)), 0.0, 1.0);
    return t * t;
}

float LightWindowing(float distanceToLight, float maxDistance) {
    return pow(clamp(1.f - pow((distanceToLight / maxDistance), 4), 0.0, 1.0), 2);
}

float LightFalloff(float distanceToLight) {
    return 1.f / pow(max(distanceToLight, 1.f), 2);
}

/**
 * Computes the visibility factor for a given vector to a light.
 */
float LightVisibility(Payload payload, vec3 lightVector, float tmax, float normalBias, float viewBias, accelerationStructureEXT bvh) {
    RayDesc ray;
    ray.origin = payload.worldPosition + (payload.normal * normalBias); // TODO: not using viewBias!
    ray.direction = normalize(lightVector);
    ray.tmin = 0.f;
    ray.tmax = tmax;

    // Trace a visibility ray
    // Skip the CHS to avoid evaluating materials
    packedPayload = PackedPayload(0.f, vec3(0.f), uvec4(0u), uvec3(0u));
    traceRayEXT(
        bvh,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
        0xFFu,
        0u,
        0u,
        0u,
        ray.origin,
        ray.tmin,
        ray.direction,
        ray.tmax,
        0);

    return float(packedPayload.hitT < 0.f);
}

/**
 * Evaluate direct lighting and showing for the current surface and the spot light.
 */
vec3 EvaluateSpotLight(Payload payload, float normalBias, float viewBias, accelerationStructureEXT bvh) {
    vec3 color = vec3(0.f);
    for (uint lightIndex = 0; lightIndex < GetNumSpotLights(); lightIndex++) {
        // Get the index of the light
        uint index = (HasDirectionalLight() + lightIndex);

        // Load the spot light
        Light spotLight = Lights[index];

        vec3 lightVector = (spotLight.position - payload.worldPosition);
        float lightDistance = length(lightVector);

        // Early out, light energy doesn't reach the surface
        if (lightDistance > spotLight.radius) return vec3(0.f, 0.f, 0.f);

        float tmax = (lightDistance - viewBias);
        float visibility = LightVisibility(payload, lightVector, tmax, normalBias, viewBias, bvh);

        // Early out, this light isn't visible from the surface
        if (visibility <= 0.f) continue;

        // Compute lighting
        vec3 lightDirection = normalize(lightVector);
        float nol = max(dot(payload.normal, lightDirection), 0.f);
        vec3 spotDirection = normalize(spotLight.direction);
        float attenuation = SpotAttenuation(spotDirection, -lightDirection, spotLight.umbraAngle, spotLight.penumbraAngle);
        float falloff = LightFalloff(lightDistance);
        float window = LightWindowing(lightDistance, spotLight.radius);

        color += spotLight.power * spotLight.color * nol * attenuation * falloff * window * visibility;
    }
    return color;
}

/**
 * Evaluate direct lighting for the current surface and all influential point lights.
 */
vec3 EvaluatePointLight(Payload payload, float normalBias, float viewBias, accelerationStructureEXT bvh) {
    vec3 color = vec3(0.f);
    for (uint lightIndex = 0; lightIndex < GetNumPointLights(); lightIndex++) {
        // Get the index of the point light
        uint index = HasDirectionalLight() + GetNumSpotLights();

        // Load the point light
        Light pointLight = Lights[index];

        vec3 lightVector = (pointLight.position - payload.worldPosition);
        float lightDistance = length(lightVector);

        // Early out, light energy doesn't reach the surface
        if (lightDistance > pointLight.radius) return vec3(0.f, 0.f, 0.f);

        float tmax = (lightDistance - viewBias);
        float visibility = LightVisibility(payload, lightVector, tmax, normalBias, viewBias, bvh);

        // Early out, this light isn't visible from the surface
        if (visibility <= 0.f) return vec3(0.f, 0.f, 0.f);

        // Compute lighting
        vec3 lightDirection = normalize(lightVector);
        float nol = max(dot(payload.normal, lightDirection), 0.f);
        float falloff = LightFalloff(lightDistance);
        float window = LightWindowing(lightDistance, pointLight.radius);

        color += pointLight.power * pointLight.color * nol * falloff * window * visibility;
    }
    return color;
}

/**
 * Evaluate direct lighting for the current surface and the directional light.
 */
vec3 EvaluateDirectionalLight(Payload payload, float normalBias, float viewBias, accelerationStructureEXT bvh) {
    // Load the directional light data (directional light is always the first light)
    Light directionalLight = Lights[0];

    float visibility = LightVisibility(payload, -directionalLight.direction, 1e27f, normalBias, viewBias, bvh);

    // Early out, the light isn't visible from the surface
    if (visibility <= 0.f) return vec3(0.f, 0.f, 0.f);

    // Compute lighting
    vec3 lightDirection = -normalize(directionalLight.direction);
    float nol = max(dot(payload.shadingNormal, lightDirection), 0.f);

    return directionalLight.power * directionalLight.color * nol * visibility;
}

/**
 * Computes the diffuse reflection of light off the given surface (direct lighting).
 */
vec3 DirectDiffuseLighting(Payload payload, float normalBias, float viewBias, accelerationStructureEXT bvh) {
    vec3 brdf = (payload.albedo / PI);
    vec3 lighting = vec3(0.f);

    if (bool(HasDirectionalLight())) {
        lighting += EvaluateDirectionalLight(payload, normalBias, viewBias, bvh);
    }

    if (GetNumSpotLights() > 0) {
        lighting += EvaluateSpotLight(payload, normalBias, viewBias, bvh);
    }

    if (GetNumPointLights() > 0) {
        lighting += EvaluatePointLight(payload, normalBias, viewBias, bvh);
    }

    return (brdf * lighting);
}

#endif // LIGHTING_GLSL
