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
    float scale; \n\
    float smoothness; \n\
    float seed; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float random(vec2 co) \n\
{ \n\
    float a = p.seed; \n\
    float b = 78.233; \n\
    float c = 43758.5453; \n\
    float dt= dot(co.xy ,vec2(a,b)); \n\
    float sn= mod(dt,3.14); \n\
    return fract(sin(sn) * c); \n\
} \n\
\n\
float noise(in vec2 st) \n\
{ \n\
    vec2 i = floor(st); \n\
    vec2 f = fract(st); \n\
\n\
    // Four corners in 2D of a tile \n\
    float a = random(i); \n\
    float b = random(i + vec2(1.0, 0.0)); \n\
    float c = random(i + vec2(0.0, 1.0)); \n\
    float d = random(i + vec2(1.0, 1.0)); \n\
\n\
    // Smooth Interpolation \n\
\n\
    // Cubic Hermine Curve.  Same as SmoothStep() \n\
    vec2 u = f*f*(3.0-2.0*f); \n\
    // u = smoothstep(0.,1.,f); \n\
\n\
    // Mix 4 coorners porcentages \n\
    return mix(a, b, u.x) + \n\
            (c - a)* u.y * (1.0 - u.x) + \n\
            (d - b) * u.x * u.y; \n\
} \n\
\n\
sfpvec4 transition (vec2 uv) \n\
{ \n\
    sfpvec4 from = load_rgba(int(uv.x * (p.w - 1)), int((1.f - uv.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 to = load_rgba_src2(int(uv.x * (p.w2 - 1)), int((1.f - uv.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    float n = noise(uv * p.scale); \n\
\n\
    float point = mix(-p.smoothness, 1.0 + p.smoothness, p.progress); \n\
    float lower = point - p.smoothness; \n\
    float higher = point + p.smoothness; \n\
\n\
    float q = smoothstep(lower, higher, n); \n\
\n\
    return mix( \n\
        from, \n\
        to, \n\
        sfp(1.0 - q) \n\
    ); \n\
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

static const char Perlin_data[] = 
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
