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
    int direction; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float check(vec2 p1, vec2 p2, vec2 p3) \n\
{ \n\
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y); \n\
} \n\
\n\
bool PointInTriangle(vec2 pt, vec2 p1, vec2 p2, vec2 p3) \n\
{ \n\
    bool b1, b2, b3; \n\
    b1 = check(pt, p1, p2) < 0.0; \n\
    b2 = check(pt, p2, p3) < 0.0; \n\
    b3 = check(pt, p3, p1) < 0.0; \n\
    return ((b1 == b2) && (b2 == b3)); \n\
} \n\
\n\
bool in_left_triangle(vec2 point) \n\
{ \n\
    vec2 vertex1, vertex2, vertex3; \n\
    vertex1 = vec2(p.progress, 0.5); \n\
    vertex2 = vec2(0.0, 0.5 - p.progress); \n\
    vertex3 = vec2(0.0, 0.5 + p.progress); \n\
    if (PointInTriangle(point, vertex1, vertex2, vertex3)) \n\
    { \n\
        return true; \n\
    } \n\
    return false; \n\
} \n\
\n\
bool in_right_triangle(vec2 point) \n\
{ \n\
    vec2 vertex1, vertex2, vertex3; \n\
    vertex1 = vec2(1.0 - p.progress, 0.5); \n\
    vertex2 = vec2(1.0, 0.5 - p.progress); \n\
    vertex3 = vec2(1.0, 0.5 + p.progress); \n\
    if (PointInTriangle(point, vertex1, vertex2, vertex3)) \n\
    { \n\
        return true; \n\
    } \n\
    return false; \n\
} \n\
\n\
bool in_top_triangle(vec2 point) \n\
{ \n\
    vec2 vertex1, vertex2, vertex3; \n\
    vertex1 = vec2(0.5, p.progress); \n\
    vertex2 = vec2(0.5 - p.progress, 0.0); \n\
    vertex3 = vec2(0.5 + p.progress, 0.0); \n\
    if (PointInTriangle(point, vertex1, vertex2, vertex3)) \n\
    { \n\
        return true; \n\
    } \n\
    return false; \n\
} \n\
\n\
bool in_bottom_triangle(vec2 point) \n\
{ \n\
    vec2 vertex1, vertex2, vertex3; \n\
    vertex1 = vec2(0.5, 1.0 - p.progress); \n\
    vertex2 = vec2(0.5 - p.progress, 1.0); \n\
    vertex3 = vec2(0.5 + p.progress, 1.0); \n\
    if (PointInTriangle(point, vertex1, vertex2, vertex3)) \n\
    { \n\
        return true; \n\
    } \n\
    return false; \n\
} \n\
\n\
float blur_edge(vec2 bot1, vec2 bot2, vec2 top, vec2 testPt) \n\
{ \n\
    vec2 lineDir = bot1 - top; \n\
    vec2 perpDir = vec2(lineDir.y, -lineDir.x); \n\
    vec2 dirToPt1 = bot1 - testPt; \n\
    float dist1 = abs(dot(normalize(perpDir), dirToPt1)); \n\
\n\
    lineDir = bot2 - top; \n\
    perpDir = vec2(lineDir.y, -lineDir.x); \n\
    dirToPt1 = bot2 - testPt; \n\
    float min_dist = min(abs(dot(normalize(perpDir), dirToPt1)), dist1); \n\
\n\
    if (min_dist < 0.005) \n\
        return min_dist / 0.005; \n\
    else \n\
        return 1.0; \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.w), float(uv.y) / float(p.h)); \n\
    sfpvec4 result = sfpvec4(0.f); \n\
    sfpvec4 rgba_src1 = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_src2 = load_rgba_src2(uv.x, uv.y, p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    if (p.direction == 0) \n\
    { \n\
        if (in_left_triangle(point)) \n\
        { \n\
            if (p.progress < 0.1) \n\
            { \n\
                result = rgba_src1; \n\
            } \n\
            else \n\
            { \n\
                if (point.x < 0.5) \n\
                { \n\
                    vec2 vertex1 = vec2(p.progress, 0.5); \n\
                    vec2 vertex2 = vec2(0.0, 0.5 - p.progress); \n\
                    vec2 vertex3 = vec2(0.0, 0.5 + p.progress); \n\
                    result = mix( \n\
                        rgba_src1, \n\
                        rgba_src2, \n\
                        sfp(blur_edge(vertex2, vertex3, vertex1, point)) \n\
                    ); \n\
                } \n\
                else \n\
                { \n\
                    if (p.progress > 0.0) \n\
                        result = rgba_src2; \n\
                    else \n\
                        result = rgba_src1; \n\
                } \n\
            } \n\
        } \n\
        else if (in_right_triangle(point)) \n\
        { \n\
            if (point.x >= 0.5) \n\
            { \n\
                vec2 vertex1 = vec2(1.0 - p.progress, 0.5); \n\
                vec2 vertex2 = vec2(1.0, 0.5 - p.progress); \n\
                vec2 vertex3 = vec2(1.0, 0.5 + p.progress); \n\
                result = mix( \n\
                    rgba_src1, \n\
                    rgba_src2, \n\
                    sfp(blur_edge(vertex2, vertex3, vertex1, point)) \n\
                ); \n\
            } \n\
            else \n\
            { \n\
                result = rgba_src1; \n\
            } \n\
        } \n\
        else \n\
            result = rgba_src1; \n\
    } \n\
    else if (p.direction == 1) \n\
    { \n\
        if (in_top_triangle(point)) \n\
        { \n\
            if (p.progress < 0.1) \n\
            { \n\
                result = rgba_src1; \n\
            } \n\
            else \n\
            { \n\
                if (point.y < 0.5) \n\
                { \n\
                    vec2 vertex1 = vec2(0.5, p.progress); \n\
                    vec2 vertex2 = vec2(0.5 - p.progress, 0.0); \n\
                    vec2 vertex3 = vec2(0.5 + p.progress, 0.0); \n\
                    result = mix( \n\
                        rgba_src1, \n\
                        rgba_src2, \n\
                        sfp(blur_edge(vertex2, vertex3, vertex1, point)) \n\
                    ); \n\
                } \n\
                else \n\
                { \n\
                    if (p.progress > 0.0) \n\
                        result = rgba_src2; \n\
                    else \n\
                        result = rgba_src1; \n\
                } \n\
            } \n\
        } \n\
        else if (in_bottom_triangle(point)) \n\
        { \n\
            if (point.y >= 0.5) \n\
            { \n\
                vec2 vertex1 = vec2(0.5, 1.0 - p.progress); \n\
                vec2 vertex2 = vec2(0.5 - p.progress, 1.0); \n\
                vec2 vertex3 = vec2(0.5 + p.progress, 1.0); \n\
                result = mix( \n\
                    rgba_src1, \n\
                    rgba_src2, \n\
                    sfp(blur_edge(vertex2, vertex3, vertex1, point)) \n\
                ); \n\
            } \n\
            else \n\
                result = rgba_src1; \n\
        } \n\
        else \n\
            result = rgba_src1; \n\
    } \n\
    else \n\
    { \n\
        if (p.progress < 0.5) \n\
        { \n\
            if (point.y < 0.5) \n\
            { \n\
                vec2 botLeft = vec2(-0., p.progress - 0.5); \n\
                vec2 botRight = vec2(1., p.progress - 0.5); \n\
                vec2 tip = vec2(0.5, p.progress); \n\
                if (PointInTriangle(point, botLeft, botRight, tip)) \n\
                    result = rgba_src2; \n\
                else \n\
                    result = rgba_src1; \n\
            } \n\
            else \n\
            { \n\
                vec2 topLeft = vec2(-0., 1. - p.progress + 0.5); \n\
                vec2 topRight = vec2(1., 1. - p.progress + 0.5); \n\
                vec2 tip = vec2(0.5, 1. - p.progress); \n\
                if (PointInTriangle(point, topLeft, topRight, tip)) \n\
                    result = rgba_src2; \n\
                else \n\
                    result = rgba_src1; \n\
            } \n\
        } \n\
        else \n\
        { \n\
            if (point.x > 0.5) \n\
            { \n\
                vec2 top = vec2(p.progress + 0.5,  1.); \n\
                vec2 bot = vec2(p.progress + 0.5, -0.); \n\
                vec2 tip = vec2(mix(0.5, 1.0, 2.0 * (p.progress - 0.5)), 0.5); \n\
                if (PointInTriangle(point, top, bot, tip)) \n\
                    result = rgba_src1; \n\
                else \n\
                    result = rgba_src2; \n\
            } \n\
            else \n\
            { \n\
                vec2 top = vec2(1.0 - p.progress - 0.5,  1.); \n\
                vec2 bot = vec2(1.0 - p.progress - 0.5, -0.); \n\
                vec2 tip = vec2(mix(0.5, 0.0, 2.0 * (p.progress - 0.5)), 0.5); \n\
                if (PointInTriangle(point, top, bot, tip)) \n\
                    result = rgba_src1; \n\
                else \n\
                    result = rgba_src2; \n\
            } \n\
        } \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char BowTie_data[] = 
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
