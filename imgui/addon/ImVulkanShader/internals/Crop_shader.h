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
	int _x;	\n\
    int _y;	\n\
} p; \
"

#define CROP_SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result = load_rgba(uv.x + p._x, uv.y + p._y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char CropShader_data[] = 
SHADER_HEADER
CROP_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
CROP_SHADER_MAIN
;

#define CROPTO_SHADER_PARAM \
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
	int _x;	\n\
    int _y;	\n\
    int _w;	\n\
    int _h;	\n\
    int _dx; \n\
    int _dy; \n\
    \n\
    int fill; \n\
} p; \
"

#define CROPTO_SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    ivec2 sre_pos = uv - ivec2(p._dx, p._dy) + ivec2(p._x, p._y); \n\
    if (sre_pos.x < p._x || sre_pos.x >= p._x + p._w || sre_pos.y < p._y || sre_pos.y >= p._y + p._h) \n\
    { \n\
        if (p.fill == 1) \n\
            store_rgba(sfpvec4(0.f), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        return; \n\
    } \n\
    else \n\
    { \n\
        sfpvec4 result = load_rgba(sre_pos.x, sre_pos.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \
"

static const char CropToShader_data[] = 
SHADER_HEADER
CROPTO_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
CROPTO_SHADER_MAIN
;
