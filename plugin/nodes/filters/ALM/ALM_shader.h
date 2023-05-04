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
    float strength; \n\
    float bias; \n\
    float gamma; \n\
} p; \
"

#define SHADER_ALM \
" \n\
sfpmat3 matrix_mat_r2x = { \n\
    {sfp(0.4124f), sfp(0.3576f), sfp(0.1805f)}, \n\
    {sfp(0.2126f), sfp(0.7152f), sfp(0.0722f)}, \n\
    {sfp(0.0193f), sfp(0.1192f), sfp(0.9505f)}, \n\
}; \n\
sfpmat3 matrix_mat_x2r = { \n\
    {sfp( 3.2410f), sfp(-1.5374f), sfp(-0.4986f)}, \n\
    {sfp(-0.9692f), sfp( 1.8760f), sfp( 0.0416f)}, \n\
    {sfp( 0.0556f), sfp(-0.2040f), sfp( 1.0570f)}, \n\
}; \n\
sfpvec3 Transform(sfpvec3 rgb, float gamma) \n\
{ \n\
    sfpvec3 _gamma = {sfp(0.9f / gamma), sfp(0.9f / gamma), sfp(0.9f / gamma)}; \n\
    rgb = sfp(1.099f) * pow(rgb, _gamma) - sfp(0.099f); \n\
    return rgb; \n\
} \n\
\n\
sfpvec3 rgb_to_xyz(sfpvec3 rgb) \n\
{ \n\
    sfpvec3 xyz = rgb * matrix_mat_r2x; \n\
    return xyz; \n\
} \n\
sfpvec3 xyz_to_rgb(sfpvec3 xyz) \n\
{ \n\
    sfpvec3 rgb = xyz * matrix_mat_x2r; \n\
    return clamp(rgb, sfp(0.f), sfp(1.f)); \n\
} \n\
sfpvec4 alm(int x, int y) \n\
{ \n\
    sfpvec4 rgba = load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 xyz = rgb_to_xyz(rgba.rgb); \n\
    sfp xx = xyz.x / (xyz.x + xyz.y + xyz.z); \n\
    sfp yy = xyz.y / (xyz.x + xyz.y + xyz.z); \n\
    sfp _log = log(xyz.y + sfp(1.0f)); \n\
    sfp _pow = pow(xyz.y, log(sfp(p.bias)) / sfp(-0.693147180559945f)); \n\
    sfp log_pow = log(sfp(2.0f) + sfp(8.0f) * _pow); \n\
    xyz.y = sfp(p.strength) * _log / log_pow / sfp(0.301029995663981f); \n\
    xyz.x = xyz.y / yy * xx; \n\
    xyz.z = xyz.y / yy * (sfp(1.0f) - xx - yy); \n\
    sfpvec3 rgb = Transform(xyz_to_rgb(xyz), p.gamma); \n\
    return sfpvec4(rgb, rgba.a); \n\
} \
"

#define SHADER_ALM_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
\n\
    sfpvec4 vout = alm(gx, gy); \n\
    store_rgba(vout, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char ALM_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_ALM
SHADER_ALM_MAIN
;
