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
    int in_out; \n\
    int type; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
#define rad2 rad / 2.0 \n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 uv0 = uv; \n\
    float rad = p.in_out == 1 ? p.progress : 1.0 - p.progress; \n\
    float xc1, yc1, cx, cy; \n\
    // I used if/else instead of switch in case it's an old GPU \n\
    if (p.type == 0) { cx = .5; cy = 0.0; xc1 = .5 - rad2; yc1 = 0.0; } \n\
    else if (p.type == 1) { cx = 1.0; cy = .5; xc1 = 1.0 - rad; yc1 = .5 - rad2; } \n\
    else if (p.type == 2) { cx = .5; cy = 1.0; xc1 = .5 - rad2; yc1 = 1.0 - rad; } \n\
    else if (p.type == 3) { cx = 0.0; cy = .5; xc1 = 0.0; yc1 = .5 - rad2; } \n\
    else if (p.type == 4) { cx = 1.0; cy = 0.0; xc1 = 1.0 - rad; yc1 = 0.0; } \n\
    else if (p.type == 5) { cx = cy = 1.0; xc1 = 1.0 - rad; yc1 = 1.0 - rad; } \n\
    else if (p.type == 6) { cx = 0.0; cy = 1.0; xc1 = 0.0; yc1 = 1.0 - rad; } \n\
    else if (p.type == 7) { cx = cy = 0.0; xc1 = 0.0; yc1 = 0.0; } \n\
    else if (p.type == 8) { cx = cy = .5; xc1 = .5 - rad2; yc1 = .5 - rad2; } \n\
    uv.y = 1.0 - uv.y; \n\
    vec2 uv2; \n\
    if ((uv.x >= xc1) && (uv.x < xc1 + rad) && (uv.y >= yc1) && (uv.y < yc1 + rad)) \n\
    { \n\
        uv2 = vec2((uv.x - xc1) / rad, 1.0 - (uv.y - yc1) / rad); \n\
        return(p.in_out == 1 ?  load_rgba_src2(int(uv2.x * (p.w2 - 1)), int((1.f - uv2.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2) : \n\
                                load_rgba(int(uv2.x * (p.w - 1)), int((1.f - uv2.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type)); \n\
    } \n\
    return(p.in_out == 1 ?  load_rgba(int(uv0.x * (p.w - 1)), int((1.f - uv0.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type) : \n\
                            load_rgba_src2(int(uv0.x * (p.w2 - 1)), int((1.f - uv0.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2)); \n\
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

static const char Slider_data[] = 
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
