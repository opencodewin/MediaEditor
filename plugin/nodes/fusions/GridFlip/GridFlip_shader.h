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
    float red_back; \n\
    float green_back; \n\
    float blue_back; \n\
    float alpha_back; \n\
    \n\
    float pause; \n\
    float dividerWidth; \n\
    float randomness; \n\
    int size_w; \n\
    int size_h; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
vec2 size = vec2(p.size_w, p.size_h); \n\
sfpvec4 bgcolor = sfpvec4(sfp(p.red_back), sfp(p.green_back), sfp(p.blue_back), sfp(p.alpha_back)); \n\
float rand (vec2 co) \n\
{ \n\
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); \n\
} \n\
\n\
float getDelta(vec2 point) \n\
{ \n\
    vec2 rectanglePos = floor(size * point); \n\
    vec2 rectangleSize = vec2(1.0 / size.x, 1.0 / size.y); \n\
    float top = rectangleSize.y * (rectanglePos.y + 1.0); \n\
    float bottom = rectangleSize.y * rectanglePos.y; \n\
    float left = rectangleSize.x * rectanglePos.x; \n\
    float right = rectangleSize.x * (rectanglePos.x + 1.0); \n\
    float minX = min(abs(point.x - left), abs(point.x - right)); \n\
    float minY = min(abs(point.y - top), abs(point.y - bottom)); \n\
    return min(minX, minY); \n\
} \n\
\n\
float getDividerSize() \n\
{ \n\
    vec2 rectangleSize = vec2(1.0 / size.x, 1.0 / size.y); \n\
    return min(rectangleSize.x, rectangleSize.y) * p.dividerWidth; \n\
} \n\
\n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    sfpvec4 rgba_to = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_from = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    if (p.progress < p.pause) \n\
    { \n\
        float currentProg = p.progress / p.pause; \n\
        float a = 1.0; \n\
        if (getDelta(point) < getDividerSize()) \n\
        { \n\
            a = 1.0 - currentProg; \n\
        } \n\
        return mix(bgcolor, rgba_from, sfp(a)); \n\
    } \n\
    else if (p.progress < 1.0 - p.pause) \n\
    { \n\
        if (getDelta(point) < getDividerSize()) \n\
        { \n\
            return bgcolor; \n\
        } \n\
        else \n\
        { \n\
            float currentProg = (p.progress - p.pause) / (1.0 - p.pause * 2.0); \n\
            vec2 q = point; \n\
            vec2 rectanglePos = floor(size * q); \n\
\n\
            float r = rand(rectanglePos) - p.randomness; \n\
            float cp = smoothstep(0.0, 1.0 - r, currentProg); \n\
\n\
            float rectangleSize = 1.0 / size.x; \n\
            float delta = rectanglePos.x * rectangleSize; \n\
            float offset = rectangleSize / 2.0 + delta; \n\
\n\
            point.x = (point.x - offset) / abs(cp - 0.5) * 0.5 + offset; \n\
            point = clamp(point, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
            sfpvec4 a = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            sfpvec4 b = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
\n\
            float s = step(abs(size.x * (q.x - delta) - 0.5), abs(cp - 0.5)); \n\
            return mix(bgcolor, mix(b, a, sfp(step(cp, 0.5))), sfp(s)); \n\
        } \n\
    } \n\
    else \n\
    { \n\
        float currentProg = (p.progress - 1.0 + p.pause) / p.pause; \n\
        float a = 1.0; \n\
        if (getDelta(point) < getDividerSize()) \n\
        { \n\
            a = currentProg; \n\
        } \n\
\n\
        return mix(bgcolor, rgba_to, sfp(a)); \n\
    } \n\
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

static const char GridFlip_data[] = 
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
