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
    float size; \n\
    float zoom; \n\
    float colorSeparation; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    float inv = 1. - p.progress; \n\
    vec2 disp = p.size * vec2(cos(p.zoom * point.x), sin(p.zoom * point.y)); \n\
    vec2 to_point = clamp(point + inv * disp, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    sfpvec4 texTo = load_rgba_src2(int(to_point.x * (p.w2 - 1)), int((1.f - to_point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    vec2 from_point_p1 = clamp(point + p.progress * disp * (1.0 - p.colorSeparation), vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    vec2 from_point_p2 = clamp(point + p.progress * disp, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    vec2 from_point_p3 = clamp(point + p.progress * disp * (1.0 + p.colorSeparation), vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    sfpvec4 rgba_from_p1 = load_rgba(int(from_point_p1.x * (p.w - 1)), int((1.f - from_point_p1.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_from_p2 = load_rgba(int(from_point_p2.x * (p.w - 1)), int((1.f - from_point_p2.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_from_p3 = load_rgba(int(from_point_p3.x * (p.w - 1)), int((1.f - from_point_p3.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 texFrom = sfpvec4( \n\
        rgba_from_p1.r, \n\
        rgba_from_p2.g, \n\
        rgba_from_p3.b, \n\
        sfp(1.0)); \n\
    return texTo * sfp(p.progress) + texFrom * sfp(inv); \n\
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

static const char Flyeye_data[] = 
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
