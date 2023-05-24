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
    float rotations; \n\
    float scale; \n\
    \n\
    float red_back; \n\
    float green_back; \n\
    float blue_back; \n\
    float alpha_back; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
#define PI 3.14159265359 \n\
vec2 center = vec2(0.5, 0.5); \n\
sfpvec4 bgcolor = sfpvec4(sfp(p.red_back), sfp(p.green_back), sfp(p.blue_back), sfp(p.alpha_back)); \n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 difference = uv - center; \n\
    vec2 dir = normalize(difference); \n\
    float dist = length(difference); \n\
\n\
    float angle = 2.0 * PI * p.rotations * p.progress; \n\
\n\
    float c = cos(angle); \n\
    float s = sin(angle); \n\
\n\
    float currentScale = mix(p.scale, 1.0, 2.0 * abs(p.progress - 0.5)); \n\
\n\
    vec2 rotatedDir = vec2(dir.x  * c - dir.y * s, dir.x * s + dir.y * c); \n\
    vec2 rotatedUv = center + rotatedDir * dist / currentScale; \n\
\n\
    if (rotatedUv.x < 0.0 || rotatedUv.x > 1.0 || \n\
        rotatedUv.y < 0.0 || rotatedUv.y > 1.0) \n\
    return bgcolor; \n\
\n\
    rotatedUv = clamp(rotatedUv, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    sfpvec4 rgba_to = load_rgba_src2(int(rotatedUv.x * (p.w2 - 1)), int((1.f - rotatedUv.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_from = load_rgba(int(rotatedUv.x * (p.w - 1)), int((1.f - rotatedUv.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
\n\
    return mix(rgba_from, rgba_to, sfp(p.progress)); \n\
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

static const char RotateScale_data[] = 
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
