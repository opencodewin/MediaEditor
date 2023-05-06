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
    float thickness; \n\
    float brightness; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
sfpvec4 detectEdgeColor(sfpvec3[9] c) \n\
{ \n\
    /* adjacent texel array for texel c[4] \n\
    036 \n\
    147 \n\
    258 \n\
    */ \n\
    sfpvec3 dx = sfp(2.0) * abs(c[7]-c[1]) + abs(c[2] - c[6]) + abs(c[8] - c[0]); \n\
	sfpvec3 dy = sfp(2.0) * abs(c[3]-c[5]) + abs(c[6] - c[8]) + abs(c[0] - c[2]); \n\
    sfp delta = length(sfp(0.25f) * (dx + dy) * sfp(0.5f)); \n\
	return sfpvec4(clamp(sfp(p.brightness) * delta, sfp(0.f), sfp(1.f)) * c[4], sfp(1.f)); \n\
} \n\
sfpvec4 getFromEdgeColor(vec2 uv) \n\
{ \n\
	sfpvec3 c[9]; \n\
	for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) \n\
	{ \n\
        vec2 from_point = uv + p.thickness * vec2(i - 1, j - 1); \n\
        from_point = clamp(from_point, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
        sfpvec4 color = load_rgba(int(from_point.x * (p.w - 1)), int((1.f - from_point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
	    //sfpvec4 color = getFromColor(uv + p.thickness * vec2(i-1,j-1)); \n\
        c[3 * i + j] = color.rgb; \n\
	} \n\
	return detectEdgeColor(c); \n\
} \n\
sfpvec4 getToEdgeColor(vec2 uv) \n\
{ \n\
	sfpvec3 c[9]; \n\
	for (int i=0; i < 3; ++i) for (int j=0; j < 3; ++j) \n\
	{ \n\
        vec2 to_point = uv + p.thickness * vec2(i - 1,j - 1); \n\
        to_point = clamp(to_point, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
        sfpvec4 color = load_rgba_src2(int(to_point.x * (p.w2 - 1)), int((1.f - to_point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
	    //vec4 color = getToColor(uv + p.thickness * vec2(i-1,j-1)); \n\
        c[3 * i + j] = color.rgb; \n\
	} \n\
	return detectEdgeColor(c); \n\
} \n\
sfpvec4 transition (vec2 uv) \n\
{ \n\
    sfpvec4 rgba_from = load_rgba(int(uv.x * (p.w - 1)), int((1.f - uv.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 rgba_to = load_rgba_src2(int(uv.x * (p.w2 - 1)), int((1.f - uv.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 start = mix(rgba_from, getFromEdgeColor(uv), sfp(clamp(2.0 * p.progress, 0.0, 1.0))); \n\
    sfpvec4 end = mix(getToEdgeColor(uv), rgba_to, sfp(clamp(2.0 * (p.progress - 0.5), 0.0, 1.0))); \n\
    return mix(start, end, sfp(p.progress)); \n\
} \n\
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

static const char Edge_data[] = 
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
