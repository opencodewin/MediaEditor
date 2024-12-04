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
    float persp; \n\
    float unzoom; \n\
    float reflection; \n\
    float floating; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
vec2 project(vec2 point) \n\
{ \n\
    return point * vec2(1.0, -1.2) + vec2(0.0, -p.floating / 100.); \n\
} \n\
bool inBounds(vec2 point) \n\
{ \n\
    return all(lessThan(vec2(0.0), point)) && all(lessThan(point, vec2(1.0))); \n\
} \n\
sfpvec4 bgColor(vec2 pfr, vec2 pto) \n\
{ \n\
    sfpvec4 c = sfpvec4(sfp(0.0), sfp(0.0), sfp(0.0), sfp(1.0)); \n\
    pfr = project(pfr); \n\
    // FIXME avoid branching might help perf! \n\
    if (inBounds(pfr)) \n\
    { \n\
        sfpvec4 rgba_from = load_rgba(int(pfr.x * (p.w - 1)), int((1.f - pfr.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        c += mix(sfpvec4(0.0), rgba_from, sfp(p.reflection * mix(1.0, 0.0, pfr.y))); \n\
    } \n\
    pto = project(pto); \n\
    if (inBounds(pto)) \n\
    { \n\
        sfpvec4 rgba_to = load_rgba_src2(int(pto.x * (p.w2 - 1)), int((1.f - pto.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        c += mix(sfpvec4(0.0), rgba_to, sfp(p.reflection * mix(1.0, 0.0, pto.y))); \n\
    } \n\
    return c; \n\
} \n\
vec2 xskew(vec2 point, float persp, float center) \n\
{ \n\
    float x = mix(point.x, 1.0 - point.x, center); \n\
    return ( \n\
        ( \n\
            vec2(x, (point.y - 0.5 * (1.0 - persp) * x) / (1.0 + (persp - 1.0) * x)) \n\
            - vec2(0.5 - distance(center, 0.5), 0.0) \n\
        ) \n\
        * vec2(0.5 / distance(center, 0.5) * (center < 0.5 ? 1.0 : -1.0), 1.0) \n\
        + vec2(center < 0.5 ? 0.0 : 1.0, 0.0) \n\
    ); \n\
} \n\
sfpvec4 transition(vec2 op) \n\
{ \n\
    float uz = p.unzoom * 2.0 * (0.5-distance(0.5, p.progress)); \n\
    vec2 point = -uz * 0.5 + (1.0 + uz) * op; \n\
    vec2 fromP = xskew( \n\
        (point - vec2(p.progress, 0.0)) / vec2(1.0 - p.progress, 1.0), \n\
        1.0 - mix(p.progress, 0.0, p.persp), \n\
        0.0 \n\
    ); \n\
    vec2 toP = xskew( \n\
        point / vec2(p.progress, 1.0), \n\
        mix(pow(p.progress, 2.0), 1.0, p.persp), \n\
        1.0 \n\
    ); \n\
    // FIXME avoid branching might help perf! \n\
    if (inBounds(fromP)) \n\
    { \n\
        return load_rgba(int(fromP.x * (p.w - 1)), int((1.f - fromP.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    } \n\
    else if (inBounds(toP)) \n\
    { \n\
        return load_rgba_src2(int(toP.x * (p.w2 - 1)), int((1.f - toP.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    } \n\
    return bgColor(fromP, toP); \n\
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

static const char Cube_data[] = 
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
