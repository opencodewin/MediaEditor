#pragma once
#include <imvk_mat_shader.h>

#define SHADER_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int format; \n\
    int type; \n\
\n\
    int outw; \n\
    int outh; \n\
    int outcstep; \n\
\n\
} p; \
"

#define SHADER_MAIN \
" \n\
sfpvec2 hs_to_point(sfpvec3 hs) \n\
{ \n\
    sfpvec2 point = sfpvec2(0.f); \n\
    sfp angle = hs.x * sfp(0.017453); \n\
    sfp length = clamp(hs.y, sfp(0.f), sfp(1.f)); \n\
    if (hs.z <= sfp(0.5)) \n\
    { \n\
        length *= hs.z * sfp(2.0); \n\
    } \n\
    else \n\
    { \n\
        length *= (sfp(1.0) - hs.z) * sfp(2.0); \n\
    } \n\
    if (hs.x == sfp(0.f)) \n\
    { \n\
        point.x = length; \n\
    } \n\
    else if (hs.x == sfp(180.f)) \n\
    { \n\
        point.x = -length; \n\
    } \n\
    else if (hs.x == sfp(90.f)) \n\
    { \n\
        point.y = length; \n\
    } \n\
    else if (hs.x == sfp(270.f)) \n\
    { \n\
        point.y = -length; \n\
    } \n\
    else \n\
    { \n\
        point.x = length * cos(angle); \n\
        point.y = length * sin(angle); \n\
    } \n\
    return point; \n\
} \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.w || gy >= p.h) \n\
        return; \n\
    if (mod(float(gx), 4) != 0 || mod(float(gy), 4) != 0) // reduce to half size\n\
        return; \n\
    sfpvec4 rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.format, p.type); \n\
    sfpvec3 hs = rgb_to_hsl(rgba.rgb); \n\
    sfpvec2 vector_point = hs_to_point(hs); \n\
    int length = int(hs.z * sfp(20.f)); \n\
    ivec2 point; \n\
    point.x = p.outw / 2 + int(vector_point.x * sfp(p.outw / 2)); \n\
    point.y = p.outh / 2 - int(vector_point.y * sfp(p.outh / 2)); \n\
    if (point.x > 0 && point.x < p.outw && point.y >=0 && point.y < p.outh) \n\
    { \n\
        memoryBarrierBuffer(); \n\
        int offset = point.y * p.outw + point.x; \n\
        atomicAdd(alpha_blob_data[offset], length); \n\
        memoryBarrierBuffer(); \n\
    } \n\
} \
"

static const char Vector_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_SRC_DATA
R"(
layout (binding = 4) restrict buffer alpha_blob { int alpha_blob_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_RGB_TO_HSL
SHADER_MAIN
;


#define SHADER_ZERO_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
} p; \
"

#define SHADER_ZERO_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.w || gy >= p.h) \n\
        return; \n\
    int in_offset = gy * p.w + gx; \n\
    alpha_blob_data[in_offset] = 0; \n\
} \
"

static const char Zero_data[] =
SHADER_HEADER
SHADER_ZERO_PARAM
R"(
layout (binding = 0) restrict buffer alpha_blob { int alpha_blob_data[]; };
)"
SHADER_ZERO_MAIN
;

// merge shader
#define SHADER_MERGE_PARAM \
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
    float intensity; \n\
} p; \
"

#define SHADER_MERGE \
" \n\
sfp point_to_angle(sfpvec2 point) \n\
{ \n\
    sfp angle = sfp(0.f); \n\
    if (point.y == sfp(0.f) && point.x > sfp(0.f)) \n\
        angle = sfp(0.f); \n\
    else if (point.y == sfp(0.f) && point.x < sfp(0.f)) \n\
        angle = sfp(180.f); \n\
    else if (point.x == sfp(0.f) && point.y < sfp(0.f)) \n\
        angle = sfp(270.f); \n\
    else if (point.x == sfp(0.f) && point.y > sfp(0.f)) \n\
        angle = sfp(90.f); \n\
    else if (point.x > sfp(0.f) && point.y > sfp(0.f)) \n\
        angle = sfp(90.f) - atan(point.x, point.y) * sfp(180.f) / M_PI; \n\
    else if (point.x < sfp(0.f) && point.y > sfp(0.f)) \n\
        angle = sfp(90.f) + atan(-point.x, point.y) * sfp(180.f) / M_PI; \n\
    else if (point.x < sfp(0.f) && point.y < sfp(0.f)) \n\
        angle = sfp(270.f) - atan(-point.x, -point.y) * sfp(180.f) / M_PI; \n\
    else if (point.x > sfp(0.f) && point.y < sfp(0.f)) \n\
        angle = sfp(270.f) + atan(point.x, -point.y) * sfp(180.f) / M_PI; \n\
    return angle; \n\
} \n\
void merge(int x, int y) \n\
{ \n\
    sfp x_offset = sfp(x - p.out_w / 2) / (sfp(p.out_w) / sfp(2.0)); \n\
    sfp y_offset = - sfp(y - p.out_h / 2) / (sfp(p.out_h) / sfp(2.0)); \n\
    sfpvec2 point = sfpvec2(x_offset, y_offset); \n\
    sfpvec2 center = sfpvec2(0, 0); \n\
    sfp dist = distance(point, center); \n\
    if (dist <= sfp(1.f)) \n\
    { \n\
        sfp angle = clamp(point_to_angle(point), sfp(0.f), sfp(360.f)); \n\
        sfpvec3 rgb = hsv_to_rgb(sfpvec3(angle, dist, sfp(1.f))); \n\
        int offset = y * p.w + x; \n\
        int alpha = int(alpha_blob_data[offset] * p.intensity); \n\
        if (alpha > 0) \n\
        { \n\
            sfp fa = sfp(clamp(alpha / 255.f, 0.f, 1.f)); \n\
            store_rgba(sfpvec4(rgb, fa), x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } else { \n\
            store_rgba(sfpvec4(rgb, sfp(0.f)), x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        } \n\
    } \n\
} \
"

#define SHADER_MERGE_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
\n\
    merge(gx, gy); \n\
} \
"

static const char Vector_merge_data[] = 
SHADER_HEADER
SHADER_MERGE_PARAM
SHADER_OUTPUT_DATA
R"(
layout (binding = 4) readonly buffer alpha_blob { int alpha_blob_data[]; };
)"
SHADER_STORE_RGBA
SHADER_HSV_TO_RGB
SHADER_MERGE
SHADER_MERGE_MAIN
;
