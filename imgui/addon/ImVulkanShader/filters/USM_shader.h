#pragma once
#include <imvk_mat_shader.h>

#define USM_SHADER_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    \n\
    int blur_w; \n\
    int blur_h; \n\
    int blur_cstep; \n\
    int blur_format; \n\
    int blur_type; \n\
    \n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    \n\
    float amount; \n\
    float threshold; \n\
} p; \
"

#define SHADER_LOAD_BLUR_RGBA_INT8 \
" \n\
sfpvec4 load_blur_rgba_int8(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(blur_int8_data[i_offset.r]) / sfp(255.f); \n\
    rgb_in.g = sfp(blur_int8_data[i_offset.g]) / sfp(255.f); \n\
    rgb_in.b = sfp(blur_int8_data[i_offset.b]) / sfp(255.f); \n\
    rgb_in.a = sfp(blur_int8_data[i_offset.a]) / sfp(255.f); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_BLUR_RGBA_INT16 \
" \n\
sfpvec4 load_blur_rgba_int16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(blur_int16_data[i_offset.r]) / sfp(65535.f); \n\
    rgb_in.g = sfp(blur_int16_data[i_offset.g]) / sfp(65535.f); \n\
    rgb_in.b = sfp(blur_int16_data[i_offset.b]) / sfp(65535.f); \n\
    rgb_in.a = sfp(blur_int16_data[i_offset.a]) / sfp(65535.f); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_BLUR_RGBA_FLOAT16 \
" \n\
sfpvec4 load_blur_rgba_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(blur_float16_data[i_offset.r]); \n\
    rgb_in.g = sfp(blur_float16_data[i_offset.g]); \n\
    rgb_in.b = sfp(blur_float16_data[i_offset.b]); \n\
    rgb_in.a = sfp(blur_float16_data[i_offset.a]); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_BLUR_RGBA_FLOAT32 \
" \n\
sfpvec4 load_blur_rgba_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(blur_float32_data[i_offset.r]); \n\
    rgb_in.g = sfp(blur_float32_data[i_offset.g]); \n\
    rgb_in.b = sfp(blur_float32_data[i_offset.b]); \n\
    rgb_in.a = sfp(blur_float32_data[i_offset.a]); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_BLUR_RGBA \
SHADER_LOAD_BLUR_RGBA_INT8 \
SHADER_LOAD_BLUR_RGBA_INT16 \
SHADER_LOAD_BLUR_RGBA_FLOAT16 \
SHADER_LOAD_BLUR_RGBA_FLOAT32 \
" \n\
sfpvec4 load_blur_rgba(int x, int y, int w, int cstep, int format, int type) \n\
{ \n\
if (type == DT_INT8) \n\
        return load_blur_rgba_int8(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load_blur_rgba_int16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load_blur_rgba_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_blur_rgba_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec4(0.f); \n\
} \
"

#define SHADER_USM_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 src_rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 blur_rgb = load_blur_rgba(gx, gy, p.blur_w, p.blur_cstep, p.blur_format, p.blur_type).rgb; \n\
    sfpvec3 diff = abs(src_rgba.rgb - blur_rgb); \n\
    sfpvec4 result = sfpvec4(0.f); \n\
    result.a = src_rgba.a; \n\
    sfpvec3 amount = src_rgba.rgb * sfp(p.amount) + blur_rgb * sfp(1.f - p.amount); \n\
    if (diff.r > p.threshold) \n\
        result.r = src_rgba.r; \n\
    else \n\
        result.r = clamp(amount.r, sfp(0.f), sfp(1.0f)); \n\
    if (diff.g > p.threshold) \n\
        result.g = src_rgba.g; \n\
    else \n\
        result.g = clamp(amount.g, sfp(0.f), sfp(1.0f)); \n\
    if (diff.b > p.threshold) \n\
        result.b = src_rgba.b; \n\
    else \n\
        result.b = clamp(amount.b, sfp(0.f), sfp(1.0f)); \n\
    store_rgba(result, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char USMFilter_data[] = 
SHADER_HEADER
USM_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8)  readonly buffer blur_int8     { uint8_t   blur_int8_data[]; };
layout (binding = 9)  readonly buffer blur_int16    { uint16_t  blur_int16_data[]; };
layout (binding = 10) readonly buffer blur_float16  { float16_t blur_float16_data[]; };
layout (binding = 11) readonly buffer blur_float32  { float     blur_float32_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_BLUR_RGBA
SHADER_STORE_RGBA
SHADER_USM_MAIN
;
