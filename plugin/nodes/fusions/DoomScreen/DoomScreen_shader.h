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
    float amplitude; \n\
    float noise; \n\
    float frequency; \n\
    float dripScale; \n\
    int bars; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float rand(int num) \n\
{ \n\
    return fract(mod(float(num) * 67123.313, 12.0) * sin(float(num) * 10.3) * cos(float(num))); \n\
} \n\
\n\
float wave(int num) \n\
{ \n\
    float fn = float(num) * p.frequency * 0.1 * float(p.bars); \n\
    return cos(fn * 0.5) * cos(fn * 0.13) * sin((fn+10.0) * 0.3) / 2.0 + 0.5; \n\
} \n\
\n\
float drip(int num) \n\
{ \n\
    return sin(float(num) / float(p.bars - 1) * 3.141592) * p.dripScale; \n\
} \n\
\n\
float pos(int num) \n\
{ \n\
    return (p.noise == 0.0 ? wave(num) : mix(wave(num), rand(num), p.noise)) + (p.dripScale == 0.0 ? 0.0 : drip(num)); \n\
} \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    int bar = int(uv.x * (float(p.bars))); \n\
    float scale = 1.0 + pos(bar) * p.amplitude; \n\
    float phase = p.progress * scale; \n\
    float posY = uv.y / vec2(1.0).y; \n\
    vec2 point; \n\
    sfpvec4 c; \n\
    if (phase + posY < 1.0) \n\
    { \n\
        point = clamp(vec2(uv.x, uv.y + mix(0.0, vec2(1.0).y, phase)) / vec2(1.0).xy, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
        c = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    } \n\
    else \n\
    { \n\
        point = uv.xy / vec2(1.0).xy; \n\
        c = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    } \n\
\n\
    // Finally, apply the color \n\
    return c; \n\
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

static const char DoomScreen_data[] = 
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
