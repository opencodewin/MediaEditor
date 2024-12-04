#include "warpPerspective_vulkan.h"
#include "warpPerspective_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
warpPerspective_vulkan::warpPerspective_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "warpPerspective");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(Filter_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }
    
    cmd->reset();
}

warpPerspective_vulkan::~warpPerspective_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void warpPerspective_vulkan::upload_param(const VkMat& src, VkMat& dst, const VkMat& M, ImInterpolateMode type, ImPixel border_col, ImPixel crop) const
{
    std::vector<VkMat> bindings(9);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[7] = src;

    bindings[8] = M;

    std::vector<vk_constant_type> constants(19);
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
    constants[10].f = border_col.r; // board R
    constants[11].f = border_col.g; // board G
    constants[12].f = border_col.b; // board B
    constants[13].f = border_col.a; // board A
    constants[14].i = crop.r;       // crop l
    constants[15].i = crop.g;       // crop t
    constants[16].i = crop.b;       // crop r
    constants[17].i = crop.a;       // crop b
    constants[18].i = type;
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double warpPerspective_vulkan::warp(const ImMat& src, ImMat& dst, const ImMat& M, ImInterpolateMode type, ImPixel border_col, ImPixel crop) const
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(dst.w > 0 ? dst.w : src.w, dst.h > 0 ? dst.h : src.h, 4, dst.type, opt.blob_vkallocator);

    VkMat src_gpu;
    if (src.device == IM_DD_VULKAN)
    {
        src_gpu = src;
    }
    else if (src.device == IM_DD_CPU)
    {
        cmd->record_clone(src, src_gpu, opt);
    }

    VkMat m_gpu;
    if (M.device == IM_DD_VULKAN)
    {
        m_gpu = M;
    }
    else if(M.device == IM_DD_CPU)
    {
        cmd->record_clone(M, m_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, m_gpu, type, border_col, crop);

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
