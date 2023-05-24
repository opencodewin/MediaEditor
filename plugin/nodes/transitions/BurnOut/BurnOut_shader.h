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
    int w2; \n\
    int h2; \n\
    int cstep2; \n\
    int in_format2; \n\
    int in_type2; \n\
\n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
\n\
    float progress; \n\
    float smoothness; \n\
    \n\
    float red_shadow; \n\
    float green_shadows; \n\
    float blue_shadows; \n\
    float alpha_shadows; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
sfpvec4 shadow_colour = sfpvec4(sfp(p.red_shadow), sfp(p.green_shadows), sfp(p.blue_shadows), sfp(p.alpha_shadows)); \n\
vec2 center = vec2(0.5); \n\
const float PI = 3.14159265358979323846; \n\
float quadraticInOut(float t) \n\
{ \n\
    float point = 2.0 * t * t; \n\
    return t < 0.5 ? point : -point + (4.0 * t) - 1.0; \n\
} \n\
\n\
float getGradient(float r, float dist) \n\
{ \n\
    float d = r - dist; \n\
    return mix( \n\
        smoothstep(-p.smoothness, 0.0, r - dist * (1.0 + p.smoothness)), \n\
        -1.0 - step(0.005, d), \n\
        step(-0.005, d) * step(d, 0.01) \n\
    ); \n\
} \n\
\n\
float getWave(vec2 point) \n\
{ \n\
    vec2 _p = point - center; // offset from center \n\
    float rads = atan(_p.y, _p.x); \n\
    float degs = degrees(rads) + 180.0; \n\
    float ratio = (PI * 30.0) / 360.0; \n\
    degs = degs * ratio; \n\
    float x = p.progress; \n\
    float magnitude = mix(0.02, 0.09, smoothstep(0.0, 1.0, x)); \n\
    float offset = mix(40.0, 30.0, smoothstep(0.0, 1.0, x)); \n\
    float ease_degs = quadraticInOut(sin(degs)); \n\
    float deg_wave_pos = (ease_degs * magnitude) * sin(x * offset); \n\
    return x + deg_wave_pos; \n\
} \n\
\n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    float dist = distance(center, point); \n\
    float m = getGradient(getWave(point), dist); \n\
    sfpvec4 cfrom = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 cto = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    return mix(mix(cfrom, cto, sfp(m)), mix(cfrom, sfpvec4(shadow_colour.rgb, sfp(1.0)), sfp(0.75)), sfp(step(m, -2.0))); \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.out_w - 1), 1.f - float(uv.y) / float(p.out_h - 1)); \n\
    sfpvec4 result = transition(point); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char BurnOut_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer src2_int8       { uint8_t   src2_data_int8[]; };
layout (binding =  9) readonly buffer src2_int16      { uint16_t  src2_data_int16[]; };
layout (binding = 10) readonly buffer src2_float16    { float16_t src2_data_float16[]; };
layout (binding = 11) readonly buffer src2_float32    { float     src2_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_RGBA_NAME(src2)
SHADER_STORE_RGBA
SHADER_MAIN
;
