static const char cast_fp16_to_fp32_pack4[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#define shape_constant_id_offset 0
layout (constant_id = shape_constant_id_offset + 0) const int dims = 0;
layout (constant_id = shape_constant_id_offset + 1) const int w = 0;
layout (constant_id = shape_constant_id_offset + 2) const int h = 0;
layout (constant_id = shape_constant_id_offset + 3) const int c = 0;
layout (constant_id = shape_constant_id_offset + 4) const int cstep = 0;
layout (constant_id = shape_constant_id_offset + 5) const int outdims = 0;
layout (constant_id = shape_constant_id_offset + 6) const int outw = 0;
layout (constant_id = shape_constant_id_offset + 7) const int outh = 0;
layout (constant_id = shape_constant_id_offset + 8) const int outc = 0;
layout (constant_id = shape_constant_id_offset + 9) const int outcstep = 0;
#if ImVulkan_image_shader
layout (binding = 0) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 1, rgba32f) writeonly uniform highp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { vec4 top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int dims;
    int w;
    int h;
    int c;
    int cstep;

    int outdims;
    int outw;
    int outh;
    int outc;
    int outcstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= psc(w) || gy >= psc(h) || gz >= psc(c))
        return;
#if ImVulkan_image_shader
    image3d_cp4(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
    const int v_offset = gz * psc(cstep) + gy * psc(w) + gx;
    top_blob_data[gi] = vec4(buffer_ld4(bottom_blob_data, v_offset));
#endif
}
)";

static const char cast_fp16_to_fp32_pack8[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#define shape_constant_id_offset 0
layout (constant_id = shape_constant_id_offset + 0) const int dims = 0;
layout (constant_id = shape_constant_id_offset + 1) const int w = 0;
layout (constant_id = shape_constant_id_offset + 2) const int h = 0;
layout (constant_id = shape_constant_id_offset + 3) const int c = 0;
layout (constant_id = shape_constant_id_offset + 4) const int cstep = 0;
layout (constant_id = shape_constant_id_offset + 5) const int outdims = 0;
layout (constant_id = shape_constant_id_offset + 6) const int outw = 0;
layout (constant_id = shape_constant_id_offset + 7) const int outh = 0;
layout (constant_id = shape_constant_id_offset + 8) const int outc = 0;
layout (constant_id = shape_constant_id_offset + 9) const int outcstep = 0;
#if ImVulkan_image_shader
layout (binding = 0) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 1, rgba32f) writeonly uniform highp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { mat2x4 top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int dims;
    int w;
    int h;
    int c;
    int cstep;
    int outdims;
    int outw;
    int outh;
    int outc;
    int outcstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= psc(w) || gy >= psc(h) || gz >= psc(c))
        return;
#if ImVulkan_image_shader
    image3d_cp8(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
    const int v_offset = gz * psc(cstep) + gy * psc(w) + gx;
    top_blob_data[gi] = mat2x4(buffer_ld8(bottom_blob_data, v_offset));
#endif
}
)";

static const char cast_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#define shape_constant_id_offset 0
layout (constant_id = shape_constant_id_offset + 0) const int dims = 0;
layout (constant_id = shape_constant_id_offset + 1) const int w = 0;
layout (constant_id = shape_constant_id_offset + 2) const int h = 0;
layout (constant_id = shape_constant_id_offset + 3) const int c = 0;
layout (constant_id = shape_constant_id_offset + 4) const int cstep = 0;
layout (constant_id = shape_constant_id_offset + 5) const int outdims = 0;
layout (constant_id = shape_constant_id_offset + 6) const int outw = 0;
layout (constant_id = shape_constant_id_offset + 7) const int outh = 0;
layout (constant_id = shape_constant_id_offset + 8) const int outc = 0;
layout (constant_id = shape_constant_id_offset + 9) const int outcstep = 0;
#if ImVulkan_image_shader
layout (binding = 0) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 1, r32f) writeonly uniform highp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { float top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int dims;
    int w;
    int h;
    int c;
    int cstep;
    int outdims;
    int outw;
    int outh;
    int outc;
    int outcstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= psc(w) || gy >= psc(h) || gz >= psc(c))
        return;
#if ImVulkan_image_shader
    image3d_cp1(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
    const int v_offset = gz * psc(cstep) + gy * psc(w) + gx;
    top_blob_data[gi] = float(buffer_ld1(bottom_blob_data, v_offset));
#endif
}
)";

static const char cast_fp32_to_fp16_pack4[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#define shape_constant_id_offset 0
layout (constant_id = shape_constant_id_offset + 0) const int dims = 0;
layout (constant_id = shape_constant_id_offset + 1) const int w = 0;
layout (constant_id = shape_constant_id_offset + 2) const int h = 0;
layout (constant_id = shape_constant_id_offset + 3) const int c = 0;
layout (constant_id = shape_constant_id_offset + 4) const int cstep = 0;
layout (constant_id = shape_constant_id_offset + 5) const int outdims = 0;
layout (constant_id = shape_constant_id_offset + 6) const int outw = 0;
layout (constant_id = shape_constant_id_offset + 7) const int outh = 0;
layout (constant_id = shape_constant_id_offset + 8) const int outc = 0;
layout (constant_id = shape_constant_id_offset + 9) const int outcstep = 0;
#if ImVulkan_image_shader
layout (binding = 0) uniform highp sampler3D bottom_blob_3d;
layout (binding = 1, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { vec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int dims;
    int w;
    int h;
    int c;
    int cstep;

    int outdims;
    int outw;
    int outh;
    int outc;
    int outcstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= psc(w) || gy >= psc(h) || gz >= psc(c))
        return;
#if ImVulkan_image_shader
    image3d_cp4(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
    const int v_offset = gz * psc(cstep) + gy * psc(w) + gx;
    buffer_st4(top_blob_data, gi, afpvec4(bottom_blob_data[v_offset]));
#endif
}
)";

static const char cast_fp32_to_fp16_pack8[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#define shape_constant_id_offset 0
layout (constant_id = shape_constant_id_offset + 0) const int dims = 0;
layout (constant_id = shape_constant_id_offset + 1) const int w = 0;
layout (constant_id = shape_constant_id_offset + 2) const int h = 0;
layout (constant_id = shape_constant_id_offset + 3) const int c = 0;
layout (constant_id = shape_constant_id_offset + 4) const int cstep = 0;
layout (constant_id = shape_constant_id_offset + 5) const int outdims = 0;
layout (constant_id = shape_constant_id_offset + 6) const int outw = 0;
layout (constant_id = shape_constant_id_offset + 7) const int outh = 0;
layout (constant_id = shape_constant_id_offset + 8) const int outc = 0;
layout (constant_id = shape_constant_id_offset + 9) const int outcstep = 0;
#if ImVulkan_image_shader
layout (binding = 0) uniform highp sampler3D bottom_blob_3d;
layout (binding = 1, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { mat2x4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int dims;
    int w;
    int h;
    int c;
    int cstep;

    int outdims;
    int outw;
    int outh;
    int outc;
    int outcstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= psc(w) || gy >= psc(h) || gz >= psc(c))
        return;
#if ImVulkan_image_shader
    image3d_cp8(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
    const int v_offset = gz * psc(cstep) + gy * psc(w) + gx;
    buffer_st8(top_blob_data, gi, afpvec8(bottom_blob_data[v_offset]));
#endif
}
)";

static const char cast_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
#define shape_constant_id_offset 0
layout (constant_id = shape_constant_id_offset + 0) const int dims = 0;
layout (constant_id = shape_constant_id_offset + 1) const int w = 0;
layout (constant_id = shape_constant_id_offset + 2) const int h = 0;
layout (constant_id = shape_constant_id_offset + 3) const int c = 0;
layout (constant_id = shape_constant_id_offset + 4) const int cstep = 0;
layout (constant_id = shape_constant_id_offset + 5) const int outdims = 0;
layout (constant_id = shape_constant_id_offset + 6) const int outw = 0;
layout (constant_id = shape_constant_id_offset + 7) const int outh = 0;
layout (constant_id = shape_constant_id_offset + 8) const int outc = 0;
layout (constant_id = shape_constant_id_offset + 9) const int outcstep = 0;
#if ImVulkan_image_shader
layout (binding = 0) uniform highp sampler3D bottom_blob_3d;
layout (binding = 1, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
#else
layout (binding = 0) readonly buffer bottom_blob { float bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#endif
layout (push_constant) uniform parameter
{
    int dims;
    int w;
    int h;
    int c;
    int cstep;
    int outdims;
    int outw;
    int outh;
    int outc;
    int outcstep;
} p;
void main()
{
    int gx = int(gl_GlobalInvocationID.x);
    int gy = int(gl_GlobalInvocationID.y);
    int gz = int(gl_GlobalInvocationID.z);
    if (gx >= psc(w) || gy >= psc(h) || gz >= psc(c))
        return;
#if ImVulkan_image_shader
    image3d_cp1(top_blob_3d, ivec3(gx, gy, gz), bottom_blob_3d, ivec3(gx, gy, gz));
#else
    const int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
    const int v_offset = gz * psc(cstep) + gy * psc(w) + gx;
    buffer_st1(top_blob_data, gi, afp(bottom_blob_data[v_offset]));
#endif
}
)";

