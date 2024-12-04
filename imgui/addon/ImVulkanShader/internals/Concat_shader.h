#pragma once
#include <imvk_mat_shader.h>

// Load data as rgba
#define SHADER_LOAD2_RGBA_INT8 \
" \n\
sfpvec4 load2_rgba_int8(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(src2_data_int8[i_offset.r])) / sfp(255.f); \n\
    rgb_in.g = sfp(uint(src2_data_int8[i_offset.g])) / sfp(255.f); \n\
    rgb_in.b = sfp(uint(src2_data_int8[i_offset.b])) / sfp(255.f); \n\
    rgb_in.a = sfp(uint(src2_data_int8[i_offset.a])) / sfp(255.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD2_RGBA_INT16 \
" \n\
sfpvec4 load2_rgba_int16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(src2_data_int16[i_offset.r])) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(src2_data_int16[i_offset.g])) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(src2_data_int16[i_offset.b])) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint(src2_data_int16[i_offset.a])) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD2_RGBA_FLOAT16 \
" \n\
sfpvec4 load2_rgba_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(src2_data_float16[i_offset.r]); \n\
    rgb_in.g = sfp(src2_data_float16[i_offset.g]); \n\
    rgb_in.b = sfp(src2_data_float16[i_offset.b]); \n\
    rgb_in.a = sfp(src2_data_float16[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD2_RGBA_FLOAT32 \
" \n\
sfpvec4 load2_rgba_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(src2_data_float32[i_offset.r]); \n\
    rgb_in.g = sfp(src2_data_float32[i_offset.g]); \n\
    rgb_in.b = sfp(src2_data_float32[i_offset.b]); \n\
    rgb_in.a = sfp(src2_data_float32[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD2_RGBA \
SHADER_LOAD2_RGBA_INT8 \
SHADER_LOAD2_RGBA_INT16 \
SHADER_LOAD2_RGBA_FLOAT16 \
SHADER_LOAD2_RGBA_FLOAT32 \
" \n\
sfpvec4 load2_rgba(int x, int y, int w, int cstep, int format, int type) \n\
{ \n\
    if (type == DT_INT8) \n\
        return load2_rgba_int8(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load2_rgba_int16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load2_rgba_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load2_rgba_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec4(0.f); \n\
} \n\
" // 58 lines

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
	int direction;	\n\
} p; \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    if (p.direction == CONCAT_HORIZONTAL) \n\
    { \n\
        if (uv.x < p.w) \n\
        { \n\
            sfpvec4 rgba = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            store_rgba(rgba, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
        else \n\
        { \n\
            sfpvec4 rgba = load2_rgba(uv.x - p.w, uv.y, p.w2, p.cstep2, p.in_format2, p.in_type2); \n\
            store_rgba(rgba, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
    } \n\
    else \n\
    { \n\
        if (uv.y < p.h) \n\
        { \n\
            sfpvec4 rgba = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            store_rgba(rgba, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
        else \n\
        { \n\
            sfpvec4 rgba = load2_rgba(uv.x, uv.y - p.h, p.w2, p.cstep2, p.in_format2, p.in_type2); \n\
            store_rgba(rgba, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
    } \n\
} \
"

static const char Shader_data[] = 
SHADER_HEADER
R"(
#define CONCAT_HORIZONTAL   0
#define CONCAT_VERTICAL     1
)"
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_INPUT2_DATA
SHADER_LOAD_RGBA
SHADER_LOAD2_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
