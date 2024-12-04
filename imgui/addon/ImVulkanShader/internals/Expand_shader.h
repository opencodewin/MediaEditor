#pragma once
#include <imvk_mat_shader.h>

#define CROP_SHADER_PARAM \
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
	int t;	\n\
    int b;	\n\
    int l;	\n\
    int r;	\n\
} p; \
"

#define EXPAND_SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    if (uv.y >= p.t && uv.y < p.t + p.h && uv.x >= p.l && uv.x < p.l + p.w) \n\
    { \n\
        sfpvec4 result = load_rgba(uv.x - p.l, uv.y - p.t, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
    else \n\
        store_rgba(sfpvec4(0.f, 0.f, 0.f, 1.f), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char ExpandShader_data[] = 
SHADER_HEADER
CROP_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
EXPAND_SHADER_MAIN
;
