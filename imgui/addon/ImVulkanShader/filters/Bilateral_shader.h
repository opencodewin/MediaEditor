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
    int ksz; \n\
    float sigma_spatial2_inv_half; \n\
    float sigma_color2_inv_half; \n\
} p; \
"

#ifndef BILATERAL_DEFECT
#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 center = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 sum1 = sfpvec3(0.0f); \n\
    sfp sum2 = sfp(0.0f); \n\
    int r = p.ksz / 2; \n\
    sfp r2 = sfp(r * r); \n\
    // 最长半径 \n\
    int tx = uv.x - r + p.ksz; \n\
    int ty = uv.y - r + p.ksz; \n\
    // 半径为r的圆里数据根据比重混合 \n\
    for (int cy = uv.y - r; cy < ty; ++cy) \n\
    { \n\
        for (int cx = uv.x - r; cx < tx; ++cx) \n\
        { \n\
            sfp space2 = sfp((uv.x - cx) * (uv.x - cx) + (uv.y - cy) * (uv.y - cy)); \n\
            if (space2 < r2) \n\
            { \n\
                int bx = max(0, min(cx, p.out_w - 1)); \n\
                int by = max(0, min(cy, p.out_h - 1)); \n\
                sfpvec3 color = load_rgba(bx, by, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
                sfp norm = dot(abs(color - center.rgb), sfpvec3(1.0f)); \n\
                sfp weight = exp(space2 * sfp(p.sigma_spatial2_inv_half) + norm * norm * sfp(p.sigma_color2_inv_half)); \n\
                sum1 = sum1 + weight * color; \n\
                sum2 = sum2 + weight; \n\
            } \n\
        } \n\
    } \n\
    store_rgba(sfpvec4(sfpvec3(sum1/sum2), center.a), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"
#else
#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfp gaussianWeightTotal; \n\
    sfpvec3 sum; \n\
    sfpvec3 sampleColor; \n\
    sfp distanceFromCentralColor; \n\
    sfp gaussianWeight; \n\
    sfpvec4 centralColor = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    gaussianWeightTotal = sfp(0.18f); \n\
    sum = centralColor.rgb * sfp(0.18f); \n\
    int sigma_spatial = int(p.ksz * p.sigma_spatial2_inv_half); \n\
    sfp sigma_color = - sfp(p.sigma_color2_inv_half); \n\
    \n\
    sampleColor = load_rgba(uv.x - sigma_spatial, uv.y - sigma_spatial, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.05) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x, uv.y - sigma_spatial, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.09) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x + sigma_spatial, uv.y - sigma_spatial, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.12) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x - sigma_spatial, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.15) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x + sigma_spatial, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.15) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x - sigma_spatial, uv.y + sigma_spatial, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.12) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x, uv.y + sigma_spatial, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.09) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sampleColor = load_rgba(uv.x + sigma_spatial, uv.y + sigma_spatial, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    distanceFromCentralColor = min(distance(centralColor.rgb, sampleColor) * sigma_color, sfp(1.0f)); \n\
    gaussianWeight = sfp(0.05) * (sfp(1.0f) - distanceFromCentralColor); \n\
    gaussianWeightTotal += gaussianWeight; \n\
    sum += sampleColor * gaussianWeight; \n\
    \n\
    sfpvec3 result = sum / gaussianWeightTotal; \n\
    store_rgba(sfpvec4(result, centralColor.a), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"
#endif

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
