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
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    \n\
    float mean_x; \n\
    float mean_y; \n\
    float mean_z; \n\
    float mean_w; \n\
    \n\
    float norm_x; \n\
    float norm_y; \n\
    float norm_z; \n\
    float norm_w; \n\
} p; \n\
"

// Store data as float rgba without clamp
#define SHADER_STORE_RGBA_PLANER_FLOAT16_NO_CLAMP \
" \n\
void store_rgba_planer_float16_no_clamp(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = color_format_mapping_vec4(format) + (y * w + x); \n\
    dst_data_float16[o_offset.r] = float16_t(val.r); \n\
    dst_data_float16[o_offset.g] = float16_t(val.g); \n\
    dst_data_float16[o_offset.b] = float16_t(val.b); \n\
    dst_data_float16[o_offset.a] = float16_t(val.a); \n\
} \n\
"

#define SHADER_STORE_RGBA_PLANER_FLOAT32_NO_CLAMP \
" \n\
void store_rgba_planer_float32_no_clamp(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = color_format_mapping_vec4(format) + (y * w + x); \n\
    dst_data_float32[o_offset.r] = float(val.r); \n\
    dst_data_float32[o_offset.g] = float(val.g); \n\
    dst_data_float32[o_offset.b] = float(val.b); \n\
    dst_data_float32[o_offset.a] = float(val.a); \n\
} \n\
"

#define SHADER_STORE_RGBA_PLANER_FLOAT_NO_CLAMP \
SHADER_STORE_RGBA_PLANER_FLOAT16_NO_CLAMP \
SHADER_STORE_RGBA_PLANER_FLOAT32_NO_CLAMP \
" \n\
void store_rgba_planer_float_no_clamp(sfpvec4 val, int x, int y, int w, int cstep, int format, int type) \n\
{ \n\
    if (type == DT_FLOAT16) \n\
        store_rgba_planer_float16_no_clamp(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgba_planer_float32_no_clamp(val, x, y, w, cstep, format); \n\
} \n\
"

// Store data as float rgb without clamp
#define SHADER_STORE_RGB_PLANER_FLOAT16_NO_CLAMP \
" \n\
void store_rgb_planer_float16_no_clamp(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = color_format_mapping_vec3(format) * cstep + (y * w + x); \n\
    dst_data_float16[o_offset.r] = float16_t(val.r); \n\
    dst_data_float16[o_offset.g] = float16_t(val.g); \n\
    dst_data_float16[o_offset.b] = float16_t(val.b); \n\
} \n\
"

#define SHADER_STORE_RGB_PLANER_FLOAT32_NO_CLAMP \
" \n\
void store_rgb_planer_float32_no_clamp(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = color_format_mapping_vec3(format) * cstep + (y * w + x); \n\
    dst_data_float32[o_offset.r] = float(val.r); \n\
    dst_data_float32[o_offset.g] = float(val.g); \n\
    dst_data_float32[o_offset.b] = float(val.b); \n\
} \n\
"

#define SHADER_STORE_RGB_PLANER_FLOAT_NO_CLAMP \
SHADER_STORE_RGB_PLANER_FLOAT16_NO_CLAMP \
SHADER_STORE_RGB_PLANER_FLOAT32_NO_CLAMP \
" \n\
void store_rgb_planer_float_no_clamp(sfpvec3 val, int x, int y, int w, int cstep, int format, int type) \n\
{ \n\
    if (type == DT_FLOAT16) \n\
        store_rgb_planer_float16_no_clamp(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgb_planer_float32_no_clamp(val, x, y, w, cstep, format); \n\
} \n\
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 mean_val = sfpvec4(sfp(p.mean_x), sfp(p.mean_y), sfp(p.mean_z), sfp(p.mean_w)); \n\
    sfpvec4 norm_val = sfpvec4(sfp(p.norm_x), sfp(p.norm_y), sfp(p.norm_z), sfp(p.norm_w)); \n\
    sfpvec4 rgba = sfpvec4(0.f, 0.f, 0.f, 1.f); \n\
    if (p.in_format == CF_ABGR || p.in_format == CF_ARGB) \n\
        rgba = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    else if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        rgba.rgb = load_rgb(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 result = (rgba - mean_val) * norm_val; \n\
    if (p.out_format == CF_ABGR || p.out_format == CF_ARGB) \n\
        store_rgba_planer_float_no_clamp(result, uv.x, uv.y, p.out_w, p.out_cstep, p.out_format, p.out_type); \n\
    else if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
        store_rgb_planer_float_no_clamp(result.rgb, uv.x, uv.y, p.out_w, p.out_cstep, p.out_format, p.out_type); \n\
} \n\
"

static const char Filter_data[] =
SHADER_HEADER
SHADER_PARAM
SHADER_SRC_DATA
R"(
layout (binding = 4) writeonly buffer dst_float16 { float16_t dst_data_float16[]; };
layout (binding = 5) writeonly buffer dst_float32 { float dst_data_float32[]; };
)"
SHADER_LOAD_RGB
SHADER_LOAD_RGBA
SHADER_STORE_RGBA_PLANER_FLOAT_NO_CLAMP
SHADER_STORE_RGB_PLANER_FLOAT_NO_CLAMP
SHADER_MAIN
;