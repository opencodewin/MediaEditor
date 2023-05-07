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
} p; \
"

#define SHADER_DEINTERLACE \
" \n\
void deinterlace(int x, int y) \n\
{ \n\
    sfpvec4 d0 = load_rgba(x, y + 0, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    if (y % 2 == 0 || y == p.out_h -1) \n\
    { \n\
        store_rgba(d0, x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
    else \n\
    { \n\
        sfpvec4 m1 = load_rgba(x, y < 2 ? y - 1 : y - 2, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfpvec4 s0 = load_rgba(x, y - 1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfpvec4 p1 = load_rgba(x, y + 1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfpvec4 p2 = load_rgba(x, y + 2, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfpvec4 sum = sfpvec4(0); \n\
        sum -= m1; \n\
        sum += s0 * sfpvec4(4.0); \n\
        sum += d0 * sfpvec4(2.0); \n\
        sum += p1 * sfpvec4(4.0); \n\
        sum -= p2; \n\
        sum += sfpvec4(4.0/256.0); \n\
        sum /= sfpvec4(8.0); \n\
        sfpvec4 cm; \n\
        cm.r = sfp(table_data[int(sum.r * sfp(256.0)) + 1024]); \n\
        cm.g = sfp(table_data[int(sum.g * sfp(256.0)) + 1024]); \n\
        cm.b = sfp(table_data[int(sum.b * sfp(256.0)) + 1024]); \n\
        cm.a = d0.a; \n\
        store_rgba(cm, x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \
"

#define SHADER_DEINTERLACE_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    deinterlace(gx, gy); \n\
} \
"

static const char DeInterlace_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer table { float table_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_DEINTERLACE
SHADER_DEINTERLACE_MAIN
;