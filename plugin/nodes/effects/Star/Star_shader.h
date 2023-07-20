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
    float progress; \n\
    float speed; \n\
    float layers; \n\
    \n\
    float red; \n\
    float green; \n\
    float blue; \n\
    float alpha; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float PI = 3.1415; \n\
float MIN_DIVIDE = 64.0; \n\
float MAX_DIVIDE = 0.01; \n\
\n\
mat2 Rotate(float angle) \n\
{ \n\
    float s = sin(angle); \n\
    float c = cos(angle); \n\
    return mat2(c, -s, s, c); \n\
} \n\
\n\
float Star(vec2 uv, float flaresize, float rotAngle, float randomN) \n\
{ \n\
    float d = length(uv); \n\
\n\
    float starcore = 0.05 / d; \n\
    uv *= Rotate(-2.0 * PI * rotAngle); \n\
    float flareMax = 1.0; \n\
\n\
    float starflares = max(0.0, flareMax - abs(uv.x * uv.y * 3000.0)); \n\
    starcore += starflares * flaresize; \n\
    uv *= Rotate(PI * 0.25); \n\
\n\
    starflares = max(0.0, flareMax - abs(uv.x * uv.y * 3000.0)); \n\
    starcore += starflares * 0.3 * flaresize; \n\
    starcore *= smoothstep(1.0, 0.05, d); \n\
\n\
    return starcore; \n\
} \n\
\n\
float PseudoRandomizer(vec2 point) \n\
{ \n\
    point = fract(point * vec2(123.45, 345.67)); \n\
    point += dot(point, point + 45.32); \n\
\n\
    return (fract(point.x * point.y)); \n\
} \n\
\n\
vec3 StarFieldLayer(vec2 uv, float rotAngle) \n\
{ \n\
    vec3 col = vec3(0); \n\
    vec2 gv = fract(uv) - 0.5; \n\
    vec2 id = floor(uv); \n\
\n\
    float deltaTimeTwinkle = p.progress; \n\
\n\
    for (int y = -1; y <= 1; y++) \n\
    { \n\
        for (int x = -1; x <= 1; x++) \n\
        { \n\
            vec2 offset = vec2(x, y); \n\
            \n\
            float randomN = PseudoRandomizer(id + offset); \n\
            float randoX = randomN - 0.5; \n\
            float randoY = fract(randomN * 45.0) - 0.5; \n\
            vec2 randomPosition = gv - offset - vec2(randoX, randoY); \n\
            \n\
            float size = fract(randomN * 1356.33); \n\
            float flareSwitch = smoothstep(0.9, 1.0, size); \n\
            \n\
            float star = Star(randomPosition, flareSwitch, rotAngle, randomN); \n\
            \n\
            float randomStarColorSeed = fract(randomN * 2150.0) * (3.0 * PI) * deltaTimeTwinkle; \n\
            vec3 color = sin(vec3(p.red, p.green, p.blue) * randomStarColorSeed); \n\
            \n\
            color = color * (0.4 * sin(deltaTimeTwinkle)) + 0.6; \n\
            \n\
            color = color * vec3(p.red, p.green, p.blue + size); \n\
            float dimByDensity = 15.0 / p.layers; \n\
            col += star * size * color * dimByDensity; \n\
        } \n\
    } \n\
    return col; \n\
} \n\
\n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 tuv = vec2(float(gl_GlobalInvocationID.x) / float(p.out_w - 1), float(gl_GlobalInvocationID.y) / float(p.out_h - 1)); \n\
    float deltaTime = p.progress * p.speed * 0.01; \n\
    vec3 col = vec3(0.f); \n\
    float rotAngle = p.progress * p.speed * 0.09f; \n\
    for (float i = 0.0; i < 1.0; i += (1.0 / p.layers)) \n\
    { \n\
        float layerDepth = fract(i + deltaTime); \n\
        float layerScale = mix(MIN_DIVIDE, MAX_DIVIDE, layerDepth); \n\
        float layerFader = layerDepth * smoothstep(0.1, 1.1, layerDepth); \n\
        float layerOffset = i * (3430.00 + fract(i)); \n\
        mat2 layerRot = Rotate(rotAngle * i * -10.0); \n\
        tuv *= layerRot; \n\
        vec2 starfieldUv = tuv * layerScale + layerOffset; \n\
        col += StarFieldLayer(starfieldUv, rotAngle) * layerFader; \n\
    } \n\
    sfpvec4 rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    store_rgba(sfpvec4(rgba.r + sfp(col.r), rgba.g + sfp(col.g), rgba.b + sfp(col.b), rgba.a), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \n\
"

static const char Effect_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
