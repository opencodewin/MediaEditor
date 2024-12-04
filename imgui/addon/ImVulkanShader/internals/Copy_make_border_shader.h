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
    int top; \n\
    int bottom; \n\
    int left; \n\
    int right; \n\
    \n\
    float value; \n\
} p; \n\
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    if (uv.x >= p.left && uv.x < p.out_w - p.right && \n\
        uv.y >= p.top && uv.y < p.out_h - p.bottom) \n\
    { \n\
        if (p.in_format == CF_ABGR || p.in_format == CF_ARGB) \n\
        { \n\
            sfpvec4 rgba = load_rgba(uv.x - p.left, uv.y - p.top, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            if (p.out_format == CF_ABGR || p.out_format == CF_ARGB) \n\
                store_rgba(rgba, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
            else if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
                store_rgb(rgba.rgb, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
        else if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        { \n\
            sfpvec3 rgb = load_rgb(uv.x - p.left, uv.y - p.top, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            if (p.out_format == CF_ABGR || p.out_format == CF_ARGB) \n\
                store_rgba(sfpvec4(rgb, sfp(1.0f)), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
            else if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
                store_rgb(rgb, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
    } \n\
    else \n\
    { \n\
        if (p.out_format == CF_ABGR || p.out_format == CF_ARGB) \n\
            store_rgba(sfpvec4(p.value, p.value, p.value, sfp(1.f)), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        else if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
            store_rgb(sfpvec3(p.value), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \n\
"

static const char Filter_data[] =
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGB
SHADER_LOAD_RGBA
SHADER_STORE_RGB
SHADER_STORE_RGBA
SHADER_MAIN
;