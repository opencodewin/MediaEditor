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
    float red_shadow; \n\
    float green_shadows; \n\
    float blue_shadows; \n\
    \n\
    float red_midtones; \n\
    float green_midtones; \n\
    float blue_midtones; \n\
    \n\
    float red_highlights; \n\
    float green_highlights; \n\
    float blue_highlights; \n\
    \n\
    int preserve_lightness; \n\
} p; \n\
"

#define SHADER_MAIN \
" \n\
sfp hfun(sfp n, sfp h, sfp s, sfp l) \n\
{ \n\
    sfp a = s * min(l, sfp(1.f) - l); \n\
    sfp k = mod(n + h / sfp(30.f), sfp(12.f)); \n\
    \n\
    return clamp(l - a * max(min(k - sfp(3.f), min(sfp(9.f) - k, sfp(1.0))), sfp(-1.f)), sfp(0.f), sfp(1.f)); \n\
} \n\
\n\
sfpvec3 preservel(sfpvec3 rgb, sfp l) \n\
{ \n\
    sfpvec3 val = sfpvec3(0.f); \n\
    sfp vmax = max(rgb.r, max(rgb.g, rgb.b)); \n\
    sfp vmin = min(rgb.r, min(rgb.g, rgb.b)); \n\
    sfp h, s; \n\
    \n\
    l *= sfp(0.5f); \n\
    \n\
    if (rgb.r == rgb.g && rgb.g == rgb.b) \n\
    { \n\
        h = sfp(0.f); \n\
    } \n\
    else if (vmax == rgb.r) \n\
    { \n\
        h = sfp(60.f) * (sfp(0.f) + (rgb.g - rgb.b) / (vmax - vmin)); \n\
    } \n\
    else if (vmax == rgb.g) \n\
    { \n\
        h = sfp(60.f) * (sfp(2.f) + (rgb.b - rgb.r) / (vmax - vmin)); \n\
    } \n\
    else if (vmax == rgb.b) \n\
    { \n\
        h = sfp(60.f) * (sfp(4.f) + (rgb.r - rgb.g) / (vmax - vmin)); \n\
    } \n\
    else \n\
    { \n\
        h = sfp(0.f); \n\
    } \n\
    if (h < sfp(0.f)) \n\
        h += sfp(360.f); \n\
    \n\
    if (vmax == 1.f || vmin == 0.f) \n\
    { \n\
        s = sfp(0.f); \n\
    } \n\
    else \n\
    { \n\
        s = (vmax - vmin) / (sfp(1.f) - (abs(sfp(2.f) * l - sfp(1.f)))); \n\
    } \n\
    \n\
    val.r = hfun(sfp(0.f), h, s, l); \n\
    val.g = hfun(sfp(8.f), h, s, l); \n\
    val.b = hfun(sfp(4.f), h, s, l); \n\
    return val; \n\
} \n\
\n\
sfp get_component(sfp v, sfp l, sfp s, sfp m, sfp h) \n\
{ \n\
    sfp a = sfp(4.f), b = sfp(0.333f), scale = sfp(0.7f); \n\
    \n\
    s *= clamp((b - l) * a + sfp(0.5f), sfp(0.f), sfp(1.f)) * scale; \n\
    m *= clamp((l - b) * a + sfp(0.5f), sfp(0.f), sfp(1.f)) * clamp((sfp(1.f) - l - b) * a + sfp(0.5f), sfp(0.f), sfp(1.f)) * scale; \n\
    h *= clamp((l + b - sfp(1.f)) * a + sfp(0.5f), sfp(0.f), sfp(1.f)) * scale; \n\
    \n\
    v += s; \n\
    v += m; \n\
    v += h; \n\
    \n\
    return clamp(v, sfp(0.f), sfp(1.f)); \n\
} \n\
\n\
sfpvec3 balance(sfpvec3 rgb) \n\
{ \n\
    sfpvec3 value = sfpvec3(0.f); \n\
    sfp l = max(rgb.r, max(rgb.g, rgb.b)) + min(rgb.r, min(rgb.g, rgb.b)); \n\
    value.r = get_component(rgb.r, l, sfp(p.red_shadow),    sfp(p.red_midtones),    sfp(p.red_highlights)); \n\
    value.g = get_component(rgb.g, l, sfp(p.green_shadows), sfp(p.green_midtones),  sfp(p.green_highlights)); \n\
    value.b = get_component(rgb.b, l, sfp(p.blue_shadows),  sfp(p.blue_midtones),   sfp(p.blue_highlights)); \n\
    if (p.preserve_lightness == 1) \n\
        value = preservel(value, l); \n\
    return clamp(value, sfp(0.f), sfp(1.f)); \n\
} \n\
\n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 color = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 result = balance(color.rgb); \n\
    store_rgba(sfpvec4(result, color.a), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
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
