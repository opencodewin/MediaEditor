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
    float reflection; \n\
    float perspective; \n\
    float depth; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const sfpvec4 black = sfpvec4(0.0, 0.0, 0.0, 1.0); \n\
const vec2 boundMin = vec2(0.0, 0.0); \n\
const vec2 boundMax = vec2(1.0, 1.0); \n\
\n\
bool inBounds(vec2 p) \n\
{ \n\
    return all(lessThan(boundMin, p)) && all(lessThan(p, boundMax)); \n\
} \n\
\n\
vec2 project(vec2 p) \n\
{ \n\
    return p * vec2(1.0, -1.2) + vec2(0.0, -0.02); \n\
} \n\
\n\
sfpvec4 bgColor(vec2 pfr, vec2 pto) \n\
{ \n\
    sfpvec4 c = black; \n\
    pfr = project(pfr); \n\
    if (inBounds(pfr)) \n\
    { \n\
        sfpvec4 rgba_from = load_rgba(int(pfr.x * (p.w - 1)), int((1.0 - pfr.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        c += mix(black, rgba_from, sfp(p.reflection * mix(1.0, 0.0, pfr.y))); \n\
    } \n\
    pto = project(pto); \n\
    if (inBounds(pto)) \n\
    { \n\
        sfpvec4 rgba_to = load_rgba_src2(int(pto.x * (p.w2 - 1)), int((1.0 - pto.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        c += mix(black, rgba_to, sfp(p.reflection * mix(1.0, 0.0, pto.y))); \n\
    } \n\
    return c; \n\
} \n\
\n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    vec2 pfr, pto = vec2(-1.); \n\
    float size = mix(1.0, p.depth, p.progress); \n\
    float persp = p.perspective * p.progress; \n\
    pfr = (point + vec2(-0.0, -0.5)) * vec2(size / (1.0 - p.perspective * p.progress), size / (1.0 - size * persp * point.x)) + vec2(0.0, 0.5); \n\
    size = mix(1.0, p.depth, 1. - p.progress); \n\
    persp = p.perspective * (1. - p.progress); \n\
    pto = (point + vec2(-1.0, -0.5)) * vec2(size / (1.0 - p.perspective * (1.0 - p.progress)), size / (1.0 - size * persp * (0.5 - point.x))) + vec2(1.0, 0.5); \n\
    if (p.progress < 0.5) \n\
    { \n\
        if (inBounds(pfr)) \n\
        { \n\
            sfpvec4 rgba_from = load_rgba(int(pfr.x * (p.w - 1)), int((1.0f - pfr.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            return rgba_from; \n\
        } \n\
        if (inBounds(pto)) \n\
        { \n\
            sfpvec4 rgba_to = load_rgba_src2(int(pto.x * (p.w2 - 1)), int((1.0f - pto.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
            return rgba_to; \n\
        } \n\
    } \n\
    if (inBounds(pto)) \n\
    { \n\
        sfpvec4 rgba_to = load_rgba_src2(int(pto.x * (p.w2 - 1)), int((1.0f - pto.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        return rgba_to; \n\
    } \n\
    if (inBounds(pfr)) \n\
    { \n\
        sfpvec4 rgba_from = load_rgba(int(pfr.x * (p.w - 1)), int((1.0f - pfr.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        return rgba_from; \n\
    } \n\
    return bgColor(pfr, pto); \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.out_w - 1), 1.0f - float(uv.y) / float(p.out_h - 1)); \n\
    sfpvec4 result = transition(point); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Swap_data[] = 
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
