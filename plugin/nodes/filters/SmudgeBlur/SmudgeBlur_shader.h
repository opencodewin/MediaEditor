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
    float radius; \n\
    float iterations; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    vec3 color = vec3(0, 0, 0); \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 uv = vec2(float(gl_GlobalInvocationID.x) / float(p.out_w - 1), float(gl_GlobalInvocationID.y) / float(p.out_h - 1)); \n\
    for (float i = 0.0; i < 360.0 / p.iterations; i++) \n\
    { \n\
	    vec2 uv_new = vec2((uv.x + sin(i * p.iterations) * p.radius * color.r), (uv.y + cos(i * p.iterations) * p.radius * (color.g + color.b))); \n\
        sfpvec4 color_new = load_rgba(int(uv_new.x * (p.w - 1)), int(uv_new.y * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        color = color + vec3(color_new.r, color_new.g, color_new.b); \n\
    } \n\
    color = color / (360.0 / p.iterations); \n\
    sfpvec4 rgba = sfpvec4(sfp(color.x), sfp(color.y), sfp(color.z), sfp(1.0)); \n\
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
