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
} p; \
"

#define SHADER_SHARPEN \
" \n\
#define MAX3(a,b,c) max(max(a,b),c) \n\
#define MIN3(a,b,c) min(min(a,b),c) \n\
sfpvec4 sharpen(int x, int y) \n\
{ \n\
    sfpvec4 rgba = sfpvec4(0.f); \n\
    int y0 = max(y - 1, 0); \n\
    int y1 = min(y + 1, p.h - 1); \n\
    int x0 = max(x - 1, 0); \n\
    int x1 = min(x + 1, p.w - 1); \n\
    sfpvec4 current = load_rgba(x , y , p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
\n\
    sfpvec3 a = load_rgba(x0, y0, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 b = load_rgba(x , y0, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 c = load_rgba(x1, y0, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 d = load_rgba(x0, y , p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 e = current.rgb; \n\
    sfpvec3 f = load_rgba(x1, y , p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 g = load_rgba(x0, y1, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 h = load_rgba(x , y1, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 i = load_rgba(x1, y1, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
\n\
    sfp mn, mn2, mx, mx2; \n\
    sfp amp, weight; \n\
    /* R */ \n\
    { \n\
        mn  = MIN3(MIN3( d.x, e.x, f.x), b.x, h.x); \n\
        mn2 = MIN3(MIN3(mn  , a.x, c.x), g.x, i.x); \n\
        mn = mn + mn2; \n\
\n\
        mx  = MAX3(MAX3( d.x, e.x, f.x), b.x, h.x); \n\
        mx2 = MAX3(MAX3(mx  , a.x, c.x), g.x, i.x); \n\
\n\
        mx = mx + mx2; \n\
\n\
        amp = sqrt(clamp(min(mn, sfp(2.0f) - mx) / mx, sfp(0.f), sfp(1.f))); \n\
        weight = amp / sfp(p.strength); \n\
        rgba.r = ((b.x + d.x + f.x + h.x) * weight + e.x) / (sfp(1.f) + sfp(4.f) * weight); \n\
    } \n\
    /* G */ \n\
    { \n\
        mn  = MIN3(MIN3( d.y, e.y, f.y), b.y, h.y); \n\
        mn2 = MIN3(MIN3(mn  , a.y, c.y), g.y, i.y); \n\
        mn = mn + mn2; \n\
\n\
        mx  = MAX3(MAX3( d.y, e.y, f.y), b.y, h.y); \n\
        mx2 = MAX3(MAX3(mx  , a.y, c.y), g.y, i.y); \n\
\n\
        mx = mx + mx2; \n\
\n\
        amp = sqrt(clamp(min(mn, sfp(2.0f) - mx) / mx, sfp(0.f), sfp(1.f))); \n\
        weight = amp / sfp(p.strength); \n\
        rgba.g = ((b.y + d.y + f.y + h.y) * weight + e.y) / (sfp(1.f) + sfp(4.f) * weight); \n\
    } \n\
    /* B */ \n\
    { \n\
        mn  = MIN3(MIN3( d.z, e.z, f.z), b.z, h.z); \n\
        mn2 = MIN3(MIN3(mn  , a.z, c.z), g.z, i.z); \n\
        mn = mn + mn2; \n\
\n\
        mx  = MAX3(MAX3( d.z, e.z, f.z), b.z, h.z); \n\
        mx2 = MAX3(MAX3(mx  , a.z, c.z), g.z, i.z); \n\
\n\
        mx = mx + mx2; \n\
\n\
        amp = sqrt(clamp(min(mn, sfp(2.0f) - mx) / mx, sfp(0.f), sfp(1.f))); \n\
        weight = amp / sfp(p.strength); \n\
        rgba.b = ((b.z + d.z + f.z + h.z) * weight + e.z) / (sfp(1.f) + sfp(4.f) * weight); \n\
    } \n\
\n\
    rgba.a = current.a; \n\
    return rgba; \n\
} \
"

#define SHADER_CAS_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 result = sharpen(gx, gy); \n\
    store_rgba(result, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char CAS_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_SHARPEN
SHADER_CAS_MAIN
;
