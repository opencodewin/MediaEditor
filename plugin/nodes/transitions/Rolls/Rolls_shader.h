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
    int RotDown; \n\
    int type; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const float ratio = 2.0f; \n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    float theta, c1, s1; \n\
    vec2 iResolution = vec2(ratio, 1.0); \n\
    vec2 uvi; \n\
    // I used if/else instead of switch in case it's an old GPU \n\
    if (     p.type == 0) { theta = (p.RotDown == 1 ? M_PI : -M_PI) / 2.0 * p.progress; uvi.x = 1.0 - uv.x; uvi.y = uv.y; } \n\
    else if (p.type == 1) { theta = (p.RotDown == 1 ? M_PI : -M_PI) / 2.0 * p.progress; uvi = uv; } \n\
    else if (p.type == 2) { theta = (p.RotDown == 1 ? -M_PI : M_PI) / 2.0 * p.progress; uvi.x = uv.x; uvi.y = 1.0 - uv.y; } \n\
    else if (p.type == 3) { theta = (p.RotDown == 1 ? -M_PI : M_PI) / 2.0 * p.progress; uvi = 1.0 - uv; } \n\
    c1 = cos(theta); s1 = sin(theta); \n\
    vec2 uv2; \n\
    uv2.x = (uvi.x * iResolution.x * c1 - uvi.y * iResolution.y * s1); \n\
    uv2.y = (uvi.x * iResolution.x * s1 + uvi.y * iResolution.y * c1); \n\
    if ((uv2.x >= 0.0) && (uv2.x <= iResolution.x) && (uv2.y >= 0.0) && (uv2.y <= iResolution.y)) \n\
    { \n\
        uv2 /= iResolution; \n\
        if (     p.type == 0) { uv2.x = 1.0 - uv2.x; } \n\
        else if (p.type == 2) { uv2.y = 1.0 - uv2.y; } \n\
        else if (p.type == 3) { uv2 = 1.0 - uv2; } \n\
        return ( \n\
            load_rgba(int(uv2.x * (p.w - 1)), int((1.f - uv2.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type) \n\
        ); \n\
    } \n\
    return ( \n\
        load_rgba_src2(int(uv.x * (p.w2 - 1)), int((1.f - uv.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2) \n\
    ); \n\
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

static const char Rolls_data[] = 
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
