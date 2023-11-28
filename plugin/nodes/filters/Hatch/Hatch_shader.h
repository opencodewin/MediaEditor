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
    float spacing; \n\
    float width; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const sfpvec3 W = sfpvec3(0.2125, 0.7154, 0.0721); \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 uv = vec2(float(gl_GlobalInvocationID.x) / float(p.out_w - 1), float(gl_GlobalInvocationID.y) / float(p.out_h - 1)); \n\
    sfpvec4 color = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfp luminance = dot(color.rgb, W); \n\
    sfpvec4 colorToDisplay = sfpvec4(1.0, 1.0, 1.0, color.a); \n\
    if (luminance < sfp(1.00)) \n\
    { \n\
        if (mod(uv.x + uv.y, p.spacing) <= p.width) \n\
        { \n\
            colorToDisplay.rgb = sfpvec3(0.0, 0.0, 0.0); \n\
        } \n\
    } \n\
    if (luminance < sfp(0.75)) \n\
    { \n\
        if (mod(uv.x - uv.y, p.spacing) <= p.width) \n\
        { \n\
            colorToDisplay.rgb = sfpvec3(0.0, 0.0, 0.0); \n\
        } \n\
    } \n\
    if (luminance < sfp(0.50)) \n\
    { \n\
        if (mod(uv.x + uv.y - (p.spacing / 2.0), p.spacing) <= p.width) \n\
        { \n\
            colorToDisplay.rgb = sfpvec3(0.0, 0.0, 0.0); \n\
        } \n\
    } \n\
    if (luminance < sfp(0.3)) \n\
    { \n\
        if (mod(uv.x - uv.y - (p.spacing / 2.0), p.spacing) <= p.width) \n\
        { \n\
            colorToDisplay.rgb = sfpvec3(0.0, 0.0, 0.0); \n\
        } \n\
    } \n\
    store_rgba(colorToDisplay, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
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
