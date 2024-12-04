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
    float scale; \n\
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
    vec2 res = vec2(int(p.h), int(p.w)); \n\
    vec4[4] area = vec4[]( \n\
        vec4(1, p.scale, 1, p.scale), vec4(-p.scale, 1, 1, p.scale), \n\
        vec4(-p.scale, 1, -p.scale, 1), vec4(1, p.scale, -p.scale, 1) \n\
    ); \n\
    float val = (p.scale + 1) * (p.scale + 1); \n\
    vec3 ave_A = vec3(0.0); \n\
    vec3 var_A = vec3(0.0); \n\
    for(float i = area[0].x; i <= area[0].y; i++) { \n\
        for(float j = area[0].z; j <= area[0].w; j++) { \n\
            sfpvec4 tmp = load_rgba(int((uv.x + i/res.x) * (p.w - 1)), int((uv.y + j/res.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            vec3 src = vec3(float(tmp.r), float(tmp.g), float(tmp.b)); \n\
            ave_A += src; \n\
            var_A += src * src; \n\
        } \n\
    } \n\
    ave_A /= val; \n\
    var_A = abs(var_A / val - ave_A * ave_A); \n\
    vec3 ave_B = vec3(0.0); \n\
    vec3 var_B = vec3(0.0); \n\
    for(float i = area[1].x; i <= area[1].y; i++) { \n\
        for(float j = area[1].z; j <= area[1].w; j++) { \n\
            sfpvec4 tmp = load_rgba(int((uv.x + i/res.x) * (p.w - 1)), int((uv.y + j/res.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            vec3 src = vec3(float(tmp.r), float(tmp.g), float(tmp.b)); \n\
            ave_B += src; \n\
            var_B += src * src; \n\
        } \n\
    } \n\
    ave_B /= val; \n\
    var_B = abs(var_B / val - ave_B * ave_B); \n\
    vec3 ave_C = vec3(0.0); \n\
    vec3 var_C = vec3(0.0); \n\
    for(float i = area[2].x; i <= area[2].y; i++) { \n\
        for(float j = area[2].z; j <= area[2].w; j++) { \n\
            sfpvec4 tmp = load_rgba(int((uv.x + i/res.x) * (p.w - 1)), int((uv.y + j/res.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            vec3 src = vec3(float(tmp.r), float(tmp.g), float(tmp.b)); \n\
            ave_C += src; \n\
            var_C += src * src; \n\
        } \n\
    } \n\
    ave_C /= val; \n\
    var_C = abs(var_C / val - ave_C * ave_C); \n\
    vec3 ave_D = vec3(0.0); \n\
    vec3 var_D = vec3(0.0); \n\
    for(float i = area[3].x; i <= area[3].y; i++) { \n\
        for(float j = area[3].z; j <= area[3].w; j++) { \n\
            sfpvec4 tmp = load_rgba(int((uv.x + i/res.x) * (p.w - 1)), int((uv.y + j/res.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            vec3 src = vec3(float(tmp.r), float(tmp.g), float(tmp.b)); \n\
            ave_D += src; \n\
            var_D += src * src; \n\
        } \n\
    } \n\
    ave_D /= val; \n\
    var_D = abs(var_D / val - ave_D * ave_D); \n\
    float sigma = 1e+2; \n\
    float sigma_A = var_A.r + var_A.g + var_A.b; \n\
    sfpvec4 color = sfpvec4(sfp(0.0f)); \n\
    if (sigma_A < sigma) { \n\
        sigma = sigma_A; \n\
        color = sfpvec4(sfp(ave_A.r), sfp(ave_A.g), sfp(ave_A.b), sfp(1.0f)); \n\
    } \n\
    float sigma_B = var_B.r + var_B.g + var_B.b; \n\
    if (sigma_B < sigma) { \n\
        sigma = sigma_B; \n\
        color = sfpvec4(sfp(ave_B.r), sfp(ave_B.g), sfp(ave_B.b), sfp(1.0f)); \n\
    } \n\
    float sigma_C = var_C.r + var_C.g + var_C.b; \n\
    if (sigma_C < sigma) { \n\
        sigma = sigma_C; \n\
        color = sfpvec4(sfp(ave_C.r), sfp(ave_C.g), sfp(ave_C.b), sfp(1.0f)); \n\
    } \n\
    float sigma_D = var_D.r + var_D.g + var_D.b; \n\
    if (sigma_D < sigma) { \n\
        sigma = sigma_D; \n\
        color = sfpvec4(sfp(ave_D.r), sfp(ave_D.g), sfp(ave_D.b), sfp(1.0f)); \n\
    } \n\
    store_rgba(color, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
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
