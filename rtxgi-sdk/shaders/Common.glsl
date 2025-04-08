#ifndef RTXGI_COMMON_GLSL
#define RTXGI_COMMON_GLSL

const float RTXGI_PI = 3.1415926535897932f;
const float RTXGI_2PI = 6.2831853071795864f;

//------------------------------------------------------------------------
// Math Helpers
//------------------------------------------------------------------------

/**
 * Returns the largest component of the vector.
 */
float RTXGIMaxComponent(vec3 a)
{
    return max(a.x, max(a.y, a.z));
}

/**
 * Returns either -1 or 1 based on the sign of the input value.
 * If the input is zero, 1 is returned.
 */
float RTXGISignNotZero(float v)
{
    return (v >= 0.f) ? 1.f : -1.f;
}

/**
 * 2-component version of RTXGISignNotZero.
 */
vec2 RTXGISignNotZero(vec2 v)
{
    return vec2(RTXGISignNotZero(v.x), RTXGISignNotZero(v.y));
}

//------------------------------------------------------------------------
// Sampling Helpers
//------------------------------------------------------------------------

/**
 * Computes a low discrepancy spherically distributed direction on the unit sphere,
 * for the given index in a set of samples. Each direction is unique in
 * the set, but the set of directions is always the same.
 */
vec3 RTXGISphericalFibonacci(float sampleIndex, float numSamples)
{
    const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    float phi = RTXGI_2PI * fract(sampleIndex * b);
    float cosTheta = 1.f - (2.f * sampleIndex + 1.f) * (1.f / numSamples);
    float sinTheta = sqrt(clamp(1.f - (cosTheta * cosTheta), 0.0, 1.0));

    return vec3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta);
}

//------------------------------------------------------------------------
// Format Conversion Helpers
//------------------------------------------------------------------------

/**
 * Return the given float value as an unsigned integer within the given numerical scale.
 */
uint RTXGIFloatToUint(float v, float scale)
{
    return uint(floor(v * scale + 0.5f));
}

/**
 * Pack a float3 into a 32-bit unsigned integer.
 * All channels use 10 bits and 2 bits are unused.
 * Compliment of RTXGIUintToFloat3().
 */
uint RTXGIFloat3ToUint(vec3 input_vec)
{
    return (RTXGIFloatToUint(input_vec.r, 1023.f)) | (RTXGIFloatToUint(input_vec.g, 1023.f) << 10) | (RTXGIFloatToUint(input_vec.b, 1023.f) << 20);
}

/**
 * Unpack a packed 32-bit unsigned integer to a float3.
 * Compliment of RTXGIFloat3ToUint().
 */
vec3 RTXGIUintToFloat3(uint input_int)
{
    vec3 output_vec;
    output_vec.x = float(input_int & 0x000003FF) / 1023.f;
    output_vec.y = float((input_int >> 10) & 0x000003FF) / 1023.f;
    output_vec.z = float((input_int >> 20) & 0x000003FF) / 1023.f;
    return output_vec;
}

//------------------------------------------------------------------------
// Quaternion Helpers
//------------------------------------------------------------------------

/**
 * Rotate vector v with quaternion q.
 */
vec3 RTXGIQuaternionRotate(vec3 v, vec4 q)
{
    vec3 b = q.xyz;
    float b2 = dot(b, b);
    return (v * (q.w * q.w - b2) + b * (dot(v, b) * 2.f) + cross(b, v) * (q.w * 2.f));
}

/**
 * Quaternion conjugate.
 * For unit quaternions, conjugate equals inverse.
 * Use this to create a quaternion that rotates in the opposite direction.
 */
vec4 RTXGIQuaternionConjugate(vec4 q)
{
    return vec4(-q.xyz, q.w);
}

//------------------------------------------------------------------------
// Luminance Helper
//------------------------------------------------------------------------

/**
 * Convert Linear RGB value to Luminance
 */
float RTXGILinearRGBToLuminance(vec3 rgb)
{
    const vec3 LuminanceWeights = vec3(0.2126, 0.7152, 0.0722);
    return dot(rgb, LuminanceWeights);
}

#endif // RTXGI_COMMON_GLSL
