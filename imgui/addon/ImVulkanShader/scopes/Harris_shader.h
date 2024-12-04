#pragma once
#include <imvk_mat_shader.h>

#define PREWITT_PARAM \
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
    float edgeStrength; \n\
} p; \
"

#define SHADER_PREWITT_MAIN \
" \n\
// prewitt算子,水平与垂直是否反了?先按照GPUImage来,后面查证 对应GPUImageXYDerivativeFilter \n\
const sfp horizontKernel[9] = { sfp(-1.0f), sfp(0.0f), sfp(1.0f), \n\
                                sfp(-1.0f), sfp(0.0f), sfp(1.0f), \n\
                                sfp(-1.0f), sfp(0.0f), sfp(1.0f)}; \n\
const sfp verticalKernel[9] = { sfp(-1.0f), sfp(-1.0f), sfp(-1.0f), \n\
                                sfp( 0.0f), sfp( 0.0f), sfp( 0.0f), \n\
                                sfp( 1.0f), sfp( 1.0f), sfp( 1.0f)}; \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp vertical = sfp(0.0f); \n\
    sfp horizont = sfp(0.0f); \n\
    for (int i = 0; i < 3; ++i) \n\
    { \n\
        for (int j = 0; j < 3; ++j) \n\
        { \n\
            int x = gx - 1 + j; \n\
            int y = gy - 1 + i; \n\
            // REPLICATE border \n\
            x = max(0, min(x, p.out_w - 1)); \n\
            y = max(0, min(y, p.out_h - 1)); \n\
            int index = j + i * 3; \n\
            sfp value = load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type).r; \n\
            vertical += value * verticalKernel[index]; \n\
            horizont += value * horizontKernel[index]; \n\
        } \n\
    } \n\
    vertical = vertical * sfp(p.edgeStrength); \n\
    horizont = horizont * sfp(p.edgeStrength); \n\
    sfpvec4 sum = sfpvec4(0.0f); \n\
    sum.x = horizont * horizont; \n\
    sum.y = vertical* vertical; \n\
    sum.z = ((horizont * vertical) + sfp(1.0f)) / sfp(2.0f); \n\
    sum.w = sfp(1.0f); \n\
    store_rgba(sum, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char PrewittFilter_data[] = 
SHADER_HEADER
PREWITT_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_PREWITT_MAIN
;

#define HARRIS_SHADER_PARAM \
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
    float harris; \n\
    float sensitivity; \n\
} p; \
"

#define SHADER_HARRIS_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 derivativeElements = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfp derivativeSum = derivativeElements.x + derivativeElements.y; \n\
    sfp zElement = (derivativeElements.z * sfp(2.0f)) - sfp(1.0f); \n\
    // R = Ix^2 * Iy^2 - Ixy * Ixy - k * (Ix^2 + Iy^2)^2 \n\
    sfp cornerness = derivativeElements.x * derivativeElements.y - (zElement * zElement) - sfp(p.harris) * derivativeSum * derivativeSum; \n\
    cornerness = cornerness * sfp(p.sensitivity); \n\
    store_gray(cornerness, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char HarrisFilter_data[] = 
SHADER_HEADER
HARRIS_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_GRAY
SHADER_HARRIS_MAIN
;

#define NMS_SHADER_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    \n\
    int mono_w; \n\
    int mono_h; \n\
    int mono_cstep; \n\
    int mono_format; \n\
    int mono_type; \n\
    \n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    \n\
    float threshold; \n\
} p; \
"

// Load data as gray
#define SHADER_LOAD_MONO_INT8 \
" \n\
sfp load_mono_int8(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(uint(gray_int8_data[i_offset.x])) / sfp(255.f); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_MONO_INT16 \
" \n\
sfp load_mono_int16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(uint(gray_int16_data[i_offset.x])) / sfp(65535.f); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_MONO_FLOAT16 \
" \n\
sfp load_mono_float16(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(gray_float16_data[i_offset.x]); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_MONO_FLOAT32 \
" \n\
sfp load_mono_float32(int x, int y, int w, int cstep, int format) \n\
{ \n\
    sfp rgb_in = sfp(0.f); \n\
    ivec3 i_offset = (y * w + x) * cstep + ivec3(0, 0, 0); \n\
    rgb_in = sfp(gray_float32_data[i_offset.x]); \n\
    return rgb_in; \n\
} \
"

#define SHADER_LOAD_MONO \
SHADER_LOAD_MONO_INT8 \
SHADER_LOAD_MONO_INT16 \
SHADER_LOAD_MONO_FLOAT16 \
SHADER_LOAD_MONO_FLOAT32 \
" \n\
sfp load_mono(int x, int y, int w, int cstep, int format, int type) \n\
{ \n\
    if (type == DT_INT8) \n\
        return load_mono_int8(x, y, w, cstep, format); \n\
    else if (type == DT_INT16) \n\
        return load_mono_int16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT16) \n\
        return load_mono_float16(x, y, w, cstep, format); \n\
    else if (type == DT_FLOAT32) \n\
        return load_mono_float32(x, y, w, cstep, format); \n\
    else \n\
        return sfp(0.f); \n\
} \
" // 46 lines


#define SHADER_NMS_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp values[9]; \n\
    for (int i = 0; i < 3; ++i) \n\
    { \n\
        for (int j = 0; j < 3; ++j) \n\
        { \n\
            int x = gx - 1 + j; \n\
            int y = gy - 1 + i; \n\
            // REPLICATE border \n\
            x = max(0, min(x, p.out_w - 1)); \n\
            y = max(0, min(y, p.out_h - 1)); \n\
            sfp value = load_mono(x, y, p.mono_w, p.mono_cstep, p.mono_format, p.mono_type); \n\
            values[j + i * 3] = value; \n\
        } \n\
    } \n\
    // 找4是最大的点 \n\
    // 0 1 2 \n\
    // 3 4 5 \n\
    // 6 7 8 \n\
    // 如果左上角(0,1,3,6)大于当前点,multiplier=0 \n\
    sfp multiplier = sfp(1.0f) - step(values[4], values[1]); \n\
    multiplier = multiplier * (sfp(1.0f) - step(values[4], values[0])); \n\
    multiplier = multiplier * (sfp(1.0f) - step(values[4], values[3])); \n\
    multiplier = multiplier * (sfp(1.0f) - step(values[4], values[6])); \n\
    // 查找右下角(2,5,7,8)的最大值 \n\
    sfp maxValue = max(values[4], values[7]); \n\
    maxValue = max(values[4], values[8]); \n\
    maxValue = max(values[4], values[5]); \n\
    maxValue = max(values[4], values[2]); \n\
    // step(maxValue, values[4])需要当前值最大才为1 \n\
    sfp result = values[4]* step(maxValue, values[4]) * multiplier; \n\
    result = step(sfp(p.threshold), result); \n\
    sfpvec4 rgba_in = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 rgb_in = rgba_in.rgb; \n\
    sfp alpha = rgba_in.a; \n\
    if (result > sfp(0.f)) \n\
    { \n\
        rgb_in = sfpvec3(1.0, 0.0, 0.0); \n\
        for (int i = 0; i < 3; ++i) \n\
        { \n\
            for (int j = 0; j < 3; ++j) \n\
            { \n\
                int x = gx - 1 + j; \n\
                int y = gy - 1 + i; \n\
                // REPLICATE border \n\
                x = max(0, min(x, p.out_w - 1)); \n\
                y = max(0, min(y, p.out_h - 1)); \n\
                store_rgba(sfpvec4(rgb_in, alpha), x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
            } \n\
        } \n\
    } \n\
    else \n\
        store_rgba(sfpvec4(rgb_in, alpha), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char NMSFilter_data[] = 
SHADER_HEADER
NMS_SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer gray_int8     { uint8_t   gray_int8_data[]; };
layout (binding =  9) readonly buffer gray_int16    { uint16_t  gray_int16_data[]; };
layout (binding = 10) readonly buffer gray_float16  { float16_t gray_float16_data[]; };
layout (binding = 11) readonly buffer gray_float32  { float     gray_float32_data[]; };
)"
SHADER_LOAD_MONO
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_NMS_MAIN
;

