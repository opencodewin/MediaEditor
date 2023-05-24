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
    int w2; \n\
    int h2; \n\
    int cstep2; \n\
    int in_format2; \n\
    int in_type2; \n\
\n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
\n\
    float progress; \n\
    float strength; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const float PI = 3.141592653589793; \n\
float Linear_ease(in float begin, in float change, in float duration, in float time) \n\
{ \n\
    return change * time / duration + begin; \n\
} \n\
\n\
float Exponential_easeInOut(in float begin, in float change, in float duration, in float time) \n\
{ \n\
    if (time == 0.0) \n\
        return begin; \n\
    else if (time == duration) \n\
        return begin + change; \n\
    time = time / (duration / 2.0); \n\
    if (time < 1.0) \n\
        return change / 2.0 * pow(2.0, 10.0 * (time - 1.0)) + begin; \n\
    return change / 2.0 * (-pow(2.0, -10.0 * (time - 1.0)) + 2.0) + begin; \n\
} \n\
\n\
float Sinusoidal_easeInOut(in float begin, in float change, in float duration, in float time) \n\
{ \n\
    return -change / 2.0 * (cos(PI * time / duration) - 1.0) + begin; \n\
} \n\
\n\
float rand(vec2 co) \n\
{ \n\
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); \n\
} \n\
\n\
sfpvec3 crossFade(in vec2 uv, in float dissolve) \n\
{ \n\
    uv = clamp(uv, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    sfpvec4 rgba_to = load_rgba_src2(int(uv.x * (p.w2 - 1)), int((1.f - uv.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_from = load_rgba(int(uv.x * (p.w - 1)), int((1.f - uv.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    return mix(rgba_from.rgb, rgba_to.rgb, sfp(dissolve)); \n\
} \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 texCoord = uv.xy / vec2(1.0).xy; \n\
\n\
    // Linear interpolate center across center half of the image \n\
    vec2 center = vec2(Linear_ease(0.25, 0.5, 1.0, p.progress), 0.5); \n\
    float dissolve = Exponential_easeInOut(0.0, 1.0, 1.0, p.progress); \n\
\n\
    // Mirrored sinusoidal loop. 0->strength then strength->0 \n\
    float strength = Sinusoidal_easeInOut(0.0, p.strength, 0.5, p.progress); \n\
\n\
    sfpvec3 color = sfpvec3(0.0); \n\
    float total = 0.0; \n\
    vec2 toCenter = center - texCoord; \n\
\n\
    /* randomize the lookup values to hide the fixed number of samples */ \n\
    float offset = rand(uv); \n\
\n\
    for (float t = 0.0; t <= 40.0; t++) \n\
    { \n\
        float percent = (t + offset) / 40.0; \n\
        float weight = 4.0 * (percent - percent * percent); \n\
        color += crossFade(texCoord + toCenter * percent * strength, dissolve) * sfp(weight); \n\
        total += weight; \n\
    } \n\
    return sfpvec4(color / sfp(total), sfp(1.0)); \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.out_w - 1), 1.f - float(uv.y) / float(p.out_h - 1)); \n\
    sfpvec4 result = transition(point); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char CrossZoom_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer src2_int8       { uint8_t   src2_data_int8[]; };
layout (binding =  9) readonly buffer src2_int16      { uint16_t  src2_data_int16[]; };
layout (binding = 10) readonly buffer src2_float16    { float16_t src2_data_float16[]; };
layout (binding = 11) readonly buffer src2_float32    { float     src2_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_RGBA_NAME(src2)
SHADER_STORE_RGBA
SHADER_MAIN
;
