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
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
} p; \
"

#define SHADER_TOMATING_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    store_rgba_float16_no_clamp_outTex1(sfpvec4(rgba.rgb * rgba.a, 1.0f), gx, gy, p.out_w, p.out_cstep, p.out_format); \n\
    store_rgba_float16_no_clamp_outTex2(sfpvec4(rgba.rgb * rgba.r, 1.0f), gx, gy, p.out_w, p.out_cstep, p.out_format); \n\
    store_rgba_float16_no_clamp_outTex2(sfpvec4(rgba.g * rgba.g, rgba.g * rgba.b, rgba.b * rgba.b, 1.0f), gx, gy, p.out_w, p.out_cstep, p.out_format); \n\
} \
"

static const char ToMatting_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_SRC_DATA
R"(
layout (binding = 4) writeonly buffer outTex1 { float16_t outTex1_data_float16[]; };
layout (binding = 5) writeonly buffer outTex2 { float16_t outTex2_data_float16[]; };
layout (binding = 6) writeonly buffer outTex3 { float16_t outTex3_data_float16[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA_FLOAT16_NO_CLAMP(outTex1)
SHADER_STORE_RGBA_FLOAT16_NO_CLAMP(outTex2)
SHADER_STORE_RGBA_FLOAT16_NO_CLAMP(outTex3)
SHADER_TOMATING_MAIN
;

#define SHADER_GUIDED_PARAM \
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
    float float_eps; \n\
} p; \
"

#define SHADER_GUIDED_MAIN \
" \n\
sfpmat3 inverse_mat3(sfpmat3 m) \n\
{ \n\
    sfp a00 = m[0][0], a01 = m[0][1], a02 = m[0][2]; \n\
    sfp a10 = m[1][0], a11 = m[1][1], a12 = m[1][2]; \n\
    sfp a20 = m[2][0], a21 = m[2][1], a22 = m[2][2]; \n\
    \n\
    sfp b01 = a22 * a11 - a12 * a21; \n\
    sfp b11 = -a22 * a10 + a12 * a20; \n\
    sfp b21 = a21 * a10 - a11 * a20; \n\
    \n\
    sfp det = a00 * b01 + a01 * b11 + a02 * b21;  \n\
    \n\
    return sfpmat3(b01, (-a22 * a01 + a02 * a21), (a12 * a01 - a02 * a11),          \n\
                    b11, (a22 * a00 - a02 * a20), (-a12 * a00 + a02 * a10),         \n\
                    b21, (-a21 * a00 + a01 * a20), (a11 * a00 - a01 * a10)) / det;  \n\
} \n\
\n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 color = load_float_rgba_inTex1(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 mean_I = color.xyz; \n\
	sfp mean_p = color.w; \n\
	sfpvec3 mean_Ip = load_float_rgba_inTex2(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type).xyz; \n\
	sfpvec3 var_I_r = load_float_rgba_inTex3(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type).xyz - mean_I.x * mean_I; \n\
	sfpvec3 var_I_gbxfv = load_float_rgba_inTex4(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type).xyz; \n\
    //计算方差 \n\
	sfp gg = var_I_gbxfv.x - mean_I.y * mean_I.y; \n\
	sfp gb = var_I_gbxfv.y - mean_I.y * mean_I.z; \n\
	sfp bb = var_I_gbxfv.z - mean_I.z * mean_I.z; \n\
	//cov为协方差 \n\
	sfpvec3 cov_Ip = mean_Ip - mean_I * mean_p; \n\
	sfpvec3 col0 = var_I_r + sfpvec3(sfp(p.float_eps), sfp(0.f), sfp(0.f)); \n\
	sfpvec3 col1 = sfpvec3(var_I_r.y, gg + sfp(p.float_eps), gb); \n\
	sfpvec3 col2 = sfpvec3(var_I_r.z, gb, bb + sfp(p.float_eps)); \n\
    sfpmat3 colMat = sfpmat3(col0, col1, col2); \n\
	sfpmat3 invMat = inverse_mat3(colMat); \n\
    sfpvec3 a = cov_Ip * invMat; \n\
	sfp b = mean_p - dot(a, mean_I); \n\
    store_rgba_float16_no_clamp_outTex(sfpvec4(a, b), gx, gy, p.out_w, p.out_cstep, p.out_format); \n\
} \
"

static const char Guided_data[] = 
SHADER_HEADER
SHADER_GUIDED_PARAM
R"(
layout (binding = 0) readonly buffer inTex1 { float16_t inTex1_data[]; };
layout (binding = 1) readonly buffer inTex2 { float16_t inTex2_data[]; };
layout (binding = 2) readonly buffer inTex3 { float16_t inTex3_data[]; };
layout (binding = 3) readonly buffer inTex4 { float16_t inTex4_data[]; };
layout (binding = 4) writeonly buffer outTex { float16_t outTex_data_float16[]; };
)"
SHADER_LOAD_FLOAT_RGBA(inTex1)
SHADER_LOAD_FLOAT_RGBA(inTex2)
SHADER_LOAD_FLOAT_RGBA(inTex3)
SHADER_LOAD_FLOAT_RGBA(inTex4)
SHADER_STORE_RGBA_FLOAT16_NO_CLAMP(outTex)
SHADER_GUIDED_MAIN
;


#define SHADER_MATTING_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    \n\
    int mean_w; \n\
    int mean_h; \n\
    int mean_cstep; \n\
    int mean_format; \n\
    int mean_type; \n\
    \n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
} p; \
"

#define SHADER_MATTING_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 color = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 mean = load_float_rgba_inTex(gx, gy, p.mean_w, p.mean_h, p.mean_cstep, p.mean_format, p.mean_type); \n\
    //sfp q = clamp(color.x * mean.x + color.y * mean.y + color.z * mean.z + mean.w, sfp(0.f), sfp(1.f)); \n\
    color.r = clamp(color.x * mean.x + mean.w, sfp(0.f), sfp(1.f)); \n\
    color.g = clamp(color.y * mean.y + mean.w, sfp(0.f), sfp(1.f)); \n\
    color.b = clamp(color.z * mean.z + mean.w, sfp(0.f), sfp(1.f)); \n\
    store_rgba(color, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Matting_data[] = 
SHADER_HEADER
SHADER_MATTING_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer inTex { float16_t inTex_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_FLOAT_RGBA(inTex)
SHADER_STORE_RGBA
SHADER_MATTING_MAIN
;