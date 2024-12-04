#pragma once
// for Shader benchmark
static const char glsl_p1_data[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int count = 0;
layout (constant_id = 1) const int loop = 1;
layout (binding = 0) readonly buffer a_blob { sfp a_blob_data[]; };
layout (binding = 1) readonly buffer b_blob { sfp b_blob_data[]; };
layout (binding = 2) writeonly buffer c_blob { sfp c_blob_data[]; };
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= count || gy >= 1 || gz >= 1)
        return;
    afp a = buffer_ld1(a_blob_data, gx);
    afp b = buffer_ld1(b_blob_data, gx);
    afp c = afp(1.f);
    for (int i = 0; i < loop; i++)
    {
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
    }
    buffer_st1(c_blob_data, gx, c);
}
)";

static const char glsl_p4_data[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int count = 0;
layout (constant_id = 1) const int loop = 1;
layout (binding = 0) readonly buffer a_blob { sfpvec4 a_blob_data[]; };
layout (binding = 1) readonly buffer b_blob { sfpvec4 b_blob_data[]; };
layout (binding = 2) writeonly buffer c_blob { sfpvec4 c_blob_data[]; };
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= count || gy >= 1 || gz >= 1)
        return;
    afpvec4 a = buffer_ld4(a_blob_data, gx);
    afpvec4 b = buffer_ld4(b_blob_data, gx);
    afpvec4 c = afpvec4(1.f);
    for (int i = 0; i < loop; i++)
    {
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
        c = a * c + b;
    }
    buffer_st4(c_blob_data, gx, c);
}
)";

static const char glsl_p8_data[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int count = 0;
layout (constant_id = 1) const int loop = 1;
layout (binding = 0) readonly buffer a_blob { sfpvec8 a_blob_data[]; };
layout (binding = 1) readonly buffer b_blob { sfpvec8 b_blob_data[]; };
layout (binding = 2) writeonly buffer c_blob { sfpvec8 c_blob_data[]; };
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= count || gy >= 1 || gz >= 1)
        return;
    afpvec8 a = buffer_ld8(a_blob_data, gx);
    afpvec8 b = buffer_ld8(b_blob_data, gx);
    afpvec8 c = afpvec8(afpvec4(1.f), afpvec4(1.f));
    for (int i = 0; i < loop; i++)
    {
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
        c[0] = a[0] * c[0] + b[0];
        c[1] = a[1] * c[1] + b[1];
    }
    buffer_st8(c_blob_data, gx, c);
}
)";

static const char glsl_copy[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#if ImVulkan_image_shader
layout (binding = 0) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 1, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int w;
    int h;
    int c;
    int cstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);

    if (gx >= p.w || gy >= p.h || gz >= p.c)
        return;
#if ImVulkan_image_shader
    image3d_cp1(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * p.cstep + gy * p.w + gx;
    buffer_cp1(top_blob_data, gi, bottom_blob_data, gi);
#endif
}
)";

// for shader common header
#define SHADER_HEADER \
"\
#version 450 \n\
#extension GL_EXT_shader_8bit_storage: require \n\
#extension GL_EXT_shader_16bit_storage: require \n\
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require \n\
#extension GL_EXT_shader_explicit_arithmetic_types: require \n\
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require \n\
#extension GL_EXT_shader_atomic_float: require \n\
#extension GL_EXT_shader_atomic_float2: require \n\
#define CF_GRAY     0 \n\
#define CF_BGR      1 \n\
#define CF_ABGR     2 \n\
#define CF_BGRA     3 \n\
#define CF_RGB      4 \n\
#define CF_ARGB     5 \n\
#define CF_RGBA     6 \n\
#define CF_YUV420   7 \n\
#define CF_YUV422   8 \n\
#define CF_YUV440   9 \n\
#define CF_YUV444   10 \n\
#define CF_YUVA     11 \n\
#define CF_NV12     12 \n\
#define CF_P010LE   13 \n\
#define CF_LAB      14 \n\
#define CF_HSV      15 \n\
#define CF_HLS      16 \n\
#define INTERPOLATE_NONE        0 \n\
#define INTERPOLATE_NEAREST     1 \n\
#define INTERPOLATE_BILINEAR    2 \n\
#define INTERPOLATE_BICUBIC     3 \n\
#define INTERPOLATE_AREA        4 \n\
#define INTERPOLATE_TRILINEAR   5 \n\
#define INTERPOLATE_TETRAHEDRAL 6 \n\
#define CS_SRGB     0 \n\
#define CS_BT601    1 \n\
#define CS_BT709    2 \n\
#define CS_BT2020   3 \n\
#define CS_HSV      4 \n\
#define CS_HLS      5 \n\
#define CS_CMY      6 \n\
#define CS_LAB      7 \n\
#define CR_FULL_RANGE   0 \n\
#define CR_NARROW_RANGE 1 \n\
#define DT_INT8         0 \n\
#define DT_INT16        1 \n\
#define DT_INT32        2 \n\
#define DT_INT64        3 \n\
#define DT_FLOAT16      4 \n\
#define DT_FLOAT32      5 \n\
#define DT_FLOAT64      6 \n\
#define DT_INT16_BE     7 \n\
#define M_PI sfp(3.141592653589793) \n\
#define M_SQRT1_2 sfp(0.707106781186547524401) \n\
ivec3 color_format_mapping_vec3(int format) \n\
{ \n\
    if (format == CF_BGR) \n\
        return ivec3(0, 1, 2); \n\
    else if (format == CF_RGB) \n\
        return ivec3(2, 1, 0); \n\
    else \n\
        return ivec3(0, 1, 2); \n\
} \n\
ivec4 color_format_mapping_vec4(int format) \n\
{ \n\
    if (format == CF_ABGR) \n\
        return ivec4(0, 1, 2, 3); \n\
    else if (format == CF_ARGB) \n\
        return ivec4(2, 1, 0, 3); \n\
    else if (format == CF_BGRA) \n\
        return ivec4(1, 2, 3, 0); \n\
    else if (format == CF_RGBA) \n\
        return ivec4(3, 2, 1, 0); \n\
    else \n\
        return ivec4(0, 1, 2, 3); \n\
} \n\
\
uint16_t BE2LE_16BIT(uint16_t a) \n\
{ \n\
    return (a << 8) | (a >> 8); \n\
} \n\
" // 73 lines

// shader binding for command data
#define SHADER_SRC_DATA \
" \n\
layout (binding = 0) readonly buffer src_int8       { uint8_t   src_data_int8[]; };   \n\
layout (binding = 1) readonly buffer src_int16      { uint16_t  src_data_int16[]; };  \n\
layout (binding = 2) readonly buffer src_float16    { float16_t src_data_float16[]; };\n\
layout (binding = 3) readonly buffer src_float32    { float     src_data_float32[]; };\n\
"

#define SHADER_INPUT_DATA \
" \n\
layout (binding = 4) readonly buffer src_int8       { uint8_t   src_data_int8[]; };   \n\
layout (binding = 5) readonly buffer src_int16      { uint16_t  src_data_int16[]; };  \n\
layout (binding = 6) readonly buffer src_float16    { float16_t src_data_float16[]; };\n\
layout (binding = 7) readonly buffer src_float32    { float     src_data_float32[]; };\n\
"

#define SHADER_INPUT2_DATA \
" \n\
layout (binding =  8) readonly buffer src2_int8       { uint8_t   src2_data_int8[]; };   \n\
layout (binding =  9) readonly buffer src2_int16      { uint16_t  src2_data_int16[]; };  \n\
layout (binding = 10) readonly buffer src2_float16    { float16_t src2_data_float16[]; };\n\
layout (binding = 11) readonly buffer src2_float32    { float     src2_data_float32[]; };\n\
"

#define SHADER_OUTPUT_DATA \
" \n\
layout (binding = 0) writeonly buffer dst_int8      { uint8_t   dst_data_int8[]; };   \n\
layout (binding = 1) writeonly buffer dst_int16     { uint16_t  dst_data_int16[]; };  \n\
layout (binding = 2) writeonly buffer dst_float16   { float16_t dst_data_float16[]; };\n\
layout (binding = 3) writeonly buffer dst_float32   { float     dst_data_float32[]; };\n\
"

#define SHADER_OUTPUT_RDWR_DATA \
" \n\
layout (binding = 0) buffer dst_int8      { uint8_t   dst_data_int8[]; };   \n\
layout (binding = 1) buffer dst_int16     { uint16_t  dst_data_int16[]; };  \n\
layout (binding = 2) buffer dst_float16   { float16_t dst_data_float16[]; };\n\
layout (binding = 3) buffer dst_float32   { float     dst_data_float32[]; };\n\
"

// 8 lines
#define SHADER_INPUT_OUTPUT_DATA \
SHADER_INPUT_DATA \
SHADER_OUTPUT_DATA

#define SHADER_INPUT2_OUTPUT_DATA \
SHADER_INPUT_DATA \
SHADER_INPUT2_DATA \
SHADER_OUTPUT_DATA

// Load data as gray
#define SHADER_LOAD_GRAY_INT8 \
" \n\
sfp load_gray_int8(int x, int y, int w, int cstep, int format, float scale) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(uint(src_data_int8[i_offset.x])) / sfp(scale); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_GRAY_INT16 \
" \n\
sfp load_gray_int16(int x, int y, int w, int cstep, int format, float scale) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(uint(src_data_int16[i_offset.x])) / sfp(scale); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_GRAY_INT16_BE \
" \n\
sfp load_gray_int16be(int x, int y, int w, int cstep, int format, float scale) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.x]))) / sfp(scale); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_GRAY_FLOAT16 \
" \n\
sfp load_gray_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(src_data_float16[i_offset.x]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_GRAY_FLOAT32 \
" \n\
sfp load_gray_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(src_data_float32[i_offset.x]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_GRAY \
SHADER_LOAD_GRAY_INT8 \
SHADER_LOAD_GRAY_INT16 \
SHADER_LOAD_GRAY_INT16_BE \
SHADER_LOAD_GRAY_FLOAT16 \
SHADER_LOAD_GRAY_FLOAT32 \
" \n\
sfp load_gray(int x, int y, int w, int h, int cstep, int format, int type, float scale) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_gray_int8(x, y, w, cstep, format, scale); \n\
    else if (type == DT_INT16) \n\
        return load_gray_int16(x, y, w, cstep, format, scale); \n\
    else if (type == DT_INT16_BE) \n\
        return load_gray_int16be(x, y, w, cstep, format, scale); \n\
    else if (type == DT_FLOAT16) \n\
        return load_gray_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_gray_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfp(0.f); \n\
} \n\
" // 46 lines

#define SHADER_LOAD_GRAY_2 \
SHADER_LOAD_GRAY_INT8 \
SHADER_LOAD_GRAY_INT16 \
SHADER_LOAD_GRAY_INT16_BE \
SHADER_LOAD_GRAY_FLOAT16 \
SHADER_LOAD_GRAY_FLOAT32 \
" \n\
sfp load_gray_2(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_gray_int8(x, y, w, cstep, format, 255.f); \n\
    else if (type == DT_INT16) \n\
        return load_gray_int16(x, y, w, cstep, format, 65535.f); \n\
    else if (type == DT_INT16_BE) \n\
        return load_gray_int16be(x, y, w, cstep, format, 65535.f); \n\
    else if (type == DT_FLOAT16) \n\
        return load_gray_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_gray_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfp(0.f); \n\
} \n\
"

// Load data as rgb, if cstep is 4, means input is rgba
#define SHADER_LOAD_RGB_INT8 \
" \n\
sfpvec3 load_rgb_int8(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec3 rgb_in = sfpvec3(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    rgb_in.r = sfp(uint(src_data_int8[i_offset.r])) / sfp(255.f); \n\
    rgb_in.g = sfp(uint(src_data_int8[i_offset.g])) / sfp(255.f); \n\
    rgb_in.b = sfp(uint(src_data_int8[i_offset.b])) / sfp(255.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGB_INT16 \
" \n\
sfpvec3 load_rgb_int16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec3 rgb_in = sfpvec3(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    rgb_in.r = sfp(uint(src_data_int16[i_offset.r])) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(src_data_int16[i_offset.g])) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(src_data_int16[i_offset.b])) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGB_INT16_BE \
" \n\
sfpvec3 load_rgb_int16be(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec3 rgb_in = sfpvec3(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    rgb_in.r = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.r]))) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.g]))) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.b]))) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGB_FLOAT16 \
" \n\
sfpvec3 load_rgb_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec3 rgb_in = sfpvec3(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    rgb_in.r = sfp(src_data_float16[i_offset.r]); \n\
    rgb_in.g = sfp(src_data_float16[i_offset.g]); \n\
    rgb_in.b = sfp(src_data_float16[i_offset.b]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGB_FLOAT32 \
" \n\
sfpvec3 load_rgb_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec3 rgb_in = sfpvec3(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    rgb_in.r = sfp(src_data_float32[i_offset.r]); \n\
    rgb_in.g = sfp(src_data_float32[i_offset.g]); \n\
    rgb_in.b = sfp(src_data_float32[i_offset.b]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGB \
SHADER_LOAD_RGB_INT8 \
SHADER_LOAD_RGB_INT16 \
SHADER_LOAD_RGB_INT16_BE \
SHADER_LOAD_RGB_FLOAT16 \
SHADER_LOAD_RGB_FLOAT32 \
" \n\
sfpvec3 load_rgb(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_rgb_int8(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load_rgb_int16(x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        return load_rgb_int16be(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load_rgb_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_rgb_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec3(0.f); \n\
} \n\
" // 54 lines

// Load data as rgba
#define SHADER_LOAD_RGBA_INT8 \
" \n\
sfpvec4 load_rgba_int8(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(src_data_int8[i_offset.r])) / sfp(255.f); \n\
    rgb_in.g = sfp(uint(src_data_int8[i_offset.g])) / sfp(255.f); \n\
    rgb_in.b = sfp(uint(src_data_int8[i_offset.b])) / sfp(255.f); \n\
    rgb_in.a = sfp(uint(src_data_int8[i_offset.a])) / sfp(255.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_INT16 \
" \n\
sfpvec4 load_rgba_int16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(src_data_int16[i_offset.r])) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(src_data_int16[i_offset.g])) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(src_data_int16[i_offset.b])) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint(src_data_int16[i_offset.a])) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_INT16_BE \
" \n\
sfpvec4 load_rgba_int16be(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.r]))) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.g]))) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.b]))) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint(BE2LE_16BIT(src_data_int16[i_offset.a]))) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_FLOAT16 \
" \n\
sfpvec4 load_rgba_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(src_data_float16[i_offset.r]); \n\
    rgb_in.g = sfp(src_data_float16[i_offset.g]); \n\
    rgb_in.b = sfp(src_data_float16[i_offset.b]); \n\
    rgb_in.a = sfp(src_data_float16[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_FLOAT32 \
" \n\
sfpvec4 load_rgba_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(src_data_float32[i_offset.r]); \n\
    rgb_in.g = sfp(src_data_float32[i_offset.g]); \n\
    rgb_in.b = sfp(src_data_float32[i_offset.b]); \n\
    rgb_in.a = sfp(src_data_float32[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA \
SHADER_LOAD_RGBA_INT8 \
SHADER_LOAD_RGBA_INT16 \
SHADER_LOAD_RGBA_INT16_BE \
SHADER_LOAD_RGBA_FLOAT16 \
SHADER_LOAD_RGBA_FLOAT32 \
" \n\
sfpvec4 load_rgba(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_rgba_int8(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load_rgba_int16(x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        return load_rgba_int16be(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load_rgba_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_rgba_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec4(0.f); \n\
} \n\
" // 58 lines

// Load only the specific channel (located by 'ch_offset') from rgba
#define SHADER_LOAD_RGBA_CHANNEL_INT8 \
" \n\
sfp load_rgba_channel_int8(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(uint(src_data_int8[i_offset])) / sfp(255.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_CHANNEL_INT16 \
" \n\
sfp load_rgba_channel_int16(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(uint(src_data_int16[i_offset])) / sfp(65535.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_CHANNEL_INT16_BE \
" \n\
sfp load_rgba_channel_int16be(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(uint(BE2LE_16BIT(src_data_int16[i_offset]))) / sfp(65535.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_CHANNEL_FLOAT16 \
" \n\
sfp load_rgba_channel_float16(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(src_data_float16[i_offset]); \n\
} \n\
"

#define SHADER_LOAD_RGBA_CHANNEL_FLOAT32 \
" \n\
sfp load_rgba_channel_float32(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(src_data_float32[i_offset]); \n\
} \n\
"

#define SHADER_LOAD_RGBA_CHANNEL \
SHADER_LOAD_RGBA_CHANNEL_INT8 \
SHADER_LOAD_RGBA_CHANNEL_INT16 \
SHADER_LOAD_RGBA_CHANNEL_INT16_BE \
SHADER_LOAD_RGBA_CHANNEL_FLOAT16 \
SHADER_LOAD_RGBA_CHANNEL_FLOAT32 \
" \n\
sfp load_rgba_channel(int x, int y, int w, int h, int cstep, int ch_offset, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_rgba_channel_int8(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_INT16) \n\
        return load_rgba_channel_int16(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_INT16_BE) \n\
        return load_rgba_channel_int16be(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_FLOAT16) \n\
        return load_rgba_channel_float16(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_FLOAT32) \n\
        return load_rgba_channel_float32(x, y, w, cstep, ch_offset); \n\
    else \n\
        return sfp(0.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_INT8(src) \
" \n\
sfpvec4 load_rgba_int8_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint("#src"_data_int8[i_offset.r])) / sfp(255.f); \n\
    rgb_in.g = sfp(uint("#src"_data_int8[i_offset.g])) / sfp(255.f); \n\
    rgb_in.b = sfp(uint("#src"_data_int8[i_offset.b])) / sfp(255.f); \n\
    rgb_in.a = sfp(uint("#src"_data_int8[i_offset.a])) / sfp(255.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_INT16(src) \
" \n\
sfpvec4 load_rgba_int16_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint("#src"_data_int16[i_offset.r])) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint("#src"_data_int16[i_offset.g])) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint("#src"_data_int16[i_offset.b])) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint("#src"_data_int16[i_offset.a])) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_INT16_BE(src) \
" \n\
sfpvec4 load_rgba_int16_"#src"be(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(BE2LE_16BIT("#src"_data_int16[i_offset.r]))) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(BE2LE_16BIT("#src"_data_int16[i_offset.g]))) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(BE2LE_16BIT("#src"_data_int16[i_offset.b]))) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint(BE2LE_16BIT("#src"_data_int16[i_offset.a]))) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_FLOAT16(src) \
" \n\
sfpvec4 load_rgba_float16_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp("#src"_data_float16[i_offset.r]); \n\
    rgb_in.g = sfp("#src"_data_float16[i_offset.g]); \n\
    rgb_in.b = sfp("#src"_data_float16[i_offset.b]); \n\
    rgb_in.a = sfp("#src"_data_float16[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_FLOAT32(src) \
" \n\
sfpvec4 load_rgba_float32_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp("#src"_data_float32[i_offset.r]); \n\
    rgb_in.g = sfp("#src"_data_float32[i_offset.g]); \n\
    rgb_in.b = sfp("#src"_data_float32[i_offset.b]); \n\
    rgb_in.a = sfp("#src"_data_float32[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME(src) \
SHADER_LOAD_RGBA_NAME_INT8(src) \
SHADER_LOAD_RGBA_NAME_INT16(src) \
SHADER_LOAD_RGBA_NAME_INT16_BE(src) \
SHADER_LOAD_RGBA_NAME_FLOAT16(src) \
SHADER_LOAD_RGBA_NAME_FLOAT32(src) \
" \n\
sfpvec4 load_rgba_"#src"(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_rgba_int8_"#src"(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load_rgba_int16_"#src"(x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        return load_rgba_int16_"#src"be(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load_rgba_float16_"#src"(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_rgba_float32_"#src"(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec4(0.f); \n\
} \n\
"

// Load only the specific channel (located by 'ch_offset') from rgba
#define SHADER_LOAD_RGBA_NAME_CHANNEL_INT8(src) \
" \n\
sfp load_rgba_"#src"_channel_int8(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(uint("#src"_data_int8[i_offset])) / sfp(255.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_CHANNEL_INT16(src) \
" \n\
sfp load_rgba_"#src"_channel_int16(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(uint("#src"_data_int16[i_offset])) / sfp(65535.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_CHANNEL_INT16_BE(src) \
" \n\
sfp load_rgba_"#src"_channel_int16be(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp(uint(BE2LE_16BIT("#src"_data_int16[i_offset]))) / sfp(65535.f); \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_CHANNEL_FLOAT16(src) \
" \n\
sfp load_rgba_"#src"_channel_float16(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp("#src"_data_float16[i_offset]); \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_CHANNEL_FLOAT32(src) \
" \n\
sfp load_rgba_"#src"_channel_float32(int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int i_offset = (y * w + x) * cstep + ch_offset; \n\
    return sfp("#src"_data_float32[i_offset]); \n\
} \n\
"

#define SHADER_LOAD_RGBA_NAME_CHANNEL(src) \
SHADER_LOAD_RGBA_NAME_CHANNEL_INT8(src) \
SHADER_LOAD_RGBA_NAME_CHANNEL_INT16(src) \
SHADER_LOAD_RGBA_NAME_CHANNEL_INT16_BE(src) \
SHADER_LOAD_RGBA_NAME_CHANNEL_FLOAT16(src) \
SHADER_LOAD_RGBA_NAME_CHANNEL_FLOAT32(src) \
" \n\
sfp load_rgba_"#src"_channel(int x, int y, int w, int h, int cstep, int ch_offset, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_rgba_"#src"_channel_int8(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_INT16) \n\
        return load_rgba_"#src"_channel_int16(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_INT16_BE) \n\
        return load_rgba_"#src"_channel_int16be(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_FLOAT16) \n\
        return load_rgba_"#src"_channel_float16(x, y, w, cstep, ch_offset); \n\
    else if (type == DT_FLOAT32) \n\
        return load_rgba_"#src"_channel_float32(x, y, w, cstep, ch_offset); \n\
    else \n\
        return sfp(0.f); \n\
} \n\
"

// Load gray data with name
#define SHADER_LOAD_GRAY_NAME_INT8(src) \
" \n\
sfp load_gray_int8_"#src"(int x, int y, int w, int cstep, int format, float scale) \n\
{ \n\
    int i_offset = (y * w + x) * cstep; \n\
    return sfp(uint("#src"_data_int8[i_offset])) / sfp(scale); \n\
} \n\
"

#define SHADER_LOAD_GRAY_NAME_INT16(src) \
" \n\
sfp load_gray_int16_"#src"(int x, int y, int w, int cstep, int format, float scale) \n\
{ \n\
    int i_offset = (y * w + x) * cstep; \n\
    return sfp(uint("#src"_data_int16[i_offset])) / sfp(scale); \n\
} \n\
"

#define SHADER_LOAD_GRAY_NAME_INT16_BE(src) \
" \n\
sfp load_gray_int16be_"#src"(int x, int y, int w, int cstep, int format, float scale) \n\
{ \n\
    int i_offset = (y * w + x) * cstep; \n\
    return sfp(uint(BE2LE_16BIT("#src"_data_int16[i_offset]))) / sfp(scale); \n\
} \n\
"

#define SHADER_LOAD_GRAY_NAME_FLOAT16(src) \
" \n\
sfp load_gray_float16_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    int i_offset = (y * w + x) * cstep; \n\
    return sfp("#src"_data_float16[i_offset]); \n\
} \n\
"

#define SHADER_LOAD_GRAY_NAME_FLOAT32(src) \
" \n\
sfp load_gray_float32_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    int i_offset = (y * w + x) * cstep; \n\
    return sfp("#src"_data_float32[i_offset]); \n\
} \n\
"

#define SHADER_LOAD_GRAY_NAME(src) \
SHADER_LOAD_GRAY_NAME_INT8(src) \
SHADER_LOAD_GRAY_NAME_INT16(src) \
SHADER_LOAD_GRAY_NAME_INT16_BE(src) \
SHADER_LOAD_GRAY_NAME_FLOAT16(src) \
SHADER_LOAD_GRAY_NAME_FLOAT32(src) \
" \n\
sfp load_gray_"#src"(int x, int y, int w, int h, int cstep, int format, int type, float scale) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_gray_int8_"#src"(x, y, w, cstep, format, scale); \n\
    else if (type == DT_INT16) \n\
        return load_gray_int16_"#src"(x, y, w, cstep, format, scale); \n\
    else if (type == DT_INT16_BE) \n\
        return load_gray_int16be_"#src"(x, y, w, cstep, format, scale); \n\
    else if (type == DT_FLOAT16) \n\
        return load_gray_float16_"#src"(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_gray_float32_"#src"(x, y, w, cstep, format); \n\
    else \n\
        return sfp(0.f); \n\
} \n\
"

#define SHADER_LOAD_GRAY_NAME_2(src) \
SHADER_LOAD_GRAY_NAME_INT8(src) \
SHADER_LOAD_GRAY_NAME_INT16(src) \
SHADER_LOAD_GRAY_NAME_INT16_BE(src) \
SHADER_LOAD_GRAY_NAME_FLOAT16(src) \
SHADER_LOAD_GRAY_NAME_FLOAT32(src) \
" \n\
sfp load_gray_"#src"_2(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_gray_int8_"#src"(x, y, w, cstep, format, 255.f); \n\
    else if (type == DT_INT16) \n\
        return load_gray_int16_"#src"(x, y, w, cstep, format, 65535.f); \n\
    else if (type == DT_INT16_BE) \n\
        return load_gray_int16be_"#src"(x, y, w, cstep, format, 65535.f); \n\
    else if (type == DT_FLOAT16) \n\
        return load_gray_float16_"#src"(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_gray_float32_"#src"(x, y, w, cstep, format); \n\
    else \n\
        return sfp(0.f); \n\
} \n\
"

// Load data from dst as rgba
#define SHADER_LOAD_DST_RGBA_INT8 \
" \n\
sfpvec4 load_dst_rgba_int8(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(dst_data_int8[i_offset.r])) / sfp(255.f); \n\
    rgb_in.g = sfp(uint(dst_data_int8[i_offset.g])) / sfp(255.f); \n\
    rgb_in.b = sfp(uint(dst_data_int8[i_offset.b])) / sfp(255.f); \n\
    rgb_in.a = sfp(uint(dst_data_int8[i_offset.a])) / sfp(255.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_DST_RGBA_INT16 \
" \n\
sfpvec4 load_dst_rgba_int16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(dst_data_int16[i_offset.r])) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(dst_data_int16[i_offset.g])) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(dst_data_int16[i_offset.b])) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint(dst_data_int16[i_offset.a])) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_DST_RGBA_INT16_BE \
" \n\
sfpvec4 load_dst_rgba_int16be(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(uint(BE2LE_16BIT(dst_data_int16[i_offset.r]))) / sfp(65535.f); \n\
    rgb_in.g = sfp(uint(BE2LE_16BIT(dst_data_int16[i_offset.g]))) / sfp(65535.f); \n\
    rgb_in.b = sfp(uint(BE2LE_16BIT(dst_data_int16[i_offset.b]))) / sfp(65535.f); \n\
    rgb_in.a = sfp(uint(BE2LE_16BIT(dst_data_int16[i_offset.a]))) / sfp(65535.f); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_DST_RGBA_FLOAT16 \
" \n\
sfpvec4 load_dst_rgba_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(dst_data_float16[i_offset.r]); \n\
    rgb_in.g = sfp(dst_data_float16[i_offset.g]); \n\
    rgb_in.b = sfp(dst_data_float16[i_offset.b]); \n\
    rgb_in.a = sfp(dst_data_float16[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_DST_RGBA_FLOAT32 \
" \n\
sfpvec4 load_dst_rgba_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp(dst_data_float32[i_offset.r]); \n\
    rgb_in.g = sfp(dst_data_float32[i_offset.g]); \n\
    rgb_in.b = sfp(dst_data_float32[i_offset.b]); \n\
    rgb_in.a = sfp(dst_data_float32[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_DST_RGBA \
SHADER_LOAD_DST_RGBA_INT8 \
SHADER_LOAD_DST_RGBA_INT16 \
SHADER_LOAD_DST_RGBA_INT16_BE \
SHADER_LOAD_DST_RGBA_FLOAT16 \
SHADER_LOAD_DST_RGBA_FLOAT32 \
" \n\
sfpvec4 load_dst_rgba(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        return load_dst_rgba_int8(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load_dst_rgba_int16(x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        return load_dst_rgba_int16be(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load_dst_rgba_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_dst_rgba_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec4(0.f); \n\
} \n\
" // 58 lines

// Store data as gray
#define SHADER_STORE_GRAY_INT8 \
" \n\
void store_gray_int8(sfp val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    dst_data_int8[o_offset.x] = uint8_t(clamp(uint(floor(val * sfp(255.0f))), 0, 255)); \n\
} \n\
"

#define SHADER_STORE_GRAY_INT16 \
" \n\
void store_gray_int16(sfp val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    dst_data_int16[o_offset.x] = uint16_t(clamp(uint(floor(val * sfp(65535.0f))), 0, 65535)); \n\
} \n\
"

#define SHADER_STORE_GRAY_INT16_BE \
" \n\
void store_gray_int16be(sfp val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    dst_data_int16[o_offset.x] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val * sfp(65535.0f))), 0, 65535))); \n\
} \n\
"


#define SHADER_STORE_GRAY_FLOAT16 \
" \n\
void store_gray_float16(sfp val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    dst_data_float16[o_offset.x] = float16_t(clamp(val, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_GRAY_FLOAT32 \
" \n\
void store_gray_float32(sfp val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    dst_data_float32[o_offset.x] = float(clamp(val, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_GRAY \
SHADER_STORE_GRAY_INT8 \
SHADER_STORE_GRAY_INT16 \
SHADER_STORE_GRAY_INT16_BE \
SHADER_STORE_GRAY_FLOAT16 \
SHADER_STORE_GRAY_FLOAT32 \
" \n\
void store_gray(sfp val, int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        store_gray_int8(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        store_gray_int16(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        store_gray_int16be(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        store_gray_float16(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_gray_float32(val, x, y, w, cstep, format); \n\
} \n\
" // 36 lines

// Store data as rgb
#define SHADER_STORE_RGB_INT8 \
" \n\
void store_rgb_int8(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_int8[o_offset.r] = uint8_t(clamp(uint(floor(val.r * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.g] = uint8_t(clamp(uint(floor(val.g * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.b] = uint8_t(clamp(uint(floor(val.b * sfp(255.0f))), 0, 255)); \n\
} \n\
"

#define SHADER_STORE_RGB_INT16 \
" \n\
void store_rgb_int16(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_int16[o_offset.r] = uint16_t(clamp(uint(floor(val.r * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.g] = uint16_t(clamp(uint(floor(val.g * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.b] = uint16_t(clamp(uint(floor(val.b * sfp(65535.0f))), 0, 65535)); \n\
} \n\
"

#define SHADER_STORE_RGB_INT16_BE \
" \n\
void store_rgb_int16be(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_int16[o_offset.r] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.r * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.g] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.g * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.b] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.b * sfp(65535.0f))), 0, 65535))); \n\
} \n\
"

#define SHADER_STORE_RGB_FLOAT16 \
" \n\
void store_rgb_float16(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_float16[o_offset.r] = float16_t(clamp(val.r, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.g] = float16_t(clamp(val.g, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.b] = float16_t(clamp(val.b, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGB_FLOAT32 \
" \n\
void store_rgb_float32(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_float32[o_offset.r] = float(clamp(val.r, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.g] = float(clamp(val.g, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.b] = float(clamp(val.b, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGB \
SHADER_STORE_RGB_INT8 \
SHADER_STORE_RGB_INT16 \
SHADER_STORE_RGB_INT16_BE \
SHADER_STORE_RGB_FLOAT16 \
SHADER_STORE_RGB_FLOAT32 \
" \n\
void store_rgb(sfpvec3 val, int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        store_rgb_int8(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        store_rgb_int16(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        store_rgb_int16be(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        store_rgb_float16(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgb_float32(val, x, y, w, cstep, format); \n\
} \n\
" // 44 lines

// Store data as rgba
#define SHADER_STORE_RGBA_INT8 \
" \n\
void store_rgba_int8(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_int8[o_offset.r] = uint8_t(clamp(uint(floor(val.r * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.g] = uint8_t(clamp(uint(floor(val.g * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.b] = uint8_t(clamp(uint(floor(val.b * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.a] = uint8_t(clamp(uint(floor(val.a * sfp(255.0f))), 0, 255)); \n\
} \n\
"

#define SHADER_STORE_RGBA_INT16 \
" \n\
void store_rgba_int16(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_int16[o_offset.r] = uint16_t(clamp(uint(floor(val.r * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.g] = uint16_t(clamp(uint(floor(val.g * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.b] = uint16_t(clamp(uint(floor(val.b * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.a] = uint16_t(clamp(uint(floor(val.a * sfp(65535.0f))), 0, 65535)); \n\
} \n\
"

#define SHADER_STORE_RGBA_INT16_BE \
" \n\
void store_rgba_int16be(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_int16[o_offset.r] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.r * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.g] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.g * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.b] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.b * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.a] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.a * sfp(65535.0f))), 0, 65535))); \n\
} \n\
"

#define SHADER_STORE_RGBA_FLOAT16 \
" \n\
void store_rgba_float16(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_float16[o_offset.r] = float16_t(clamp(val.r, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.g] = float16_t(clamp(val.g, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.b] = float16_t(clamp(val.b, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.a] = float16_t(clamp(val.a, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGBA_FLOAT32 \
" \n\
void store_rgba_float32(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_float32[o_offset.r] = float(clamp(val.r, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.g] = float(clamp(val.g, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.b] = float(clamp(val.b, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.a] = float(clamp(val.a, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGBA \
SHADER_STORE_RGBA_INT8 \
SHADER_STORE_RGBA_INT16 \
SHADER_STORE_RGBA_INT16_BE \
SHADER_STORE_RGBA_FLOAT16 \
SHADER_STORE_RGBA_FLOAT32 \
" \n\
void store_rgba(sfpvec4 val, int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        store_rgba_int8(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        store_rgba_int16(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        store_rgba_int16be(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        store_rgba_float16(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgba_float32(val, x, y, w, cstep, format); \n\
} \n\
" // 48 lines

// Store only the specific channel (located by 'ch_offset') to rgba buffer
#define SHADER_STORE_RGBA_CHANNEL_INT8 \
" \n\
void store_rgba_channel_int8(sfp val, int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int o_offset = (y * w + x) * cstep + ch_offset; \n\
    dst_data_int8[o_offset] = uint8_t(clamp(uint(floor(val * sfp(255.0f))), 0, 255)); \n\
} \n\
"

#define SHADER_STORE_RGBA_CHANNEL_INT16 \
" \n\
void store_rgba_channel_int16(sfp val, int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int o_offset = (y * w + x) * cstep + ch_offset; \n\
    dst_data_int16[o_offset] = uint16_t(clamp(uint(floor(val * sfp(65535.0f))), 0, 65535)); \n\
} \n\
"

#define SHADER_STORE_RGBA_CHANNEL_INT16_BE \
" \n\
void store_rgba_channel_int16be(sfp val, int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int o_offset = (y * w + x) * cstep + ch_offset; \n\
    dst_data_int16[o_offset] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val * sfp(65535.0f))), 0, 65535))); \n\
} \n\
"

#define SHADER_STORE_RGBA_CHANNEL_FLOAT16 \
" \n\
void store_rgba_channel_float16(sfp val, int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int o_offset = (y * w + x) * cstep + ch_offset; \n\
    dst_data_float16[o_offset] = float16_t(clamp(val, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGBA_CHANNEL_FLOAT32 \
" \n\
void store_rgba_channel_float32(sfp val, int x, int y, int w, int cstep, int ch_offset) \n\
{ \n\
    int o_offset = (y * w + x) * cstep + ch_offset; \n\
    dst_data_float32[o_offset] = float(clamp(val, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGBA_CHANNEL \
SHADER_STORE_RGBA_CHANNEL_INT8 \
SHADER_STORE_RGBA_CHANNEL_INT16 \
SHADER_STORE_RGBA_CHANNEL_INT16_BE \
SHADER_STORE_RGBA_CHANNEL_FLOAT16 \
SHADER_STORE_RGBA_CHANNEL_FLOAT32 \
" \n\
void store_rgba_channel(sfp val, int x, int y, int w, int h, int cstep, int ch_offset, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_INT8) \n\
        store_rgba_channel_int8(val, x, y, w, cstep, ch_offset); \n\
    else if (type == DT_INT16) \n\
        store_rgba_channel_int16(val, x, y, w, cstep, ch_offset); \n\
    else if (type == DT_INT16_BE) \n\
        store_rgba_channel_int16be(val, x, y, w, cstep, ch_offset); \n\
    else if (type == DT_FLOAT16) \n\
        store_rgba_channel_float16(val, x, y, w, cstep, ch_offset); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgba_channel_float32(val, x, y, w, cstep, ch_offset); \n\
} \n\
" // 48 lines

// Store data to RGBA Side By Side
#define SHADER_STORE_RGBA_INT8_SIDE_BY_SIDE \
" \n\
void store_rgba_int8_side_by_side(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    int planar = x % 2 == 0 ? x / 2 : x / 2 + p.w / 2; \n\
    ivec4 o_offset = (y * w + planar) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_int8[o_offset.r] = uint8_t(clamp(uint(floor(val.r * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.g] = uint8_t(clamp(uint(floor(val.g * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.b] = uint8_t(clamp(uint(floor(val.b * sfp(255.0f))), 0, 255)); \n\
    dst_data_int8[o_offset.a] = uint8_t(clamp(uint(floor(val.a * sfp(255.0f))), 0, 255)); \n\
} \n\
"

#define SHADER_STORE_RGBA_INT16_SIDE_BY_SIDE \
" \n\
void store_rgba_int16_side_by_side(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    int planar = x % 2 == 0 ? x / 2 : x / 2 + p.w / 2; \n\
    ivec4 o_offset = (y * w + planar) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_int16[o_offset.r] = uint16_t(clamp(uint(floor(val.r * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.g] = uint16_t(clamp(uint(floor(val.g * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.b] = uint16_t(clamp(uint(floor(val.b * sfp(65535.0f))), 0, 65535)); \n\
    dst_data_int16[o_offset.a] = uint16_t(clamp(uint(floor(val.a * sfp(65535.0f))), 0, 65535)); \n\
} \n\
"

#define SHADER_STORE_RGBA_INT16_BE_SIDE_BY_SIDE \
" \n\
void store_rgba_int16be_side_by_side(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    int planar = x % 2 == 0 ? x / 2 : x / 2 + p.w / 2; \n\
    ivec4 o_offset = (y * w + planar) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_int16[o_offset.r] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.r * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.g] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.g * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.b] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.b * sfp(65535.0f))), 0, 65535))); \n\
    dst_data_int16[o_offset.a] = BE2LE_16BIT(uint16_t(clamp(uint(floor(val.a * sfp(65535.0f))), 0, 65535))); \n\
} \n\
"

#define SHADER_STORE_RGBA_FLOAT16_SIDE_BY_SIDE \
" \n\
void store_rgba_float16_side_by_side(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    int planar = x % 2 == 0 ? x / 2 : x / 2 + p.w / 2; \n\
    ivec4 o_offset = (y * w + planar) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_float16[o_offset.r] = float16_t(clamp(val.r, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.g] = float16_t(clamp(val.g, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.b] = float16_t(clamp(val.b, sfp(0.f), sfp(1.f))); \n\
    dst_data_float16[o_offset.a] = float16_t(clamp(val.a, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGBA_FLOAT32_SIDE_BY_SIDE \
" \n\
void store_rgba_float32_side_by_side(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    int planar = x % 2 == 0 ? x / 2 : x / 2 + p.w / 2; \n\
    ivec4 o_offset = (y * w + planar) * cstep + color_format_mapping_vec4(format); \n\
    dst_data_float32[o_offset.r] = float(clamp(val.r, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.g] = float(clamp(val.g, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.b] = float(clamp(val.b, sfp(0.f), sfp(1.f))); \n\
    dst_data_float32[o_offset.a] = float(clamp(val.a, sfp(0.f), sfp(1.f))); \n\
} \n\
"

#define SHADER_STORE_RGBA_SIDE_BY_SIDE \
SHADER_STORE_RGBA_INT8_SIDE_BY_SIDE \
SHADER_STORE_RGBA_INT16_SIDE_BY_SIDE \
SHADER_STORE_RGBA_INT16_BE_SIDE_BY_SIDE \
SHADER_STORE_RGBA_FLOAT16_SIDE_BY_SIDE \
SHADER_STORE_RGBA_FLOAT32_SIDE_BY_SIDE \
" \n\
void store_rgba_side_by_side(sfpvec4 val, int x, int y, int w, int cstep, int format, int type) \n\
{ \n\
    if (type == DT_INT8) \n\
        store_rgba_int8_side_by_side(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        store_rgba_int16_side_by_side(val, x, y, w, cstep, format); \n\
    else if (type == DT_INT16_BE) \n\
        store_rgba_int16be_side_by_side(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        store_rgba_float16_side_by_side(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgba_float32_side_by_side(val, x, y, w, cstep, format); \n\
} \n\
" // 52 lines

// Store data as float rgb without clamp
#define SHADER_STORE_RGB_FLOAT16_NO_CLAMP \
" \n\
void store_rgb_float16_no_clamp(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_float16[o_offset.r] = float16_t(val.r); \n\
    dst_data_float16[o_offset.g] = float16_t(val.g); \n\
    dst_data_float16[o_offset.b] = float16_t(val.b); \n\
} \
"

#define SHADER_STORE_RGB_FLOAT32_NO_CLAMP \
" \n\
void store_rgb_float32_no_clamp(sfpvec3 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec3 o_offset = (y * w + x) * cstep + color_format_mapping_vec3(format); \n\
    dst_data_float32[o_offset.r] = float(val.r); \n\
    dst_data_float32[o_offset.g] = float(val.g); \n\
    dst_data_float32[o_offset.b] = float(val.b); \n\
} \n\
"

#define SHADER_STORE_RGB_FLOAT_NO_CLAMP \
SHADER_STORE_RGB_FLOAT16_NO_CLAMP \
SHADER_STORE_RGB_FLOAT32_NO_CLAMP \
" \n\
void store_rgb_float_no_clamp(sfpvec3 val, int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_FLOAT16) \n\
        store_rgb_float16_no_clamp(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgb_float32_no_clamp(val, x, y, w, cstep, format); \n\
} \n\
" // 44 lines

// Store data as float rgba without clamp
#define SHADER_STORE_RGBA_FLOAT16_NO_CLAMP(dst) \
" \n\
void store_rgba_float16_no_clamp_"#dst"(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    "#dst"_data_float16[o_offset.r] = float16_t(val.r); \n\
    "#dst"_data_float16[o_offset.g] = float16_t(val.g); \n\
    "#dst"_data_float16[o_offset.b] = float16_t(val.b); \n\
    "#dst"_data_float16[o_offset.a] = float16_t(val.a); \n\
} \n\
"

#define SHADER_STORE_RGBA_FLOAT32_NO_CLAMP(dst) \
" \n\
void store_rgba_float32_no_clamp_"#dst"(sfpvec4 val, int x, int y, int w, int cstep, int format) \n\
{ \n\
    ivec4 o_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    "#dst"_data_float32[o_offset.r] = float(val.r); \n\
    "#dst"_data_float32[o_offset.g] = float(val.g); \n\
    "#dst"_data_float32[o_offset.b] = float(val.b); \n\
    "#dst"_data_float32[o_offset.a] = float(val.a); \n\
} \n\
"

#define SHADER_STORE_RGBA_FLOAT_NO_CLAMP(dst) \
SHADER_STORE_RGBA_FLOAT16_NO_CLAMP(dst) \
SHADER_STORE_RGBA_FLOAT32_NO_CLAMP(dst) \
" \n\
void store_rgba_float_no_clamp_"#dst"(sfpvec4 val, int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_FLOAT16) \n\
        store_rgba_float16_no_clamp_"#dst"(val, x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        store_rgba_float32_no_clamp_"#dst"(val, x, y, w, cstep, format); \n\
} \n\
" // 46 lines


#define SHADER_LOAD_FLOAT16_RGBA(src) \
" \n\
sfpvec4 load_float16_rgba_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp("#src"_data[i_offset.r]); \n\
    rgb_in.g = sfp("#src"_data[i_offset.g]); \n\
    rgb_in.b = sfp("#src"_data[i_offset.b]); \n\
    rgb_in.a = sfp("#src"_data[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_FLOAT32_RGBA(src) \
" \n\
sfpvec4 load_float32_rgba_"#src"(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfpvec4 rgb_in = sfpvec4(0.f); \n\
    ivec4 i_offset = (y * w + x) * cstep + color_format_mapping_vec4(format); \n\
    rgb_in.r = sfp("#src"_data[i_offset.r]); \n\
    rgb_in.g = sfp("#src"_data[i_offset.g]); \n\
    rgb_in.b = sfp("#src"_data[i_offset.b]); \n\
    rgb_in.a = sfp("#src"_data[i_offset.a]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_FLOAT_RGBA(src) \
SHADER_LOAD_FLOAT16_RGBA(src) \
SHADER_LOAD_FLOAT32_RGBA(src) \
" \n\
sfpvec4 load_float_rgba_"#src"(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    if (type == DT_FLOAT16) \n\
        return load_float16_rgba_"#src"(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_float32_rgba_"#src"(x, y, w, cstep, format); \n\
    else \n\
        return sfpvec4(0.f); \n\
} \n\
" // 46 lines

#define SHADER_LOAD_FLOAT16_GRAY(src) \
" \n\
sfp load_float16_gray_"#src"(int x, int y, int w, int h, int cstep, int format) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp("#src"_data_float16[i_offset.x]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_LOAD_FLOAT32_GRAY(src) \
" \n\
sfp load_float32_gray_"#src"(int x, int y, int w, int h, int cstep, int format) \n\
{ \n\
    x = clamp(x, 0, w - 1); \n\
    y = clamp(y, 0, h - 1); \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp("#src"_data_float32[i_offset.x]); \n\
    return rgb_in; \n\
} \n\
"

#define SHADER_DEFAULT_PARAM_HEADER \
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
"

#define SHADER_DEFAULT_PARAM2_HEADER \
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
"

#define SHADER_DEFAULT_PARAM_TAIL \
" \n\
} p; \n\
"

#define SHADER_RGB_TO_HSL \
" \n\
sfpvec3 rgb_to_hsl(sfpvec3 rgb) \n\
{ \n\
    sfpvec3 hsl = sfpvec3(0.f); \n\
    sfp vmax = max(max(rgb.r, rgb.g), rgb.b); \n\
    sfp vmin = min(min(rgb.r, rgb.g), rgb.b); \n\
    if (vmax == vmin) \n\
        hsl.x = sfp(0.f); \n\
    else if (vmax == rgb.r) \n\
        hsl.x = sfp(60.f) * (rgb.g - rgb.b) / (vmax - vmin); \n\
    else if (vmax == rgb.g) \n\
        hsl.x = sfp(60.f) * (sfp(2.0f) + (rgb.b - rgb.r) / (vmax - vmin)); \n\
    else if (vmax == rgb.b) \n\
        hsl.x = sfp(60.f) * (sfp(4.0f) + (rgb.r - rgb.g) / (vmax - vmin)); \n\
    \n\
    hsl.z = (vmax + vmin) / sfp(2.f); \n\
    \n\
    if (hsl.z == sfp(0.f) || hsl.z == sfp(1.f)) \n\
        hsl.y = sfp(0.f); \n\
    else \n\
        hsl.y = (vmax - hsl.z) / min(hsl.z, sfp(1.0) - hsl.z); \n\
    return hsl; \n\
} \
"

#define SHADER_RGB_TO_HSV \
" \n\
sfpvec3 rgb_to_hsv(sfpvec3 rgb) \n\
{ \n\
    sfpvec3 hsv = sfpvec3(0.f); \n\
    sfp vmax = max(max(rgb.r, rgb.g), rgb.b); \n\
    sfp vmin = min(min(rgb.r, rgb.g), rgb.b); \n\
    if (vmax == vmin) \n\
        hsv.x = sfp(0.f); \n\
    else if (vmax == rgb.r) \n\
        hsv.x = sfp(60.f) * (rgb.g - rgb.b) / (vmax - vmin); \n\
    else if (vmax == rgb.g) \n\
        hsv.x = sfp(60.f) * (sfp(2.0f) + (rgb.b - rgb.r) / (vmax - vmin)); \n\
    else if (vmax == rgb.b) \n\
        hsv.x = sfp(60.f) * (sfp(4.0f) + (rgb.r - rgb.g) / (vmax - vmin)); \n\
    \n\
    if (vmax == sfp(0.f)) \n\
        hsv.y = sfp(0.f); \n\
    else \n\
        hsv.y = (vmax - vmin) / vmax; \n\
    \n\
    hsv.z = vmax; \n\
    return hsv; \n\
} \
"

#define SHADER_HSL_TO_RGB \
" \n\
sfpvec3 hsl_to_rgb(sfpvec3 hsl) \n\
{ \n\
    sfpvec3 rgb = sfpvec3(0.f); \n\
    sfp c = (sfp(1.f) - abs(sfp(2.f) * hsl.z - sfp(1.f))) * hsl.y; \n\
    sfp h = hsl.x / sfp(60.f); \n\
    sfp x = c * (sfp(1.f) - abs(mod(h, sfp(2.f)) -  sfp(1.f))); \n\
    sfp m = hsl.z - c / sfp(2.0f); \n\
    if (h >= sfp(0.f) && h < sfp(1.f)) \n\
        rgb = sfpvec3(c, x, 0); \n\
    else if (h >= sfp(1.f) && h < sfp(2.f)) \n\
        rgb = sfpvec3(x, c, 0); \n\
    else if (h >= sfp(2.f) && h < sfp(3.f)) \n\
        rgb = sfpvec3(0, c, x); \n\
    else if (h >= sfp(3.f) && h < sfp(4.f)) \n\
        rgb = sfpvec3(0, x, c); \n\
    else if (h >= sfp(4.f) && h < sfp(5.f)) \n\
        rgb = sfpvec3(x, 0, c); \n\
    else if (h >= sfp(5.f) && h < sfp(6.f)) \n\
        rgb = sfpvec3(c, 0, x); \n\
    rgb += m; \n\
    return rgb; \n\
} \
"

#define SHADER_HSV_TO_RGB \
" \n\
sfpvec3 hsv_to_rgb(sfpvec3 hsv) \n\
{ \n\
    sfpvec3 rgb = sfpvec3(0.f); \n\
    sfp angle = hsv.x / sfp(360.f); \n\
    if (hsv.y == sfp(0.f)) \n\
    { \n\
        rgb.r = rgb.g = rgb.b = hsv.z; \n\
    } \n\
    else \n\
    { \n\
        sfp h = mod(angle, sfp(1.0f)) / sfp(60.0f / 360.0f); \n\
        sfp v = hsv.z; \n\
        int i = int(h); \n\
        sfp f = h - sfp(i); \n\
        sfp p = hsv.z * (sfp(1.0f) - hsv.y); \n\
        sfp q = hsv.z * (sfp(1.0f) - hsv.y * f); \n\
        sfp t = hsv.z * (sfp(1.0f) - hsv.y * (sfp(1.0f) - f)); \n\
        if      (i == 0) { rgb.r = v; rgb.g = t; rgb.b = p; } \n\
        else if (i == 1) { rgb.r = q; rgb.g = v; rgb.b = p; } \n\
        else if (i == 2) { rgb.r = p; rgb.g = v; rgb.b = t; } \n\
        else if (i == 3) { rgb.r = p; rgb.g = q; rgb.b = v; } \n\
        else if (i == 4) { rgb.r = t; rgb.g = p; rgb.b = v; } \n\
        else             { rgb.r = v; rgb.g = p; rgb.b = q; } \n\
    } \n\
    return rgb; \n\
} \
"

#define SHADER_XYZ_TO_LAB \
" \n\
sfpvec3 xyz2lab(sfpvec3 xyz) \n\
{ \n\
    sfpvec3 n = xyz / sfpvec3(sfp(95.047), sfp(100.0), sfp(108.883)); \n\
    sfpvec3 c0 = pow(n, sfpvec3(sfp(1.0) / sfp(3.0))); \n\
    sfpvec3 c1 = (sfp(7.787) * n) + (sfp(16.0) / sfp(116.0)); \n\
    sfpvec3 v = mix(c0, c1, step(n, sfpvec3(0.008856))); \n\
    return sfpvec3((sfp(116.0) * v.y) - sfp(16.0), \n\
                    sfp(500.0) * (v.x - v.y), \n\
                    sfp(200.0) * (v.y - v.z)); \n\
} \
"

#define SHADER_LAB_TO_XYZ \
" \n\
sfpvec3 lab2xyz(sfpvec3 lab) \n\
{ \n\
    sfp fy = ( lab.x + sfp(16.0) ) / sfp(116.0); \n\
    sfp fx = lab.y / sfp(500.0) + fy; \n\
    sfp fz = fy - lab.z / sfp(200.0); \n\
    sfpvec3 CIE_WHITE = sfpvec3(0.95045592705, 1.0, 1.08905775076); \n\
    return CIE_WHITE * sfp(100.0) * sfpvec3( \n\
            ( fx > sfp(0.206897) ) ? fx * fx * fx : ( fx - sfp(16.0) / sfp(116.0) ) / sfp(7.787), \n\
            ( fy > sfp(0.206897) ) ? fy * fy * fy : ( fy - sfp(16.0) / sfp(116.0) ) / sfp(7.787), \n\
            ( fz > sfp(0.206897) ) ? fz * fz * fz : ( fz - sfp(16.0) / sfp(116.0) ) / sfp(7.787) \n\
    ); \n\
} \
"

#define SHADER_LOAD_RGB_IMAGE \
SHADER_LOAD_GRAY \
SHADER_LOAD_RGB \
SHADER_LOAD_RGBA \
" \n\
sfpvec4 load_rgb_image(int x, int y, int w, int h, int cstep, int format, int type) \n\
{ \n\
    sfpvec4 rgba_result = sfpvec4(0.f); \n\
    if (format == CF_ABGR || format == CF_ARGB || format == CF_BGRA || format == CF_RGBA) \n\
    { \n\
        rgba_result = load_rgba(x, y, w, h, cstep, format, type); \n\
    } \n\
    else if (format == CF_BGR || format == CF_RGB) \n\
    { \n\
        sfpvec3 rgb = load_rgb(x, y, w, h, cstep, format, type); \n\
        rgba_result = sfpvec4(rgb, sfp(1.0)); \n\
    } \n\
    else if (format == CF_GRAY) \n\
    { \n\
        sfp scale = type == DT_INT8 ? sfp(255) : type == DT_INT16 ? sfp(65535) : sfp(1.0); \n\
        sfp gray = load_gray(x, y, w, h, cstep, format, type, scale); \n\
        rgba_result = sfpvec4(gray, gray, gray, sfp(1.0)); \n\
    } \n\
    return rgba_result; \n\
} \
"
