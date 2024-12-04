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
    int ksz; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
#define INIT_VUL sfpvec4(0.0) \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result = INIT_VUL; \n\
    for (int i = 0; i < p.ksz; ++i) \n\
    { \n\
        int x = uv.x - p.ksz / 2 + i; \n\
        x = max(0, min(x, p.w - 1)); \n\
        for (int j = 0; j < p.ksz; ++j) \n\
        { \n\
            int y = uv.y - p.ksz / 2 + j; \n\
            y = max(0, min(y, p.h - 1)); \n\
            sfpvec4 r = load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
            result = max(result, r); \n\
        } \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
