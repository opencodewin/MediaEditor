#pragma once
#include <imvk_shader.h>

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
    int channel; \n\
} p; \
"

#define SHADER_SPLIT_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba = load_rgb_image(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 dst_color; \n\
    dst_color.r = p.channel == 0 ? rgba.r : p.channel == 1 ? rgba.g : p.channel == 2 ? rgba.b : rgba.a; \n\
    store_rgb_image(dst_color, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Split_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_IMAGE
SHADER_STORE_IMAGE
SHADER_SPLIT_MAIN
;

#define SHADER_MERGE_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 src_rgba = load_rgb_image(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 dst_color = load_dst_rgb_image(uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    if (p.channel == 0) dst_color.r = src_rgba.r; \n\
    if (p.channel == 1) dst_color.g = src_rgba.r; \n\
    if (p.channel == 2) dst_color.b = src_rgba.r; \n\
    else                dst_color.a = src_rgba.r; \n\
    store_rgb_image(dst_color, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Merge_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_DATA
SHADER_OUTPUT_RDWR_DATA
SHADER_LOAD_IMAGE
SHADER_LOAD_DST_IMAGE
SHADER_STORE_IMAGE
SHADER_MERGE_MAIN
;
