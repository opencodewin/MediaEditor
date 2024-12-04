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
} p; \
"

#define SHADER_MAIN \
" \n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    vec2 block = floor(point.xy / vec2(16)); \n\
    vec2 uv_noise = block / vec2(64); \n\
    uv_noise += floor(vec2(p.progress) * vec2(1200.0, 3500.0)) / vec2(64); \n\
    vec2 dist = p.progress > 0.0 ? (fract(uv_noise) - 0.5) * 0.3 *(1.0 - p.progress) : vec2(0.0); \n\
    vec2 red = clamp(point + dist * 0.2, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    vec2 green = clamp(point + dist * .3, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    vec2 blue = clamp(point + dist * .5, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
\n\
    sfpvec4 rgba_from_red = load_rgba(int(red.x * (p.w - 1)), int((1.f - red.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_from_green = load_rgba(int(green.x * (p.w - 1)), int((1.f - green.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_from_blue = load_rgba(int(blue.x * (p.w - 1)), int((1.f - blue.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
\n\
    sfpvec4 rgba_to_red = load_rgba_src2(int(red.x * (p.w2 - 1)), int((1.f - red.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_to_green = load_rgba_src2(int(green.x * (p.w2 - 1)), int((1.f - green.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_to_blue = load_rgba_src2(int(blue.x * (p.w2 - 1)), int((1.f - blue.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
\n\
    return sfpvec4(mix(rgba_from_red, rgba_to_red, sfp(p.progress)).r, mix(rgba_from_green, rgba_to_green, sfp(p.progress)).g, mix(rgba_from_blue, rgba_to_blue, sfp(p.progress)).b, sfp(1.0)); \n\
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

static const char GlitchMemories_data[] = 
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
