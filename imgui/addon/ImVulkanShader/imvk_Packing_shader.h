static const char packing_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { float top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, r32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afp v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld1(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld1(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            top_blob_fp32_data[gi] = float(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, 0, 0), vec4(v));
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld1(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = float(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, 0), vec4(v));
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld1(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = float(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, gz), vec4(v));
        }
#endif
    }
}
)";

static const char packing_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { float bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afp v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = afp(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, 0, 0), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st1(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st1(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = afp(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, 0), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st1(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st1(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = afp(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, gz), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st1(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st1(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack1to4_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { vec4 top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, rgba32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec4 x4 = ivec4(gx * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            v.r = buffer_ld1(bottom_blob_data, v_offset.r);
            v.g = buffer_ld1(bottom_blob_data, v_offset.g);
            v.b = buffer_ld1(bottom_blob_data, v_offset.b);
            v.a = buffer_ld1(bottom_blob_data, v_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x4 = gx * 4;
            v.r = image3d_ld1(bottom_blob_3d, ivec3(x4 + 0, 0, 0));
            v.g = image3d_ld1(bottom_blob_3d, ivec3(x4 + 1, 0, 0));
            v.b = image3d_ld1(bottom_blob_3d, ivec3(x4 + 2, 0, 0));
            v.a = image3d_ld1(bottom_blob_3d, ivec3(x4 + 3, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            top_blob_fp32_data[gi] = vec4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec4 y4 = ivec4(gy * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(w) + gx;
            v.r = buffer_ld1(bottom_blob_data, v_offset.r);
            v.g = buffer_ld1(bottom_blob_data, v_offset.g);
            v.b = buffer_ld1(bottom_blob_data, v_offset.b);
            v.a = buffer_ld1(bottom_blob_data, v_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y4 = gy * 4;
            v.r = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 0, 0));
            v.g = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 1, 0));
            v.b = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 2, 0));
            v.a = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 3, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = vec4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec4 z4 = ivec4(gz * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(cstep) + ivec4(gy * psc(w) + gx);
            v.r = buffer_ld1(bottom_blob_data, v_offset.r);
            v.g = buffer_ld1(bottom_blob_data, v_offset.g);
            v.b = buffer_ld1(bottom_blob_data, v_offset.b);
            v.a = buffer_ld1(bottom_blob_data, v_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z4 = gz * 4;
            v.r = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 0));
            v.g = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 1));
            v.b = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 2));
            v.a = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 3));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = vec4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack1to4_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { float bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec4 x4 = ivec4(gx * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            v.r = afp(bottom_blob_fp32_data[v_offset.r]);
            v.g = afp(bottom_blob_fp32_data[v_offset.g]);
            v.b = afp(bottom_blob_fp32_data[v_offset.b]);
            v.a = afp(bottom_blob_fp32_data[v_offset.a]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x4 = gx * 4;
            v.r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 0, 0, 0), 0).r);
            v.g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 1, 0, 0), 0).r);
            v.b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 2, 0, 0), 0).r);
            v.a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 3, 0, 0), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec4 y4 = ivec4(gy * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(w) + gx;
            v.r = afp(bottom_blob_fp32_data[v_offset.r]);
            v.g = afp(bottom_blob_fp32_data[v_offset.g]);
            v.b = afp(bottom_blob_fp32_data[v_offset.b]);
            v.a = afp(bottom_blob_fp32_data[v_offset.a]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y4 = gy * 4;

            v.r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 0, 0), 0).r);
            v.g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 1, 0), 0).r);
            v.b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 2, 0), 0).r);
            v.a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 3, 0), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec4 z4 = ivec4(gz * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(cstep) + ivec4(gy * psc(w) + gx);
            v.r = afp(bottom_blob_fp32_data[v_offset.r]);
            v.g = afp(bottom_blob_fp32_data[v_offset.g]);
            v.b = afp(bottom_blob_fp32_data[v_offset.b]);
            v.a = afp(bottom_blob_fp32_data[v_offset.a]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z4 = gz * 4;
            v.r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 0), 0).r);
            v.g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 1), 0).r);
            v.b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 2), 0).r);
            v.a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 3), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack1to4[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec4 x4 = ivec4(gx * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            v.r = buffer_ld1(bottom_blob_data, v_offset.r);
            v.g = buffer_ld1(bottom_blob_data, v_offset.g);
            v.b = buffer_ld1(bottom_blob_data, v_offset.b);
            v.a = buffer_ld1(bottom_blob_data, v_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x4 = gx * 4;
            v.r = image3d_ld1(bottom_blob_3d, ivec3(x4 + 0, 0, 0));
            v.g = image3d_ld1(bottom_blob_3d, ivec3(x4 + 1, 0, 0));
            v.b = image3d_ld1(bottom_blob_3d, ivec3(x4 + 2, 0, 0));
            v.a = image3d_ld1(bottom_blob_3d, ivec3(x4 + 3, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec4 y4 = ivec4(gy * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(w) + gx;
            v.r = buffer_ld1(bottom_blob_data, v_offset.r);
            v.g = buffer_ld1(bottom_blob_data, v_offset.g);
            v.b = buffer_ld1(bottom_blob_data, v_offset.b);
            v.a = buffer_ld1(bottom_blob_data, v_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y4 = gy * 4;
            v.r = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 0, 0));
            v.g = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 1, 0));
            v.b = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 2, 0));
            v.a = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 3, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;

            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec4 z4 = ivec4(gz * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(cstep) + ivec4(gy * psc(w) + gx);
            v.r = buffer_ld1(bottom_blob_data, v_offset.r);
            v.g = buffer_ld1(bottom_blob_data, v_offset.g);
            v.b = buffer_ld1(bottom_blob_data, v_offset.b);
            v.a = buffer_ld1(bottom_blob_data, v_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z4 = gz * 4;
            v.r = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 0));
            v.g = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 1));
            v.b = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 2));
            v.a = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 3));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack1to8_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { mat2x4 top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, rgba32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec4 x4 = ivec4(gx * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            ivec4 vv_offset = x4 + 4;
            v[0].r = buffer_ld1(bottom_blob_data, v_offset.r);
            v[0].g = buffer_ld1(bottom_blob_data, v_offset.g);
            v[0].b = buffer_ld1(bottom_blob_data, v_offset.b);
            v[0].a = buffer_ld1(bottom_blob_data, v_offset.a);
            v[1].r = buffer_ld1(bottom_blob_data, vv_offset.r);
            v[1].g = buffer_ld1(bottom_blob_data, vv_offset.g);
            v[1].b = buffer_ld1(bottom_blob_data, vv_offset.b);
            v[1].a = buffer_ld1(bottom_blob_data, vv_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x4 = gx * 8;
            v[0].r = image3d_ld1(bottom_blob_3d, ivec3(x4 + 0, 0, 0));
            v[0].g = image3d_ld1(bottom_blob_3d, ivec3(x4 + 1, 0, 0));
            v[0].b = image3d_ld1(bottom_blob_3d, ivec3(x4 + 2, 0, 0));
            v[0].a = image3d_ld1(bottom_blob_3d, ivec3(x4 + 3, 0, 0));
            v[1].r = image3d_ld1(bottom_blob_3d, ivec3(x4 + 4, 0, 0));
            v[1].g = image3d_ld1(bottom_blob_3d, ivec3(x4 + 5, 0, 0));
            v[1].b = image3d_ld1(bottom_blob_3d, ivec3(x4 + 6, 0, 0));
            v[1].a = image3d_ld1(bottom_blob_3d, ivec3(x4 + 7, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            top_blob_fp32_data[gi] = mat2x4(vec4(v[0]), vec4(v[1]));
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx * 2, 0, 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(gx * 2 + 1, 0, 0), v[1]);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec4 y4 = ivec4(gy * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(w) + gx;
            ivec4 vv_offset = (y4 + 4) * psc(w) + gx;
            v[0].r = buffer_ld1(bottom_blob_data, v_offset.r);
            v[0].g = buffer_ld1(bottom_blob_data, v_offset.g);
            v[0].b = buffer_ld1(bottom_blob_data, v_offset.b);
            v[0].a = buffer_ld1(bottom_blob_data, v_offset.a);
            v[1].r = buffer_ld1(bottom_blob_data, vv_offset.r);
            v[1].g = buffer_ld1(bottom_blob_data, vv_offset.g);
            v[1].b = buffer_ld1(bottom_blob_data, vv_offset.b);
            v[1].a = buffer_ld1(bottom_blob_data, vv_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y4 = gy * 8;
            v[0].r = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 0, 0));
            v[0].g = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 1, 0));
            v[0].b = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 2, 0));
            v[0].a = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 3, 0));
            v[1].r = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 4, 0));
            v[1].g = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 5, 0));
            v[1].b = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 6, 0));
            v[1].a = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 7, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = mat2x4(vec4(v[0]), vec4(v[1]));
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx * 2, gy, 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(gx * 2 + 1, gy, 0), v[1]);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec4 z4 = ivec4(gz * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(cstep) + ivec4(gy * psc(w) + gx);
            ivec4 vv_offset = (z4 + 4) * psc(cstep) + ivec4(gy * psc(w) + gx);
            v[0].r = buffer_ld1(bottom_blob_data, v_offset.r);
            v[0].g = buffer_ld1(bottom_blob_data, v_offset.g);
            v[0].b = buffer_ld1(bottom_blob_data, v_offset.b);
            v[0].a = buffer_ld1(bottom_blob_data, v_offset.a);
            v[1].r = buffer_ld1(bottom_blob_data, vv_offset.r);
            v[1].g = buffer_ld1(bottom_blob_data, vv_offset.g);
            v[1].b = buffer_ld1(bottom_blob_data, vv_offset.b);
            v[1].a = buffer_ld1(bottom_blob_data, vv_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z4 = gz * 8;
            v[0].r = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 0));
            v[0].g = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 1));
            v[0].b = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 2));
            v[0].a = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 3));
            v[1].r = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 4));
            v[1].g = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 5));
            v[1].b = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 6));
            v[1].a = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 7));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = mat2x4(vec4(v[0]), vec4(v[1]));
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx * 2, gy, gz), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(gx * 2 + 1, gy, gz), v[1]);
        }
#endif
    }
}
)";

static const char packing_pack1to8_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { float bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec4 x4 = ivec4(gx * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            ivec4 vv_offset = x4 + 4;
            v[0].r = afp(bottom_blob_fp32_data[v_offset.r]);
            v[0].g = afp(bottom_blob_fp32_data[v_offset.g]);
            v[0].b = afp(bottom_blob_fp32_data[v_offset.b]);
            v[0].a = afp(bottom_blob_fp32_data[v_offset.a]);
            v[1].r = afp(bottom_blob_fp32_data[vv_offset.r]);
            v[1].g = afp(bottom_blob_fp32_data[vv_offset.g]);
            v[1].b = afp(bottom_blob_fp32_data[vv_offset.b]);
            v[1].a = afp(bottom_blob_fp32_data[vv_offset.a]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x4 = gx * 8;
            v[0].r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 0, 0, 0), 0).r);
            v[0].g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 1, 0, 0), 0).r);
            v[0].b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 2, 0, 0), 0).r);
            v[0].a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 3, 0, 0), 0).r);
            v[1].r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 4, 0, 0), 0).r);
            v[1].g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 5, 0, 0), 0).r);
            v[1].b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 6, 0, 0), 0).r);
            v[1].a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(x4 + 7, 0, 0), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec4 y4 = ivec4(gy * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(w) + gx;
            ivec4 vv_offset = (y4 + 4) * psc(w) + gx;
            v[0].r = afp(bottom_blob_fp32_data[v_offset.r]);
            v[0].g = afp(bottom_blob_fp32_data[v_offset.g]);
            v[0].b = afp(bottom_blob_fp32_data[v_offset.b]);
            v[0].a = afp(bottom_blob_fp32_data[v_offset.a]);
            v[1].r = afp(bottom_blob_fp32_data[vv_offset.r]);
            v[1].g = afp(bottom_blob_fp32_data[vv_offset.g]);
            v[1].b = afp(bottom_blob_fp32_data[vv_offset.b]);
            v[1].a = afp(bottom_blob_fp32_data[vv_offset.a]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y4 = gy * 8;
            v[0].r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 0, 0), 0).r);
            v[0].g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 1, 0), 0).r);
            v[0].b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 2, 0), 0).r);
            v[0].a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 3, 0), 0).r);
            v[1].r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 4, 0), 0).r);
            v[1].g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 5, 0), 0).r);
            v[1].b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 6, 0), 0).r);
            v[1].a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, y4 + 7, 0), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec4 z4 = ivec4(gz * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(cstep) + ivec4(gy * psc(w) + gx);
            ivec4 vv_offset = (z4 + 4) * psc(cstep) + ivec4(gy * psc(w) + gx);
            v[0].r = afp(bottom_blob_fp32_data[v_offset.r]);
            v[0].g = afp(bottom_blob_fp32_data[v_offset.g]);
            v[0].b = afp(bottom_blob_fp32_data[v_offset.b]);
            v[0].a = afp(bottom_blob_fp32_data[v_offset.a]);
            v[1].r = afp(bottom_blob_fp32_data[vv_offset.r]);
            v[1].g = afp(bottom_blob_fp32_data[vv_offset.g]);
            v[1].b = afp(bottom_blob_fp32_data[vv_offset.b]);
            v[1].a = afp(bottom_blob_fp32_data[vv_offset.a]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z4 = gz * 8;
            v[0].r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 0), 0).r);
            v[0].g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 1), 0).r);
            v[0].b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 2), 0).r);
            v[0].a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 3), 0).r);
            v[1].r = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 4), 0).r);
            v[1].g = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 5), 0).r);
            v[1].b = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 6), 0).r);
            v[1].a = afp(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, z4 + 7), 0).r);
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack1to8[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec4 x4 = ivec4(gx * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            ivec4 vv_offset = x4 + 4;
            v[0].r = buffer_ld1(bottom_blob_data, v_offset.r);
            v[0].g = buffer_ld1(bottom_blob_data, v_offset.g);
            v[0].b = buffer_ld1(bottom_blob_data, v_offset.b);
            v[0].a = buffer_ld1(bottom_blob_data, v_offset.a);
            v[1].r = buffer_ld1(bottom_blob_data, vv_offset.r);
            v[1].g = buffer_ld1(bottom_blob_data, vv_offset.g);
            v[1].b = buffer_ld1(bottom_blob_data, vv_offset.b);
            v[1].a = buffer_ld1(bottom_blob_data, vv_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x4 = gx * 8;
            v[0].r = image3d_ld1(bottom_blob_3d, ivec3(x4 + 0, 0, 0));
            v[0].g = image3d_ld1(bottom_blob_3d, ivec3(x4 + 1, 0, 0));
            v[0].b = image3d_ld1(bottom_blob_3d, ivec3(x4 + 2, 0, 0));
            v[0].a = image3d_ld1(bottom_blob_3d, ivec3(x4 + 3, 0, 0));
            v[1].r = image3d_ld1(bottom_blob_3d, ivec3(x4 + 4, 0, 0));
            v[1].g = image3d_ld1(bottom_blob_3d, ivec3(x4 + 5, 0, 0));
            v[1].b = image3d_ld1(bottom_blob_3d, ivec3(x4 + 6, 0, 0));
            v[1].a = image3d_ld1(bottom_blob_3d, ivec3(x4 + 7, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec4 y4 = ivec4(gy * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(w) + gx;
            ivec4 vv_offset = (y4 + 4) * psc(w) + gx;
            v[0].r = buffer_ld1(bottom_blob_data, v_offset.r);
            v[0].g = buffer_ld1(bottom_blob_data, v_offset.g);
            v[0].b = buffer_ld1(bottom_blob_data, v_offset.b);
            v[0].a = buffer_ld1(bottom_blob_data, v_offset.a);
            v[1].r = buffer_ld1(bottom_blob_data, vv_offset.r);
            v[1].g = buffer_ld1(bottom_blob_data, vv_offset.g);
            v[1].b = buffer_ld1(bottom_blob_data, vv_offset.b);
            v[1].a = buffer_ld1(bottom_blob_data, vv_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y4 = gy * 8;
            v[0].r = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 0, 0));
            v[0].g = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 1, 0));
            v[0].b = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 2, 0));
            v[0].a = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 3, 0));
            v[1].r = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 4, 0));
            v[1].g = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 5, 0));
            v[1].b = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 6, 0));
            v[1].a = image3d_ld1(bottom_blob_3d, ivec3(gx, y4 + 7, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec4 z4 = ivec4(gz * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(cstep) + ivec4(gy * psc(w) + gx);
            ivec4 vv_offset = (z4 + 4) * psc(cstep) + ivec4(gy * psc(w) + gx);
            v[0].r = buffer_ld1(bottom_blob_data, v_offset.r);
            v[0].g = buffer_ld1(bottom_blob_data, v_offset.g);
            v[0].b = buffer_ld1(bottom_blob_data, v_offset.b);
            v[0].a = buffer_ld1(bottom_blob_data, v_offset.a);
            v[1].r = buffer_ld1(bottom_blob_data, vv_offset.r);
            v[1].g = buffer_ld1(bottom_blob_data, vv_offset.g);
            v[1].b = buffer_ld1(bottom_blob_data, vv_offset.b);
            v[1].a = buffer_ld1(bottom_blob_data, vv_offset.a);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z4 = gz * 8;
            v[0].r = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 0));
            v[0].g = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 1));
            v[0].b = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 2));
            v[0].a = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 3));
            v[1].r = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 4));
            v[1].g = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 5));
            v[1].b = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 6));
            v[1].a = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, z4 + 7));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack4_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { vec4 top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, rgba32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            top_blob_fp32_data[gi] = vec4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = vec4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = vec4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack4_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { vec4 bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = afpvec4(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec4(texelFetch(bottom_blob_3d_fp32, ivec3(gx, 0, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = afpvec4(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec4(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = afpvec4(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec4(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, gz), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack4[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st4(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st4(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack4to1_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { float top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, r32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 x4 = ivec4(gx * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            top_blob_fp32_data[v_offset.r] = float(v.r);
            top_blob_fp32_data[v_offset.g] = float(v.g);
            top_blob_fp32_data[v_offset.b] = float(v.b);
            top_blob_fp32_data[v_offset.a] = float(v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x4 = gx * 4;
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 0, 0, 0), v.r);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 1, 0, 0), v.g);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 2, 0, 0), v.b);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 3, 0, 0), v.a);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 y4 = ivec4(gy * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(outw) + gx;
            top_blob_fp32_data[v_offset.r] = float(v.r);
            top_blob_fp32_data[v_offset.g] = float(v.g);
            top_blob_fp32_data[v_offset.b] = float(v.b);
            top_blob_fp32_data[v_offset.a] = float(v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y4 = gy * 4;
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 0, 0), v.r);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 1, 0), v.g);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 2, 0), v.b);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 3, 0), v.a);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 z4 = ivec4(gz * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            top_blob_fp32_data[v_offset.r] = float(v.r);
            top_blob_fp32_data[v_offset.g] = float(v.g);
            top_blob_fp32_data[v_offset.b] = float(v.b);
            top_blob_fp32_data[v_offset.a] = float(v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z4 = gz * 4;
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 0), v.r);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 1), v.g);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 2), v.b);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 3), v.a);
        }
#endif
    }
}
)";

static const char packing_pack4to1_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { vec4 bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
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
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = afpvec4(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec4(texelFetch(bottom_blob_3d_fp32, ivec3(gx, 0, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 x4 = ivec4(gx * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            buffer_st1(top_blob_data, v_offset.r, v.r);
            buffer_st1(top_blob_data, v_offset.g, v.g);
            buffer_st1(top_blob_data, v_offset.b, v.b);
            buffer_st1(top_blob_data, v_offset.a, v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x4 = gx * 4;
            image3d_st1(top_blob_3d, ivec3(x4 + 0, 0, 0), v.r);
            image3d_st1(top_blob_3d, ivec3(x4 + 1, 0, 0), v.g);
            image3d_st1(top_blob_3d, ivec3(x4 + 2, 0, 0), v.b);
            image3d_st1(top_blob_3d, ivec3(x4 + 3, 0, 0), v.a);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = afpvec4(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec4(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 y4 = ivec4(gy * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(outw) + gx;
            buffer_st1(top_blob_data, v_offset.r, v.r);
            buffer_st1(top_blob_data, v_offset.g, v.g);
            buffer_st1(top_blob_data, v_offset.b, v.b);
            buffer_st1(top_blob_data, v_offset.a, v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y4 = gy * 4;
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 0, 0), v.r);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 1, 0), v.g);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 2, 0), v.b);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 3, 0), v.a);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = afpvec4(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec4(texelFetch(bottom_blob_3d_fp32, ivec3(gx, gy, gz), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 z4 = ivec4(gz * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            buffer_st1(top_blob_data, v_offset.r, v.r);
            buffer_st1(top_blob_data, v_offset.g, v.g);
            buffer_st1(top_blob_data, v_offset.b, v.b);
            buffer_st1(top_blob_data, v_offset.a, v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z4 = gz * 4;

            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 0), v.r);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 1), v.g);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 2), v.b);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 3), v.a);
        }
#endif
    }
}
)";

static const char packing_pack4to1[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
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
    afpvec4 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 x4 = ivec4(gx * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            buffer_st1(top_blob_data, v_offset.r, v.r);
            buffer_st1(top_blob_data, v_offset.g, v.g);
            buffer_st1(top_blob_data, v_offset.b, v.b);
            buffer_st1(top_blob_data, v_offset.a, v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x4 = gx * 4;
            image3d_st1(top_blob_3d, ivec3(x4 + 0, 0, 0), v.r);
            image3d_st1(top_blob_3d, ivec3(x4 + 1, 0, 0), v.g);
            image3d_st1(top_blob_3d, ivec3(x4 + 2, 0, 0), v.b);
            image3d_st1(top_blob_3d, ivec3(x4 + 3, 0, 0), v.a);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 y4 = ivec4(gy * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(outw) + gx;
            buffer_st1(top_blob_data, v_offset.r, v.r);
            buffer_st1(top_blob_data, v_offset.g, v.g);
            buffer_st1(top_blob_data, v_offset.b, v.b);
            buffer_st1(top_blob_data, v_offset.a, v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y4 = gy * 4;
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 0, 0), v.r);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 1, 0), v.g);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 2, 0), v.b);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 3, 0), v.a);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld4(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 z4 = ivec4(gz * 4) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            buffer_st1(top_blob_data, v_offset.r, v.r);
            buffer_st1(top_blob_data, v_offset.g, v.g);
            buffer_st1(top_blob_data, v_offset.b, v.b);
            buffer_st1(top_blob_data, v_offset.a, v.a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z4 = gz * 4;
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 0), v.r);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 1), v.g);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 2), v.b);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 3), v.a);
        }
#endif
    }
}
)";

static const char packing_pack4to8_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { mat2x4 top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, rgba32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec2 x2 = ivec2(gx * 2) + ivec2(0, 1);
            ivec2 v_offset = x2;
            v[0] = buffer_ld4(bottom_blob_data, v_offset.r);
            v[1] = buffer_ld4(bottom_blob_data, v_offset.g);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x2 = gx * 2;
            v[0] = image3d_ld4(bottom_blob_3d, ivec3(x2 + 0, 0, 0));
            v[1] = image3d_ld4(bottom_blob_3d, ivec3(x2 + 1, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            top_blob_fp32_data[gi] = mat2x4(vec4(v[0]), vec4(v[1]));
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d_fp32, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec2 y2 = ivec2(gy * 2) + ivec2(0, 1);
            ivec2 v_offset = y2 * psc(w) + gx;
            v[0] = buffer_ld4(bottom_blob_data, v_offset.r);
            v[1] = buffer_ld4(bottom_blob_data, v_offset.g);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y2 = gy * 2;
            v[0] = image3d_ld4(bottom_blob_3d, ivec3(gx, y2 + 0, 0));
            v[1] = image3d_ld4(bottom_blob_3d, ivec3(gx, y2 + 1, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = mat2x4(vec4(v[0]), vec4(v[1]));
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d_fp32, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec2 z2 = ivec2(gz * 2) + ivec2(0, 1);
            ivec2 v_offset = z2 * psc(cstep) + ivec2(gy * psc(w) + gx);
            v[0] = buffer_ld4(bottom_blob_data, v_offset.r);
            v[1] = buffer_ld4(bottom_blob_data, v_offset.g);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z2 = gz * 2;
            v[0] = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, z2 + 0));
            v[1] = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, z2 + 1));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = mat2x4(vec4(v[0]), vec4(v[1]));
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d_fp32, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack4to8_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { vec4 bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec2 x2 = ivec2(gx * 2) + ivec2(0, 1);
            ivec2 v_offset = x2;
            v[0] = afpvec4(bottom_blob_fp32_data[v_offset.r]);
            v[1] = afpvec4(bottom_blob_fp32_data[v_offset.g]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x2 = gx * 2;
            v[0] = image3d_ld4(bottom_blob_3d_fp32, ivec3(x2 + 0, 0, 0));
            v[1] = image3d_ld4(bottom_blob_3d_fp32, ivec3(x2 + 1, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec2 y2 = ivec2(gy * 2) + ivec2(0, 1);
            ivec2 v_offset = y2 * psc(w) + gx;
            v[0] = afpvec4(bottom_blob_fp32_data[v_offset.r]);
            v[1] = afpvec4(bottom_blob_fp32_data[v_offset.g]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y2 = gy * 2;
            v[0] = image3d_ld4(bottom_blob_3d_fp32, ivec3(gx, y2 + 0, 0));
            v[1] = image3d_ld4(bottom_blob_3d_fp32, ivec3(gx, y2 + 1, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec2 z2 = ivec2(gz * 2) + ivec2(0, 1);
            ivec2 v_offset = z2 * psc(cstep) + ivec2(gy * psc(w) + gx);
            v[0] = afpvec4(bottom_blob_fp32_data[v_offset.r]);
            v[1] = afpvec4(bottom_blob_fp32_data[v_offset.g]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z2 = gz * 2;
            v[0] = image3d_ld4(bottom_blob_3d_fp32, ivec3(gx, gy, z2 + 0));
            v[1] = image3d_ld4(bottom_blob_3d_fp32, ivec3(gx, gy, z2 + 1));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack4to8[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec4 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            ivec2 x2 = ivec2(gx * 2) + ivec2(0, 1);
            ivec2 v_offset = x2;
            v[0] = buffer_ld4(bottom_blob_data, v_offset.r);
            v[1] = buffer_ld4(bottom_blob_data, v_offset.g);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int x2 = gx * 2;
            v[0] = image3d_ld4(bottom_blob_3d, ivec3(x2 + 0, 0, 0));
            v[1] = image3d_ld4(bottom_blob_3d, ivec3(x2 + 1, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            ivec2 y2 = ivec2(gy * 2) + ivec2(0, 1);
            ivec2 v_offset = y2 * psc(w) + gx;
            v[0] = buffer_ld4(bottom_blob_data, v_offset.r);
            v[1] = buffer_ld4(bottom_blob_data, v_offset.g);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int y2 = gy * 2;
            v[0] = image3d_ld4(bottom_blob_3d, ivec3(gx, y2 + 0, 0));
            v[1] = image3d_ld4(bottom_blob_3d, ivec3(gx, y2 + 1, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            ivec2 z2 = ivec2(gz * 2) + ivec2(0, 1);
            ivec2 v_offset = z2 * psc(cstep) + ivec2(gy * psc(w) + gx);
            v[0] = buffer_ld4(bottom_blob_data, v_offset.r);
            v[1] = buffer_ld4(bottom_blob_data, v_offset.g);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            int z2 = gz * 2;
            v[0] = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, z2 + 0));
            v[1] = image3d_ld4(bottom_blob_3d, ivec3(gx, gy, z2 + 1));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack8_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { mat2x4 top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, rgba32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            top_blob_fp32_data[gi] = mat2x4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x2 = gx * 2;
            imageStore(top_blob_3d_fp32, ivec3(x2 + 0, 0, 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(x2 + 1, 0, 0), v[1]);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = mat2x4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x2 = gx * 2;
            imageStore(top_blob_3d_fp32, ivec3(x2 + 0, gy, 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(x2 + 1, gy, 0), v[1]);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            top_blob_fp32_data[gi] = mat2x4(v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x2 = gx * 2;
            imageStore(top_blob_3d_fp32, ivec3(x2 + 0, gy, gz), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(x2 + 1, gy, gz), v[1]);
        }
#endif
    }
}
)";

static const char packing_pack8_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { mat2x4 bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, 0, 0), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, 0, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, gy, 0), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, gy, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, gy, gz), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, gy, gz), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack8[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec8 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st8(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st8(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";

static const char packing_pack8to1_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { float top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, r32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 x4 = ivec4(gx * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            ivec4 vv_offset = x4 + 4;
            top_blob_fp32_data[v_offset.r] = float(v[0].r);
            top_blob_fp32_data[v_offset.g] = float(v[0].g);
            top_blob_fp32_data[v_offset.b] = float(v[0].b);
            top_blob_fp32_data[v_offset.a] = float(v[0].a);
            top_blob_fp32_data[vv_offset.r] = float(v[1].r);
            top_blob_fp32_data[vv_offset.g] = float(v[1].g);
            top_blob_fp32_data[vv_offset.b] = float(v[1].b);
            top_blob_fp32_data[vv_offset.a] = float(v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x4 = gx * 8;
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 0, 0, 0), v[0].r);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 1, 0, 0), v[0].g);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 2, 0, 0), v[0].b);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 3, 0, 0), v[0].a);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 4, 0, 0), v[1].r);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 5, 0, 0), v[1].g);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 6, 0, 0), v[1].b);
            image3d_st1(top_blob_3d_fp32, ivec3(x4 + 7, 0, 0), v[1].a);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 y4 = ivec4(gy * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(outw) + gx;
            ivec4 vv_offset = (y4 + 4) * psc(outw) + gx;
            top_blob_fp32_data[v_offset.r] = float(v[0].r);
            top_blob_fp32_data[v_offset.g] = float(v[0].g);
            top_blob_fp32_data[v_offset.b] = float(v[0].b);
            top_blob_fp32_data[v_offset.a] = float(v[0].a);
            top_blob_fp32_data[vv_offset.r] = float(v[1].r);
            top_blob_fp32_data[vv_offset.g] = float(v[1].g);
            top_blob_fp32_data[vv_offset.b] = float(v[1].b);
            top_blob_fp32_data[vv_offset.a] = float(v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y4 = gy * 8;
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 0, 0), v[0].r);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 1, 0), v[0].g);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 2, 0), v[0].b);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 3, 0), v[0].a);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 4, 0), v[1].r);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 5, 0), v[1].g);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 6, 0), v[1].b);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, y4 + 7, 0), v[1].a);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 z4 = ivec4(gz * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            ivec4 vv_offset = (z4 + 4) * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            top_blob_fp32_data[v_offset.r] = float(v[0].r);
            top_blob_fp32_data[v_offset.g] = float(v[0].g);
            top_blob_fp32_data[v_offset.b] = float(v[0].b);
            top_blob_fp32_data[v_offset.a] = float(v[0].a);
            top_blob_fp32_data[vv_offset.r] = float(v[1].r);
            top_blob_fp32_data[vv_offset.g] = float(v[1].g);
            top_blob_fp32_data[vv_offset.b] = float(v[1].b);
            top_blob_fp32_data[vv_offset.a] = float(v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z4 = gz * 8;
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 0), v[0].r);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 1), v[0].g);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 2), v[0].b);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 3), v[0].a);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 4), v[1].r);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 5), v[1].g);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 6), v[1].b);
            image3d_st1(top_blob_3d_fp32, ivec3(gx, gy, z4 + 7), v[1].a);
        }
#endif
    }
}
)";

static const char packing_pack8to1_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { mat2x4 bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
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
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, 0, 0), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, 0, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 x4 = ivec4(gx * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            ivec4 vv_offset = x4 + 4;
            buffer_st1(top_blob_data, v_offset.r, v[0].r);
            buffer_st1(top_blob_data, v_offset.g, v[0].g);
            buffer_st1(top_blob_data, v_offset.b, v[0].b);
            buffer_st1(top_blob_data, v_offset.a, v[0].a);
            buffer_st1(top_blob_data, vv_offset.r, v[1].r);
            buffer_st1(top_blob_data, vv_offset.g, v[1].g);
            buffer_st1(top_blob_data, vv_offset.b, v[1].b);
            buffer_st1(top_blob_data, vv_offset.a, v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x4 = gx * 8;
            image3d_st1(top_blob_3d, ivec3(x4 + 0, 0, 0), v[0].r);
            image3d_st1(top_blob_3d, ivec3(x4 + 1, 0, 0), v[0].g);
            image3d_st1(top_blob_3d, ivec3(x4 + 2, 0, 0), v[0].b);
            image3d_st1(top_blob_3d, ivec3(x4 + 3, 0, 0), v[0].a);
            image3d_st1(top_blob_3d, ivec3(x4 + 4, 0, 0), v[1].r);
            image3d_st1(top_blob_3d, ivec3(x4 + 5, 0, 0), v[1].g);
            image3d_st1(top_blob_3d, ivec3(x4 + 6, 0, 0), v[1].b);
            image3d_st1(top_blob_3d, ivec3(x4 + 7, 0, 0), v[1].a);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, gy, 0), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, gy, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 y4 = ivec4(gy * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(outw) + gx;
            ivec4 vv_offset = (y4 + 4) * psc(outw) + gx;
            buffer_st1(top_blob_data, v_offset.r, v[0].r);
            buffer_st1(top_blob_data, v_offset.g, v[0].g);
            buffer_st1(top_blob_data, v_offset.b, v[0].b);
            buffer_st1(top_blob_data, v_offset.a, v[0].a);
            buffer_st1(top_blob_data, vv_offset.r, v[1].r);
            buffer_st1(top_blob_data, vv_offset.g, v[1].g);
            buffer_st1(top_blob_data, vv_offset.b, v[1].b);
            buffer_st1(top_blob_data, vv_offset.a, v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y4 = gy * 8;
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 0, 0), v[0].r);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 1, 0), v[0].g);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 2, 0), v[0].b);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 3, 0), v[0].a);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 4, 0), v[1].r);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 5, 0), v[1].g);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 6, 0), v[1].b);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 7, 0), v[1].a);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, gy, gz), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, gy, gz), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 z4 = ivec4(gz * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            ivec4 vv_offset = (z4 + 4) * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            buffer_st1(top_blob_data, v_offset.r, v[0].r);
            buffer_st1(top_blob_data, v_offset.g, v[0].g);
            buffer_st1(top_blob_data, v_offset.b, v[0].b);
            buffer_st1(top_blob_data, v_offset.a, v[0].a);
            buffer_st1(top_blob_data, vv_offset.r, v[1].r);
            buffer_st1(top_blob_data, vv_offset.g, v[1].g);
            buffer_st1(top_blob_data, vv_offset.b, v[1].b);
            buffer_st1(top_blob_data, vv_offset.a, v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z4 = gz * 8;
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 0), v[0].r);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 1), v[0].g);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 2), v[0].b);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 3), v[0].a);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 4), v[1].r);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 5), v[1].g);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 6), v[1].b);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 7), v[1].a);
        }
#endif
    }
}
)";

static const char packing_pack8to1[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
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
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 x4 = ivec4(gx * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = x4;
            ivec4 vv_offset = x4 + 4;
            buffer_st1(top_blob_data, v_offset.r, v[0].r);
            buffer_st1(top_blob_data, v_offset.g, v[0].g);
            buffer_st1(top_blob_data, v_offset.b, v[0].b);
            buffer_st1(top_blob_data, v_offset.a, v[0].a);
            buffer_st1(top_blob_data, vv_offset.r, v[1].r);
            buffer_st1(top_blob_data, vv_offset.g, v[1].g);
            buffer_st1(top_blob_data, vv_offset.b, v[1].b);
            buffer_st1(top_blob_data, vv_offset.a, v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x4 = gx * 8;
            image3d_st1(top_blob_3d, ivec3(x4 + 0, 0, 0), v[0].r);
            image3d_st1(top_blob_3d, ivec3(x4 + 1, 0, 0), v[0].g);
            image3d_st1(top_blob_3d, ivec3(x4 + 2, 0, 0), v[0].b);
            image3d_st1(top_blob_3d, ivec3(x4 + 3, 0, 0), v[0].a);
            image3d_st1(top_blob_3d, ivec3(x4 + 4, 0, 0), v[1].r);
            image3d_st1(top_blob_3d, ivec3(x4 + 5, 0, 0), v[1].g);
            image3d_st1(top_blob_3d, ivec3(x4 + 6, 0, 0), v[1].b);
            image3d_st1(top_blob_3d, ivec3(x4 + 7, 0, 0), v[1].a);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 y4 = ivec4(gy * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = y4 * psc(outw) + gx;
            ivec4 vv_offset = (y4 + 4) * psc(outw) + gx;
            buffer_st1(top_blob_data, v_offset.r, v[0].r);
            buffer_st1(top_blob_data, v_offset.g, v[0].g);
            buffer_st1(top_blob_data, v_offset.b, v[0].b);
            buffer_st1(top_blob_data, v_offset.a, v[0].a);
            buffer_st1(top_blob_data, vv_offset.r, v[1].r);
            buffer_st1(top_blob_data, vv_offset.g, v[1].g);
            buffer_st1(top_blob_data, vv_offset.b, v[1].b);
            buffer_st1(top_blob_data, vv_offset.a, v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y4 = gy * 8;
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 0, 0), v[0].r);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 1, 0), v[0].g);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 2, 0), v[0].b);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 3, 0), v[0].a);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 4, 0), v[1].r);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 5, 0), v[1].g);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 6, 0), v[1].b);
            image3d_st1(top_blob_3d, ivec3(gx, y4 + 7, 0), v[1].a);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec4 z4 = ivec4(gz * 8) + ivec4(0, 1, 2, 3);
            ivec4 v_offset = z4 * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            ivec4 vv_offset = (z4 + 4) * psc(outcstep) + ivec4(gy * psc(outw) + gx);
            buffer_st1(top_blob_data, v_offset.r, v[0].r);
            buffer_st1(top_blob_data, v_offset.g, v[0].g);
            buffer_st1(top_blob_data, v_offset.b, v[0].b);
            buffer_st1(top_blob_data, v_offset.a, v[0].a);
            buffer_st1(top_blob_data, vv_offset.r, v[1].r);
            buffer_st1(top_blob_data, vv_offset.g, v[1].g);
            buffer_st1(top_blob_data, vv_offset.b, v[1].b);
            buffer_st1(top_blob_data, vv_offset.a, v[1].a);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z4 = gz * 8;
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 0), v[0].r);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 1), v[0].g);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 2), v[0].b);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 3), v[0].a);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 4), v[1].r);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 5), v[1].g);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 6), v[1].b);
            image3d_st1(top_blob_3d, ivec3(gx, gy, z4 + 7), v[1].a);
        }
#endif
    }
}
)";

static const char packing_pack8to4_fp16_to_fp32[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob_fp32 { vec4 top_blob_fp32_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, rgba32f) writeonly uniform highp image3D top_blob_3d_fp32;
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
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 x2 = ivec2(gx * 2) + ivec2(0, 1);
            ivec2 v_offset = x2;
            top_blob_fp32_data[v_offset.r] = vec4(v[0]);
            top_blob_fp32_data[v_offset.g] = vec4(v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x2 = gx * 2;
            imageStore(top_blob_3d_fp32, ivec3(x2 + 0, 0, 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(x2 + 1, 0, 0), v[1]);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 y2 = ivec2(gy * 2) + ivec2(0, 1);
            ivec2 v_offset = y2 * psc(outw) + gx;
            top_blob_fp32_data[v_offset.r] = vec4(v[0]);
            top_blob_fp32_data[v_offset.g] = vec4(v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y2 = gy * 2;
            imageStore(top_blob_3d_fp32, ivec3(gx, y2 + 0, 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(gx, y2 + 1, 0), v[1]);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 z2 = ivec2(gz * 2) + ivec2(0, 1);
            ivec2 v_offset = z2 * psc(outcstep) + ivec2(gy * psc(outw) + gx);
            top_blob_fp32_data[v_offset.r] = vec4(v[0]);
            top_blob_fp32_data[v_offset.g] = vec4(v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z2 = gz * 2;
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, z2 + 0), v[0]);
            imageStore(top_blob_3d_fp32, ivec3(gx, gy, z2 + 1), v[1]);
        }
#endif
    }
}
)";

static const char packing_pack8to4_fp32_to_fp16[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob_fp32 { mat2x4 bottom_blob_fp32_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform highp sampler3D bottom_blob_3d_fp32;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, 0, 0), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, 0, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 x2 = ivec2(gx * 2) + ivec2(0, 1);
            ivec2 v_offset = x2;
            buffer_st4(top_blob_data, v_offset.r, v[0]);
            buffer_st4(top_blob_data, v_offset.g, v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x2 = gx * 2;
            image3d_st4(top_blob_3d, ivec3(x2 + 0, 0, 0), v[0]);
            image3d_st4(top_blob_3d, ivec3(x2 + 1, 0, 0), v[1]);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, gy, 0), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, gy, 0), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 y2 = ivec2(gy * 2) + ivec2(0, 1);
            ivec2 v_offset = y2 * psc(outw) + gx;
            buffer_st4(top_blob_data, v_offset.r, v[0]);
            buffer_st4(top_blob_data, v_offset.g, v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y2 = gy * 2;
            image3d_st4(top_blob_3d, ivec3(gx, y2 + 0, 0), v[0]);
            image3d_st4(top_blob_3d, ivec3(gx, y2 + 1, 0), v[1]);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = afpvec8(bottom_blob_fp32_data[gi]);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = afpvec8(texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2, gy, gz), 0), texelFetch(bottom_blob_3d_fp32, ivec3(gx * 2 + 1, gy, gz), 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 z2 = ivec2(gz * 2) + ivec2(0, 1);
            ivec2 v_offset = z2 * psc(outcstep) + ivec2(gy * psc(outw) + gx);
            buffer_st4(top_blob_data, v_offset.r, v[0]);
            buffer_st4(top_blob_data, v_offset.g, v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z2 = gz * 2;
            image3d_st4(top_blob_3d, ivec3(gx, gy, z2 + 0), v[0]);
            image3d_st4(top_blob_3d, ivec3(gx, gy, z2 + 1), v[1]);
        }
#endif
    }
}
)";

static const char packing_pack8to4[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
struct sfpvec8 { f16vec4 abcd; f16vec4 efgh; };
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfpvec8 bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfpvec4 top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc4) writeonly uniform unfp image3D top_blob_3d;
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
    afpvec8 v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 x2 = ivec2(gx * 2) + ivec2(0, 1);
            ivec2 v_offset = x2;
            buffer_st4(top_blob_data, v_offset.r, v[0]);
            buffer_st4(top_blob_data, v_offset.g, v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int x2 = gx * 2;
            image3d_st4(top_blob_3d, ivec3(x2 + 0, 0, 0), v[0]);
            image3d_st4(top_blob_3d, ivec3(x2 + 1, 0, 0), v[1]);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 y2 = ivec2(gy * 2) + ivec2(0, 1);
            ivec2 v_offset = y2 * psc(outw) + gx;
            buffer_st4(top_blob_data, v_offset.r, v[0]);
            buffer_st4(top_blob_data, v_offset.g, v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int y2 = gy * 2;
            image3d_st4(top_blob_3d, ivec3(gx, y2 + 0, 0), v[0]);
            image3d_st4(top_blob_3d, ivec3(gx, y2 + 1, 0), v[1]);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld8(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld8(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            ivec2 z2 = ivec2(gz * 2) + ivec2(0, 1);
            ivec2 v_offset = z2 * psc(outcstep) + ivec2(gy * psc(outw) + gx);
            buffer_st4(top_blob_data, v_offset.r, v[0]);
            buffer_st4(top_blob_data, v_offset.g, v[1]);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            int z2 = gz * 2;
            image3d_st4(top_blob_3d, ivec3(gx, gy, z2 + 0), v[0]);
            image3d_st4(top_blob_3d, ivec3(gx, gy, z2 + 1), v[1]);
        }
#endif
    }
}
)";

static const char packing[] = R"(
#version 450
#if ImVulkan_fp16_storage
#extension GL_EXT_shader_16bit_storage: require
#endif
#if ImVulkan_fp16_arithmetic
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require
#endif
layout (constant_id = 0) const int storage_type_from = 0;
layout (constant_id = 1) const int storage_type_to = 0;
#define shape_constant_id_offset 2
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
layout (binding = 0) readonly buffer bottom_blob { sfp bottom_blob_data[]; };
layout (binding = 1) writeonly buffer top_blob { sfp top_blob_data[]; };
#if ImVulkan_image_shader
layout (binding = 2) uniform unfp sampler3D bottom_blob_3d;
layout (binding = 3, imfmtc1) writeonly uniform unfp image3D top_blob_3d;
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
    if (gx >= psc(outw) || gy >= psc(outh) || gz >= psc(outc))
        return;
    afp v;
    if (psc(dims) == 1)
    {
        if (storage_type_from == 0)
        {
            int gi = gx;
            v = buffer_ld1(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld1(bottom_blob_3d, ivec3(gx, 0, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gx;
            buffer_st1(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st1(top_blob_3d, ivec3(gx, 0, 0), v);
        }
#endif
    }
    else if (psc(dims) == 2)
    {
        if (storage_type_from == 0)
        {
            int gi = gy * psc(w) + gx;
            v = buffer_ld1(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, 0));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gy * psc(outw) + gx;
            buffer_st1(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st1(top_blob_3d, ivec3(gx, gy, 0), v);
        }
#endif
    }
    else // if (psc(dims) == 3)
    {
        if (storage_type_from == 0)
        {
            int gi = gz * psc(cstep) + gy * psc(w) + gx;
            v = buffer_ld1(bottom_blob_data, gi);
        }
#if ImVulkan_image_shader
        if (storage_type_from == 1)
        {
            v = image3d_ld1(bottom_blob_3d, ivec3(gx, gy, gz));
        }
#endif
        if (storage_type_to == 0)
        {
            int gi = gz * psc(outcstep) + gy * psc(outw) + gx;
            buffer_st1(top_blob_data, gi, v);
        }
#if ImVulkan_image_shader
        if (storage_type_to == 1)
        {
            image3d_st1(top_blob_3d, ivec3(gx, gy, gz), v);
        }
#endif
    }
}
)";
