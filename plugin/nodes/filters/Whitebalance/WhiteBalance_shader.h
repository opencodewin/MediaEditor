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
    float temperature; \n\
} p; \
"

#define SHADER_WHITEBALANCE \
" \n\
const sfp tint = sfp(0.f); \n\
const sfpvec3 warmFilter = sfpvec3(sfp(0.93f), sfp(0.54f), sfp(0.0f)); \n\
const sfpmat3 RGBtoYIQ = sfpmat3(sfp(0.299f), sfp(0.587f), sfp(0.114f), sfp(0.596f), sfp(-0.274f), sfp(-0.322f), sfp(0.212f), sfp(-0.523f), sfp(0.311f)); \n\
const sfpmat3 YIQtoRGB = sfpmat3(sfp(1.0f), sfp(0.956f), sfp(0.621f), sfp(1.0f), sfp(-0.272f), sfp(-0.647f), sfp(1.0f), sfp(-1.105f), sfp(1.702f)); \n\
sfpvec3 whitebalance(sfpvec3 color) \n\
{ \n\
    sfpvec3 yiq = RGBtoYIQ * color.rgb; \n\
    yiq.b = clamp(yiq.b + tint * sfp(0.5226f * 0.1f), sfp(-0.5226f), sfp(0.5226f)); \n\
    sfpvec3 rgb = YIQtoRGB * yiq; \n\
    sfpvec3 processed = sfpvec3((rgb.r < sfp(0.5f) ? (sfp(2.0f) * rgb.r * warmFilter.r) : (sfp(1.0f) - sfp(2.0f) * (sfp(1.0f) - rgb.r) * (sfp(1.0f) - warmFilter.r))), \n\
                        (rgb.g < sfp(0.5f) ? (sfp(2.0f) * rgb.g * warmFilter.g) : (sfp(1.0f) - sfp(2.0f) * (sfp(1.0f) - rgb.g) * (sfp(1.0f) - warmFilter.g))), \n\
                        (rgb.b < sfp(0.5f) ? (sfp(2.0f) * rgb.b * warmFilter.b) : (sfp(1.0f) - sfp(2.0f) * (sfp(1.0f) - rgb.b) * (sfp(1.0f) - warmFilter.b)))); \n\
    return mix(rgb, processed, sfp(p.temperature)); \n\
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
    sfpvec3 result = whitebalance(color.rgb); \n\
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
SHADER_WHITEBALANCE
SHADER_MAIN
;
