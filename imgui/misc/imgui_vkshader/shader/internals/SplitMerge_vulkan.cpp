#include "SplitMerge_vulkan.h"
#include "SplitMerge_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
SplitMerge_vulkan::SplitMerge_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "SplitMerge");

    std::vector<vk_specialization_type> specializations(0);

    if (compile_spirv_module(Split_data, opt, spirv_data) == 0)
    {
        pipe_split = new Pipeline(vkdev);
        pipe_split->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(Merge_data, opt, spirv_data) == 0)
    {
        pipe_merge = new Pipeline(vkdev);
        pipe_merge->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    
    cmd->reset();
}

SplitMerge_vulkan::~SplitMerge_vulkan()
{
    if (vkdev)
    {
        if (pipe_split) { delete pipe_split; pipe_split = nullptr; }
        if (pipe_merge) { delete pipe_merge; pipe_merge = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void SplitMerge_vulkan::upload_param(const VkMat& src, std::vector<VkMat>& dst) const
{
    for (int i = 0; i < src.c; i++)
    {
        std::vector<VkMat> bindings(8);
        if      (dst[i].type == IM_DT_INT8)     bindings[0] = dst[i];
        else if (dst[i].type == IM_DT_INT16 || dst[i].type == IM_DT_INT16_BE)    bindings[1] = dst[i];
        else if (dst[i].type == IM_DT_FLOAT16)  bindings[2] = dst[i];
        else if (dst[i].type == IM_DT_FLOAT32)  bindings[3] = dst[i];

        if      (src.type == IM_DT_INT8)      bindings[4] = src;
        else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)     bindings[5] = src;
        else if (src.type == IM_DT_FLOAT16)   bindings[6] = src;
        else if (src.type == IM_DT_FLOAT32)   bindings[7] = src;

        std::vector<vk_constant_type> constants(11);
        constants[0].i = src.w;
        constants[1].i = src.h;
        constants[2].i = src.c;
        constants[3].i = src.color_format;
        constants[4].i = src.type;
        constants[5].i = dst[i].w;
        constants[6].i = dst[i].h;
        constants[7].i = dst[i].c;
        constants[8].i = dst[i].color_format;
        constants[9].i = dst[i].type;
        constants[10].i = i;
        cmd->record_pipeline(pipe_split, bindings, constants, dst[i]);
    }
}

double SplitMerge_vulkan::split(const ImMat& src, std::vector<ImMat>& dst) const
{
    double ret = -1.0;
    if (!vkdev || !pipe_split || !cmd)
        return ret;

    if (IM_ISMONO(src.color_format))
        return ret;

    if (!IM_ISRGB(src.color_format))
        return ret;

    VkMat src_gpu;
    if (src.device == IM_DD_VULKAN)
    {
        src_gpu = src;
    }
    else if (src.device == IM_DD_CPU)
    {
        cmd->record_clone(src, src_gpu, opt);
    }

    if (dst.empty())
    {
        dst.resize(src.c);
    }

    std::vector<VkMat> dsts_gpu;
    for (int i = 0; i < src.c; i++)
    {
        VkMat dst_gpu;
        dst_gpu.create_type(src.w, src.h, 1, src.type, opt.blob_vkallocator);
        dsts_gpu.push_back(dst_gpu);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dsts_gpu);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    for (int i = 0; i < dsts_gpu.size(); i++)
    {
        if (dst[i].device == IM_DD_CPU)
            cmd->record_clone(dsts_gpu[i], dst[i], opt);
        else if (dst[i].device == IM_DD_VULKAN)
            dst[i] = dsts_gpu[i];
        // TODO::copy attrib?
    }

    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#endif
    cmd->reset();
    return ret;
}

void SplitMerge_vulkan::upload_param(const std::vector<VkMat>& src, VkMat& dst) const
{
    for (int i = 0; i < src.size(); i++)
    {
        std::vector<VkMat> bindings(8);
        if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
        else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
        else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
        else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

        if      (src[i].type == IM_DT_INT8)      bindings[4] = src[i];
        else if (src[i].type == IM_DT_INT16 || src[i].type == IM_DT_INT16_BE)     bindings[5] = src[i];
        else if (src[i].type == IM_DT_FLOAT16)   bindings[6] = src[i];
        else if (src[i].type == IM_DT_FLOAT32)   bindings[7] = src[i];

        std::vector<vk_constant_type> constants(11);
        constants[0].i = src[i].w;
        constants[1].i = src[i].h;
        constants[2].i = src[i].c;
        constants[3].i = src[i].color_format;
        constants[4].i = src[i].type;
        constants[5].i = dst.w;
        constants[6].i = dst.h;
        constants[7].i = dst.c;
        constants[8].i = dst.color_format;
        constants[9].i = dst.type;
        constants[10].i = i;
        cmd->record_pipeline(pipe_merge, bindings, constants, dst);
    }
}

double SplitMerge_vulkan::merge(const std::vector<ImMat>& src, ImMat& dst) const
{
    double ret = -1.0;
    if (!vkdev || !pipe_merge || !cmd)
        return ret;

    if (src.empty())
        return ret;
    
    VkMat dst_gpu;
    if (dst.empty())
        dst_gpu.create_type(src[0].w, src[0].h, src.size(), src[0].type, opt.blob_vkallocator);
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

    std::vector<VkMat> srcs_gpu;
    for (int i = 0; i < src.size(); i++)
    {
        VkMat src_gpu;
        if (src[i].device == IM_DD_VULKAN)
        {
            src_gpu = src[i];
        }
        else if (src[i].device == IM_DD_CPU)
        {
            cmd->record_clone(src[i], src_gpu, opt);
        }
        srcs_gpu.push_back(src_gpu);
    }
#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(srcs_gpu, dst_gpu);

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
    return ret;
}
} // namespace ImGui 
