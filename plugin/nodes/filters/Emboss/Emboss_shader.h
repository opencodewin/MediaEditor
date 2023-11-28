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
    float angle; \n\
    int stride; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp cr = sfp(cos(p.angle)); \n\
    sfp sr = sfp(sin(p.angle)); \n\
    sfpvec2 uv = sfpvec2(sfp(gl_GlobalInvocationID.x), sfp(gl_GlobalInvocationID.y)); \n\
    sfpmat2 mat_rot = sfpmat2(cr, sr, -sr, cr); \n\
    sfpvec2 rot = mat_rot * sfpvec2(sfp(1.0), sfp(0.0)); \n\
    uv = uv + sfp(p.stride) * rot; \n\
    sfpvec4 color = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 tmp = load_rgba(int(uv.x), int(uv.y), p.w, p.h, p.cstep, p.in_format, p.in_type).rgb - color.rgb + sfp(0.5); \n\
    sfp f = (tmp.r + tmp.g + tmp.b) / sfp(3.0); \n\
    sfpvec4 rgba = sfpvec4(mix(color.rgb, sfpvec3(f, f, f), sfp(p.intensity)), color.a); \n\
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
