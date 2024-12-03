#include "CopyTo_vulkan.h"
#include "CopyTo_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
CopyTo_vulkan::CopyTo_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "CopyTo");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(CopyTo_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }
    
    cmd->reset();
}

CopyTo_vulkan::~CopyTo_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void CopyTo_vulkan::upload_param(const VkMat& src, VkMat& dst, int x, int y, float alpha) const
{
    std::vector<VkMat> bindings(8);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src.type == IM_DT_INT8)      bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)     bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)   bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)   bindings[7] = src;

    std::vector<vk_constant_type> constants(13);
    constants[0].i = src.w;
    constants[1].i = src.h;
    constants[2].i = src.c;
    constants[3].i = src.color_format;
    constants[4].i = src.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.c;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    constants[10].i = x;
    constants[11].i = y;
    constants[12].f = alpha;
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double CopyTo_vulkan::copyTo(const ImMat& src, ImMat& dst, int x, int y, float alpha) const
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }

    //if (src.dims != dst.dims || src.color_space != dst.color_space || src.color_range != dst.color_range)
    //    return ret;

    if (!dst.empty())
    {
        if (x >= dst.w || y >= dst.h || x <= -dst.w || y <= -dst.h)
            return ret;
    }
    else
    {
        if (x >= src.w || y >= src.h || x <= -src.w || y <= -src.h)
            return ret;
    }

    VkMat dst_gpu;
    if (dst.empty())
        dst_gpu.create_type(src.w, src.h, 4, dst.type, opt.blob_vkallocator);
    else
    {
        if (dst.device == IM_DD_VULKAN)
        {
            dst_gpu = dst;
        }
        else if (dst.device == IM_DD_CPU)
        {
            cmd->record_clone(dst, dst_gpu, opt);
        }
    }

    VkMat src_gpu;
    if (src.device == IM_DD_VULKAN)
    {
        src_gpu = src;
    }
    else if (src.device == IM_DD_CPU)
    {
        cmd->record_clone(src, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, x, y, alpha);

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
    dst.copy_attribute(src);
    return ret;
}
} //namespace ImGui 
