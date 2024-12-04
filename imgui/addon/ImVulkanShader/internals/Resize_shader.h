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
    int interp_type; \n\
} p; \
"

#define INTERPLATE_NONE \
" \n\
sfpvec4 interplate_none(int x, int y) \n\
{ \n\
    return load_rgb_image(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
} \
"

#define INTERPLATE_NEAREST \
" \n\
sfpvec4 interplate_nearest(int x, int y) \n\
{ \n\
    float fx = float(p.out_w) / float(p.w); \n\
    float fy = float(p.out_h) / float(p.h); \n\
    int srcx = int(floor(x / fx)); \n\
    int srcy = int(floor(y / fy)); \n\
    srcx = min(srcx, p.w - 1); \n\
    srcy = min(srcy, p.h - 1); \n\
    return load_rgb_image(srcx, srcy, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
} \
"

#define INTERPLATE_BILINEAR \
" \n\
sfpvec4 interplate_bilinear(int x, int y) \n\
{ \n\
    float fx = float(p.out_w) / float(p.w); \n\
    float fy = float(p.out_h) / float(p.h); \n\
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
    sfpvec4 _v = {sfp(0.f), sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    sfpvec4 _x_y   = load_rgb_image(_x,     _y,      p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y  = load_rgb_image(_x + 1, _y,      p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y1  = load_rgb_image(_x,     _y + 1,  p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y1 = load_rgb_image(_x + 1, _y + 1,  p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    _v = (sfp(1.f) - u) * (sfp(1.f) - v) * _x_y +  \n\
        (sfp(1.f) - u) * v * _x_y1 + \n\
        u * (sfp(1.f) - v) * _x1_y + \n\
        u * v * _x1_y1; \n\
    return _v; \n\
} \
"

#define INTERPLATE_BICUBIC \
" \n\
sfpvec4 interplate_bicubic(int x, int y) \n\
{ \n\
    const sfp A = sfp(-0.75f); \n\
    sfp scale_x = sfp(p.w) / sfp(p.out_w); \n\
    sfp scale_y = sfp(p.h) / sfp(p.out_h); \n\
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
    sfpvec4 _v = {sfp(0.f), sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    sfpvec4 _x_1_y_1 = load_rgb_image(sx-1, sy-1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_1_y   = load_rgb_image(sx-1, sy,   p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_1_y1  = load_rgb_image(sx-1, sy+1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_1_y2  = load_rgb_image(sx-1, sy+2, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y_1   = load_rgb_image(sx,   sy-1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y     = load_rgb_image(sx,   sy,   p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y1    = load_rgb_image(sx,   sy+1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x_y2    = load_rgb_image(sx,   sy+2, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y_1  = load_rgb_image(sx+1, sy-1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y    = load_rgb_image(sx+1, sy,   p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y1   = load_rgb_image(sx+1, sy+1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x1_y2   = load_rgb_image(sx+1, sy+2, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x2_y_1  = load_rgb_image(sx+2, sy-1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x2_y    = load_rgb_image(sx+2, sy,   p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x2_y1   = load_rgb_image(sx+2, sy+1, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 _x2_y2   = load_rgb_image(sx+2, sy+2, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
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

#define INTERPLATE_AREA \
" \n\
sfpvec4 interplate_area(int x, int y) \n\
{ \n\
    sfpvec4 _v = {sfp(0.f), sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    sfp scale_x = sfp(p.w) / sfp(p.out_w); \n\
    sfp scale_y = sfp(p.h) / sfp(p.out_h); \n\
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
                _v += load_rgb_image(sx1 + i, sy1 + j, p.w, p.h, p.cstep, p.in_format, p.in_type);// * dx * dy; \n\
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
        _v = load_rgb_image(sx,     sy,     p.w, p.h, p.cstep, p.in_format, p.in_type) * cbufx[0] * cbufy[0] + \n\
             load_rgb_image(sx,     sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type) * cbufx[0] * cbufy[1] + \n\
             load_rgb_image(sx + 1, sy,     p.w, p.h, p.cstep, p.in_format, p.in_type) * cbufx[1] * cbufy[0] + \n\
             load_rgb_image(sx + 1, sy + 1, p.w, p.h, p.cstep, p.in_format, p.in_type) * cbufx[1] * cbufy[1]; \n\
    } \n\
    return _v; \n\
} \
"

#define INTERPLATE \
" \n\
sfpvec4 interplate(int x, int y) \n\
{ \n\
    if (p.interp_type == INTERPOLATE_NONE) \n\
        return interplate_none(x, y); \n\
    if (p.interp_type == INTERPOLATE_NEAREST) \n\
        return interplate_nearest(x, y); \n\
    else if (p.interp_type == INTERPOLATE_BILINEAR) \n\
        return interplate_bilinear(x, y); \n\
    else if (p.interp_type == INTERPOLATE_BICUBIC) \n\
        return interplate_bicubic(x, y); \n\
    else if (p.interp_type == INTERPOLATE_AREA) \n\
        return interplate_area(x, y); \n\
    else \n\
        return interplate_nearest(x, y); \n\
} \
"

#define RESIZE_MAIN \
" \n\
void main() \n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    sfpvec4 v = interplate(gx, gy); \n\
    if (p.out_cstep == 4) \n\
        store_rgba(v, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else if (p.out_cstep == 3) \n\
        store_rgb(v.rgb, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    else if (p.out_cstep == 1) \n\
        store_gray(v.r, gx, gy, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Resize_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGB_IMAGE
SHADER_STORE_RGBA
SHADER_STORE_RGB
SHADER_STORE_GRAY
INTERPLATE_NONE
INTERPLATE_NEAREST
INTERPLATE_BILINEAR
INTERPLATE_BICUBIC
INTERPLATE_AREA
INTERPLATE
RESIZE_MAIN
;
