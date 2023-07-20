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
    float freq; \n\
    float amount; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const vec2 center = vec2(0.5, 0.5); \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 uv = vec2(float(gl_GlobalInvocationID.x) / float(p.out_w - 1), float(gl_GlobalInvocationID.y) / float(p.out_h - 1)); \n\
    vec2 direction = uv - center; \n\
    float len = length(direction); \n\
    vec2 uv_new = uv + direction * p.amount * abs(sin(len * p.freq)) / len; \n\
    sfpvec4 rgba = load_rgba(int(uv_new.x * (p.w - 1)), int(uv_new.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
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
