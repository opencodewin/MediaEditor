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
    int curve_w; \n\
    int curve_c; \n\
} p;\
"

#define SHADER_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 color = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    ivec4 color_index = ivec4(floor(color * sfp(p.curve_w - 1))); \n\
    sfpvec4 result = sfpvec4(0.f); \n\
    sfp yuv_org = sfp(0.299) * color.r + sfp(0.587) * color.g + sfp(0.114) * color.b; \n\
    result.r = sfp(curve[color_index.r + p.curve_w * 1]); \n\
    result.g = sfp(curve[color_index.g + p.curve_w * 2]); \n\
    result.b = sfp(curve[color_index.b + p.curve_w * 3]); \n\
    sfp yuv = sfp(0.299) * result.r + sfp(0.587) * result.g + sfp(0.114) * result.b; \n\
    sfp deta_y = yuv_org - yuv; \n\
    int y_index = int(floor(yuv * sfp(p.curve_w))); \n\
    deta_y += sfp(curve[y_index]) - yuv; \n\
    result += deta_y; \n\
    store_rgba(sfpvec4(result.rgb, color.a), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer mat_curve { float curve[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_MAIN
;
