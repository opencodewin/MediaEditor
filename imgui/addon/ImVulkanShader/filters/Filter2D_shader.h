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
	int xksize;	\n\
    int yksize;	\n\
    int xanchor; \n\
    int yanchor; \n\
} p; \
"

#define SHADER_FILTER_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfp alpha = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type).a; \n\
    sfpvec3 sum = sfpvec3(sfp(0.0f)); \n\
    int kInd = 0; \n\
    for (int i = 0; i < p.yksize; ++i) \n\
    { \n\
        for (int j= 0; j < p.xksize; ++j) \n\
        { \n\
            int x = uv.x - p.xanchor + j; \n\
            int y = uv.y - p.yanchor + i; \n\
            // REPLICATE border \n\
            x = max(0, min(x, p.out_w - 1)); \n\
            y = max(0, min(y, p.out_h - 1)); \n\
            sfpvec3 rgb = load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb * sfp(kernel_data[kInd++]); \n\
            sum = sum + rgb; \n\
        } \n\
    } \n\
    store_rgba(sfpvec4(sum, alpha), uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer kernel_float { float kernel_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_FILTER_MAIN
;
