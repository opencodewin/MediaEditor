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
    float threshold; \n\
    int blur; \n\
} p; \
"

#define SHADER_DEBAND \
" \n\
#define CLIP(a, b, c) max(b, min(a, c)) \n\
#define AVG(a, b, c , d) ((a + b + c + d) / sfp(4.0f)) \n\
sfpvec4 deband(int x, int y) \n\
{ \n\
    sfp avg, diff; \n\
    sfpvec4 rgba = sfpvec4(0); \n\
    int x_pos = xpos_data[y * p.w + x]; \n\
    int y_pos = ypos_data[y * p.w + x]; \n\
    sfpvec4 ref0 = load_rgba(CLIP(x +  x_pos, 0, p.w - 1), CLIP(y +  y_pos, 0, p.h - 1), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 ref1 = load_rgba(CLIP(x +  x_pos, 0, p.w - 1), CLIP(y + -y_pos, 0, p.h - 1), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 ref2 = load_rgba(CLIP(x + -x_pos, 0, p.w - 1), CLIP(y + -y_pos, 0, p.h - 1), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 ref3 = load_rgba(CLIP(x + -x_pos, 0, p.w - 1), CLIP(y +  y_pos, 0, p.h - 1), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 src0 = load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    // R \n\
    if (p.blur != 0) \n\
    { \n\
        avg = AVG(ref0.x, ref1.x, ref2.x, ref3.x); \n\
        diff = abs(src0.x - avg); \n\
        rgba.r = diff < sfp(p.threshold) ? avg : src0.x; \n\
    } \n\
    else \n\
    { \n\
        rgba.r = ((abs(src0.x - ref0.x) < sfp(p.threshold)) && \n\
                (abs(src0.x - ref1.x) < sfp(p.threshold)) && \n\
                (abs(src0.x - ref2.x) < sfp(p.threshold)) && \n\
                (abs(src0.x - ref3.x) < sfp(p.threshold))) ? AVG(ref0.x, ref1.x, ref2.x, ref3.x) : src0.x; \n\
    } \n\
    // G \n\
    if (p.blur != 0) \n\
    { \n\
        avg = AVG(ref0.y, ref1.y, ref2.y, ref3.y); \n\
        diff = abs(src0.y - avg); \n\
        rgba.g = diff < sfp(p.threshold) ? avg : src0.y; \n\
    } \n\
    else \n\
    { \n\
        rgba.g = ((abs(src0.y - ref0.y) < sfp(p.threshold)) && \n\
                (abs(src0.y - ref1.y) < sfp(p.threshold)) && \n\
                (abs(src0.y - ref2.y) < sfp(p.threshold)) && \n\
                (abs(src0.y - ref3.y) < sfp(p.threshold))) ? AVG(ref0.y, ref1.y, ref2.y, ref3.y) : src0.y; \n\
    } \n\
    // B \n\
    if (p.blur != 0) \n\
    { \n\
        avg = AVG(ref0.z, ref1.z, ref2.z, ref3.z); \n\
        diff = abs(src0.z - avg); \n\
        rgba.b = diff < sfp(p.threshold) ? avg : src0.z; \n\
    } \n\
    else \n\
    { \n\
        rgba.b = ((abs(src0.z - ref0.z) < sfp(p.threshold)) && \n\
                (abs(src0.z - ref1.z) < sfp(p.threshold)) && \n\
                (abs(src0.z - ref2.z) < sfp(p.threshold)) && \n\
                (abs(src0.z - ref3.z) < sfp(p.threshold))) ? AVG(ref0.z, ref1.z, ref2.z, ref3.z) : src0.z; \n\
    } \n\
    rgba.a = src0.a; \n\
    return rgba; \n\
} \
"

#define SHADER_DEBAND_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 result = deband(gx, gy); \n\
    store_rgba(result, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char DeBand_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer xpos { int xpos_data[]; };
layout (binding = 9) readonly buffer ypos { int ypos_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_DEBAND
SHADER_DEBAND_MAIN
;