#pragma once
#include <imvk_mat_shader.h>

#define MASK_OPACITY_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int format; \n\
    int type; \n\
\n\
    int mask_cstep; \n\
    int mask_format; \n\
    int mask_type; \n\
    int mask_inverse; \n\
\n\
    float opacity; \n\
} p; \
"

#define MASK_OPACITY_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.w || uv.y >= p.h) \n\
        return; \n\
    ivec4 ch_mapping = color_format_mapping_vec4(p.format); \n\
    sfp alpha = load_rgba_channel(uv.x, uv.y, p.w, p.h, p.cstep, ch_mapping.a, p.type); \n\
    sfp mask_val = load_gray_mask_2(uv.x, uv.y, p.w, p.h, p.mask_cstep, p.mask_format, p.mask_type); \n\
    if (p.mask_inverse != 0) mask_val = sfp(1.f)-mask_val; \n\
    store_rgba_channel(alpha*mask_val*sfp(p.opacity), uv.x, uv.y, p.w, p.h, p.cstep, ch_mapping.a, p.type); \n\
} \
"

static const char MASK_OPACITY_SHADER[] = 
SHADER_HEADER
MASK_OPACITY_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer mask_int8       { uint8_t   mask_data_int8[]; };
layout (binding =  9) readonly buffer mask_int16      { uint16_t  mask_data_int16[]; };
layout (binding = 10) readonly buffer mask_float16    { float16_t mask_data_float16[]; };
layout (binding = 11) readonly buffer mask_float32    { float     mask_data_float32[]; };
)"
SHADER_LOAD_RGBA_CHANNEL
SHADER_LOAD_GRAY_NAME_2(mask)
SHADER_STORE_RGBA_CHANNEL
MASK_OPACITY_MAIN
;

#define MASK_OPACITY_INPLACE_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.w || uv.y >= p.h) \n\
        return; \n\
    ivec4 ch_mapping = color_format_mapping_vec4(p.format); \n\
    sfp alpha = load_rgba_dst_channel(uv.x, uv.y, p.w, p.h, p.cstep, ch_mapping.a, p.type); \n\
    sfp mask_val = load_gray_mask_2(uv.x, uv.y, p.w, p.h, p.mask_cstep, p.mask_format, p.mask_type); \n\
    if (p.mask_inverse != 0) mask_val = sfp(1.f)-mask_val; \n\
    store_rgba_channel(alpha*mask_val*sfp(p.opacity), uv.x, uv.y, p.w, p.h, p.cstep, ch_mapping.a, p.type); \n\
} \
"

static const char MASK_OPACITY_INPLACE_SHADER[] = 
SHADER_HEADER
MASK_OPACITY_PARAM
SHADER_OUTPUT_RDWR_DATA
R"(
layout (binding = 4) readonly buffer mask_int8       { uint8_t   mask_data_int8[]; };
layout (binding = 5) readonly buffer mask_int16      { uint16_t  mask_data_int16[]; };
layout (binding = 6) readonly buffer mask_float16    { float16_t mask_data_float16[]; };
layout (binding = 7) readonly buffer mask_float32    { float     mask_data_float32[]; };
)"
SHADER_LOAD_RGBA_NAME_CHANNEL(dst)
SHADER_LOAD_GRAY_NAME_2(mask)
SHADER_STORE_RGBA_CHANNEL
MASK_OPACITY_INPLACE_MAIN
;
