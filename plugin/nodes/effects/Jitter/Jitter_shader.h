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
    float offset; \n\
    int shrink; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float rand(float n) \n\
{ \n\
    return fract(sin(n) * 43758.5453123); \n\
} \n\
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
    vec2 offsetCoords = vec2(p.offset, p.offset) * progress; \n\
    float scale = 1.0 + (p.max_scale - 1.0) * progress; \n\
    vec2 ScaleTextureCoords = clamp(vec2(0.5, 0.5) + (uv - vec2(0.5, 0.5)) / scale, vec2(0, 0), vec2(1.0, 1.0)); \n\
    vec2 mask_r_coord = clamp(ScaleTextureCoords + offsetCoords, vec2(0, 0), vec2(1.0, 1.0)); \n\
    vec2 mask_b_coord = clamp(ScaleTextureCoords - offsetCoords, vec2(0, 0), vec2(1.0, 1.0)); \n\
    sfpvec4 mask_r = load_rgba(int(mask_r_coord.x * (p.w - 1)), int(mask_r_coord.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 mask_b = load_rgba(int(mask_b_coord.x * (p.w - 1)), int(mask_b_coord.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 mask   = load_rgba(int(ScaleTextureCoords.x * (p.w - 1)), int(ScaleTextureCoords.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba = sfpvec4(sfp(mask_r.r), sfp(mask.g), sfp(mask_b.b), sfp(mask.a)); \n\
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
