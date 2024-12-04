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
    float hue; \n\
} p; \
"

#define SHADER_HUE \
" \n\
const sfpvec3  kRGBToYPrime = sfpvec3(sfp(0.299f), sfp(0.587f), sfp(0.114f)); \n\
const sfpvec3  kRGBToI      = sfpvec3(sfp(0.595716f), sfp(-0.274453f), sfp(-0.321263f)); \n\
const sfpvec3  kRGBToQ      = sfpvec3(sfp(0.211456f), sfp(-0.522591f), sfp(0.31135f)); \n\
const sfpvec3  kYIQToR      = sfpvec3(sfp(1.0f), sfp(0.9563f), sfp(0.6210f)); \n\
const sfpvec3  kYIQToG      = sfpvec3(sfp(1.0f), sfp(-0.2721f), sfp(-0.6474f)); \n\
const sfpvec3  kYIQToB      = sfpvec3(sfp(1.0f), sfp(-1.1070f), sfp(1.7046f)); \n\
sfpvec3 hue(sfpvec3 color) \n\
{ \n\
    sfp   YPrime    = dot(color, kRGBToYPrime); \n\
    sfp   I         = dot(color, kRGBToI); \n\
    sfp   Q         = dot(color, kRGBToQ); \n\
    // Calculate the hue and chroma \n\
    sfp   hue       = atan (Q, I); \n\
    sfp   chroma    = sqrt (I * I + Q * Q); \n\
    // Make the user's adjustments \n\
    hue += sfp(-p.hue); //why negative rotation? \n\
    // Convert back to YIQ \n\
    Q = chroma * sin (hue); \n\
    I = chroma * cos (hue); \n\
    // Convert back to RGB \n\
    sfpvec3    yIQ   = sfpvec3 (YPrime, I, Q); \n\
    color.r = dot(yIQ, kYIQToR); \n\
    color.g = dot(yIQ, kYIQToG); \n\
    color.b = dot(yIQ, kYIQToB); \n\
    return color; \n\
} \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 color = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 result = hue(color.rgb); \n\
    result = clamp(result, sfpvec3(0.f), sfpvec3(1.0f)); \n\
    store_rgba(sfpvec4(result, color.a), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_HUE
SHADER_MAIN
;
