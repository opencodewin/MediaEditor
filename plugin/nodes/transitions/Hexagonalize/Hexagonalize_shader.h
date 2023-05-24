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
    float horizontalHexagons; \n\
    int steps; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const float ratio = 2.0f; \n\
struct Hexagon \n\
{ \n\
    float q; \n\
    float r; \n\
    float s; \n\
}; \n\
\n\
Hexagon createHexagon(float q, float r) \n\
{ \n\
    Hexagon hex; \n\
    hex.q = q; \n\
    hex.r = r; \n\
    hex.s = -q - r; \n\
    return hex; \n\
} \n\
\n\
Hexagon roundHexagon(Hexagon hex) \n\
{ \n\
    float q = floor(hex.q + 0.5); \n\
    float r = floor(hex.r + 0.5); \n\
    float s = floor(hex.s + 0.5); \n\
\n\
    float deltaQ = abs(q - hex.q); \n\
    float deltaR = abs(r - hex.r); \n\
    float deltaS = abs(s - hex.s); \n\
\n\
    if (deltaQ > deltaR && deltaQ > deltaS) \n\
        q = -r - s; \n\
    else if (deltaR > deltaS) \n\
        r = -q - s; \n\
    else \n\
        s = -q - r; \n\
\n\
    return createHexagon(q, r); \n\
} \n\
\n\
Hexagon hexagonFromPoint(vec2 point, float size) \n\
{ \n\
    point.y /= ratio; \n\
    point = (point - 0.5) / size; \n\
\n\
    float q = (sqrt(3.0) / 3.0) * point.x + (-1.0 / 3.0) * point.y; \n\
    float r = 0.0 * point.x + 2.0 / 3.0 * point.y; \n\
\n\
    Hexagon hex = createHexagon(q, r); \n\
    return roundHexagon(hex); \n\
} \n\
\n\
vec2 pointFromHexagon(Hexagon hex, float size) \n\
{ \n\
    float x = (sqrt(3.0) * hex.q + (sqrt(3.0) / 2.0) * hex.r) * size + 0.5; \n\
    float y = (0.0 * hex.q + (3.0 / 2.0) * hex.r) * size + 0.5; \n\
\n\
    return vec2(x, y * ratio); \n\
} \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    float dist = 2.0 * min(p.progress, 1.0 - p.progress); \n\
    dist = p.steps > 0 ? ceil(dist * float(p.steps)) / float(p.steps) : dist; \n\
\n\
    float size = (sqrt(3.0) / 3.0) * dist / p.horizontalHexagons; \n\
\n\
    vec2 point = dist > 0.0 ? pointFromHexagon(hexagonFromPoint(uv, size), size) : uv; \n\
    point = clamp(point, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    sfpvec4 rgba_to = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_from = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
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

static const char Hexagonalize_data[] = 
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
