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
    float progress; \n\
} p; \
"

#define SHADER_MAIN \
" \n\
float random(vec2 co) \n\
{ \n\
    float a = 12.9898; \n\
    float b = 78.233; \n\
    float c = 43758.5453; \n\
    float dt= dot(co.xy ,vec2(a,b)); \n\
    float sn= mod(dt,3.14); \n\
    return fract(sin(sn) * c); \n\
} \n\
\n\
vec2 displace(sfpvec4 tex, vec2 texCoord, float dotDepth, float textureDepth, float strength) \n\
{ \n\
    sfpvec4 dt = tex * sfp(1.0); \n\
    sfpvec4 dis = dt * sfp(dotDepth) + sfp(1.0) - tex * sfp(textureDepth); \n\
\n\
    dis.x = dis.x - sfp(1.0) + sfp(textureDepth * dotDepth); \n\
    dis.y = dis.y - sfp(1.0) + sfp(textureDepth * dotDepth); \n\
    dis.x *= sfp(strength); \n\
    dis.y *= sfp(strength); \n\
    vec2 res_uv = texCoord ; \n\
    res_uv.x = res_uv.x + float(dis.x) - 0.0; \n\
    res_uv.y = res_uv.y + float(dis.y); \n\
    return res_uv; \n\
} \n\
\n\
float ease1(float t) \n\
{ \n\
    return t == 0.0 || t == 1.0 \n\
    ? t \n\
    : t < 0.5 \n\
      ? +0.5 * pow(2.0, (20.0 * t) - 10.0) \n\
      : -0.5 * pow(2.0, 10.0 - (t * 20.0)) + 1.0; \n\
} \n\
\n\
float ease2(float t) \n\
{ \n\
    return t == 1.0 ? t : 1.0 - pow(2.0, -10.0 * t); \n\
} \n\
\n\
sfpvec4 transition(vec2 uv) \n\
{ \n\
    vec2 point = uv.xy / vec2(1.0).xy; \n\
    sfpvec4 color1 = load_rgba(int(point.x * (p.w - 1)), int((1.f - point.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    sfpvec4 color2 = load_rgba_src2(int(point.x * (p.w2 - 1)), int((1.f - point.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    vec2 disp = displace(color1, point, 0.33, 0.7, 1.0 - ease1(p.progress)); \n\
    vec2 disp2 = displace(color2, point, 0.33, 0.5, ease2(p.progress)); \n\
    disp = clamp(disp, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    disp2 = clamp(disp2, vec2(0.f, 0.f), vec2(1.f, 1.f)); \n\
    sfpvec4 dColor1 = load_rgba_src2(int(disp.x * (p.w2 - 1)), int((1.f - disp.y) * (p.h2 - 1)), p.w2, p.h2, p.cstep2, p.in_format2, p.in_type2); \n\
    sfpvec4 dColor2 = load_rgba(int(disp2.x * (p.w - 1)), int((1.f - disp2.y) * (p.h - 1)), p.w, p.h, p.cstep, p.in_format, p.in_type); \n\
    float val = ease1(p.progress); \n\
    sfpvec3 gray = sfpvec3(dot(min(dColor2, dColor1).rgb, sfpvec3(sfp(0.299), sfp(0.587), sfp(0.114)))); \n\
    dColor2 = sfpvec4(gray, sfp(1.0)); \n\
    dColor2 *= sfp(2.0); \n\
    color1 = mix(color1, dColor2, sfp(smoothstep(0.0, 0.5, p.progress))); \n\
    color2 = mix(color2, dColor1, sfp(smoothstep(1.0, 0.5, p.progress))); \n\
    return mix(color1, color2, sfp(val)); \n\
} \n\
\n\
void main() \n\
{ \n\
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy); \n\
    if (uv.x >= p.out_w || uv.y >= p.out_h) \n\
        return; \n\
    vec2 point = vec2(float(uv.x) / float(p.out_w - 1), 1.f - float(uv.y) / float(p.out_h - 1)); \n\
    sfpvec4 result = transition(point); \n\
    store_rgba(result, uv.x, uv.y, p.out_w, p.out_h, p.out_cstep, p.out_format, p.out_type); \n\
} \
"

static const char GlitchDisplace_data[] = 
SHADER_HEADER
SHADER_PARAM
SHADER_INPUT_OUTPUT_DATA
R"(
layout (binding =  8) readonly buffer src2_int8       { uint8_t   src2_data_int8[]; };
layout (binding =  9) readonly buffer src2_int16      { uint16_t  src2_data_int16[]; };
layout (binding = 10) readonly buffer src2_float16    { float16_t src2_data_float16[]; };
layout (binding = 11) readonly buffer src2_float32    { float     src2_data_float32[]; };
)"
SHADER_LOAD_RGBA
SHADER_LOAD_RGBA_NAME(src2)
SHADER_STORE_RGBA
SHADER_MAIN
;
