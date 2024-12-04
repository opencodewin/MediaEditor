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
    int x_offset; \n\
    int y_offset; \n\
    float alpha; \n\
} p; \
"

#define SHADER_ALPHA_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_src2 = load_rgba_src2(uv.x, uv.y, p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    if (uv.x - p.x_offset >= 0 && uv.y - p.y_offset >= 0 && \n\
        uv.x - p.x_offset < p.w && uv.y - p.y_offset < p.h) \n\
    { \n\
        sfpvec4 rgba_src1 = load_rgba(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfp alpha = clamp(rgba_src1.a + rgba_src2.a, sfp(0.f), sfp(1.f)); \n\
        result = sfpvec4(mix(rgba_src2.rgb, rgba_src1.rgb, rgba_src1.a), alpha); \n\
    } \n\
    else \n\
    { \n\
        result = rgba_src2; \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char AlphaBlending_data[] = 
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
SHADER_ALPHA_MAIN
;


#define SHADER_ALPHA_WITH_ALPHA_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_src2 = load_rgba_src2(uv.x, uv.y, p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    if (uv.x - p.x_offset >= 0 && uv.y - p.y_offset >= 0 && \n\
        uv.x - p.x_offset < p.w && uv.y - p.y_offset < p.h) \n\
    { \n\
        sfpvec4 rgba_src1 = load_rgba(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        result = sfpvec4(mix(rgba_src2.rgb, rgba_src1.rgb * rgba_src1.a, sfp(p.alpha)), rgba_src2.a); \n\
    } \n\
    else \n\
    { \n\
        result = rgba_src2; \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char AlphaBlending_alpha_data[] = 
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
SHADER_ALPHA_WITH_ALPHA_MAIN
;


#define SHADER_PARAM_WITH_ALPHA_MAT \
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
    int x_offset; \n\
    int y_offset; \n\
    float alpha; \n\
} p; \
"

#define SHADER_ALPHA_WITH_ALPHA_MAT \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_src2 = load_rgba_src2(uv.x, uv.y, p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    if (uv.x - p.x_offset >= 0 && uv.y - p.y_offset >= 0 && \n\
        uv.x - p.x_offset < p.w && uv.y - p.y_offset < p.h) \n\
    { \n\
        sfpvec4 rgba_src1 = load_rgba(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfp alpha = load_gray_alpha(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, 1, 0, DT_FLOAT32, 1); \n\
        result = sfpvec4(mix(rgba_src1.rgb, rgba_src2.rgb, alpha), rgba_src2.a); \n\
    } \n\
    else \n\
    { \n\
        result = rgba_src2; \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char AlphaBlending_alpha_mat_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer src2_int8       { uint8_t   src2_data_int8[]; };
layout (binding =  9) readonly buffer src2_int16      { uint16_t  src2_data_int16[]; };
layout (binding = 10) readonly buffer src2_float16    { float16_t src2_data_float16[]; };
layout (binding = 11) readonly buffer src2_float32    { float     src2_data_float32[]; };
layout (binding = 12) readonly buffer alpha_int8      { uint8_t   alpha_data_int8[]; };
layout (binding = 13) readonly buffer alpha_int16     { uint16_t  alpha_data_int16[]; };
layout (binding = 14) readonly buffer alpha_float16   { float16_t alpha_data_float16[]; };
layout (binding = 15) readonly buffer alpha_float32   { float     alpha_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_RGBA_NAME(src2)
SHADER_LOAD_GRAY_NAME(alpha)
SHADER_STORE_RGBA
SHADER_ALPHA_WITH_ALPHA_MAT
;

#define SHADER_ALPHA_OVERLAY_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_src1 = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    if (uv.x - p.x_offset >= 0 && uv.y - p.y_offset >= 0 && \n\
        uv.x - p.x_offset < p.w && uv.y - p.y_offset < p.h) \n\
    { \n\
        sfpvec4 rgba_src2 = load_rgba_src2(uv.x - p.x_offset, uv.y - p.y_offset, p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
        sfp alpha2 = rgba_src2.a*sfp(p.alpha); \n\
        sfp alpha = sfp(1.f)-(sfp(1.f)-rgba_src1.a)*(sfp(1.f)-alpha2); \n\
        result = sfpvec4(mix(rgba_src1.rgb, rgba_src2.rgb, alpha2), alpha); \n\
    } \n\
    else \n\
    { \n\
        result = rgba_src1; \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char AlphaBlending_overlay_data[] = 
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
SHADER_ALPHA_OVERLAY_MAIN
;


#define SHADER_PARAM_WITH_ALPHA_MAT_TO \
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
    int x_offset; \n\
    int y_offset; \n\
} p; \
"

#define SHADER_ALPHA_WITH_ALPHA_MAT_TO \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_dst = load_rgba_dst(uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    if (uv.x - p.x_offset >= 0 && uv.y - p.y_offset >= 0 && \n\
        uv.x - p.x_offset < p.w && uv.y - p.y_offset < p.h) \n\
    { \n\
        sfpvec4 rgba_src = load_rgba(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfp alpha = load_gray_alpha(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, 1, 0, DT_FLOAT32, 1); \n\
        result = sfpvec4(mix(rgba_dst.rgb, rgba_src.rgb, alpha), rgba_dst.a); \n\
        store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \
"

static const char AlphaBlending_alpha_mat_to_data[] = 
SHADER_HEADER
SHADER_PARAM_WITH_ALPHA_MAT_TO
SHADER_OUTPUT_RDWR_DATA
SHADER_INPUT_DATA
R"(
layout (binding = 8) readonly buffer alpha_int8      { uint8_t   alpha_data_int8[]; };
layout (binding = 9) readonly buffer alpha_int16     { uint16_t  alpha_data_int16[]; };
layout (binding = 10) readonly buffer alpha_float16   { float16_t alpha_data_float16[]; };
layout (binding = 11) readonly buffer alpha_float32   { float     alpha_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_RGBA_NAME(dst)
SHADER_LOAD_GRAY_NAME(alpha)
SHADER_STORE_RGBA
SHADER_ALPHA_WITH_ALPHA_MAT_TO
;

#define SHADER_PARAM_WITH_ALPHA_MAT_MASK \
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
} p; \
"

#define SHADER_ALPHA_WITH_ALPHA_MAT_MASK \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_src = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfp alpha = load_gray_alpha(uv.x, uv.y, p.w, p.h, 1, 0, DT_FLOAT32, 1); \n\
    result = sfpvec4(rgba_src.rgb, alpha); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char AlphaBlending_alpha_mat_mask_data[] = 
SHADER_HEADER
SHADER_PARAM_WITH_ALPHA_MAT_MASK
SHADER_OUTPUT_RDWR_DATA
SHADER_INPUT_DATA
R"(
layout (binding = 8) readonly buffer alpha_int8      { uint8_t   alpha_data_int8[]; };
layout (binding = 9) readonly buffer alpha_int16     { uint16_t  alpha_data_int16[]; };
layout (binding = 10) readonly buffer alpha_float16   { float16_t alpha_data_float16[]; };
layout (binding = 11) readonly buffer alpha_float32   { float     alpha_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_GRAY_NAME(alpha)
SHADER_STORE_RGBA
SHADER_ALPHA_WITH_ALPHA_MAT_MASK
;
