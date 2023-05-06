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
    float threshold; \n\
    int direction; \n\
    int above; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float rand(vec2 co) \n\
{ \n\
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); \n\
} \n\
\n\
vec3 mod289(vec3 x) \n\
{ \n\
    return x - floor(x * (1.0 / 289.0)) * 289.0; \n\
} \n\
\n\
vec2 mod289(vec2 x) \n\
{ \n\
    return x - floor(x * (1.0 / 289.0)) * 289.0; \n\
} \n\
\n\
vec3 permute(vec3 x) \n\
{ \n\
    return mod289(((x*34.0)+1.0)*x); \n\
} \n\
\n\
float snoise(vec2 v) \n\
{ \n\
    const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0 \n\
                        0.366025403784439,  // 0.5*(sqrt(3.0)-1.0) \n\
                       -0.577350269189626,  // -1.0 + 2.0 * C.x \n\
                        0.024390243902439); // 1.0 / 41.0 \n\
    // First corner \n\
    vec2 i  = floor(v + dot(v, C.yy) ); \n\
    vec2 x0 = v -   i + dot(i, C.xx); \n\
\n\
    // Other corners \n\
    vec2 i1; \n\
    //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0 \n\
    //i1.y = 1.0 - i1.x; \n\
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0); \n\
    // x0 = x0 - 0.0 + 0.0 * C.xx ; \n\
    // x1 = x0 - i1 + 1.0 * C.xx ; \n\
    // x2 = x0 - 1.0 + 2.0 * C.xx ; \n\
    vec4 x12 = x0.xyxy + C.xxzz; \n\
    x12.xy -= i1; \n\
\n\
    // Permutations \n\
    i = mod289(i); // Avoid truncation effects in permutation \n\
    vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 )) \n\
            + i.x + vec3(0.0, i1.x, 1.0 )); \n\
\n\
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0); \n\
    m = m*m ; \n\
    m = m*m ; \n\
\n\
    // Gradients: 41 points uniformly over a line, mapped onto a diamond. \n\
    // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287) \n\
\n\
    vec3 x = 2.0 * fract(p * C.www) - 1.0; \n\
    vec3 h = abs(x) - 0.5; \n\
    vec3 ox = floor(x + 0.5); \n\
    vec3 a0 = x - ox; \n\
\n\
    // Normalise gradients implicitly by scaling m \n\
    // Approximation of: m *= inversesqrt( a0*a0 + h*h ); \n\
    m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h ); \n\
\n\
    // Compute final noise value at P \n\
    vec3 g; \n\
    g.x  = a0.x  * x0.x  + h.x  * x0.y; \n\
    g.yz = a0.yz * x12.xz + h.yz * x12.yw; \n\
    return 130.0 * dot(m, g); \n\
} \n\
\n\
float luminance(sfpvec4 color) \n\
{ \n\
    //(0.299*R + 0.587*G + 0.114*B) \n\
    return color.r * sfp(0.299) + color.g * sfp(0.587) + color.b * sfp(0.114); \n\
} \n\
\n\
vec2 center = vec2(1.0, p.direction); \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 point = uv.xy / vec2(1.0).xy; \n\
    sfpvec4 rgba_to = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 rgba_from = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    if (p.progress == 0.0) { \n\
        return rgba_from; \n\
    } else if (p.progress == 1.0) { \n\
        return rgba_to; \n\
    } else { \n\
        float x = p.progress; \n\
        float dist = distance(center, point)- p.progress * exp(snoise(vec2(point.x, 0.0))); \n\
        float r = x - rand(vec2(point.x, 0.1)); \n\
        float m; \n\
        if(p.above == 1) { \n\
            m = dist <= r && luminance(rgba_from) > p.threshold ? 1.0 : (p.progress * p.progress * p.progress); \n\
        } \n\
        else { \n\
            m = dist <= r && luminance(rgba_from) < p.threshold ? 1.0 : (p.progress * p.progress * p.progress); \n\
        } \n\
        return mix(rgba_from, rgba_to, sfp(m)); \n\
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

static const char LuminanceMelt_data[] = 
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
