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
    \n\
    int FadeInSecond; \n\
    int ReverseEffect; \n\
    int ReverseRotation; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
#define _TWOPI 6.283185307179586476925286766559 \n\
const float ratio = 1.0f; \n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 iResolution = vec2(ratio, 1.0); \n\
    float t = p.ReverseEffect == 1 ? 1.0 - p.progress : p.progress; \n\
    float theta = p.ReverseRotation == 1 ? _TWOPI * t : -_TWOPI * t; \n\
    float c1 = cos(theta); \n\
    float s1 = sin(theta); \n\
    float rad = max(0.00001, 1.0 - t); \n\
    float xc1 = (uv.x - 0.5) * iResolution.x; \n\
    float yc1 = (uv.y - 0.5) * iResolution.y; \n\
    float xc2 = (xc1 * c1 - yc1 * s1) / rad; \n\
    float yc2 = (xc1 * s1 + yc1 * c1) / rad; \n\
    vec2 uv2 = vec2(xc2 + iResolution.x / 2.0, yc2 + iResolution.y / 2.0); \n\
    sfpvec4 col3; \n\
    sfpvec4 rgba_from = load_rgba(int(uv.x * (p.w - 1)), int((1.f - uv.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_to = load_rgba_src2(int(uv.x * (p.w2 - 1)), int((1.f - uv.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_from_2 = load_rgba(int(uv2.x * (p.w - 1)), int((1.f - uv2.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_to_2 = load_rgba_src2(int(uv2.x * (p.w2 - 1)), int((1.f - uv2.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 ColorTo = p.ReverseEffect == 1 ? rgba_from : rgba_to; \n\
    if ((uv2.x >= 0.0) && (uv2.x <= iResolution.x) && (uv2.y >= 0.0) && (uv2.y <= iResolution.y)) \n\
	{ \n\
        uv2 /= iResolution; \n\
        col3 = p.ReverseEffect == 1 ? rgba_to_2 : rgba_from_2; \n\
	} \n\
    else \n\
    { \n\
        col3 = p.FadeInSecond == 1 ? sfpvec4(sfp(0.0), sfp(0.0), sfp(0.0), sfp(1.0)) : ColorTo; \n\
    } \n\
    return (sfp(1.0 - t) * col3 + sfp(t) * ColorTo); // could have used mix \n\
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

static const char RotateScaleVanish_data[] = 
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
