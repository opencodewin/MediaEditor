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
    int w2; \n\
    int h2; \n\
    int cstep2; \n\
    int in_format2; \n\
    int in_type2; \n\
\n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
\n\
    float progress; \n\
    float zoom; \n\
    float corner_radius; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const sfpvec4 black = sfpvec4(sfp(0.0), sfp(0.0), sfp(0.0), sfp(1.0)); \n\
const vec2 c00 = vec2(0.0, 0.0); // the four corner points \n\
const vec2 c01 = vec2(0.0, 1.0); \n\
const vec2 c11 = vec2(1.0, 1.0); \n\
const vec2 c10 = vec2(1.0, 0.0); \n\
bool in_corner(vec2 point, vec2 corner, vec2 radius) \n\
{ \n\
    // determine the direction we want to be filled \n\
    vec2 axis = (c11 - corner) - corner; \n\
\n\
    // warp the point so we are always testing the bottom left point with the \n\
    // circle centered on the origin \n\
    point = point - (corner + axis * radius); \n\
    point *= axis / radius; \n\
    return (point.x > 0.0 && point.y > -1.0) || (point.y > 0.0 && point.x > -1.0) || dot(point, point) < 1.0; \n\
} \n\
\n\
bool test_rounded_mask(vec2 point, vec2 corner_size) \n\
{ \n\
    return \n\
        in_corner(point, c00, corner_size) && \n\
        in_corner(point, c01, corner_size) && \n\
        in_corner(point, c10, corner_size) && \n\
        in_corner(point, c11, corner_size); \n\
} \n\
\n\
sfpvec4 screen(sfpvec4 a, sfpvec4 b) \n\
{ \n\
    return sfp(1.0) - (sfp(1.0) - a) * (sfp(1.0) - b); \n\
} \n\
\n\
sfpvec4 unscreen(sfpvec4 c) \n\
{ \n\
    return sfp(1.0) - sqrt(sfp(1.0) - c); \n\
} \n\
\n\
sfpvec4 sample_with_corners_from(vec2 point, vec2 corner_size) \n\
{ \n\
    point = (point - 0.5) / p.zoom + 0.5; \n\
    if (!test_rounded_mask(point, corner_size)) \n\
    { \n\
        return black; \n\
    } \n\
    sfpvec4 rgba_from = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    return unscreen(rgba_from); \n\
} \n\
\n\
sfpvec4 sample_with_corners_to(vec2 point, vec2 corner_size) \n\
{ \n\
    point = (point - 0.5) / p.zoom + 0.5; \n\
    if (!test_rounded_mask(point, corner_size)) \n\
    { \n\
        return black; \n\
    } \n\
    sfpvec4 rgba_to = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    return unscreen(rgba_to); \n\
} \n\
\n\
sfpvec4 simple_sample_with_corners_from(vec2 point, vec2 corner_size, float zoom_amt) \n\
{ \n\
    point = (point - 0.5) / (1.0 - zoom_amt + p.zoom * zoom_amt) + 0.5; \n\
    if (!test_rounded_mask(point, corner_size)) \n\
    { \n\
        return black; \n\
    } \n\
    sfpvec4 rgba_from = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    return rgba_from; \n\
} \n\
\n\
sfpvec4 simple_sample_with_corners_to(vec2 point, vec2 corner_size, float zoom_amt) \n\
{ \n\
    point = (point - 0.5) / (1.0 - zoom_amt + p.zoom * zoom_amt) + 0.5; \n\
    if (!test_rounded_mask(point, corner_size)) \n\
    { \n\
        return black; \n\
    } \n\
    sfpvec4 rgba_to = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    return rgba_to; \n\
} \n\
\n\
mat3 rotate2d(float angle, float ratio) \n\
{ \n\
    float s = sin(angle); \n\
    float c = cos(angle); \n\
    return mat3( \n\
        c, s ,0.0, \n\
        -s, c, 0.0, \n\
        0.0, 0.0, 1.0 \n\
    ); \n\
} \n\
\n\
mat3 translate2d(float x, float y) \n\
{ \n\
    return mat3( \n\
        1.0, 0.0, 0, \n\
        0.0, 1.0, 0, \n\
        -x, -y, 1.0 \n\
    ); \n\
} \n\
\n\
mat3 scale2d(float x, float y) \n\
{ \n\
    return mat3( \n\
        x, 0.0, 0, \n\
        0.0, y, 0, \n\
        0, 0, 1.0 \n\
    ); \n\
} \n\
\n\
sfpvec4 get_cross_rotated(vec3 p3, float angle, vec2 corner_size, float ratio) \n\
{ \n\
    angle = angle * angle; // easing \n\
    angle /= 2.4; // works out to be a good number of radians \n\
\n\
    mat3 center_and_scale = translate2d(-0.5, -0.5) * scale2d(1.0, ratio); \n\
    mat3 unscale_and_uncenter = scale2d(1.0, 1.0/ratio) * translate2d(0.5,0.5); \n\
    mat3 slide_left = translate2d(-2.0,0.0); \n\
    mat3 slide_right = translate2d(2.0,0.0); \n\
    mat3 rotate = rotate2d(angle, ratio); \n\
\n\
    mat3 op_a = center_and_scale * slide_right * rotate * slide_left * unscale_and_uncenter; \n\
    mat3 op_b = center_and_scale * slide_left * rotate * slide_right * unscale_and_uncenter; \n\
\n\
    sfpvec4 a = sample_with_corners_from((op_a * p3).xy, corner_size); \n\
    sfpvec4 b = sample_with_corners_from((op_b * p3).xy, corner_size); \n\
\n\
    return screen(a, b); \n\
} \n\
\n\
sfpvec4 get_cross_masked(vec3 p3, float angle, vec2 corner_size, float ratio) \n\
{ \n\
    angle = 1.0 - angle; \n\
    angle = angle * angle; // easing \n\
    angle /= 2.4; \n\
\n\
    sfpvec4 img; \n\
\n\
    mat3 center_and_scale = translate2d(-0.5, -0.5) * scale2d(1.0, ratio); \n\
    mat3 unscale_and_uncenter = scale2d(1.0 / p.zoom, 1.0 / (p.zoom * ratio)) * translate2d(0.5,0.5); \n\
    mat3 slide_left = translate2d(-2.0,0.0); \n\
    mat3 slide_right = translate2d(2.0,0.0); \n\
    mat3 rotate = rotate2d(angle, ratio); \n\
\n\
    mat3 op_a = center_and_scale * slide_right * rotate * slide_left * unscale_and_uncenter; \n\
    mat3 op_b = center_and_scale * slide_left * rotate * slide_right * unscale_and_uncenter; \n\
\n\
    bool mask_a = test_rounded_mask((op_a * p3).xy, corner_size); \n\
    bool mask_b = test_rounded_mask((op_b * p3).xy, corner_size); \n\
\n\
    if (mask_a || mask_b) \n\
    { \n\
        img = sample_with_corners_to(p3.xy, corner_size); \n\
        return screen(mask_a ? img : black, mask_b ? img : black); \n\
    } \n\
    else \n\
    { \n\
        return black; \n\
    } \n\
} \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    float a; \n\
    vec2 point = uv.xy / vec2(1.0).xy; \n\
    vec3 p3 = vec3(point.xy, 1.0); // for 2D matrix transforms \n\
    float ratio = 2.0; \n\
\n\
    // corner is warped to represent to size after mapping to 1.0, 1.0 \n\
    vec2 corner_size = vec2(p.corner_radius / ratio, p.corner_radius); \n\
\n\
    if (p.progress <= 0.0) { \n\
        // 0.0: start with the base frame always \n\
        return load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    } else if (p.progress < 0.1) { \n\
        // 0.0-0.1: zoom out and add rounded corners \n\
        a = p.progress / 0.1; \n\
        return  simple_sample_with_corners_from(point, corner_size * a, a); \n\
    } else if (p.progress < 0.48) { \n\
        // 0.1-0.48: Split original image apart \n\
        a = (p.progress - 0.1)/0.38; \n\
        return get_cross_rotated(p3, a, corner_size, ratio); \n\
    } else if (p.progress < 0.9) { \n\
        // 0.48-0.52: black \n\
        // 0.52 - 0.9: unmask new image \n\
        return get_cross_masked(p3, (p.progress - 0.52)/0.38, corner_size, ratio); \n\
    } else if (p.progress < 1.0) { \n\
        // zoom out and add rounded corners \n\
        a = (1.0 - p.progress) / 0.1; \n\
        return simple_sample_with_corners_to(point, corner_size * a, a); \n\
    } else { \n\
        // 1.0 end with base frame \n\
        return load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    } \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.out_w - 1), 1.f - float(uv.y) / float(p.out_h - 1)); \n\
    sfpvec4 result = transition(point); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char StereoViewer_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer src2_int8       { uint8_t   src2_data_int8[]; };
layout (binding =  9) readonly buffer src2_int16      { uint16_t  src2_data_int16[]; };
layout (binding = 10) readonly buffer src2_float16    { float16_t src2_data_float16[]; };
layout (binding = 11) readonly buffer src2_float32    { float     src2_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_RGBA_NAME(src2)
SHADER_STORE_RGBA
SHADER_MAIN
;
