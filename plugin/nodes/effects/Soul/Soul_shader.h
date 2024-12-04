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
    int count; \n\
    float max_scale; \n\
    float max_alpha; \n\
    int shrink; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 uv = vec2(float(gl_GlobalInvocationID.x) / float(p.out_w - 1), float(gl_GlobalInvocationID.y) / float(p.out_h - 1)); \n\
    float duration = 1.f / float(p.count); \n\
    float progress = mod(p.progress, duration) / duration; // 0 ~ 1 \n\
    if (p.shrink == 1 && progress > 0.5) progress = 1.0 - progress; \n\
    float alpha = p.max_alpha * (1.0 - progress); \n\
    float scale = 1.0 + (p.max_scale - 1.0) * progress; \n\
    float weakX = 0.5 + (uv.x - 0.5) / scale; \n\
    float weakY = 0.5 + (uv.y - 0.5) / scale; \n\
    vec2 weakTextureCoords = clamp(vec2(weakX, weakY), vec2(0, 0), vec2(1.0, 1.0)); \n\
    sfpvec4 rgba = load_rgba(int(weakTextureCoords.x * (p.w - 1)), int(weakTextureCoords.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 mask = load_rgba(int(uv.x * (p.w - 1)), int(uv.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    rgba = mix(rgba, mask, alpha); //mask * (1.0 - alpha) + rgba * alpha; \n\
    store_rgba(rgba, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
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
