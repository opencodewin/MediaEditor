#include "Burn_vulkan.h"
#include "Burn_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
Burn_vulkan::Burn_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Burn");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(Burn_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    cmd->reset();
}

Burn_vulkan::~Burn_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Burn_vulkan::upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float progress, ImPixel& back_colour) const
{
    std::vector<VkMat> bindings(12);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src1.type == IM_DT_INT8)      bindings[4] = src1;
    else if (src1.type == IM_DT_INT16)     bindings[5] = src1;
    else if (src1.type == IM_DT_FLOAT16)   bindings[6] = src1;
    else if (src1.type == IM_DT_FLOAT32)   bindings[7] = src1;

    if      (src2.type == IM_DT_INT8)      bindings[8] = src2;
    else if (src2.type == IM_DT_INT16)     bindings[9] = src2;
    else if (src2.type == IM_DT_FLOAT16)   bindings[10] = src2;
    else if (src2.type == IM_DT_FLOAT32)   bindings[11] = src2;

    std::vector<vk_constant_type> constants(20);
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
    constants[10].i = dst.w;
    constants[11].i = dst.h;
    constants[12].i = dst.c;
    constants[13].i = dst.color_format;
    constants[14].i = dst.type;
    constants[15].f = progress;
    constants[16].f = back_colour.r;
    constants[17].f = back_colour.g;
    constants[18].f = back_colour.b;
    constants[19].f = back_colour.a;
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double Burn_vulkan::transition(const ImMat& src1, const ImMat& src2, ImMat& dst, float progress, ImPixel& back_colour) const
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

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src1_gpu, src2_gpu, dst_gpu, progress, back_colour);

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
