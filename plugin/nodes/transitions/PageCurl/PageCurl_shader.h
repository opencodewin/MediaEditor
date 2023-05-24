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
} p; \
"

#define SHADER_MAIN \
" \n\
const float PI = 3.141592653589793; \n\
const float MIN_AMOUNT = -0.16; \n\
const float MAX_AMOUNT = 1.5; \n\
float amount = p.progress * (MAX_AMOUNT - MIN_AMOUNT) + MIN_AMOUNT; \n\
const float scale = 512.0; \n\
const float sharpness = 3.0; \n\
float cylinderCenter = amount; \n\
// 360 degrees * amount \n\
float cylinderAngle = 2.0 * PI * amount; \n\
const float cylinderRadius = 1.0 / PI / 2.0; \n\
vec3 hitPoint(float hitAngle, float yc, vec3 point, mat3 rrotation) \n\
{ \n\
    float hit_point = hitAngle / (2.0 * PI); \n\
    point.y = hit_point; \n\
    return rrotation * point; \n\
} \n\
\n\
sfpvec4 antiAlias(sfpvec4 color1, sfpvec4 color2, float distanc) \n\
{ \n\
    distanc *= scale; \n\
    if (distanc < 0.0) return color2; \n\
    if (distanc > 2.0) return color1; \n\
    float dd = pow(1.0 - distanc / 2.0, sharpness); \n\
    return ((color2 - color1) * sfp(dd)) + color1; \n\
} \n\
\n\
float distanceToEdge(vec3 point) \n\
{ \n\
    float dx = abs(point.x > 0.5 ? 1.0 - point.x : point.x); \n\
    float dy = abs(point.y > 0.5 ? 1.0 - point.y : point.y); \n\
    if (point.x < 0.0) dx = -point.x; \n\
    if (point.x > 1.0) dx = point.x - 1.0; \n\
    if (point.y < 0.0) dy = -point.y; \n\
    if (point.y > 1.0) dy = point.y - 1.0; \n\
    if ((point.x < 0.0 || point.x > 1.0) && (point.y < 0.0 || point.y > 1.0)) return sqrt(dx * dx + dy * dy); \n\
    return min(dx, dy); \n\
} \n\
\n\
sfpvec4 seeThrough(float yc, vec2 p1, mat3 rotation, mat3 rrotation) \n\
{ \n\
    float hitAngle = PI - (acos(yc / cylinderRadius) - cylinderAngle); \n\
    vec3 point = hitPoint(hitAngle, yc, rotation * vec3(p1, 1.0), rrotation); \n\
    if (yc <= 0.0 && (point.x < 0.0 || point.y < 0.0 || point.x > 1.0 || point.y > 1.0)) \n\
    { \n\
        sfpvec4 rgba_to = load_rgba_src2(int(p1.x * (p.w2 - 1)), int((1.f - p1.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        return rgba_to; \n\
    } \n\
\n\
    if (yc > 0.0) \n\
    { \n\
        sfpvec4 rgba_from = load_rgba(int(p1.x * (p.w - 1)), int((1.f - p1.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        return rgba_from; \n\
    } \n\
\n\
    sfpvec4 color = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 tcolor = sfpvec4(0.0); \n\
    return antiAlias(color, tcolor, distanceToEdge(point)); \n\
} \n\
\n\
sfpvec4 seeThroughWithShadow(float yc, vec2 p, vec3 point, mat3 rotation, mat3 rrotation) \n\
{ \n\
    float shadow = distanceToEdge(point) * 30.0; \n\
    shadow = (1.0 - shadow) / 3.0; \n\
\n\
    if (shadow < 0.0) shadow = 0.0; else shadow *= amount; \n\
\n\
    sfpvec4 shadowColor = seeThrough(yc, p, rotation, rrotation); \n\
    shadowColor.r -= sfp(shadow); \n\
    shadowColor.g -= sfp(shadow); \n\
    shadowColor.b -= sfp(shadow); \n\
\n\
    return shadowColor; \n\
} \n\
\n\
sfpvec4 backside(float yc, vec3 point) \n\
{ \n\
    sfpvec4 color = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    float gray = (color.r + color.b + color.g) / 15.0; \n\
    gray += (8.0 / 10.0) * (pow(1.0 - abs(yc / cylinderRadius), 2.0 / 10.0) / 2.0 + (5.0 / 10.0)); \n\
    color.rgb = sfpvec3(sfp(gray)); \n\
    return color; \n\
} \n\
\n\
sfpvec4 behindSurface(vec2 p1, float yc, vec3 point, mat3 rrotation) \n\
{ \n\
    float shado = (1.0 - ((-cylinderRadius - yc) / amount * 7.0)) / 6.0; \n\
    shado *= 1.0 - abs(point.x - 0.5); \n\
\n\
    yc = (-cylinderRadius - cylinderRadius - yc); \n\
\n\
    float hitAngle = (acos(yc / cylinderRadius) + cylinderAngle) - PI; \n\
    point = hitPoint(hitAngle, yc, point, rrotation); \n\
\n\
    if (yc < 0.0 && point.x >= 0.0 && point.y >= 0.0 && point.x <= 1.0 && point.y <= 1.0 && (hitAngle < PI || amount > 0.5)) \n\
    { \n\
        shado = 1.0 - (sqrt(pow(point.x - 0.5, 2.0) + pow(point.y - 0.5, 2.0)) / (71.0 / 100.0)); \n\
        shado *= pow(-yc / cylinderRadius, 3.0); \n\
        shado *= 0.5; \n\
    } \n\
    else \n\
    { \n\
        shado = 0.0; \n\
    } \n\
    sfpvec4 rgba_to = load_rgba_src2(int(p1.x * (p.w2 - 1)), int((1.f - p1.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    return sfpvec4(rgba_to.rgb - sfp(shado), sfp(1.0)); \n\
} \n\
\n\
sfpvec4 transition(vec2 point) \n\
{ \n\
    const float angle = 100.0 * PI / 180.0; \n\
    float c = cos(-angle); \n\
    float s = sin(-angle); \n\
\n\
    mat3 rotation = mat3( c, s, 0, \n\
                        -s, c, 0, \n\
                        -0.801, 0.8900, 1 \n\
                        ); \n\
    c = cos(angle); \n\
    s = sin(angle); \n\
\n\
    mat3 rrotation = mat3(	c, s, 0, \n\
                            -s, c, 0, \n\
                            0.98500, 0.985, 1 \n\
                        ); \n\
\n\
    vec3 point3 = rotation * vec3(point, 1.0); \n\
\n\
    float yc = point3.y - cylinderCenter; \n\
\n\
    if (yc < -cylinderRadius) \n\
    { \n\
        // Behind surface \n\
        return behindSurface(point, yc, point3, rrotation); \n\
    } \n\
\n\
    if (yc > cylinderRadius) \n\
    { \n\
        // Flat surface \n\
        sfpvec4 rgba_from = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        return rgba_from; \n\
    } \n\
\n\
    float hitAngle = (acos(yc / cylinderRadius) + cylinderAngle) - PI; \n\
\n\
    float hitAngleMod = mod(hitAngle, 2.0 * PI); \n\
    if ((hitAngleMod > PI && amount < 0.5) || (hitAngleMod > PI/2.0 && amount < 0.0)) \n\
    { \n\
        return seeThrough(yc, point, rotation, rrotation); \n\
    } \n\
\n\
    point3 = hitPoint(hitAngle, yc, point3, rrotation); \n\
\n\
    if (point3.x < 0.0 || point3.y < 0.0 || point3.x > 1.0 || point3.y > 1.0) \n\
    { \n\
        return seeThroughWithShadow(yc, point, point3, rotation, rrotation); \n\
    } \n\
\n\
    sfpvec4 color = backside(yc, point3); \n\
\n\
    sfpvec4 otherColor; \n\
    if (yc < 0.0) \n\
    { \n\
        float shado = 1.0 - (sqrt(pow(point3.x - 0.5, 2.0) + pow(point3.y - 0.5, 2.0)) / 0.71); \n\
        shado *= pow(-yc / cylinderRadius, 3.0); \n\
        shado *= 0.5; \n\
        otherColor = sfpvec4(sfp(0.0), sfp(0.0), sfp(0.0), sfp(shado)); \n\
    } \n\
    else \n\
    { \n\
        otherColor = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    } \n\
\n\
    color = antiAlias(color, otherColor, cylinderRadius - abs(yc)); \n\
\n\
    sfpvec4 cl = seeThroughWithShadow(yc, point, point3, rotation, rrotation); \n\
    float dist = distanceToEdge(point3); \n\
\n\
    return antiAlias(color, cl, dist); \n\
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

static const char PageCurl_data[] = 
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
