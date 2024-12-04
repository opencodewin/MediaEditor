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
    int endx; \n\
    int endy; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
#define PI 3.14159265358979323 \n\
#define POW2(X) X*X \n\
#define POW3(X) X*X*X \n\
float Rand(vec2 v) \n\
{ \n\
    return fract(sin(dot(v.xy ,vec2(12.9898,78.233))) * 43758.5453); \n\
} \n\
\n\
vec2 Rotate(vec2 v, float a) \n\
{ \n\
    mat2 rm = mat2( cos(a), -sin(a), \n\
                    sin(a), cos(a)); \n\
    return rm * v; \n\
} \n\
\n\
float CosInterpolation(float x) \n\
{ \n\
    return -cos(x * PI) / 2. + .5; \n\
} \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 point = uv.xy / vec2(1.0).xy - .5; \n\
    vec2 rp = point; \n\
    float rpr = (p.progress * 2. - 1.); \n\
    float z = -(rpr * rpr * 2.) + 3.; \n\
    float az = abs(z); \n\
    rp *= az; \n\
    rp += mix(vec2(.5, .5), vec2(float(p.endx) + .5, float(p.endy) + .5), POW2(CosInterpolation(p.progress))); \n\
    vec2 mrp = mod(rp, 1.); \n\
    vec2 crp = rp; \n\
    bool onEnd = int(floor(crp.x)) == p.endx && int(floor(crp.y)) == p.endy; \n\
    if (!onEnd) \n\
    { \n\
        float ang = float(int(Rand(floor(crp)) * 4.)) * .5 * PI; \n\
        mrp = vec2(.5) + Rotate(mrp - vec2(.5), ang); \n\
    } \n\
    mrp = clamp(mrp, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    if (onEnd || Rand(floor(crp)) > .5) \n\
    { \n\
        return load_rgba_src2(int(mrp.x * (p.w2 - 1)), int((1.f - mrp.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    } \n\
    else \n\
    { \n\
        return load_rgba(int(mrp.x * (p.w - 1)), int((1.f - mrp.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    } \n\
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

static const char Mosaic_data[] = 
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
