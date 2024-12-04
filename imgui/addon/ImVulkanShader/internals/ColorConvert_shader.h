#pragma once
#include <imvk_mat_shader.h>

#define SHADER_MAT_Y2R \
" \n\
sfpmat3 matrix_mat_y2r = { \n\
    {sfp(convert_matrix_y2r[0]), sfp(convert_matrix_y2r[3]), sfp(convert_matrix_y2r[6])}, \n\
    {sfp(convert_matrix_y2r[1]), sfp(convert_matrix_y2r[4]), sfp(convert_matrix_y2r[7])}, \n\
    {sfp(convert_matrix_y2r[2]), sfp(convert_matrix_y2r[5]), sfp(convert_matrix_y2r[8])}, \n\
}; \
"

#define SHADER_MAT_R2Y \
" \n\
sfpmat3 matrix_mat_r2y = { \n\
    {sfp(convert_matrix_r2y[0]), sfp(convert_matrix_r2y[3]), sfp(convert_matrix_r2y[6])}, \n\
    {sfp(convert_matrix_r2y[1]), sfp(convert_matrix_r2y[4]), sfp(convert_matrix_r2y[7])}, \n\
    {sfp(convert_matrix_r2y[2]), sfp(convert_matrix_r2y[5]), sfp(convert_matrix_r2y[8])}, \n\
}; \
"

#define SHADER_YUV2RGB \
" \n\
sfpvec3 yuv_to_rgb(sfpvec3 yuv) \n\
{ \n\
    sfpvec3 rgb;\n\
    sfpvec3 yuv_offset = {sfp(0.f), sfp(0.5f), sfp(0.5f)}; \n\
    if (p.in_range == CR_NARROW_RANGE) \n\
        yuv_offset.x = sfp(16.0f / 255.0f); \n\
    rgb = matrix_mat_y2r * (yuv - yuv_offset); \n\
    return clamp(rgb, sfp(0.f), sfp(1.f)); \n\
}\n\
"

#define SHADER_PARAM_Y2R \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    int in_space; \n\
    int in_range; \n\
    float in_scale; \n\
    int in_real_w; \n\
    int in_real_h; \n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    int resize; \n\
    int interp_type; \n\
    int mirror; \n\
} p;\
"

#define SHADER_LOAD_SRC_YUV \
" \n\
sfpvec3 load_src_yuv(int x, int y) \n\
{ \n\
    sfpvec3 yuv_in = {sfp(0.f), sfp(0.5f), sfp(0.5f)}; \n\
    int uv_scale_w = p.in_format == CF_YUV420 || p.in_format == CF_YUV422 ? 2 : 1; \n\
    int uv_scale_h = p.in_format == CF_YUV420 || p.in_format == CF_YUV440 || p.in_format == CF_NV12 || p.in_format == CF_P010LE ? 2 : 1; \n\
    int y_offset = y * p.w + x; \n\
    int u_offset = p.w * p.h + (y / uv_scale_h) * p.w / uv_scale_w + x / uv_scale_w; \n\
    int v_offset = (p.in_format == CF_YUV444 ? p.w * p.h * 2 : p.w * p.h + (p.w / uv_scale_w) * (p.h / uv_scale_h)) \n\
                 + (y / uv_scale_h) * p.w / uv_scale_w + x / uv_scale_w; \n\
    ivec2 uv_offset = p.w * p.h + ((y / 2) * p.w / 2 + x / 2) * 2 + ivec2(0, 1); \n\
    if (p.in_type == DT_INT8) \n\
    { \n\
        yuv_in.x = sfp(uint(YUV_data_int8[y_offset])) / sfp(p.in_scale); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(uint(YUV_data_int8[uv_offset.x])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(YUV_data_int8[uv_offset.y])) / sfp(p.in_scale); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(uint(YUV_data_int8[u_offset])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(YUV_data_int8[v_offset])) / sfp(p.in_scale); \n\
        } \n\
    } \n\
    else if (p.in_type == DT_INT16) \n\
    { \n\
        if (p.in_format == CF_P010LE) \n\
            yuv_in.x = sfp(uint(YUV_data_int16[y_offset])) / sfp(65535.0); \n\
        else \n\
            yuv_in.x = sfp(uint(YUV_data_int16[y_offset])) / sfp(p.in_scale); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(uint(YUV_data_int16[uv_offset.x])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(YUV_data_int16[uv_offset.y])) / sfp(p.in_scale); \n\
        } \n\
        else if (p.in_format == CF_P010LE) \n\
        { \n\
            yuv_in.y = sfp(uint(YUV_data_int16[uv_offset.x])) / sfp(65535.0); \n\
            yuv_in.z = sfp(uint(YUV_data_int16[uv_offset.y])) / sfp(65535.0); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(uint(YUV_data_int16[u_offset])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(YUV_data_int16[v_offset])) / sfp(p.in_scale); \n\
        } \n\
    } \n\
    else if (p.in_type == DT_FLOAT16) \n\
    { \n\
        yuv_in.x = sfp(YUV_data_float16[y_offset]); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(YUV_data_float16[uv_offset.x]); \n\
            yuv_in.z = sfp(YUV_data_float16[uv_offset.y]); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(YUV_data_float16[u_offset]); \n\
            yuv_in.z = sfp(YUV_data_float16[v_offset]); \n\
        } \n\
    } \n\
    else if (p.in_type == DT_FLOAT32) \n\
    { \n\
        yuv_in.x = sfp(YUV_data_float32[y_offset]); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(YUV_data_float32[uv_offset.x]); \n\
            yuv_in.z = sfp(YUV_data_float32[uv_offset.y]); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(YUV_data_float32[u_offset]); \n\
            yuv_in.z = sfp(YUV_data_float32[v_offset]); \n\
        } \n\
    } \n\
    return yuv_in; \n\
} \
"

#define CC_INTERPLATE_NONE \
" \n\
sfpvec3 cc_interplate_none(int x, int y) \n\
{ \n\
    return load_src_yuv(x, y); \n\
} \
"

#define CC_INTERPLATE_NEAREST \
" \n\
sfpvec3 cc_interplate_nearest(int x, int y) \n\
{ \n\
    float fx = float(p.out_w) / float(p.in_real_w); \n\
    float fy = float(p.out_h) / float(p.in_real_h); \n\
    int srcx = int(floor(x / fx)); \n\
    int srcy = int(floor(y / fy)); \n\
    srcx = min(srcx, p.w - 1); \n\
    srcy = min(srcy, p.h - 1); \n\
    return load_src_yuv(srcx, srcy); \n\
} \
"

#define CC_INTERPLATE_BILINEAR \
" \n\
sfpvec3 cc_interplate_bilinear(int x, int y) \n\
{ \n\
    float fx = float(p.out_w) / float(p.in_real_w); \n\
    float fy = float(p.out_h) / float(p.in_real_h); \n\
    float srcx = x / fx; \n\
    float srcy = y / fy; \n\
    int _x = int(floor(srcx)); \n\
    sfp u = sfp(srcx) - sfp(_x); \n\
    if (u < sfp(0.f)) \n\
    { \n\
        _x = 0; \n\
        u = sfp(0.f); \n\
    } \n\
    if (_x >= p.w - 1) \n\
    { \n\
        _x = p.w - 2; \n\
        u = sfp(1.f); \n\
    } \n\
    int _y = int(floor(srcy)); \n\
    sfp v = sfp(srcy) - sfp(_y); \n\
    if (v < sfp(0.f)) \n\
    { \n\
        _y = 0; \n\
        v = sfp(0.f); \n\
    } \n\
    if (_y >= p.h - 1) \n\
    { \n\
        _y = p.h - 2; \n\
        v = sfp(1.f); \n\
    } \n\
    sfpvec3 _v = {sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    sfpvec3 _x_y   = load_src_yuv(_x,     _y    ); \n\
    sfpvec3 _x1_y  = load_src_yuv(_x + 1, _y    ); \n\
    sfpvec3 _x_y1  = load_src_yuv(_x,     _y + 1); \n\
    sfpvec3 _x1_y1 = load_src_yuv(_x + 1, _y + 1); \n\
    _v = (sfp(1.f) - u) * (sfp(1.f) - v) * _x_y +  \n\
        (sfp(1.f) - u) * v * _x_y1 + \n\
        u * (sfp(1.f) - v) * _x1_y + \n\
        u * v * _x1_y1; \n\
    return _v; \n\
} \
"

#define CC_INTERPLATE_BICUBIC \
" \n\
sfpvec3 cc_interplate_bicubic(int x, int y) \n\
{ \n\
    const sfp A = sfp(-0.75f); \n\
    sfp scale_x = sfp(p.in_real_w) / sfp(p.out_w); \n\
    sfp scale_y = sfp(p.in_real_h) / sfp(p.out_h); \n\
    sfp fx = sfp((x + 0.5f) * scale_x - 0.5f); \n\
	int sx = int(floor(fx)); \n\
	fx -= sfp(sx); \n\
	if (sx < 1)  \n\
    { \n\
		fx = sfp(0.f), sx = 1; \n\
	} \n\
	if (sx >= p.w - 3) \n\
    { \n\
		fx = sfp(0.f), sx = p.w - 3; \n\
	} \n\
	sfp cbufX[4]; \n\
	cbufX[0] = ((A*(fx + sfp(1.f)) - sfp(5.f)*A)*(fx + sfp(1.f)) + sfp(8.f)*A)*(fx + sfp(1.f)) - sfp(4.f)*A; \n\
	cbufX[1] = ((A + sfp(2.f))*fx - (A + sfp(3.f)))*fx*fx + sfp(1.f); \n\
	cbufX[2] = ((A + sfp(2.f))*(sfp(1.f) - fx) - (A + sfp(3.f)))*(sfp(1.f) - fx)*(sfp(1.f) - fx) + sfp(1.f); \n\
	cbufX[3] = sfp(1.f) - cbufX[0] - cbufX[1] - cbufX[2]; \n\
    sfp fy = sfp((y + 0.5f) * scale_y - 0.5f); \n\
    int sy = int(floor(fy)); \n\
	fy -= sfp(sy); \n\
	sy = min(sy, p.h - 3); \n\
	sy = max(1, sy); \n\
    sfp cbufY[4]; \n\
	cbufY[0] = ((A*(fy + sfp(1.f)) - sfp(5.f)*A)*(fy + sfp(1.f)) + sfp(8.f)*A)*(fy + sfp(1.f)) - sfp(4.f)*A; \n\
	cbufY[1] = ((A + sfp(2.f))*fy - (A + sfp(3.f)))*fy*fy + sfp(1.f); \n\
	cbufY[2] = ((A + sfp(2.f))*(sfp(1.f) - fy) - (A + sfp(3.f)))*(sfp(1.f) - fy)*(sfp(1.f) - fy) + sfp(1.f); \n\
	cbufY[3] = sfp(1.f) - cbufY[0] - cbufY[1] - cbufY[2]; \n\
    \n\
    sfpvec3 _v = {sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    sfpvec3 _x_1_y_1 = load_src_yuv(sx-1, sy-1); \n\
    sfpvec3 _x_1_y   = load_src_yuv(sx-1, sy  ); \n\
    sfpvec3 _x_1_y1  = load_src_yuv(sx-1, sy+1); \n\
    sfpvec3 _x_1_y2  = load_src_yuv(sx-1, sy+2); \n\
    sfpvec3 _x_y_1   = load_src_yuv(sx,   sy-1); \n\
    sfpvec3 _x_y     = load_src_yuv(sx,   sy  ); \n\
    sfpvec3 _x_y1    = load_src_yuv(sx,   sy+1); \n\
    sfpvec3 _x_y2    = load_src_yuv(sx,   sy+2); \n\
    sfpvec3 _x1_y_1  = load_src_yuv(sx+1, sy-1); \n\
    sfpvec3 _x1_y    = load_src_yuv(sx+1, sy  ); \n\
    sfpvec3 _x1_y1   = load_src_yuv(sx+1, sy+1); \n\
    sfpvec3 _x1_y2   = load_src_yuv(sx+1, sy+2); \n\
    sfpvec3 _x2_y_1  = load_src_yuv(sx+2, sy-1); \n\
    sfpvec3 _x2_y    = load_src_yuv(sx+2, sy  ); \n\
    sfpvec3 _x2_y1   = load_src_yuv(sx+2, sy+1); \n\
    sfpvec3 _x2_y2   = load_src_yuv(sx+2, sy+2); \n\
    \n\
    _v = \n\
        _x_1_y_1 * cbufX[0] * cbufY[0] + _x_1_y  * cbufX[0] * cbufY[1] + \n\
		_x_1_y1  * cbufX[0] * cbufY[2] + _x_1_y2 * cbufX[0] * cbufY[3] + \n\
		_x_y_1   * cbufX[1] * cbufY[0] + _x_y    * cbufX[1] * cbufY[1] + \n\
		_x_y1    * cbufX[1] * cbufY[2] + _x_y2   * cbufX[1] * cbufY[3] + \n\
		_x1_y_1  * cbufX[2] * cbufY[0] + _x1_y   * cbufX[2] * cbufY[1] + \n\
		_x1_y1   * cbufX[2] * cbufY[2] + _x1_y2  * cbufX[2] * cbufY[3] + \n\
		_x2_y_1  * cbufX[3] * cbufY[0] + _x2_y   * cbufX[3] * cbufY[1] + \n\
		_x2_y1   * cbufX[3] * cbufY[2] + _x2_y2  * cbufX[3] * cbufY[3]; \n\
    return _v; \n\
} \
"

#define CC_INTERPLATE_AREA \
" \n\
sfpvec3 cc_interplate_area(int x, int y) \n\
{ \n\
    sfpvec3 _v = {sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    sfp scale_x = sfp(p.in_real_w) / sfp(p.out_w); \n\
    sfp scale_y = sfp(p.in_real_h) / sfp(p.out_h); \n\
	sfp inv_scale_x = sfp(1.f) / scale_x; \n\
	sfp inv_scale_y = sfp(1.f) / scale_y; \n\
    if (scale_x > sfp(2.f) && scale_y > sfp(2.f)) \n\
    { \n\
        sfp fsx1 = sfp(x) * scale_x; \n\
        sfp fsx2 = fsx1 + scale_x; \n\
        sfp fsy1 = sfp(y) * scale_y; \n\
        sfp fsy2 = fsy1 + scale_y; \n\
        \n\
        int sx1 = int(floor(fsx1)), sx2 = int(ceil(fsx2)); \n\
        int sy1 = int(floor(fsy1)), sy2 = int(ceil(fsy2)); \n\
        if (sx1 < 0) sx1 = 0; \n\
        if (sx2 > p.w - 1) sx2 = p.w - 1; \n\
        if (sy1 < 0) sy1 = 0; \n\
        if (sy2 > p.h - 1) sy2 = p.h - 1; \n\
        int i_cellWidth = sx2 - sx1; \n\
        int i_cellHeight = sy2 - sy1; \n\
        for (int j = 0; j < i_cellHeight; j++) \n\
        { \n\
            for (int i = 0; i < i_cellWidth; i++) \n\
            { \n\
                _v += load_src_yuv(sx1 + i, sy1 + j); \n\
            } \n\
        } \n\
        _v /= sfp(i_cellWidth * i_cellHeight); \n\
    } \n\
    else \n\
    { \n\
		int sx = int(floor(sfp(x) * scale_x)); \n\
		sfp fx = sfp(sfp(x + 1) - sfp(sx + 1) * inv_scale_x); \n\
		fx = fx < sfp(0.f) ? sfp(0.f) : fx - floor(fx); \n\
		if (sx < 0) \n\
        { \n\
			fx = sfp(0.f), sx = 0; \n\
		} \n\
		if (sx >= p.w - 1) \n\
        { \n\
			fx = sfp(0.f), sx = p.w - 2; \n\
		} \n\
		sfp cbufx[2]; \n\
		cbufx[0] = sfp(1.f) - fx; \n\
		cbufx[1] = sfp(1.f) - cbufx[0]; \n\
        \n\
		int sy = int(floor(sfp(y) * scale_y)); \n\
		sfp fy = sfp(sfp(y + 1) - sfp(sy + 1) * inv_scale_y); \n\
		fy = fy <= sfp(0.f) ? sfp(0.f) : fy - floor(fy); \n\
		sy = min(sy, p.h - 2); \n\
		sfp cbufy[2]; \n\
		cbufy[0] = sfp(1.f) - fy; \n\
		cbufy[1] = sfp(1.f) - cbufy[0]; \n\
        _v = load_src_yuv(sx,     sy    ) * cbufx[0] * cbufy[0] + \n\
             load_src_yuv(sx,     sy + 1) * cbufx[0] * cbufy[1] + \n\
             load_src_yuv(sx + 1, sy    ) * cbufx[1] * cbufy[0] + \n\
             load_src_yuv(sx + 1, sy + 1) * cbufx[1] * cbufy[1]; \n\
    } \n\
    return _v; \n\
} \
"

#define CC_INTERPLATE \
CC_INTERPLATE_NONE \
CC_INTERPLATE_NEAREST \
CC_INTERPLATE_BILINEAR \
CC_INTERPLATE_BICUBIC \
CC_INTERPLATE_AREA \
" \n\
sfpvec3 cc_interplate(int x, int y) \n\
{ \n\
    if (p.interp_type == INTERPOLATE_NONE) \n\
        return cc_interplate_none(x, y); \n\
    if (p.interp_type == INTERPOLATE_NEAREST) \n\
        return cc_interplate_nearest(x, y); \n\
    else if (p.interp_type == INTERPOLATE_BILINEAR) \n\
        return cc_interplate_bilinear(x, y); \n\
    else if (p.interp_type == INTERPOLATE_BICUBIC) \n\
        return cc_interplate_bicubic(x, y); \n\
    else if (p.interp_type == INTERPOLATE_AREA) \n\
        return cc_interplate_area(x, y); \n\
    else \n\
        return cc_interplate_nearest(x, y); \n\
} \
"

#define SHADER_YUV2RGB_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec3 rgb; \n\
    if (p.resize == 1) \n\
        rgb = yuv_to_rgb(cc_interplate(gx, gy)); \n\
    else \n\
        rgb = yuv_to_rgb(load_src_yuv(gx, gy)); \n\
    if (p.out_format == CF_ABGR || p.out_format == CF_ARGB || p.out_format == CF_BGRA || p.out_format == CF_RGBA) \n\
    { \n\
        if (p.mirror == 1) \n\
            store_rgba(sfpvec4(rgb, 1.0f), p.out_w - gx - 1, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        else \n\
            store_rgba(sfpvec4(rgb, 1.0f), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
    else \n\
    { \n\
        if (p.mirror == 1) \n\
            store_rgb(rgb, p.out_w - gx - 1, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
        else \n\
            store_rgb(rgb, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    } \n\
} \
"

static const char YUV2RGB_data[] = 
SHADER_HEADER
SHADER_OUTPUT_DATA      // binging 0-3
R"(
layout (binding = 4) readonly buffer YUV_int8       { uint8_t       YUV_data_int8[]; };
layout (binding = 5) readonly buffer YUV_int16      { uint16_t      YUV_data_int16[]; };
layout (binding = 6) readonly buffer YUV_float16    { float16_t     YUV_data_float16[]; };
layout (binding = 7) readonly buffer YUV_float32    { float         YUV_data_float32[]; };
layout (binding = 8) readonly buffer mat_y2r        { float         convert_matrix_y2r[]; };
)"
SHADER_MAT_Y2R
SHADER_PARAM_Y2R
SHADER_YUV2RGB
SHADER_LOAD_SRC_YUV
CC_INTERPLATE
SHADER_STORE_RGB
SHADER_STORE_RGBA
SHADER_YUV2RGB_MAIN
;

#define SHADER_PARAM_Y_U_V_2R \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    int in_space; \n\
    int in_range; \n\
    float in_scale; \n\
    int in_real_w; \n\
    int in_real_h; \n\
    int out_w; \n\
    int out_h; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    int resize; \n\
    int interp_type; \n\
    int uw; \n\
    int vw; \n\
    int mirror; \n\
} p;\
"

#define SHADER_LOAD_SRC_Y_U_V \
" \n\
sfpvec3 load_src_yuv(int x, int y) \n\
{ \n\
    sfpvec3 yuv_in = {sfp(0.0f), sfp(0.5f), sfp(0.5f)}; \n\
    int uv_scale_w = p.in_format == CF_YUV420 || p.in_format == CF_YUV422 ? 2 : 1; \n\
    int uv_scale_h = p.in_format == CF_YUV420 || p.in_format == CF_YUV440 || p.in_format == CF_NV12 || p.in_format == CF_P010LE ? 2 : 1; \n\
    int y_offset = y * p.w + x; \n\
    int u_offset = (y / uv_scale_h) * p.uw + x / uv_scale_w; \n\
    int v_offset = (y / uv_scale_h) * p.vw + x / uv_scale_w; \n\
    if (p.in_format == CF_NV12 || p.in_format == CF_P010LE) \n\
    { u_offset -= u_offset&0x1; } \n\
    if (p.in_type == DT_INT8) \n\
    { \n\
        yuv_in.x = sfp(uint(Y_data_int8[y_offset])) / sfp(p.in_scale); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(uint(U_data_int8[u_offset  ])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(U_data_int8[u_offset+1])) / sfp(p.in_scale); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(uint(U_data_int8[u_offset])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(V_data_int8[v_offset])) / sfp(p.in_scale); \n\
        } \n\
    } \n\
    else if (p.in_type == DT_INT16) \n\
    { \n\
        if (p.in_format == CF_P010LE) \n\
            yuv_in.x = sfp(uint(Y_data_int16[y_offset])) / sfp(65535.0); \n\
        else \n\
            yuv_in.x = sfp(uint(Y_data_int16[y_offset])) / sfp(p.in_scale); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(uint(U_data_int16[u_offset  ])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(U_data_int16[u_offset+1])) / sfp(p.in_scale); \n\
        } \n\
        else if (p.in_format == CF_P010LE) \n\
        { \n\
            yuv_in.y = sfp(uint(U_data_int16[u_offset  ])) / sfp(65535.0); \n\
            yuv_in.z = sfp(uint(U_data_int16[u_offset+1])) / sfp(65535.0); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(uint(U_data_int16[u_offset])) / sfp(p.in_scale); \n\
            yuv_in.z = sfp(uint(V_data_int16[v_offset])) / sfp(p.in_scale); \n\
        } \n\
    } \n\
    else if (p.in_type == DT_FLOAT16) \n\
    { \n\
        yuv_in.x = sfp(Y_data_float16[y_offset]); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(U_data_float16[u_offset  ]); \n\
            yuv_in.z = sfp(U_data_float16[u_offset+1]); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(U_data_float16[u_offset]); \n\
            yuv_in.z = sfp(V_data_float16[v_offset]); \n\
        } \n\
    } \n\
    else if (p.in_type == DT_FLOAT32) \n\
    { \n\
        yuv_in.x = sfp(Y_data_float32[y_offset]); \n\
        if (p.in_format == CF_NV12) \n\
        { \n\
            yuv_in.y = sfp(U_data_float32[u_offset  ]); \n\
            yuv_in.z = sfp(U_data_float32[u_offset+1]); \n\
        } \n\
        else \n\
        { \n\
            yuv_in.y = sfp(U_data_float32[u_offset]); \n\
            yuv_in.z = sfp(V_data_float32[v_offset]); \n\
        } \n\
    } \n\
    return yuv_in; \n\
} \
"

static const char Y_U_V2RGB_data[] = 
SHADER_HEADER
SHADER_OUTPUT_DATA      // binging 0-3
R"(
layout (binding =  4) readonly buffer Y_int8       { uint8_t       Y_data_int8[]; };
layout (binding =  5) readonly buffer Y_int16      { uint16_t      Y_data_int16[]; };
layout (binding =  6) readonly buffer Y_float16    { float16_t     Y_data_float16[]; };
layout (binding =  7) readonly buffer Y_float32    { float         Y_data_float32[]; };
layout (binding =  8) readonly buffer U_int8       { uint8_t       U_data_int8[]; };
layout (binding =  9) readonly buffer U_int16      { uint16_t      U_data_int16[]; };
layout (binding = 10) readonly buffer U_float16    { float16_t     U_data_float16[]; };
layout (binding = 11) readonly buffer U_float32    { float         U_data_float32[]; };
layout (binding = 12) readonly buffer V_int8       { uint8_t       V_data_int8[]; };
layout (binding = 13) readonly buffer V_int16      { uint16_t      V_data_int16[]; };
layout (binding = 14) readonly buffer V_float16    { float16_t     V_data_float16[]; };
layout (binding = 15) readonly buffer V_float32    { float         V_data_float32[]; };
layout (binding = 16) readonly buffer mat_y2r      { float         convert_matrix_y2r[]; };
)"
SHADER_MAT_Y2R
SHADER_PARAM_Y_U_V_2R
SHADER_YUV2RGB
SHADER_LOAD_SRC_Y_U_V
CC_INTERPLATE
SHADER_STORE_RGB
SHADER_STORE_RGBA
SHADER_YUV2RGB_MAIN
;

#define SHADER_PARAM_R2Y \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
    int in_space; \n\
    int in_range; \n\
    int out_cstep; \n\
    int out_format; \n\
    int out_type; \n\
    int out_space; \n\
    int out_range; \n\
    float in_scale; \n\
} p;\
"

#define SHADER_RGB2YUV \
" \n\
sfpvec3 rgb_to_yuv(sfpvec3 rgb) \n\
{\n\
    sfpvec3 yuv; \n\
    sfpvec3 yuv_offset = {sfp(0.f), sfp(0.5f), sfp(0.5)}; \n\
    if (p.out_range == CR_NARROW_RANGE) \n\
        yuv_offset.x = sfp(16.0 / 255.0); \n\
    yuv = yuv_offset + matrix_mat_r2y * rgb; \n\
    return clamp(yuv, sfp(0.f), sfp(1.f)); \n\
} \
"

#define SHADER_STORE_YUV_INT8 \
" \n\
void store_yuv_int8(sfpvec3 yuv, int y_offset, int u_offset, int v_offset, ivec2 uv_offset) \n\
{ \n\
    dst_data_int8[y_offset] = uint8_t(clamp(uint(floor(yuv.x * sfp(255.0f))), 0, 255)); \n\
    if (p.out_format == CF_NV12) \n\
    { \n\
        dst_data_int8[uv_offset.x] = uint8_t(clamp(uint(floor(yuv.y * sfp(255.0f))), 0, 255)); \n\
        dst_data_int8[uv_offset.y] = uint8_t(clamp(uint(floor(yuv.z * sfp(255.0f))), 0, 255)); \n\
    } \n\
    else \n\
    { \n\
        dst_data_int8[u_offset] = uint8_t(clamp(uint(floor(yuv.y * sfp(255.0f))), 0, 255)); \n\
        dst_data_int8[v_offset] = uint8_t(clamp(uint(floor(yuv.z * sfp(255.0f))), 0, 255)); \n\
    } \n\
} \
"

#define SHADER_STORE_YUV_INT16 \
" \n\
void store_yuv_int16(sfpvec3 yuv, int y_offset, int u_offset, int v_offset, ivec2 uv_offset) \n\
{ \n\
    if (p.out_format == CF_P010LE) \n\
    { \n\
        uint16_t y = uint16_t(uint(floor(yuv.x * sfp(255.0)))); \n\
        uint16_t hy = uint16_t(uint(floor(yuv.x * sfp(65535.0)))); \n\
        dst_data_int16[y_offset] = y | hy; \n\
    } \n\
    else \n\
        dst_data_int16[y_offset] = uint16_t(clamp(uint(floor(yuv.x * sfp(p.in_scale))), 0, uint(p.in_scale))); \n\
    if (p.out_format == CF_NV12) \n\
    { \n\
        dst_data_int16[uv_offset.x] = uint16_t(clamp(uint(floor(yuv.y * sfp(p.in_scale))), 0, uint(p.in_scale))); \n\
        dst_data_int16[uv_offset.y] = uint16_t(clamp(uint(floor(yuv.z * sfp(p.in_scale))), 0, uint(p.in_scale))); \n\
    } \n\
    else if (p.out_format == CF_P010LE) \n\
    { \n\
        uint16_t u = uint16_t(uint(floor(yuv.y * sfp(255.0)))); \n\
        uint16_t hu = uint16_t(uint(floor(yuv.y * sfp(65535.0)))); \n\
        uint16_t v = uint16_t(uint(floor(yuv.z * sfp(255.0)))); \n\
        uint16_t hv = uint16_t(uint(floor(yuv.z * sfp(65535.0)))); \n\
        dst_data_int16[uv_offset.x] = u | hu; \n\
        dst_data_int16[uv_offset.y] = v | hv; \n\
    } \n\
    else \n\
    { \n\
        dst_data_int16[u_offset] = uint16_t(clamp(uint(floor(yuv.y * sfp(p.in_scale))), 0, uint(p.in_scale))); \n\
        dst_data_int16[v_offset] = uint16_t(clamp(uint(floor(yuv.z * sfp(p.in_scale))), 0, uint(p.in_scale))); \n\
    } \n\
} \
"

#define SHADER_STORE_YUV_FLOAT16 \
" \n\
void store_yuv_float16(sfpvec3 yuv, int y_offset, int u_offset, int v_offset, ivec2 uv_offset) \n\
{ \n\
    dst_data_float16[y_offset] = float16_t(clamp(yuv.x, sfp(0.0f), sfp(1.0f))); \n\
    if (p.out_format == CF_NV12) \n\
    { \n\
        dst_data_float16[uv_offset.x] = float16_t(clamp(yuv.y, sfp(0.0f), sfp(1.0f))); \n\
        dst_data_float16[uv_offset.y] = float16_t(clamp(yuv.z, sfp(0.0f), sfp(1.0f))); \n\
    } \n\
    else \n\
    { \n\
        dst_data_float16[u_offset] = float16_t(clamp(yuv.y, sfp(0.0f), sfp(1.0f))); \n\
        dst_data_float16[v_offset] = float16_t(clamp(yuv.z, sfp(0.0f), sfp(1.0f))); \n\
    } \n\
} \
"

#define SHADER_STORE_YUV_FLOAT32 \
" \n\
void store_yuv_float32(sfpvec3 yuv, int y_offset, int u_offset, int v_offset, ivec2 uv_offset) \n\
{ \n\
    dst_data_float32[y_offset] = clamp(yuv.x, sfp(0.0f), sfp(1.0f)); \n\
    if (p.out_format == CF_NV12) \n\
    { \n\
        dst_data_float32[uv_offset.x] = float(clamp(yuv.y, sfp(0.0f), sfp(1.0f))); \n\
        dst_data_float32[uv_offset.y] = float(clamp(yuv.z, sfp(0.0f), sfp(1.0f))); \n\
    } \n\
    else \n\
    { \n\
        dst_data_float32[u_offset] = float(clamp(yuv.y, sfp(0.0f), sfp(1.0f))); \n\
        dst_data_float32[v_offset] = float(clamp(yuv.z, sfp(0.0f), sfp(1.0f))); \n\
    } \n\
} \
"

#define SHADER_STORE_YUV \
SHADER_STORE_YUV_INT8 \
SHADER_STORE_YUV_INT16 \
SHADER_STORE_YUV_FLOAT16 \
SHADER_STORE_YUV_FLOAT32 \
" \n\
void store_dst_yuv(sfpvec3 v, int x, int y) \n\
{ \n\
    int uv_scale_w = p.out_format == CF_YUV420 || p.out_format == CF_YUV422 ? 2 : 1; \n\
    int uv_scale_h = p.out_format == CF_YUV420 || p.out_format == CF_YUV440 || p.out_format == CF_NV12 || p.out_format == CF_P010LE ? 2 : 1; \n\
    int y_offset = y * p.w + x; \n\
    int u_offset = p.out_cstep + (y / uv_scale_h) * p.w / uv_scale_w + x / uv_scale_w; \n\
    int v_offset = (p.out_format == CF_YUV444 ? p.out_cstep * 2 : p.out_cstep + (p.w / uv_scale_w) * (p.h / uv_scale_h)) \n\
                + (y / uv_scale_h) * p.w / uv_scale_w + x / uv_scale_w; \n\
    ivec2 uv_offset = p.out_cstep + ((y / 2) * p.w / 2 + x / 2) * 2 + ivec2(0, 1); \n\
    if (p.out_type == DT_INT8) \n\
        store_yuv_int8(v, y_offset, u_offset, v_offset, uv_offset); \n\
    else if (p.out_type == DT_INT16) \n\
        store_yuv_int16(v, y_offset, u_offset, v_offset, uv_offset); \n\
    else if (p.out_type == DT_FLOAT16) \n\
        store_yuv_float16(v, y_offset, u_offset, v_offset, uv_offset); \n\
    else if (p.out_type == DT_FLOAT32) \n\
        store_yuv_float32(v, y_offset, u_offset, v_offset, uv_offset); \n\
} \
"

#define SHADER_RGB2YUV_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.w || gy >= p.h) \n\
        return; \n\
    sfpvec3 yuv; \n\
    if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        yuv = rgb_to_yuv(load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb); \n\
    else \n\
        yuv = rgb_to_yuv(load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type).rgb); \n\
    store_dst_yuv(yuv, gx, gy); \n\
} \
"

static const char RGB2YUV_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer mat_r2y        { float         convert_matrix_r2y[]; };
)"
SHADER_MAT_R2Y
SHADER_PARAM_R2Y
SHADER_LOAD_RGB
SHADER_LOAD_RGBA
SHADER_RGB2YUV
SHADER_STORE_YUV
SHADER_RGB2YUV_MAIN
;

#define SHADER_PARAM_G2R \
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
    float in_scale; \n\
} p;\
"

#define SHADER_GRAY2RGB_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfp gray = load_gray(gx, gy, p.w, p.h, 1, p.in_format, p.in_type, p.in_scale); \n\
    if (p.in_format == CF_NV12) \n\
        store_rgba_side_by_side(sfpvec4(sfpvec3(gray), 1.0f), gx, gy, p.out_w, p.out_cstep, p.out_format, p.out_type); \n\
    else \n\
        store_rgba(sfpvec4(sfpvec3(gray), 1.0f), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char GRAY2RGB_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
SHADER_PARAM_G2R
SHADER_LOAD_GRAY
SHADER_STORE_RGBA
SHADER_STORE_RGBA_SIDE_BY_SIDE
SHADER_GRAY2RGB_MAIN
;

#define SHADER_PARAM_CONV \
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
} p;\
"

#define SHADER_CONV_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba; \n\
    if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        rgba = sfpvec4(load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type), sfp(1.0f)); \n\
    else \n\
        rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
        store_rgb(rgba.rgb, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else \n\
        store_rgba(rgba, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Conv_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
SHADER_PARAM_CONV
SHADER_LOAD_RGBA
SHADER_LOAD_RGB
SHADER_STORE_RGBA
SHADER_STORE_RGB
SHADER_CONV_MAIN
;

#define SHADER_MAT_R2X \
" \n\
sfpmat3 matrix_mat_r2x = { \n\
    {sfp(convert_matrix_r2x[0]), sfp(convert_matrix_r2x[3]), sfp(convert_matrix_r2x[6])}, \n\
    {sfp(convert_matrix_r2x[1]), sfp(convert_matrix_r2x[4]), sfp(convert_matrix_r2x[7])}, \n\
    {sfp(convert_matrix_r2x[2]), sfp(convert_matrix_r2x[5]), sfp(convert_matrix_r2x[8])}, \n\
}; \
"

#define RGB2LAB_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba; \n\
    if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        rgba = sfpvec4(load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type), sfp(1.0f)); \n\
    else \n\
        rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    // SRGB -> LRGB \n\
    if (rgba.r > sfp(0.04045)) rgba.r = pow((rgba.r + sfp(0.055)) / sfp(1.055), sfp(2.4)); else rgba.r = rgba.r / sfp(12.92); \n\
    if (rgba.g > sfp(0.04045)) rgba.g = pow((rgba.g + sfp(0.055)) / sfp(1.055), sfp(2.4)); else rgba.g = rgba.g / sfp(12.92); \n\
    if (rgba.b > sfp(0.04045)) rgba.b = pow((rgba.b + sfp(0.055)) / sfp(1.055), sfp(2.4)); else rgba.b = rgba.b / sfp(12.92); \n\
    sfpvec3 xyz = matrix_mat_r2x * rgba.rgb; \n\
    sfpvec3 lab = xyz2lab(xyz); \n\
    store_rgb_float_no_clamp(lab, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char RGB2LAB_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer mat_r2x        { float         convert_matrix_r2x[]; };
)"
SHADER_PARAM_CONV
SHADER_LOAD_RGBA
SHADER_LOAD_RGB
SHADER_STORE_RGB_FLOAT_NO_CLAMP
SHADER_MAT_R2X
SHADER_XYZ_TO_LAB
RGB2LAB_MAIN
;

#define SHADER_MAT_X2R \
" \n\
sfpmat3 matrix_mat_x2r = { \n\
    {sfp(convert_matrix_x2r[0]), sfp(convert_matrix_x2r[3]), sfp(convert_matrix_x2r[6])}, \n\
    {sfp(convert_matrix_x2r[1]), sfp(convert_matrix_x2r[4]), sfp(convert_matrix_x2r[7])}, \n\
    {sfp(convert_matrix_x2r[2]), sfp(convert_matrix_x2r[5]), sfp(convert_matrix_x2r[8])}, \n\
}; \
"

#define LAB2RGB_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec3 lab = load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 xyz = lab2xyz(lab); \n\
    sfpvec3 rgb = matrix_mat_x2r * xyz; \n\
    // LRGB -> SRGB \n\
    if (rgb.r > sfp(0.0031308)) rgb.r = sfp(1.055) * pow(rgb.r, sfp(1.0 / 2.4)) - sfp(0.055); else rgb.r = rgb.r * sfp(12.92); \n\
    if (rgb.g > sfp(0.0031308)) rgb.g = sfp(1.055) * pow(rgb.g, sfp(1.0 / 2.4)) - sfp(0.055); else rgb.g = rgb.g * sfp(12.92); \n\
    if (rgb.b > sfp(0.0031308)) rgb.b = sfp(1.055) * pow(rgb.b, sfp(1.0 / 2.4)) - sfp(0.055); else rgb.b = rgb.b * sfp(12.92); \n\
    if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
        store_rgb(rgb, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else \n\
        store_rgba(sfpvec4(rgb, sfp(1.0)), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char LAB2RGB_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding = 8) readonly buffer mat_x2r        { float         convert_matrix_x2r[]; };
)"
SHADER_PARAM_CONV
SHADER_LOAD_RGB
SHADER_STORE_RGBA
SHADER_STORE_RGB
SHADER_MAT_X2R
SHADER_LAB_TO_XYZ
LAB2RGB_MAIN
;

#define RGB2HSL_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba; \n\
    if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        rgba = sfpvec4(load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type), sfp(1.0f)); \n\
    else \n\
        rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 hsl = rgb_to_hsl(rgba.rgb); \n\
    store_rgb_float_no_clamp(hsl, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char RGB2HSL_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
SHADER_PARAM_CONV
SHADER_LOAD_RGBA
SHADER_LOAD_RGB
SHADER_STORE_RGB_FLOAT_NO_CLAMP
SHADER_RGB_TO_HSL
RGB2HSL_MAIN
;

#define HSL2RGB_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec3 hsl = load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 rgb = hsl_to_rgb(hsl); \n\
    if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
        store_rgb(rgb, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else \n\
        store_rgba(sfpvec4(rgb, sfp(1.0)), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char HSL2RGB_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
SHADER_PARAM_CONV
SHADER_LOAD_RGB
SHADER_STORE_RGBA
SHADER_STORE_RGB
SHADER_HSL_TO_RGB
HSL2RGB_MAIN
;

#define RGB2HSV_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 rgba; \n\
    if (p.in_format == CF_BGR || p.in_format == CF_RGB) \n\
        rgba = sfpvec4(load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type), sfp(1.0f)); \n\
    else \n\
        rgba = load_rgba(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 hsv = rgb_to_hsv(rgba.rgb); \n\
    store_rgb_float_no_clamp(hsv, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char RGB2HSV_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
SHADER_PARAM_CONV
SHADER_LOAD_RGBA
SHADER_LOAD_RGB
SHADER_STORE_RGB_FLOAT_NO_CLAMP
SHADER_RGB_TO_HSV
RGB2HSV_MAIN
;

#define HSV2RGB_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec3 hsv = load_rgb(gx, gy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec3 rgb = hsv_to_rgb(hsv); \n\
    if (p.out_format == CF_BGR || p.out_format == CF_RGB) \n\
        store_rgb(rgb, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else \n\
        store_rgba(sfpvec4(rgb, sfp(1.0)), gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char HSV2RGB_data[] = 
SHADER_HEADER
SHADER_INPUT_OUTPUT_DATA
SHADER_PARAM_CONV
SHADER_LOAD_RGB
SHADER_STORE_RGBA
SHADER_STORE_RGB
SHADER_HSV_TO_RGB
HSV2RGB_MAIN
;