#pragma once
#include <imvk_mat_shader.h>

#define SHADER_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    \n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    \n\
    float progress; \n\
    int count; \n\
    float saturation; \n\
    float light; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const float PI = 3.1415926; \n\
const float EPSILON = 1e-10; \n\
float rand(float n) \n\
{ \n\
    return fract(sin(n) * 43758.5453123); \n\
} \n\
vec3 hsv2rgb(vec3 c) \n\
{ \n\
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0); \n\
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www); \n\
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y); \n\
} \n\
vec3 rgb2hsv(vec3 c) \n\
{ \n\
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0); \n\
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g)); \n\
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r)); \n\
    \n\
    float d = q.x - min(q.w, q.y); \n\
    float e = 1.0e-10; \n\
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x); \n\
} \n\
vec3 RGBtoHCV(vec3 rgb) \n\
{ \n\
    // RGB [0..1] to Hue-Chroma-Value [0..1] \n\
    // Based on work by Sam Hocevar and Emil Persson \n\
    vec4 p = (rgb.g < rgb.b) ? vec4(rgb.bg, -1., 2. / 3.) : vec4(rgb.gb, 0., -1. / 3.); \n\
    vec4 q = (rgb.r < p.x) ? vec4(p.xyw, rgb.r) : vec4(rgb.r, p.yzx); \n\
    float c = q.x - min(q.w, q.y); \n\
    float h = abs((q.w - q.y) / (6. * c + EPSILON) + q.z); \n\
    return vec3(h, c, q.x); \n\
} \n\
vec3 HUEtoRGB(float hue) \n\
{ \n\
    // Hue [0..1] to RGB [0..1] \n\
    // See http://www.chilliant.com/rgb2hsv.html \n\
    vec3 rgb = abs(hue * 6. - vec3(3, 2, 4)) * vec3(1, -1, -1) + vec3(-1, 2, 2); \n\
    return clamp(rgb, 0., 1.); \n\
} \n\
vec3 HSVtoRGB(vec3 hsv) \n\
{ \n\
    // Hue-Saturation-Value [0..1] to RGB [0..1] \n\
    vec3 rgb = HUEtoRGB(hsv.x); \n\
    return ((rgb - 1.) * hsv.y + 1.) * hsv.z; \n\
} \n\
vec3 RGBtoHSV(vec3 rgb) \n\
{ \n\
    // RGB [0..1] to Hue-Saturation-Value [0..1] \n\
    vec3 hcv = RGBtoHCV(rgb); \n\
    float s = hcv.y / (hcv.z + EPSILON); \n\
    return vec3(hcv.x, s, hcv.z); \n\
} \n\
vec3 HSLtoRGB(vec3 hsl) \n\
{ \n\
    // Hue-Saturation-Lightness [0..1] to RGB [0..1] \n\
    vec3 rgb = HUEtoRGB(hsl.x); \n\
    float c = (1. - abs(2. * hsl.z - 1.)) * hsl.y; \n\
    return (rgb - 0.5) * c + hsl.z; \n\
} \n\
vec3 RGBtoHSL(vec3 rgb) \n\
{ \n\
    // RGB [0..1] to Hue-Saturation-Lightness [0..1] \n\
    vec3 hcv = RGBtoHCV(rgb); \n\
    float z = hcv.z - hcv.y * 0.5; \n\
    float s = hcv.y / (1. - abs(z * 2. - 1.) + EPSILON); \n\
    return vec3(hcv.x, s, z); \n\
} \n\
vec3 rgb2hsl(vec3 color) \n\
{ \n\
    vec3 hsl; // init to 0 to avoid warnings ? (and reverse if + remove first part) \n\
    \n\
    float fmin = min(min(color.r, color.g), color.b); //Min. value of RGB \n\
    float fmax = max(max(color.r, color.g), color.b); //Max. value of RGB \n\
    float delta = fmax - fmin; //Delta RGB value \n\
    \n\
    hsl.z = (fmax + fmin) / 2.0; // Luminance \n\
    \n\
    if (delta == 0.0) //This is a gray, no chroma... \n\
    { \n\
        hsl.x = 0.0; // Hue \n\
        hsl.y = 0.0; // Saturation \n\
    } \n\
    else //Chromatic data... \n\
    { \n\
        if (hsl.z < 0.5) \n\
            hsl.y = delta / (fmax + fmin); // Saturation \n\
        else \n\
            hsl.y = delta / (2.0 - fmax - fmin); // Saturation \n\
        \n\
        float deltaR = (((fmax - color.r) / 6.0) + (delta / 2.0)) / delta; \n\
        float deltaG = (((fmax - color.g) / 6.0) + (delta / 2.0)) / delta; \n\
        float deltaB = (((fmax - color.b) / 6.0) + (delta / 2.0)) / delta; \n\
        \n\
        if (color.r == fmax) \n\
            hsl.x = deltaB - deltaG; // Hue \n\
        else if (color.g == fmax) \n\
            hsl.x = (1.0 / 3.0) + deltaR - deltaB; // Hue \n\
        else if (color.b == fmax) \n\
            hsl.x = (2.0 / 3.0) + deltaG - deltaR; // Hue \n\
        \n\
        if (hsl.x < 0.0) \n\
            hsl.x += 1.0; // Hue \n\
        else if (hsl.x > 1.0) \n\
            hsl.x -= 1.0; // Hue \n\
    } \n\
    return hsl; \n\
} \n\
\n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 tuv = vec2(float(gl_GlobalInvocationID.x) / float(p.out_w - 1), float(gl_GlobalInvocationID.y) / float(p.out_h - 1)); \n\
    float duration = 1.f / float(p.count); \n\
    float progress = mod(p.progress, duration) / duration; // 0~1 \n\
    float amplitude = abs(sin(progress * (PI / duration))); \n\
    float hue = amplitude * 360.0 / 360.0; \n\
    float value = p.saturation / 10.0; \n\
    \n\
    vec3 vHSV = vec3(hue, p.saturation, value); \n\
    sfpvec4 rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    vec3 fragRGB = vec3(float(rgba.r), float(rgba.g), float(rgba.b)); \n\
    vec3 fragHSV = rgb2hsv(fragRGB); \n\
    \n\
    fragHSV.x += vHSV.x; \n\
    fragHSV.z += vHSV.z; \n\
    fragHSV.z = max(min(fragHSV.z, 1.0), 0.0); \n\
    vec3 fragMidRGB = hsv2rgb(fragHSV); \n\
    \n\
    vec3 fragHSL = RGBtoHSL(fragMidRGB); \n\
    fragHSL.y += vHSV.y * 0.5; \n\
    \n\
    fragHSL.y = max(min(fragHSL.y, 1.0), 0.0); \n\
    vec3 fragRetRGB = HSLtoRGB(fragHSL); \n\
    vec4 whiteMask = vec4(1.0, 1.0, 1.0, 1.0); \n\
    vec4 result = vec4(fragRetRGB, float(rgba.a)) * (1.0 - p.light) + whiteMask * p.light; \n\
    store_rgba(sfpvec4(result.r, result.g, result.b, rgba.a), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \n\
"

static const char Effect_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
