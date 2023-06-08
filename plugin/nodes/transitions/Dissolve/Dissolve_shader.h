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
    float line_width; \n\
    float pow; \n\
    float intensity; \n\
    \n\
    float red_spread; \n\
    float green_spread; \n\
    float blue_spread; \n\
    float alpha_spread; \n\
    \n\
    float red_hot; \n\
    float green_hot; \n\
    float blue_hot; \n\
    float alpha_hot; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
sfpvec4 spread_colour = sfpvec4(sfp(p.red_spread), sfp(p.green_spread), sfp(p.blue_spread), sfp(p.alpha_spread)); \n\
sfpvec4 hot_colour = sfpvec4(sfp(p.red_hot), sfp(p.green_hot), sfp(p.blue_hot), sfp(p.alpha_hot)); \n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    sfpvec4 cfrom = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 cto = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 result; \n\
    sfp burn = sfp(0.3) + sfp(0.7) * (sfp(0.299) * cfrom.r + sfp(0.587) * cfrom.g + sfp(0.114) * cfrom.b); \n\
    sfp show = burn - sfp(p.progress); \n\
    if (show < sfp(0.0005)) \n\
    { \n\
        result = cto; \n\
    } \n\
    else \n\
    { \n\
        sfp factor = sfp(1.0) - smoothstep(sfp(0.0), sfp(p.line_width), show); \n\
        sfpvec3 burnColor = mix(spread_colour.rgb, hot_colour.rgb, factor); \n\
        burnColor = pow(burnColor, sfpvec3(p.pow)) * sfp(p.intensity); \n\
        sfpvec3 finalRGB = mix(cfrom.rgb, burnColor, factor * sfp(step(0.0001, p.progress))); \n\
        result = sfpvec4(finalRGB * cfrom.a, cfrom.a); \n\
    } \n\
    return result; \n\
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

static const char Dissolve_data[] = 
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
