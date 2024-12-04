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
    float r,g,b,a; \n\
    \n\
    int crop_l; \n\
    int crop_t; \n\
    int crop_r; \n\
    int crop_b; \n\
    \n\
    int interp_type; \n\
} p; \n\
#define INTER_BITS 5 \n\
#define INTER_TAB_SIZE (1 << INTER_BITS) \n\
#define INTER_SCALE 1.f/INTER_TAB_SIZE \n\
#define AB_BITS max(10, int(INTER_BITS)) \n\
#define AB_SCALE (1 << AB_BITS) \n\
#define INTER_REMAP_COEF_BITS 15 \n\
#define INTER_REMAP_COEF_SCALE (1 << INTER_REMAP_COEF_BITS) \n\
#define ROUND_DELTA (1 << (AB_BITS - INTER_BITS - 1)) \n\
"

#define WARP_AFFINE_NEAREST \
" \n\
void warpAffine_nearest() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    int round_delta = (AB_SCALE >> 1); \n\
    \n\
    int X0_ = int(transform_matrix[0] * gx * AB_SCALE); \n\
    int Y0_ = int(transform_matrix[3] * gx * AB_SCALE); \n\
    for (int dy = gy, dy1 = min(p.out_h, gy + 1); dy < dy1; ++dy) \n\
    { \n\
        int X0 = X0_ + int(fma(transform_matrix[1], float(dy), transform_matrix[2]) * AB_SCALE) + round_delta; \n\
        int Y0 = Y0_ + int(fma(transform_matrix[4], float(dy), transform_matrix[5]) * AB_SCALE) + round_delta; \n\
        \n\
        int sx = int(X0 >> AB_BITS); \n\
        int sy = int(Y0 >> AB_BITS); \n\
        \n\
        if (sx >= p.crop_l && sx < p.w - p.crop_r && sy >= p.crop_t && sy < p.h - p.crop_b) \n\
        { \n\
            sfpvec4 rgba = load_rgba(sx, sy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            store_rgba(rgba, gx, dy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
        else \n\
        { \n\
            sfpvec4 rgba = sfpvec4(p.r, p.g, p.b, p.a); \n\
            store_rgba(rgba, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
    } \n\
} \
"

#define WARP_AFFINE_LINEAR \
" \n\
void warpAffine_linear() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    int tmp = gx << AB_BITS; \n\
    int X0_ = int(transform_matrix[0] * tmp); \n\
    int Y0_ = int(transform_matrix[3] * tmp); \n\
    sfpvec4 filled = sfpvec4(p.r, p.g, p.b, p.a); \n\
    for (int dy = gy, dy1 = min(p.out_h, gy + 1); dy < dy1; ++dy) \n\
    { \n\
        int X0 = X0_ + int(fma(transform_matrix[1], float(dy), transform_matrix[2]) * AB_SCALE) + ROUND_DELTA; \n\
        int Y0 = Y0_ + int(fma(transform_matrix[4], float(dy), transform_matrix[5]) * AB_SCALE) + ROUND_DELTA; \n\
        X0 = X0 >> (AB_BITS - INTER_BITS); \n\
        Y0 = Y0 >> (AB_BITS - INTER_BITS); \n\
        \n\
        int sx = int(X0 >> INTER_BITS), sy = int(Y0 >> INTER_BITS); \n\
        int ax = int(X0 & (INTER_TAB_SIZE-1)), ay = int(Y0 & (INTER_TAB_SIZE-1)); \n\
        sfpvec4 v0 = filled, v1 = filled, v2 = filled, v3 = filled; \n\
        if (sx >= p.crop_l && sx < p.w - p.crop_r) \n\
        { \n\
            if (sy + 0 >= p.crop_t && sy + 0 < p.h - p.crop_b) v0 = load_rgba(sx, sy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            if (sy + 1 >= p.crop_t && sy + 1 < p.h - p.crop_b) v2 = load_rgba(sx, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        } \n\
        if (sx + 1 >= p.crop_l && sx + 1 < p.w - p.crop_r) \n\
        { \n\
            if (sy + 0 >= p.crop_t && sy + 0 < p.h - p.crop_b) v1 = load_rgba(sx + 1, sy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            if (sy + 1 >= p.crop_t && sy + 1 < p.h - p.crop_b) v3 = load_rgba(sx + 1, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        } \n\
        \n\
        sfp taby = sfp(1.f / INTER_TAB_SIZE * ay); \n\
        sfp tabx = sfp(1.f / INTER_TAB_SIZE * ax); \n\
        sfp tabx2 = sfp(1.0f - tabx), taby2 = sfp(1.0f - taby); \n\
        sfpvec4 rgba = v0 * tabx2 * taby2 +  v1 * tabx * taby2 + v2 * tabx2 * taby + v3 * tabx * taby; \n\
        store_rgba(rgba, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \
"

#define WARP_AFFINE_CUBIC \
"\n\
void interpolateCubic(sfp x, out sfp coeffs[4]) \n\
{ \n\
    const sfp A = sfp(-0.75f); \n\
    coeffs[0] = fma(fma(fma(A, (x + sfp(1.f)), - sfp(5.0f) * A), (x + sfp(1.f)), sfp(8.0f) * A), x + sfp(1.f), - sfp(4.0f) * A); \n\
    coeffs[1] = fma(fma(A + sfp(2.f), x, - (A + sfp(3.f))), x * x, sfp(1.f)); \n\
    coeffs[2] = fma(fma(A + sfp(2.f), sfp(1.f) - x, - (A + sfp(3.f))), (sfp(1.f) - x) * (sfp(1.f) - x), sfp(1.f)); \n\
    coeffs[3] = sfp(1.f) - coeffs[0] - coeffs[1] - coeffs[2]; \n\
} \n\
void warpAffine_cubic() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 filled = sfpvec4(p.r, p.g, p.b, p.a); \n\
    int tmp = (gx << AB_BITS); \n\
    int X0 = int(transform_matrix[0] * tmp) + int(fma(transform_matrix[1], float(gy), transform_matrix[2]) * AB_SCALE) + ROUND_DELTA; \n\
    int Y0 = int(transform_matrix[3] * tmp) + int(fma(transform_matrix[4], float(gy), transform_matrix[5]) * AB_SCALE) + ROUND_DELTA; \n\
    \n\
    X0 = X0 >> (AB_BITS - INTER_BITS); \n\
    Y0 = Y0 >> (AB_BITS - INTER_BITS); \n\
    \n\
    int sx = int(X0 >> INTER_BITS) - 1, sy = int(Y0 >> INTER_BITS) - 1; \n\
    int ay = int(Y0 & (INTER_TAB_SIZE - 1)), ax = int(X0 & (INTER_TAB_SIZE - 1)); \n\
    sfpvec4 v[16]; \n\
    #if 0 \n\
    v[ 0] = (sx + 0 >= p.crop_l && sx + 0 < p.w - p.crop_r && sy + 0 >= p.crop_t && sy + 0 < p.h - p.crop_b) ? load_rgba(sx + 0, sy + 0, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 1] = (sx + 1 >= p.crop_l && sx + 1 < p.w - p.crop_r && sy + 0 >= p.crop_t && sy + 0 < p.h - p.crop_b) ? load_rgba(sx + 1, sy + 0, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 2] = (sx + 2 >= p.crop_l && sx + 2 < p.w - p.crop_r && sy + 0 >= p.crop_t && sy + 0 < p.h - p.crop_b) ? load_rgba(sx + 2, sy + 0, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 3] = (sx + 3 >= p.crop_l && sx + 3 < p.w - p.crop_r && sy + 0 >= p.crop_t && sy + 0 < p.h - p.crop_b) ? load_rgba(sx + 3, sy + 0, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 4] = (sx + 0 >= p.crop_l && sx + 0 < p.w - p.crop_r && sy + 1 >= p.crop_t && sy + 1 < p.h - p.crop_b) ? load_rgba(sx + 0, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 5] = (sx + 1 >= p.crop_l && sx + 1 < p.w - p.crop_r && sy + 1 >= p.crop_t && sy + 1 < p.h - p.crop_b) ? load_rgba(sx + 1, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 6] = (sx + 2 >= p.crop_l && sx + 2 < p.w - p.crop_r && sy + 1 >= p.crop_t && sy + 1 < p.h - p.crop_b) ? load_rgba(sx + 2, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 7] = (sx + 3 >= p.crop_l && sx + 3 < p.w - p.crop_r && sy + 1 >= p.crop_t && sy + 1 < p.h - p.crop_b) ? load_rgba(sx + 3, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 8] = (sx + 0 >= p.crop_l && sx + 0 < p.w - p.crop_r && sy + 2 >= p.crop_t && sy + 2 < p.h - p.crop_b) ? load_rgba(sx + 0, sy + 2, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[ 9] = (sx + 1 >= p.crop_l && sx + 1 < p.w - p.crop_r && sy + 2 >= p.crop_t && sy + 2 < p.h - p.crop_b) ? load_rgba(sx + 1, sy + 2, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[10] = (sx + 2 >= p.crop_l && sx + 2 < p.w - p.crop_r && sy + 2 >= p.crop_t && sy + 2 < p.h - p.crop_b) ? load_rgba(sx + 2, sy + 2, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[11] = (sx + 3 >= p.crop_l && sx + 3 < p.w - p.crop_r && sy + 2 >= p.crop_t && sy + 2 < p.h - p.crop_b) ? load_rgba(sx + 3, sy + 2, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[12] = (sx + 0 >= p.crop_l && sx + 0 < p.w - p.crop_r && sy + 3 >= p.crop_t && sy + 3 < p.h - p.crop_b) ? load_rgba(sx + 0, sy + 3, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[13] = (sx + 1 >= p.crop_l && sx + 1 < p.w - p.crop_r && sy + 3 >= p.crop_t && sy + 3 < p.h - p.crop_b) ? load_rgba(sx + 1, sy + 3, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[14] = (sx + 2 >= p.crop_l && sx + 2 < p.w - p.crop_r && sy + 3 >= p.crop_t && sy + 3 < p.h - p.crop_b) ? load_rgba(sx + 2, sy + 3, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    v[15] = (sx + 3 >= p.crop_l && sx + 3 < p.w - p.crop_r && sy + 3 >= p.crop_t && sy + 3 < p.h - p.crop_b) ? load_rgba(sx + 3, sy + 3, p.w, p.h, p.cstep, p.in_format, p.in_type) : filled; \n\
    #else \n\
    #pragma unroll \n\
    for (int y = 0; y < 4; y++) \n\
    { \n\
        if (sy + y > p.crop_t && sy + y < p.h - p.crop_b - 1) \n\
        { \n\
            #pragma unroll \n\
            for (int x = 0; x < 4; x++) \n\
            { \n\
                if (sx + x > p.crop_l && sx + x < p.w - p.crop_r - 1) \n\
                    v[y * 4 + x] = load_rgba(sx + x, sy + y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
                else if (sx + x == p.crop_l || sx + x == p.w - p.crop_r - 1) \n\
                    v[y * 4 + x] = mix(load_rgba(sx + x, sy + y, p.w, p.h, p.cstep, p.in_format, p.in_type), filled, sfp((float(y) * 4.0 + float(x)) / float(16))); \n\
                else \n\
                    v[y * 4 + x] = filled; \n\
            } \n\
        } \n\
        else if (sy + y == p.crop_t || sy + y == p.h - p.crop_b - 1) \n\
        { \n\
            #pragma unroll \n\
            for (int x = 0; x < 4; x++) \n\
            { \n\
                if (sx + x > p.crop_l && sx + x < p.w - p.crop_r - 1) \n\
                    v[y * 4 + x] = mix(load_rgba(sx + x, sy + y, p.w, p.h, p.cstep, p.in_format, p.in_type), filled, sfp((float(y) * 4.0 + float(x)) / float(16))); \n\
                else \n\
                    v[y * 4 + x] = filled; \n\
            } \n\
        } \n\
        else \n\
        { \n\
            #pragma unroll \n\
            for (int x = 0; x < 4; x++) \n\
            { \n\
                v[y * 4 + x] = filled; \n\
            } \n\
        } \n\
    } \n\
    #endif \n\
    sfp tab1y[4], tab1x[4]; \n\
    sfp ayy = sfp(INTER_SCALE * ay); \n\
    sfp axx = sfp(INTER_SCALE * ax); \n\
    interpolateCubic(ayy, tab1y); \n\
    interpolateCubic(axx, tab1x); \n\
    \n\
    sfpvec4 rgba = sfpvec4(0); \n\
    for (int i = 0; i < 16; i++) \n\
        rgba += v[i] * tab1y[(i >> 2)] * tab1x[(i & 3)]; \n\
    store_rgba(rgba, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    if (p.interp_type == INTERPOLATE_NEAREST) \n\
        warpAffine_nearest(); \n\
    else if (p.interp_type == INTERPOLATE_BILINEAR) \n\
        warpAffine_linear(); \n\
    else if (p.interp_type == INTERPOLATE_BICUBIC) \n\
        warpAffine_cubic(); \n\
    else \n\
        warpAffine_nearest(); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer transform_m { float transform_matrix[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
WARP_AFFINE_NEAREST
WARP_AFFINE_LINEAR
WARP_AFFINE_CUBIC
SHADER_MAIN
;
