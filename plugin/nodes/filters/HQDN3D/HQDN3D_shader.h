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

#define SHADER_HQDN3D \
" \n\
#define LUMA_SPATIAL   0 \n\
#define LUMA_TMP       1 \n\
#define CHROMA_SPATIAL 2 \n\
#define CHROMA_TMP     3 \n\
sfpmat3 matrix_mat_r2y = { \n\
    {sfp(0.262700f), sfp(-0.139630f), sfp( 0.500000f)}, \n\
    {sfp(0.678000f), sfp(-0.360370f), sfp(-0.459786f)}, \n\
    {sfp(0.059300f), sfp( 0.500000f), sfp(-0.040214f)} \n\
}; \n\
sfpmat3 matrix_mat_y2r = { \n\
    {sfp(1.000000f), sfp( 1.000000f),  sfp(1.000000f)}, \n\
    {sfp(0.000000f), sfp(-0.164553f),  sfp(1.881400f)}, \n\
    {sfp(1.474600f), sfp(-0.571353f),  sfp(0.000000f)} \n\
}; \n\
sfpvec3 rgb_to_yuv(sfpvec3 rgb) \n\
{ \n\
    sfpvec3 yuv_offset = {sfp(0.f), sfp(0.5f), sfp(0.5f)}; \n\
    sfpvec3 yuv = yuv_offset + matrix_mat_r2y * rgb; \n\
    return clamp(yuv, sfp(0.f), sfp(1.f)); \n\
} \n\
sfpvec3 yuv_to_rgb(sfpvec3 yuv) \n\
{ \n\
    sfpvec3 yuv_offset = {sfp(0.f), sfp(0.5f), sfp(0.5f)}; \n\
    sfpvec3 rgb = matrix_mat_y2r * (yuv - yuv_offset); \n\
    return clamp(rgb, sfp(0.f), sfp(1.f)); \n\
} \n\
int lowpass(int prev, int cur, int coef_index) \n\
{ \n\
    int d = (prev - cur) / 16 ; \n\
    return int(cur + (coef_index == LUMA_SPATIAL ? int(coefs_0_data[d + 256 * 16]) :  \n\
                      coef_index == LUMA_TMP ? int(coefs_1_data[d + 256 * 16]) : \n\
                      coef_index == CHROMA_SPATIAL ? int(coefs_2_data[d + 256 * 16]) : \n\
                      coef_index == CHROMA_TMP ? int(coefs_3_data[d + 256 * 16]) : 0)); \n\
} \n\
sfpvec4 denoise(int x, int y) \n\
{ \n\
    sfpvec3 yuv = {sfp(0.f), sfp(0.5f), sfp(0.5f)}; \n\
    int pixel_ant; \n\
    int tmp; \n\
    int spatial_y_offset = y * p.w + x; \n\
    int spatial_u_offset = p.w * p.h + y * p.w + x; \n\
    int spatial_v_offset = p.w * p.h * 2 + y * p.w + x; \n\
    sfpvec4 rgba = load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 yuv0 = rgb_to_yuv(rgba.rgb); \n\
    sfpvec3 yuv1 = x < p.w - 1 ? rgb_to_yuv(load_rgba(x + 1, y, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb) : yuv0; \n\
    // Y \n\
    pixel_ant = lowpass(frame_spatial_data[spatial_y_offset], int(yuv0.x * sfp(65535.0f)) + 128, LUMA_SPATIAL); \n\
    frame_spatial_data[spatial_y_offset] = pixel_ant = lowpass(frame_spatial_data[spatial_y_offset], pixel_ant, LUMA_SPATIAL); \n\
    tmp = lowpass(frame_temporal_data[spatial_y_offset], pixel_ant, LUMA_TMP); \n\
    if (y > 0 && x < p.w - 1) \n\
    { \n\
        frame_temporal_data[spatial_y_offset] = lowpass(pixel_ant, int(yuv1.x * sfp(65535.0f)) + 128, LUMA_TMP); \n\
    } \n\
    else \n\
        frame_temporal_data[spatial_y_offset] = tmp; \n\
    yuv.x = sfp(tmp) / sfp(65535.0f); \n\
    // U \n\
    pixel_ant = lowpass(frame_spatial_data[spatial_u_offset], int(yuv0.y * sfp(65535.f)) + 128, CHROMA_SPATIAL); \n\
    frame_spatial_data[spatial_u_offset] = pixel_ant = lowpass(frame_spatial_data[spatial_u_offset], pixel_ant, CHROMA_SPATIAL); \n\
    tmp = lowpass(frame_temporal_data[spatial_u_offset], pixel_ant, CHROMA_TMP); \n\
    if (y > 0 && x < p.w - 1) \n\
        frame_temporal_data[spatial_u_offset] = lowpass(pixel_ant, int(yuv1.y * sfp(65535.f)) + 128, CHROMA_TMP); \n\
    else \n\
        frame_temporal_data[spatial_u_offset] = tmp; \n\
    yuv.y = sfp(tmp) / sfp(65535.0f); \n\
    // V \n\
    pixel_ant = lowpass(frame_spatial_data[spatial_v_offset], int(yuv0.z * sfp(65535.0f)) + 128, CHROMA_SPATIAL); \n\
    frame_spatial_data[spatial_v_offset] = pixel_ant = lowpass(frame_spatial_data[spatial_v_offset], pixel_ant, CHROMA_SPATIAL); \n\
    tmp = lowpass(frame_temporal_data[spatial_v_offset], pixel_ant, CHROMA_TMP); \n\
    if (y > 0 && x < p.w - 1) \n\
        frame_temporal_data[spatial_v_offset] = lowpass(pixel_ant, int(yuv1.z * sfp(65535.0f)) + 128, CHROMA_TMP); \n\
    else \n\
        frame_temporal_data[spatial_v_offset] = tmp; \n\
    yuv.z = sfp(tmp) / sfp(65535.0f); \n\
    return sfpvec4(yuv_to_rgb(yuv), rgba.a); \n\
} \
"

#define SHADER_HQDN3D_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
\n\
    sfpvec4 result = denoise(gx, gy); \n\
    store_rgba(result, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char HQDN3D_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer coefs_0 { int16_t coefs_0_data[]; };
layout (binding = 9) readonly buffer coefs_1 { int16_t coefs_1_data[]; };
layout (binding = 10) readonly buffer coefs_2 { int16_t coefs_2_data[]; };
layout (binding = 11) readonly buffer coefs_3 { int16_t coefs_3_data[]; };
layout (binding = 12) buffer frame_spatial {  int frame_spatial_data[]; };
layout (binding = 13) buffer frame_temporal { int frame_temporal_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_HQDN3D
SHADER_HQDN3D_MAIN
;