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
} p; \
"

#define SHADER_MAIN \
" \n\
vec2 skewRight(vec2 point) \n\
{ \n\
    float skewX = (point.x - p.progress) / (0.5 - p.progress) * 0.5; \n\
    float skewY =  (point.y - 0.5) / (0.5 + p.progress * (point.x - 0.5) / 0.5) * 0.5  + 0.5; \n\
    return vec2(skewX, skewY); \n\
} \n\
\n\
vec2 skewLeft(vec2 point) \n\
{ \n\
    float skewX = (point.x - 0.5)/(p.progress - 0.5) * 0.5 + 0.5; \n\
    float skewY = (point.y - 0.5) / (0.5 + (1.0 - p.progress ) * (0.5 - point.x) / 0.5) * 0.5  + 0.5; \n\
    return vec2(skewX, skewY); \n\
} \n\
\n\
sfpvec4 addShade() \n\
{ \n\
    sfp shadeVal = max(sfp(0.7f), abs(sfp(p.progress) - sfp(0.5f)) * sfp(2.f)); \n\
    return sfpvec4(sfpvec3(shadeVal), sfp(1.f)); \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.out_w - 1), float(uv.y) / float(p.out_h - 1)); \n\
    float pr = step(1.0 - p.progress, point.x); \n\
    sfpvec4 result = sfpvec4(0.f); \n\
    if (point.x < 0.5) \n\
    { \n\
        sfpvec4 rgba_src1 = load_rgba(int(point.x * (p.w - 1)), int(point.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        vec2 left_point = clamp(skewLeft(point), vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
        sfpvec4 rgba_src2 = load_rgba_src2(int(left_point.x * (p.w2 - 1)), int(left_point.y * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        result = mix(rgba_src1, rgba_src2 * addShade(), sfp(pr)); \n\
    } \n\
    else \n\
    { \n\
        vec2 right_point = clamp(skewRight(point), vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
        sfpvec4 rgba_src1 = load_rgba(int(right_point.x * (p.w - 1)), int(right_point.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfpvec4 rgba_src2 = load_rgba_src2(int(point.x * (p.w2 - 1)), int(point.y * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        result = mix(rgba_src1 * addShade(), rgba_src2, sfp(pr)); \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char BookFlip_data[] = 
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
