#pragma once
#include <imvk_mat_shader.h>

//      float sigma  >  0 - sigma Standard Deviation
//      float kSigma >= 0 - sigma coefficient
//      kSigma * sigma  -->  radius of the circular kernel
//      float threshold   - edge sharpening threshold
//
//  About Standard Deviations (watch Gauss curve)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  kSigma = 1*sigma cover 68% of data
//  kSigma = 2*sigma cover 95% of data - but there are over 3 times 
//                   more points to compute
//  kSigma = 3*sigma cover 99.7% of data - but needs more than double 
//                   the calculations of 2*sigma


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
    float sigma; \n\
    float ksigma; \n\
    float threshold; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
#define INV_SQRT_OF_2PI 0.39894228040143267793994605993439  // 1.0/SQRT_OF_2PI \n\
#define INV_PI          0.31830988618379067153776752674503 \n\
#define invGamma        0.454545454545455 // 1 / 2.2 \n\
sfpvec4 smartDeNoise(vec2 uv) \n\
{ \n\
    float radius = round(p.ksigma * p.sigma); \n\
    float radQ = radius * radius; \n\
\n\
    float invSigmaQx2 = .5 / (p.sigma * p.sigma);  // 1.0 / (sigma^2 * 2.0) \n\
    float invSigmaQx2PI = INV_PI * invSigmaQx2;    // // 1/(2 * PI * sigma^2) \n\
\n\
    float invThresholdSqx2 = .5 / (p.threshold * p.threshold);   // 1.0 / (sigma^2 * 2.0) \n\
    float invThresholdSqrt2PI = INV_SQRT_OF_2PI / p.threshold;   // 1.0 / (sqrt(2*PI) * sigma) \n\
\n\
    sfpvec4 rgba = load_rgba(int(uv.x), int(uv.y), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 centrPx = pow(rgba.rgb, sfpvec3(invGamma)); \n\
\n\
    sfp zBuff = 0.0; \n\
    sfpvec3 aBuff = sfpvec3(0.0); \n\
    vec2 d; \n\
\n\
    for (d.x = -radius; d.x <= radius; d.x++) \n\
    { \n\
        float pt = sqrt(radQ - d.x * d.x);       // pt = yRadius: have circular trend \n\
        for (d.y = -pt; d.y <= pt; d.y++) \n\
        { \n\
            float blurFactor = exp( -dot(d , d) * invSigmaQx2 ) * invSigmaQx2PI; \n\
\n\
            vec2 point = uv + d; \n\
            sfpvec3 walkPx = load_rgba(int(point.x), int(point.y), p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
            sfpvec3 dC = pow(walkPx, sfpvec3(invGamma)) - centrPx; \n\
            sfp deltaFactor = exp( -dot(dC, dC) * sfp(invThresholdSqx2)) * sfp(invThresholdSqrt2PI) * sfp(blurFactor); \n\
\n\
            zBuff += deltaFactor; \n\
            aBuff += deltaFactor * walkPx; \n\
        } \n\
    } \n\
    return sfpvec4(aBuff / zBuff, sfp(1.0)); \n\
} \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 result = smartDeNoise(vec2(gx, gy)); \n\
    store_rgba(result, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
