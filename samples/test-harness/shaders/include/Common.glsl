#ifndef COMMON_GLSL
#define COMMON_GLSL

const float PI = 3.1415926535897932f;
const float TWO_PI = 6.2831853071795864f;

const float COMPOSITE_FLAG_IGNORE_PIXEL = 0.2f;
const float COMPOSITE_FLAG_POSTPROCESS_PIXEL = 0.5f;
const float COMPOSITE_FLAG_LIGHT_PIXEL = 0.8f;

#define RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE 0
#define RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE 1

vec3 LessThan(vec3 f, float value)
{
    return vec3(
        (f.x < value) ? 1.f : 0.f,
        (f.y < value) ? 1.f : 0.f,
        (f.z < value) ? 1.f : 0.f);
}

vec3 LinearToSRGB(vec3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return mix(
        pow(rgb * 1.055f, vec3(1.f / 2.4f)) - 0.055f,
        rgb * 12.92f,
        LessThan(rgb, 0.0031308f)
    );
}

vec3 SRGBToLinear(vec3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return mix(
        pow((rgb + 0.055f) / 1.055f, vec3(2.4f)),
        rgb / 12.92f,
        LessThan(rgb, 0.04045f)
    );
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}

#endif // COMMON_GLSL
