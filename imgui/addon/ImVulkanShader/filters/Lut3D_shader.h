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
    int interp_type; \n\
    int lut_size; \n\
} p; \
"

#define SHADER_LUT3D_NEARSET \
" \n\
sfpvec3 interp_nearest(sfpvec3 s) \n\
{ \n\
    int lutsize = p.lut_size; \n\
    s = s * sfp(lutsize - 1); \n\
    int offset = (int(s.r + .5) * lutsize + int(s.g + .5)) * lutsize + int(s.b + .5); \n\
    vec4 c = table_blob_data[offset]; \n\
    return sfpvec3(sfp(c.r), sfp(c.g), sfp(c.b)); \n\
} \
"

#define SHADER_LUT3D_TRILINEAR \
" \n\
sfp lerpf(sfp v0, sfp v1, sfp f) \n\
{ \n\
    return v0 + (v1 - v0) * f; \n\
} \n\
\n\
sfpvec4 lerp(const sfpvec4 v0, const sfpvec4 v1, sfp f) \n\
{ \n\
    sfpvec4 v = {lerpf(v0.r, v1.r, f), lerpf(v0.g, v1.g, f), lerpf(v0.b, v1.b, f), sfp(1.0f)}; \n\
    return v; \n\
} \n\
\n\
sfpvec3 interp_trilinear(sfpvec3 s) \n\
{ \n\
    int lutsize = p.lut_size; \n\
    s = s * sfp(lutsize - 1); \n\
    const int prev[] = {int(s.r), int(s.g), int(s.b)}; \n\
    const int next[] = {(int(s.r) + 1 > lutsize - 1) ? lutsize - 1 : int(s.r) + 1, \n\
                        (int(s.g) + 1 > lutsize - 1) ? lutsize - 1 : int(s.g) + 1, \n\
                        (int(s.b) + 1 > lutsize - 1) ? lutsize - 1 : int(s.b) + 1}; \n\
    const sfpvec3 d = {s.r - sfp(prev[0]), s.g - sfp(prev[1]), s.b - sfp(prev[2])}; \n\
    int index[8]; \n\
    sfpvec4 cccc[8]; \n\
    int p02 = prev[0] * lutsize * lutsize; \n\
    int n02 = next[0] * lutsize * lutsize; \n\
    int p10 = prev[1] * lutsize; \n\
    int n10 = next[1] * lutsize; \n\
\n\
    index[0] = p02 + p10 + prev[2]; \n\
    index[1] = p02 + p10 + next[2]; \n\
    index[2] = p02 + n10 + prev[2]; \n\
    index[3] = p02 + n10 + next[2]; \n\
    index[4] = n02 + p10 + prev[2]; \n\
    index[5] = n02 + p10 + next[2]; \n\
    index[6] = n02 + n10 + prev[2]; \n\
    index[7] = n02 + n10 + next[2]; \n\
\n\
    cccc[0] = sfpvec4(table_blob_data[index[0]]); \n\
    cccc[1] = sfpvec4(table_blob_data[index[1]]); \n\
    cccc[2] = sfpvec4(table_blob_data[index[2]]); \n\
    cccc[3] = sfpvec4(table_blob_data[index[3]]); \n\
    cccc[4] = sfpvec4(table_blob_data[index[4]]); \n\
    cccc[5] = sfpvec4(table_blob_data[index[5]]); \n\
    cccc[6] = sfpvec4(table_blob_data[index[6]]); \n\
    cccc[7] = sfpvec4(table_blob_data[index[7]]); \n\
\n\
    const sfpvec4 c00 = lerp(cccc[0], cccc[4], d.r); \n\
    const sfpvec4 c10 = lerp(cccc[2], cccc[6], d.r); \n\
    const sfpvec4 c01 = lerp(cccc[1], cccc[5], d.r); \n\
    const sfpvec4 c11 = lerp(cccc[3], cccc[7], d.r); \n\
    const sfpvec4 c0 = lerp(c00, c10, d.g); \n\
    const sfpvec4 c1 = lerp(c01, c11, d.g); \n\
    sfpvec4 c = lerp(c0, c1, d.b); \n\
\n\
    return sfpvec3(c.r, c.g, c.b); \n\
} \
"

#define SHADER_LUT3D_TETRAHEDRAL \
" \n\
sfpvec3 interp_tetrahedral(sfpvec3 s) \n\
{ \n\
    int lutsize = p.lut_size; \n\
    s = s * sfp(lutsize - 1); \n\
    const int prev[] = {int(s.r), int(s.g), int(s.b)}; \n\
    const int next[] = {(int(s.r) + 1 > lutsize - 1) ? lutsize - 1 : int(s.r) + 1, \n\
                        (int(s.g) + 1 > lutsize - 1) ? lutsize - 1 : int(s.g) + 1, \n\
                        (int(s.b) + 1 > lutsize - 1) ? lutsize - 1 : int(s.b) + 1}; \n\
    const sfpvec3 d = {s.r - sfp(prev[0]), s.g - sfp(prev[1]), s.b - sfp(prev[2])}; \n\
    sfpvec4 cccc[4]; \n\
    int p02 = prev[0] * lutsize * lutsize; \n\
    int n02 = next[0] * lutsize * lutsize; \n\
    int p10 = prev[1] * lutsize; \n\
    int n10 = next[1] * lutsize; \n\
\n\
    sfp one_sub_r = sfp(1.0f) - d.r; \n\
    sfp one_sub_b = sfp(1.0f) - d.b; \n\
    sfp one_sub_g = sfp(1.0f) - d.g; \n\
    sfp r_sub_g = d.r - d.g; \n\
    sfp r_sub_b = d.r - d.b; \n\
    sfp g_sub_b = d.g - d.b; \n\
    sfp g_sub_r = d.g - d.r; \n\
    sfp b_sub_g = d.b - d.g; \n\
    sfp b_sub_r = d.b - d.r; \n\
\n\
    sfpvec3 c; \n\
    if (d.r > d.g) \n\
    { \n\
        if (d.g > d.b)  \n\
        { \n\
            cccc[0] = sfpvec4(table_blob_data[p02 + p10 + prev[2]]); \n\
            cccc[1] = sfpvec4(table_blob_data[n02 + p10 + prev[2]]); \n\
            cccc[2] = sfpvec4(table_blob_data[n02 + n10 + prev[2]]); \n\
            cccc[3] = sfpvec4(table_blob_data[n02 + n10 + next[2]]); \n\
            c.r = one_sub_r * cccc[0].r + r_sub_g * cccc[1].r + g_sub_b * cccc[2].r + (d.b) * cccc[3].r; \n\
            c.g = one_sub_r * cccc[0].g + r_sub_g * cccc[1].g + g_sub_b * cccc[2].g + (d.b) * cccc[3].g; \n\
            c.b = one_sub_r * cccc[0].b + r_sub_g * cccc[1].b + g_sub_b * cccc[2].b + (d.b) * cccc[3].b; \n\
        } \n\
        else if (d.r > d.b) \n\
        { \n\
            cccc[0] = sfpvec4(table_blob_data[p02 + p10 + prev[2]]); \n\
            cccc[1] = sfpvec4(table_blob_data[n02 + p10 + prev[2]]); \n\
            cccc[2] = sfpvec4(table_blob_data[n02 + p10 + next[2]]); \n\
            cccc[3] = sfpvec4(table_blob_data[n02 + n10 + next[2]]); \n\
            c.r = one_sub_r * cccc[0].r + r_sub_b * cccc[1].r + b_sub_g * cccc[2].r + (d.g) * cccc[3].r; \n\
            c.g = one_sub_r * cccc[0].g + r_sub_b * cccc[1].g + b_sub_g * cccc[2].g + (d.g) * cccc[3].g; \n\
            c.b = one_sub_r * cccc[0].b + r_sub_b * cccc[1].b + b_sub_g * cccc[2].b + (d.g) * cccc[3].b; \n\
        } \n\
        else \n\
        { \n\
            cccc[0] = sfpvec4(table_blob_data[p02 + p10 + prev[2]]); \n\
            cccc[1] = sfpvec4(table_blob_data[p02 + p10 + next[2]]); \n\
            cccc[2] = sfpvec4(table_blob_data[n02 + p10 + next[2]]); \n\
            cccc[3] = sfpvec4(table_blob_data[n02 + n10 + next[2]]); \n\
            c.r = one_sub_b * cccc[0].r + b_sub_r * cccc[1].r + r_sub_g * cccc[2].r + (d.g) * cccc[3].r; \n\
            c.g = one_sub_b * cccc[0].g + b_sub_r * cccc[1].g + r_sub_g * cccc[2].g + (d.g) * cccc[3].g; \n\
            c.b = one_sub_b * cccc[0].b + b_sub_r * cccc[1].b + r_sub_g * cccc[2].b + (d.g) * cccc[3].b; \n\
        } \n\
    } \n\
    else \n\
    { \n\
        if (d.b > d.g) \n\
        { \n\
            cccc[0] = sfpvec4(table_blob_data[p02 + p10 + prev[2]]); \n\
            cccc[1] = sfpvec4(table_blob_data[p02 + p10 + next[2]]); \n\
            cccc[2] = sfpvec4(table_blob_data[p02 + n10 + next[2]]); \n\
            cccc[3] = sfpvec4(table_blob_data[n02 + n10 + next[2]]); \n\
            c.r = one_sub_b * cccc[0].r + b_sub_g * cccc[1].r + g_sub_r * cccc[2].r + (d.r) * cccc[3].r; \n\
            c.g = one_sub_b * cccc[0].g + b_sub_g * cccc[1].g + g_sub_r * cccc[2].g + (d.r) * cccc[3].g; \n\
            c.b = one_sub_b * cccc[0].b + b_sub_g * cccc[1].b + g_sub_r * cccc[2].b + (d.r) * cccc[3].b; \n\
        } \n\
        else if (d.b > d.r) \n\
        { \n\
            cccc[0] = sfpvec4(table_blob_data[p02 + p10 + prev[2]]); \n\
            cccc[1] = sfpvec4(table_blob_data[p02 + n10 + prev[2]]); \n\
            cccc[2] = sfpvec4(table_blob_data[p02 + n10 + next[2]]); \n\
            cccc[3] = sfpvec4(table_blob_data[n02 + n10 + next[2]]); \n\
            c.r = one_sub_g * cccc[0].r + g_sub_b * cccc[1].r + b_sub_r * cccc[2].r + (d.r) * cccc[3].r; \n\
            c.g = one_sub_g * cccc[0].g + g_sub_b * cccc[1].g + b_sub_r * cccc[2].g + (d.r) * cccc[3].g; \n\
            c.b = one_sub_g * cccc[0].b + g_sub_b * cccc[1].b + b_sub_r * cccc[2].b + (d.r) * cccc[3].b; \n\
        } \n\
        else \n\
        { \n\
            cccc[0] = sfpvec4(table_blob_data[p02 + p10 + prev[2]]); \n\
            cccc[1] = sfpvec4(table_blob_data[p02 + n10 + prev[2]]); \n\
            cccc[2] = sfpvec4(table_blob_data[n02 + n10 + prev[2]]); \n\
            cccc[3] = sfpvec4(table_blob_data[n02 + n10 + next[2]]); \n\
            c.r = one_sub_g * cccc[0].r + g_sub_r * cccc[1].r + r_sub_b * cccc[2].r + (d.b) * cccc[3].r; \n\
            c.g = one_sub_g * cccc[0].g + g_sub_r * cccc[1].g + r_sub_b * cccc[2].g + (d.b) * cccc[3].g; \n\
            c.b = one_sub_g * cccc[0].b + g_sub_r * cccc[1].b + r_sub_b * cccc[2].b + (d.b) * cccc[3].b; \n\
        } \n\
    } \n\
    return c; \n\
} \
"

#define SHADER_LUT3D \
" \n\
sfpvec3 rgb_lut3d(sfpvec3 rgb) \n\
{ \n\
    if (p.interp_type == INTERPOLATE_NEAREST) \n\
        return interp_nearest(rgb); \n\
    else if (p.interp_type == INTERPOLATE_TRILINEAR) \n\
        return interp_trilinear(rgb); \n\
    else if (p.interp_type == INTERPOLATE_TETRAHEDRAL) \n\
        return interp_tetrahedral(rgb); \n\
    else \n\
        return rgb; \n\
} \
"

#define SHADER_LUT3D_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec3 rgb_in = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec4 result = sfpvec4(rgb_lut3d(rgb_in), sfp(1.0f)); \n\
    store_rgba(result, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char LUT3D_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer table_blob { vec4 table_blob_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_LUT3D_NEARSET
SHADER_LUT3D_TRILINEAR
SHADER_LUT3D_TETRAHEDRAL
SHADER_LUT3D
SHADER_LUT3D_MAIN
;
