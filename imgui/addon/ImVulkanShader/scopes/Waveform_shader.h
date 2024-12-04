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
    \n\
    int separate; \n\
    int show_y; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
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
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.w || gy >= p.h) \n\
        return; \n\
    if (mod(float(gy), 2) != 0) // reduce to half size\n\
        return; \n\
    int part = p.show_y == 1 ? 4 : 3; \n\
    int dx = p.separate == 1 ? gx / part : gx; \n\
    int ox = p.separate == 1 ? p.out_w / part : 0; \n\
    sfpvec4 rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 yuv = rgb_to_yuv(rgba.rgb); \n\
    sfpvec3 rgb = yuv_to_rgb(yuv); \n\
    int dr = clamp(int(rgb.r * sfp(p.out_h - 1)), 0, p.out_h - 1); \n\
    ivec4 offset_r = (dr * p.out_w + dx) * p.out_cstep + ivec4(0, 1, 2, 3); \n\
    int dg = clamp(int(rgb.g * sfp(p.out_h - 1)), 0, p.out_h - 1); \n\
    ivec4 offset_g = (dg * p.out_w + dx + ox) * p.out_cstep + ivec4(0, 1, 2, 3); \n\
    int db = clamp(int(rgb.b * sfp(p.out_h - 1)), 0, p.out_h - 1); \n\
    ivec4 offset_b = (db * p.out_w + dx + ox + ox) * p.out_cstep + ivec4(0, 1, 2, 3); \n\
    int dy = int(yuv.x * sfp(p.out_h - 1)); \n\
    ivec4 offset_y = (dy * p.out_w + dx + ox + ox + ox) * p.out_cstep + ivec4(0, 1, 2, 3); \n\
    memoryBarrierBuffer(); \n\
    atomicAdd(waveform_int32_data[offset_r.r], 1); \n\
    atomicAdd(waveform_int32_data[offset_g.g], 1); \n\
    atomicAdd(waveform_int32_data[offset_b.b], 1); \n\
    atomicAdd(waveform_int32_data[offset_y.a], 1); \n\
    memoryBarrierBuffer(); \n\
} \
"

static const char Waveform_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_SRC_DATA
R"(
layout (binding = 4) restrict buffer waveform_int32  { int waveform_int32_data[]; };
)"
SHADER_LOAD_RGBA
SHADER_MAIN
;

#define PARAM_ZERO \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
} p;\
"

#define ZERO_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.w || gy >= p.h) \n\
        return; \n\
    ivec4 in_offset = (gy * p.w + gx) * p.cstep + ivec4(0, 1, 2, 3); \n\
    waveform_int32_data[in_offset.r] = 0; \n\
    waveform_int32_data[in_offset.g] = 0; \n\
    waveform_int32_data[in_offset.b] = 0; \n\
    waveform_int32_data[in_offset.a] = 0; \n\
} \
"

static const char Zero_data[] =
SHADER_HEADER
PARAM_ZERO
R"(
layout (binding = 0) restrict buffer waveform_int32  { int waveform_int32_data[]; };
)"
ZERO_MAIN
;

#define PARAM_CONV \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int out_cstep; \n\
    int out_format; \n\
    \n\
    float intensity; \n\
    int separate; \n\
    int show_y; \n\
} p;\
"

#define CONV_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.w || gy >= p.h) \n\
        return; \n\
    int separate_in_y = gx >= (p.w * 3 / 4) ? 1 : 0; \n\
    ivec4 in_offset = (gy * p.w + gx) * p.cstep + ivec4(0, 1, 2, 3); \n\
    ivec4 out_offset = (gy * p.w + gx) * p.out_cstep + (p.out_format == CF_ABGR ? ivec4(0, 1, 2, 3) : ivec4(2, 1, 0, 3)); \n\
    if (p.show_y == 0 || p.separate == 1) \n\
    { \n\
        // R Conv \n\
        int v_r = waveform_int32_data[in_offset.r]; \n\
        waveform_int8_data[out_offset.r] = uint8_t(clamp(v_r * p.intensity, 0, 255)); \n\
        // G Conv \n\
        int v_g = waveform_int32_data[in_offset.g]; \n\
        waveform_int8_data[out_offset.g] = uint8_t(clamp(v_g * p.intensity, 0, 255)); \n\
        // B Conv \n\
        int v_b = waveform_int32_data[in_offset.b]; \n\
        waveform_int8_data[out_offset.b] = uint8_t(clamp(v_b * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.a] = uint8_t(255); \n\
    } \n\
    else if (p.show_y == 1 && p.separate == 0) \n\
    { \n\
        // Y Conv \n\
        int v_y = waveform_int32_data[in_offset.a]; \n\
        waveform_int8_data[out_offset.r] = uint8_t(clamp(v_y * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.g] = uint8_t(clamp(v_y * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.b] = uint8_t(clamp(v_y * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.a] = uint8_t(255); \n\
    } \n\
    if (p.show_y == 1 && p.separate == 1 && separate_in_y == 1) \n\
    { \n\
        int v_y = waveform_int32_data[in_offset.a]; \n\
        waveform_int8_data[out_offset.r] = uint8_t(clamp(v_y * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.g] = uint8_t(clamp(v_y * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.b] = uint8_t(clamp(v_y * p.intensity, 0, 255)); \n\
        waveform_int8_data[out_offset.a] = uint8_t(255); \n\
    } \n\
} \
"

static const char ConvInt2Mat_data[] = 
SHADER_HEADER
PARAM_CONV
R"(
layout (binding = 0) readonly buffer waveform_int32  { int waveform_int32_data[]; };
layout (binding = 1) writeonly buffer waveform_int8  { uint8_t waveform_int8_data[]; };
)"
CONV_MAIN
;
