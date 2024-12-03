#include "Luma_vulkan.h"
#include "Luma_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
Luma_vulkan::Luma_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Heart");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(Luma_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    cmd->reset();
}

Luma_vulkan::~Luma_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Luma_vulkan::upload_param(const VkMat& src1, const VkMat& src2, const VkMat& mask, VkMat& dst, float progress) const
{
    std::vector<VkMat> bindings(16);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src1.type == IM_DT_INT8)      bindings[4] = src1;
    else if (src1.type == IM_DT_INT16 || src1.type == IM_DT_INT16_BE)     bindings[5] = src1;
    else if (src1.type == IM_DT_FLOAT16)   bindings[6] = src1;
    else if (src1.type == IM_DT_FLOAT32)   bindings[7] = src1;

    if      (src2.type == IM_DT_INT8)      bindings[8] = src2;
    else if (src2.type == IM_DT_INT16 || src2.type == IM_DT_INT16_BE)     bindings[9] = src2;
    else if (src2.type == IM_DT_FLOAT16)   bindings[10] = src2;
    else if (src2.type == IM_DT_FLOAT32)   bindings[11] = src2;

    if      (mask.type == IM_DT_INT8)      bindings[12] = mask;
    else if (mask.type == IM_DT_INT16 || mask.type == IM_DT_INT16_BE)     bindings[13] = mask;
    else if (mask.type == IM_DT_FLOAT16)   bindings[14] = mask;
    else if (mask.type == IM_DT_FLOAT32)   bindings[15] = mask;

    std::vector<vk_constant_type> constants(21);
    constants[0].i = src1.w;
    constants[1].i = src1.h;
    constants[2].i = src1.c;
    constants[3].i = src1.color_format;
    constants[4].i = src1.type;
    constants[5].i = src2.w;
    constants[6].i = src2.h;
    constants[7].i = src2.c;
    constants[8].i = src2.color_format;
    constants[9].i = src2.type;
    constants[10].i = mask.w;
    constants[11].i = mask.h;
    constants[12].i = mask.c;
    constants[13].i = mask.color_format;
    constants[14].i = mask.type;
    constants[15].i = dst.w;
    constants[16].i = dst.h;
    constants[17].i = dst.c;
    constants[18].i = dst.color_format;
    constants[19].i = dst.type;
    constants[20].f = progress;
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double Luma_vulkan::transition(const ImMat& src1, const ImMat& src2, const ImMat& mask, ImMat& dst, float progress) const
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(src2.w, src2.h, 4, dst.type, opt.blob_vkallocator);

    VkMat src1_gpu;
    if (src1.device == IM_DD_VULKAN)
    {
        src1_gpu = src1;
    }
    else if (src1.device == IM_DD_CPU)
    {
        cmd->record_clone(src1, src1_gpu, opt);
    }

    VkMat src2_gpu;
    if (src2.device == IM_DD_VULKAN)
    {
        src2_gpu = src2;
    }
    else if (src2.device == IM_DD_CPU)
    {
        cmd->record_clone(src2, src2_gpu, opt);
    }

    VkMat mask_gpu;
    if (mask.device == IM_DD_VULKAN)
    {
        mask_gpu = mask;
    }
    else if (mask.device == IM_DD_CPU)
    {
        cmd->record_clone(mask, mask_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src1_gpu, src2_gpu, mask_gpu, dst_gpu, progress);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (dst.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, dst, opt);
    else if (dst.device == IM_DD_VULKAN)
        dst = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#endif
    cmd->reset();
    dst.copy_attribute(src1);
    return ret;
}
} //namespace ImGui 
