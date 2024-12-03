#pragma once
#include <imvk_shader.h>

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
    float fov; \n\
    float cx; \n\
    float cy; \n\
    int interp_type; \n\
} p; \
"

#define SHADER_FISHEYE \
" \n\
vec2 fish_equidistance_to_orthographic(int u, int v) \n\
{ \n\
	float width = p.w; \n\
	float height = p.h; \n\
	float radius = 0.5; \n\
	vec2 center = vec2(p.cx, p.cy); \n\
	vec2 suv = (vec2(u, v) + vec2(0.5f)) / vec2(width, height);  \n\
	vec2 coord = vec2(suv.x ,(suv.y - center.y) + center.y); \n\
	float dist = distance(center, coord); \n\
	if (dist <= radius) \n\
	{ \n\
		coord = coord - center; \n\
		float theta = atan(coord.y, coord.x); \n\
		float ddist = asin(dist * 2) / M_PI; \n\
		coord.x = ddist * cos(theta); \n\
		coord.y = ddist * sin(theta); \n\
		coord = coord + center; \n\
	} \n\
	return coord * width; \n\
} \n\
"

#define SHADER_MAIN \
" \n\
#define load(x,y) load_rgba(x, y, p.w, p.h, p.cstep, p.in_format, p.in_type) \n\
#define store(v,x,y) store_rgba(v, x, y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type) \n\
void main() \n\n\
{ \n\
    int gx = int(gl_GlobalInvocationID.x); \n\
    int gy = int(gl_GlobalInvocationID.y); \n\
    if (gx >= p.out_w || gy >= p.out_h) \n\
        return; \n\
    vec2 pfish = fish_equidistance_to_orthographic(gx, gy); \n\
    sfpvec4 rgba = {sfp(0.f), sfp(0.f), sfp(0.f), sfp(0.f)}; \n\
    if (p.interp_type == INTERPOLATE_BICUBIC) \n\
    { \n\
        const sfp A = sfp(-0.75f); \n\
        sfp fx = sfp(pfish.x - floor(pfish.x)); \n\
        int sx = int(floor(pfish.x)); \n\
        sx = min(sx, p.w - 3); \n\
        sx = max(1, sx); \n\
        sfp cbufX[4]; \n\
        cbufX[0] = ((A*(fx + sfp(1.f)) - sfp(5.f)*A)*(fx + sfp(1.f)) + sfp(8.f)*A)*(fx + sfp(1.f)) - sfp(4.f)*A; \n\
        cbufX[1] = ((A + sfp(2.f))*fx - (A + sfp(3.f)))*fx*fx + sfp(1.f); \n\
        cbufX[2] = ((A + sfp(2.f))*(sfp(1.f) - fx) - (A + sfp(3.f)))*(sfp(1.f) - fx)*(sfp(1.f) - fx) + sfp(1.f); \n\
        cbufX[3] = sfp(1.f) - cbufX[0] - cbufX[1] - cbufX[2]; \n\
        sfp fy =  sfp(pfish.y - floor(pfish.y)); \n\
        int sy = int(floor(pfish.y)); \n\
        sy = min(sy, p.h - 3); \n\
        sy = max(1, sy); \n\
        sfp cbufY[4]; \n\
        cbufY[0] = ((A*(fy + sfp(1.f)) - sfp(5.f)*A)*(fy + sfp(1.f)) + sfp(8.f)*A)*(fy + sfp(1.f)) - sfp(4.f)*A; \n\
        cbufY[1] = ((A + sfp(2.f))*fy - (A + sfp(3.f)))*fy*fy + sfp(1.f); \n\
        cbufY[2] = ((A + sfp(2.f))*(sfp(1.f) - fy) - (A + sfp(3.f)))*(sfp(1.f) - fy)*(sfp(1.f) - fy) + sfp(1.f); \n\
        cbufY[3] = sfp(1.f) - cbufY[0] - cbufY[1] - cbufY[2]; \n\
        sfpvec4 _x_1_y_1 = load(sx-1, sy-1); \n\
        sfpvec4 _x_1_y   = load(sx-1, sy  ); \n\
        sfpvec4 _x_1_y1  = load(sx-1, sy+1); \n\
        sfpvec4 _x_1_y2  = load(sx-1, sy+2); \n\
        sfpvec4 _x_y_1   = load(sx,   sy-1); \n\
        sfpvec4 _x_y     = load(sx,   sy  ); \n\
        sfpvec4 _x_y1    = load(sx,   sy+1); \n\
        sfpvec4 _x_y2    = load(sx,   sy+2); \n\
        sfpvec4 _x1_y_1  = load(sx+1, sy-1); \n\
        sfpvec4 _x1_y    = load(sx+1, sy  ); \n\
        sfpvec4 _x1_y1   = load(sx+1, sy+1); \n\
        sfpvec4 _x1_y2   = load(sx+1, sy+2); \n\
        sfpvec4 _x2_y_1  = load(sx+2, sy-1); \n\
        sfpvec4 _x2_y    = load(sx+2, sy  ); \n\
        sfpvec4 _x2_y1   = load(sx+2, sy+1); \n\
        sfpvec4 _x2_y2   = load(sx+2, sy+2); \n\
        rgba = \n\
        	_x_1_y_1 * cbufX[0] * cbufY[0] + _x_1_y  * cbufX[0] * cbufY[1] + \n\
        	_x_1_y1  * cbufX[0] * cbufY[2] + _x_1_y2 * cbufX[0] * cbufY[3] + \n\
        	_x_y_1   * cbufX[1] * cbufY[0] + _x_y    * cbufX[1] * cbufY[1] + \n\
        	_x_y1    * cbufX[1] * cbufY[2] + _x_y2   * cbufX[1] * cbufY[3] + \n\
        	_x1_y_1  * cbufX[2] * cbufY[0] + _x1_y   * cbufX[2] * cbufY[1] + \n\
        	_x1_y1   * cbufX[2] * cbufY[2] + _x1_y2  * cbufX[2] * cbufY[3] + \n\
        	_x2_y_1  * cbufX[3] * cbufY[0] + _x2_y   * cbufX[3] * cbufY[1] + \n\
        	_x2_y1   * cbufX[3] * cbufY[2] + _x2_y2  * cbufX[3] * cbufY[3]; \n\
    } \n\
    else \n\
    { \n\
        float x_offset = pfish.x - floor(pfish.x); \n\
        float y_offset = pfish.y - floor(pfish.y); \n\
        int x0 = clamp(int(floor(pfish.x)), 0, p.w - 1); \n\
        int y0 = clamp(int(floor(pfish.y)), 0, p.h - 1); \n\
        int x1 = clamp(int(ceil(pfish.x)), 0, p.w - 1); \n\
        int y1 = clamp(int(ceil(pfish.y)), 0, p.h - 1); \n\
        sfpvec4 x0y0 = load(x0, y0); \n\
        sfpvec4 x0y1 = load(x0, y1); \n\
        sfpvec4 x1y0 = load(x1, y0); \n\
        sfpvec4 x1y1 = load(x1, y1); \n\
        rgba = sfp((1.f - x_offset) * (1.f - y_offset)) * x0y0 + \n\
        			sfp((1.f - x_offset) * y_offset) * x0y1 + \n\
        			sfp(x_offset * (1.f - y_offset)) * x1y0 + \n\
        			sfp(x_offset * y_offset) * x1y1; \n\
    } \n\
    store(rgba, gx, gy); \n\
} \n\
"

static const char Equidistance2Orthographic_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
SHADER_LOAD_RGBA
SHADER_STORE_RGBA
SHADER_FISHEYE
SHADER_MAIN
;
