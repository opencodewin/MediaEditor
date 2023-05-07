#pragma once
#include <imvk_mat_shader.h>

#define SHADER_CHROMA_PARAM \
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
    // 1 控制亮度的强度系数 \n\
    float lumaMask; \n\
    float chromaColorX; \n\
    float chromaColorY; \n\
    float chromaColorZ; \n\
    float alphaCutoffMin; \n\
    float alphaScale; \n\
    float alphaExponent; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
const sfp PI = sfp(3.1415926f); \n\
sfpvec3 extractColor(sfpvec3 color, sfp lumaMask) \n\
{ \n\
    sfp luma = dot(color, sfpvec3(1.0f)); \n\
    // 亮度指数 \n\
    sfp colorMask = exp(-luma * sfp(2.0f) * PI / lumaMask); \n\
    // color*(1-colorMask)+color*luma \n\
    color = mix(color, sfpvec3(luma), colorMask); \n\
    // 生成基于亮度的饱和度图 \n\
    return clamp(color / dot(color, sfpvec3(2.0)), sfp(0.0f), sfp(1.0f)); \n\
} \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec3 inputColor = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb; \n\
    sfpvec3 chromaColor = sfpvec3(p.chromaColorX, p.chromaColorY, p.chromaColorZ); \n\
    sfpvec3 color1 = extractColor(chromaColor, sfp(p.lumaMask)); \n\
    sfpvec3 color2 = extractColor(inputColor, sfp(p.lumaMask)); \n\
    sfpvec3 subColor = color1 - color2; \n\
    sfp diffSize = length(subColor); \n\
    sfp minClamp = max((diffSize - sfp(p.alphaCutoffMin)) * sfp(p.alphaScale), sfp(0.0f)); \n\
    // 扣像alpha \n\
    sfp alpha = clamp(pow(minClamp, sfp(p.alphaExponent)), sfp(0.0f), sfp(1.0f)); \n\
    store_gray_float16(alpha, uv.x, uv.y, p.out_w, p.out_cstep, p.out_format); \n\
} \
"

static const char Filter_data[] = 
SHADER_HEADER
SHADER_CHROMA_PARAM
SHADER_SRC_DATA
R"(
layout (binding = 4) writeonly buffer alpha { float16_t dst_data_float16[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_GRAY_FLOAT16
SHADER_MAIN
;


#define SHADER_BLUR_PARAM \
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

#define SHADER_BLUR_MAIN \
" \n\
// sigma = 0.75 \n\
//const sfp blurKernel[9] = { sfp(0.0509), sfp(0.1238), sfp(0.0509), \n\
//                            sfp(0.1238), sfp(0.3012), sfp(0.1238), \n\
//                            sfp(0.0509), sfp(0.1238), sfp(0.0509)};\n\
// sigma = 0.8 \n\
const sfp blurKernel[9] = { sfp(0.0571), sfp(0.1248), sfp(0.0571), \n\
                            sfp(0.1248), sfp(0.2725), sfp(0.1248), \n\
                            sfp(0.0571), sfp(0.1248), sfp(0.0571)};\n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp sum = sfp(0.f); \n\
    int kInd = 0; \n\
    for (int i = 0; i < 3; ++i) \n\
    { \n\
        for (int j = 0; j< 3; ++j) \n\
        { \n\
            int x = gx - 1 + j; \n\
            int y = gy - 1 + i; \n\
            x = max(0, min(x, p.out_w - 1)); \n\
            y = max(0, min(y, p.out_h - 1)); \n\
            sfp val = load_gray_float16(x, y, p.w, p.cstep, p.in_format) * blurKernel[kInd++]; \n\
            sum = sum + val; \n\
        } \n\
    } \n\
    store_gray_float16(clamp(sum, sfp(0.f), sfp(1.f)), gx, gy, p.out_w, p.out_cstep, p.out_format); \n\
} \
"

static const char Blur_data[] = 
SHADER_HEADER
SHADER_BLUR_PARAM
R"(
layout (binding = 0) readonly buffer src { float16_t src_data_float16[]; };
layout (binding = 1) writeonly buffer dst { float16_t dst_data_float16[]; };
)"
SHADER_LOAD_GRAY_FLOAT16
SHADER_STORE_GRAY_FLOAT16
SHADER_BLUR_MAIN
;


#define SHADER_SHARPEN_PARAM \
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
    float amount; \n\
} p; \
"

#define SHADER_SHARPEN_MAIN \
" \n\
// sigma = 0.75 \n\
//const sfp blurKernel[9] = { sfp(0.0509), sfp(0.1238), sfp(0.0509), \n\
//                            sfp(0.1238), sfp(0.3012), sfp(0.1238), \n\
//                            sfp(0.0509), sfp(0.1238), sfp(0.0509)};\n\
// sigma = 0.8 \n\
const sfp blurKernel[9] = { sfp(0.0571), sfp(0.1248), sfp(0.0571), \n\
                            sfp(0.1248), sfp(0.2725), sfp(0.1248), \n\
                            sfp(0.0571), sfp(0.1248), sfp(0.0571)};\n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp sum = sfp(0.f); \n\
    int kInd = 0; \n\
    sfp current = load_gray_float16(gx, gy, p.w, p.cstep, p.in_format); \n\
    for (int i = 0; i < 3; ++i) \n\
    { \n\
        for (int j = 0; j< 3; ++j) \n\
        { \n\
            int x = gx - 1 + j; \n\
            int y = gy - 1 + i; \n\
            x = max(0, min(x, p.out_w - 1)); \n\
            y = max(0, min(y, p.out_h - 1)); \n\
            sfp val = load_gray_float16(x, y, p.w, p.cstep, p.in_format) * blurKernel[kInd++]; \n\
            sum = sum + val; \n\
        } \n\
    } \n\
    sfp amount = current * sfp(p.amount) + sum * sfp(1.f - p.amount); \n\
    store_gray_float16(clamp(amount, sfp(0.f), sfp(1.f)), gx, gy, p.out_w, p.out_cstep, p.out_format); \n\
} \
"

static const char Sharpen_data[] = 
SHADER_HEADER
SHADER_SHARPEN_PARAM
R"(
layout (binding = 0) readonly buffer src { float16_t src_data_float16[]; };
layout (binding = 1) writeonly buffer dst { float16_t dst_data_float16[]; };
)"
SHADER_LOAD_GRAY_FLOAT16
SHADER_STORE_GRAY_FLOAT16
SHADER_SHARPEN_MAIN
;

#define SHADER_DESPILL_PARAM \
" \n\
#define CHROMAKEY_OUTPUT_NORMAL      0  \n\
#define CHROMAKEY_OUTPUT_ALPHA_ONLY  1  \n\
#define CHROMAKEY_OUTPUT_ALPHA_RGBA  2  \n\
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
    int alpha_w; \n\
    int alpha_h; \n\
    int alpha_cstep; \n\
    int alpha_format; \n\
    int alpha_type; \n\
    \n\
    float chromaColorX; \n\
    float chromaColorY; \n\
    float chromaColorZ; \n\
    int output_type; \n\
} p; \
"

#define SHADER_DESPILL_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp val = load_float16_gray_alpha(gx, gy, p.alpha_w, p.alpha_h, p.alpha_cstep, p.alpha_format); \n\
    if (p.output_type == CHROMAKEY_OUTPUT_ALPHA_ONLY) \n\
        store_gray(val, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else if (p.output_type == CHROMAKEY_OUTPUT_ALPHA_RGBA) \n\
    { \n\
        store_rgba(sfpvec4(val, val, val, sfp(1.0f)), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
    else \n\
    { \n\
        sfpvec4 rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfp r_b = (rgba.r + rgba.b) / sfp(2.0f); \n\
        if (r_b < rgba.g) rgba.g = r_b; \n\
        store_rgba(sfpvec4(rgba.rgb, val), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \
"

static const char Despill_data[] = 
SHADER_HEADER
SHADER_DESPILL_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer alpha { float16_t alpha_data_float16[]; };
)"
SHADER_LOAD_FLOAT16_GRAY(alpha)
SHADER_LOAD_RGBA
SHADER_STORE_GRAY
SHADER_STORE_RGBA
SHADER_DESPILL_MAIN
;