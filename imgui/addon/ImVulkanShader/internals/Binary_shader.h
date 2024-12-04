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

#define BINARY_SHADER_PARAM \
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
	float _min;	\n\
    float _max;	\n\
} p; \
"

#define BINARY_SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfp light = (sfp(0.299) * rgba.r + sfp(0.587) * rgba.g + sfp(0.114) * rgba.b); \n\
    if (light < sfp(p._min)) light = sfp(0); \n\
    if (light > sfp(p._max)) light = sfp(0); \n\
    if (light > eps) light = sfp(1); \n\
    else light = sfp(0); \n\
    store_rgba(sfpvec4(light, light, light, rgba.a), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
BINARY_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
BINARY_SHADER_MAIN
;
