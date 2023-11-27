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
    float intensity; \n\
    int step; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
sfp lum(sfpvec4 value) \n\
{ \n\
    return dot(value.rgb, sfpvec3(sfp(0.299), sfp(0.587), sfp(0.114))); \n\
} \n\
sfpvec4 getMaxValue(sfpvec4 newValue, sfpvec4 originValue) \n\
{ \n\
    return mix(newValue, originValue, step(lum(newValue), lum(originValue))); \n\
} \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 _x_1_y_1 = load_rgba(gx-p.step, gy-p.step, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_1_y   = load_rgba(gx-p.step, gy,   p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_1_y1  = load_rgba(gx-p.step, gy+p.step, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y_1   = load_rgba(gx,        gy-p.step, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y     = load_rgba(gx,        gy,        p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y1    = load_rgba(gx,        gy+p.step, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y_1  = load_rgba(gx+p.step, gy-p.step, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y    = load_rgba(gx+p.step, gy,        p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y1   = load_rgba(gx+p.step, gy+p.step, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 vTemp; \n\
    sfpvec4 vMin = _x_y; \n\
    vMin = getMaxValue(_x_1_y_1, vMin); \n\
    vMin = getMaxValue(_x_1_y,   vMin); \n\
    vMin = getMaxValue(_x_1_y1,  vMin); \n\
    vMin = getMaxValue(_x_y_1,   vMin); \n\
    vMin = getMaxValue(_x_y1,    vMin); \n\
    vMin = getMaxValue(_x1_y_1,  vMin); \n\
    vMin = getMaxValue(_x1_y,    vMin); \n\
    vMin = getMaxValue(_x1_y1,   vMin); \n\
    sfp lumOrigin = lum(_x_y); \n\
    sfp lumMax = lum(vMin) + sfp(0.001); \n\
    sfp blendColor = min(lumOrigin / lumMax, sfp(1.0)); \n\
    sfpvec4 rgba = sfpvec4(mix(_x_y.rgb, sfpvec3(blendColor), sfp(p.intensity)), _x_y.a); \n\
    store_rgba(rgba, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \n\
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
