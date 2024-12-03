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
    int mode;   \n\
    int x_offset; \n\
    int y_offset; \n\
    float opacity; \n\
    int masked; \n\
} p; \
"

#define SHADER_COLOR \
" \n\
sfpvec3 hsv2rgb(sfpvec3 c) \n\
{ \n\
    sfpvec4 K = sfpvec4(sfp(1.0), sfp(2.0 / 3.0), sfp(1.0 / 3.0), sfp(3.0)); \n\
    sfpvec3 _p = abs(fract(c.xxx + K.xyz) * sfp(6.0) - K.www); \n\
    return c.z * mix(K.xxx, clamp(_p - K.xxx, sfp(0.0), sfp(1.0)), c.y); \n\
} \n\
sfpvec3 rgb2hsv(sfpvec3 c) \n\
{ \n\
    sfpvec4 K = sfpvec4(sfp(0.0), sfp(-1.0 / 3.0), sfp(2.0 / 3.0), sfp(-1.0)); \n\
    sfpvec4 _p = mix(sfpvec4(c.bg, K.wz), sfpvec4(c.gb, K.xy), step(c.b, c.g)); \n\
    sfpvec4 q = mix(sfpvec4(_p.xyw, c.r), sfpvec4(c.r, _p.yzx), step(_p.x, c.r)); \n\
    \n\
    sfp d = q.x - min(q.w, q.y); \n\
    sfp e = sfp(1.0e-10); \n\
    return sfpvec3(abs(q.z + (q.w - q.y) / (sfp(6.0) * d + e)), d / (q.x + e), q.x); \n\
} \n\
"
#define SHADER_BLEND \
" \n\
sfpvec3  blendAdd(in sfpvec3 base, in sfpvec3 blend) { return min(base + blend, sfpvec3(1.)); } \n\
sfpvec3  blendAverage(in sfpvec3 base, in sfpvec3 blend) { return (base + blend) * sfp(.5); } \n\
sfpvec3  blendColor(in sfpvec3 base, in sfpvec3 blend) { sfpvec3 baseHSL = rgb2hsv(base); sfpvec3 blendHSL = rgb2hsv(blend); return hsv2rgb(sfpvec3(blendHSL.x, blendHSL.y, baseHSL.z)); } \n\
sfp      blendColorBurn(in sfp base, in sfp blend) { return (blend == sfp(0.)) ? blend: max((sfp(1.) - ((sfp(1.) - base ) / blend)), sfp(0.)); } \n\
sfpvec3  blendColorBurn(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendColorBurn(base.r, blend.r), blendColorBurn(base.g, blend.g), blendColorBurn(base.b, blend.b)); } \n\
sfp      blendColorDodge(in sfp base, in sfp blend) { return (blend == sfp(1.)) ? blend: min( base / (sfp(1.) - blend), sfp(1.)); } \n\
sfpvec3  blendColorDodge(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendColorDodge(base.r, blend.r), blendColorDodge(base.g, blend.g), blendColorDodge(base.b, blend.b)); } \n\
sfp      blendDarken(in sfp base, in sfp blend) { return min(blend,base); } \n\
sfpvec3  blendDarken(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendDarken(base.r, blend.r), blendDarken(base.g, blend.g), blendDarken(base.b, blend.b)); } \n\
sfpvec3  blendDifference(in sfpvec3 base, in sfpvec3 blend) { return abs(base - blend); } \n\
sfpvec3  blendExclusion(in sfpvec3 base, in sfpvec3 blend) { return base + blend - sfp(2.) * base * blend; } \n\
sfp      blendReflect(in sfp base, in sfp blend) { return (blend == sfp(1.)) ? blend : min(base * base / (sfp(1.) - blend), sfp(1.)); } \n\
sfpvec3  blendReflect(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendReflect(base.r, blend.r), blendReflect(base.g, blend.g), blendReflect(base.b, blend.b)); } \n\
sfp      blendOverlay(in sfp base, in sfp blend) { return (base < sfp(.5)) ? (sfp(2.) * base * blend): (sfp(1.) - sfp(2.) * (sfp(1.) - base) * (sfp(1.) - blend)); } \n\
sfpvec3  blendOverlay(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendOverlay(base.r, blend.r), blendOverlay(base.g, blend.g), blendOverlay(base.b, blend.b)); } \n\
sfp      blendVividLight(in sfp base, in sfp blend) { return (blend < sfp(.5)) ? blendColorBurn(base, (sfp(2.) * blend)): blendColorDodge(base, (sfp(2.) * (blend - sfp(.5)))); } \n\
sfpvec3  blendVividLight(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendVividLight(base.r, blend.r), blendVividLight(base.g, blend.g), blendVividLight(base.b, blend.b)); } \n\
sfp      blendHardMix(in sfp base, in sfp blend) { return (blendVividLight(base, blend) < sfp(.5)) ? sfp(0.) : sfp(1.); } \n\
sfpvec3  blendHardMix(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendHardMix(base.r, blend.r), blendHardMix(base.g, blend.g), blendHardMix(base.b, blend.b)); } \n\
sfpvec3  blendHue(in sfpvec3 base, in sfpvec3 blend) { sfpvec3 baseHSL = rgb2hsv(base); sfpvec3 blendHSL = rgb2hsv(blend); return hsv2rgb(sfpvec3(blendHSL.x, baseHSL.y, baseHSL.z)); } \n\
sfp      blendLighten(in sfp base, in sfp blend) { return max(blend, base); } \n\
sfpvec3  blendLighten(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendLighten(base.r, blend.r), blendLighten(base.g, blend.g), blendLighten(base.b, blend.b)); } \n\
sfp      blendLinearBurn(in sfp base, in sfp blend) { return max(base + blend - sfp(1.), sfp(0.)); } \n\
sfpvec3  blendLinearBurn(in sfpvec3 base, in sfpvec3 blend) { return max(base + blend - sfpvec3(sfp(1.)), sfpvec3(sfp(0.))); } \n\
sfp      blendLinearDodge(in sfp base, in sfp blend) { return min(base + blend, sfp(1.)); } \n\
sfpvec3  blendLinearDodge(in sfpvec3 base, in sfpvec3 blend) { return min(base + blend, sfpvec3(1.)); } \n\
sfp      blendLinearLight(in sfp base, in sfp blend) { return blend < sfp(.5) ? blendLinearBurn(base, (sfp(2.) * blend)): blendLinearDodge(base, (sfp(2.) * (blend - sfp(.5)))); } \n\
sfpvec3  blendLinearLight(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendLinearLight(base.r, blend.r), blendLinearLight(base.g, blend.g), blendLinearLight(base.b, blend.b)); } \n\
sfpvec3  blendLuminosity(in sfpvec3 base, in sfpvec3 blend) { sfpvec3 baseHSL = rgb2hsv(base); sfpvec3 blendHSL = rgb2hsv(blend); return hsv2rgb(sfpvec3(baseHSL.x, baseHSL.y, blendHSL.z)); } \n\
sfpvec3  blendMultiply(in sfpvec3 base, in sfpvec3 blend) { return base * blend; } \n\
sfpvec3  blendNegation(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(sfp(1.)) - abs(sfpvec3(sfp(1.)) - base - blend); } \n\
sfpvec3  blendPhoenix(in sfpvec3 base, in sfpvec3 blend) { return min(base, blend) - max(base, blend) + sfpvec3(sfp(1.)); } \n\
sfp      blendPinLight(in sfp base, in sfp blend) { return (blend < sfp(.5)) ? blendDarken(base, (sfp(2.) * blend)): blendLighten(base, (sfp(2.) * (blend - sfp(.5)))); } \n\
sfpvec3  blendPinLight(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendPinLight(base.r, blend.r), blendPinLight(base.g, blend.g), blendPinLight(base.b, blend.b)); } \n\
sfpvec3  blendSaturation(in sfpvec3 base, in sfpvec3 blend) { sfpvec3 baseHSL = rgb2hsv(base); sfpvec3 blendHSL = rgb2hsv(blend); return hsv2rgb(sfpvec3(baseHSL.x, blendHSL.y, baseHSL.z)); } \n\
sfp      blendScreen(in sfp base, in sfp blend) { return sfp(1.) - ((sfp(1.) - base) * (sfp(1.) - blend)); } \n\
sfpvec3  blendScreen(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendScreen(base.r, blend.r), blendScreen(base.g, blend.g), blendScreen(base.b, blend.b)); } \n\
sfp      blendSoftLight(in sfp base, in sfp blend) { return (blend < sfp(.5)) ? (sfp(2.) * base * blend + base * base * (sfp(1.) - sfp(2.) * blend)): (sqrt(base) * (sfp(2.) * blend - sfp(1.)) + sfp(2.) * base * (sfp(1.) - blend)); } \n\
sfpvec3  blendSoftLight(in sfpvec3 base, in sfpvec3 blend) { return sfpvec3(blendSoftLight(base.r, blend.r), blendSoftLight(base.g, blend.g), blendSoftLight(base.b, blend.b)); } \n\
sfpvec3  blendSubtract(in sfpvec3 base, in sfpvec3 blend) { return max(base + blend - sfpvec3(sfp(1.)), sfpvec3(sfp(0.))); } \n\
\n\
sfpvec3 blend(in sfpvec3 base, in sfpvec3 blend, sfp opacity) \n\
{ \n\
    sfpvec3 result; \n\
    switch (p.mode) \n\
    { \n\
        case BL_ADD: result = blendAdd(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_AVERAGE: result = blendAverage(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_COLOR: result = blendColor(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_COLORBURN: result = blendColorBurn(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_COLORDODGE: result = blendColorDodge(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_DARKEN: result = blendDarken(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_DIFFERENCE: result = blendDifference(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_EXCLUSION: result = blendExclusion(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_GLOW: result = blendReflect(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_HARDLIGHT: result = blendOverlay(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_HARDMIX: result = blendHardMix(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_HUE: result = blendHue(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_LIGHTEN: result = blendLighten(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_LINEARBURN: result = blendLinearBurn(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_LINEARDODGE: result = blendLinearDodge(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_LINEARLIGHT: result = blendLinearLight(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_LUMINOSITY: result = blendLuminosity(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_MULTIPLY: result = blendMultiply(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_NEGATION: result = blendNegation(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_OVERLAY: result = blendOverlay(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_PHOENIX: result = blendPhoenix(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_PINLIGHT: result = blendPinLight(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_REFLECT: result = blendReflect(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_SATURATION: result = blendSaturation(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_SCREEN: result = blendScreen(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_SOFTLIGHT: result = blendSoftLight(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_SUBTRACT: result = blendSubtract(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        case BL_VIVIDLIGHT: result = blendVividLight(base, blend) * opacity + base * (sfp(1.) - opacity); break; \n\
        default: result = blend * opacity + base * (sfp(1.f) - opacity); break; \n\
    } \n\
    return result; \n\
} \
"

#define SHADER_BLEND_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    sfpvec4 result; \n\
    sfpvec4 rgba_dst = load_dst_rgba(uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
    if (uv.x - p.x_offset >= 0 && uv.y - p.y_offset >= 0 && \n\
        uv.x - p.x_offset < p.w && uv.y - p.y_offset < p.h) \n\
    { \n\
        sfpvec4 rgba_src = load_rgba(uv.x - p.x_offset, uv.y - p.y_offset, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
        sfp alpha = p.masked == 1 ? rgba_src.a * sfp(p.opacity): sfp(p.opacity); \n\
        result = sfpvec4(blend(rgba_dst.rgb, rgba_src.rgb, alpha), rgba_dst.a); \n\
    } \n\
    else \n\
    { \n\
        result = rgba_dst; \n\
    } \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char Blend_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_DATA
SHADER_OUTPUT_RDWR_DATA
SHADER_LOAD_RGBA
SHADER_LOAD_DST_RGBA
SHADER_STORE_RGBA
SHADER_COLOR
SHADER_BLEND
SHADER_BLEND_MAIN
;

#define SHADER_MASK_MERGE_PARAM \
" \n\
layout (push_constant) uniform parameter \n\
{ \n\
    int w; \n\
    int h; \n\
    int cstep; \n\
    int in_format; \n\
    int in_type; \n\
\n\
    int mask_w; \n\
    int mask_h; \n\
    int mask_cstep; \n\
    int mask_format; \n\
    int mask_type; \n\
} p; \
"

#define SHADER_MASK_MERGE_MAIN \
" \n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.w || uv.y >= p.h) \n\
        return; \n\
    sfpvec4 rgba_src = load_rgba(uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfp alpha = load_gray_mask(uv.x, uv.y, p.mask_w, p.mask_h, p.mask_cstep, p.mask_format, p.mask_type); \n\
    store_rgba_src(sfpvec4(rgba_src.rgb, alpha), uv.x, uv.y, p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
} \
"

static const char BlendMaskMerge_data[] = 
SHADER_HEADER
SHADER_MASK_MERGE_PARAM
SHADER_INPUT_RW_DATA
SHADER_INPUT_MASK_DATA
SHADER_LOAD_RGBA
SHADER_LOAD_GRAY_NAME(mask)
SHADER_STORE_WITHNAME_RGBA(src)
SHADER_MASK_MERGE_MAIN
;
