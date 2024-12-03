#include "Concat_vulkan.h"
#include "Concat_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
Concat_vulkan::Concat_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Concat");

    std::vector<vk_specialization_type> specializations(0);

    if (compile_spirv_module(Shader_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }
    
    cmd->reset();
}

Concat_vulkan::~Concat_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Concat_vulkan::upload_param(const VkMat& src0, const VkMat& src1, VkMat& dst, int direction) const
{
    std::vector<VkMat> bindings(12);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src0.type == IM_DT_INT8)      bindings[4] = src0;
    else if (src0.type == IM_DT_INT16 || src0.type == IM_DT_INT16_BE)     bindings[5] = src0;
    else if (src0.type == IM_DT_FLOAT16)   bindings[6] = src0;
    else if (src0.type == IM_DT_FLOAT32)   bindings[7] = src0;

    if      (src1.type == IM_DT_INT8)      bindings[8] = src1;
    else if (src1.type == IM_DT_INT16 || src1.type == IM_DT_INT16_BE)     bindings[9] = src1;
    else if (src1.type == IM_DT_FLOAT16)   bindings[10] = src1;
    else if (src1.type == IM_DT_FLOAT32)   bindings[11] = src1;


    std::vector<vk_constant_type> constants(16);
    constants[0].i = src0.w;
    constants[1].i = src0.h;
    constants[2].i = src0.c;
    constants[3].i = src0.color_format;
    constants[4].i = src0.type;
    constants[5].i = src1.w;
    constants[6].i = src1.h;
    constants[7].i = src1.c;
    constants[8].i = src1.color_format;
    constants[9].i = src1.type;
    constants[10].i = dst.w;
    constants[11].i = dst.h;
    constants[12].i = dst.c;
    constants[13].i = dst.color_format;
    constants[14].i = dst.type;
    constants[15].i = direction;
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double Concat_vulkan::concat(const ImMat& src0, const ImMat& src1, ImMat& dst, int direction) const
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }

    int dst_width, dst_height;
    if (direction == CONCAT_HORIZONTAL)
    {
        if (src0.h != src1.h)
            return ret;
        dst_width = src0.w + src1.w;
        dst_height = src0.h;
    }
    else
    {
        if (src0.w != src1.w)
            return ret;
        dst_width = src0.w;
        dst_height = src0.h + src1.h;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(dst_width, dst_height, 4, dst.type, opt.blob_vkallocator);

    VkMat src0_gpu;
    if (src0.device == IM_DD_VULKAN)
    {
        src0_gpu = src0;
    }
    else if (src0.device == IM_DD_CPU)
    {
        cmd->record_clone(src0, src0_gpu, opt);
    }
    VkMat src1_gpu;
    if (src1.device == IM_DD_VULKAN)
    {
        src1_gpu = src1;
    }
    else if (src1.device == IM_DD_CPU)
    {
        cmd->record_clone(src1, src1_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src0_gpu, src1_gpu, dst_gpu, direction);

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
    dst.copy_attribute(src0);
    return ret;
}
} //namespace ImGui 
