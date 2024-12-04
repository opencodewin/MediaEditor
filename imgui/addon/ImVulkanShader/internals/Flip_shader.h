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
	int bFlipX;	\n\
    int bFlipY;	\n\
} p; \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    int ix = uv.x; \n\
    int iy = uv.y; \n\
    if (p.bFlipX != 0) \n\
        ix = p.w - 1 - ix; \n\
    if (p.bFlipY != 0) \n\
        iy = p.h - 1 - iy; \n\
    sfpvec4 result = load_rgba(ix, iy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Shader_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
