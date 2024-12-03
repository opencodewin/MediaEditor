#include <sstream>
#include "Blend_vulkan.h"
#include "Blend_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
Blend_vulkan::Blend_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Blend");
    
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(Blend_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }
    spirv_data.clear();
    if (compile_spirv_module(BlendMaskMerge_data, opt, spirv_data) == 0)
    {
        pipe_mask_merge = new Pipeline(vkdev);
        pipe_mask_merge->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }

    fea = new ImGui::Feather_vulkan(gpu);
    cmd->reset();
}

Blend_vulkan::~Blend_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (pipe_mask_merge) { delete pipe_mask_merge; pipe_mask_merge = nullptr; }
        if (fea) { delete fea; fea = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Blend_vulkan::upload_param(const VkMat& blend, VkMat& base, ImBlendingMode mode, float opacity, ImPoint offset)
{
    std::vector<VkMat> bindings(8);
    if      (base.type == IM_DT_INT8)     bindings[0] = base;
    else if (base.type == IM_DT_INT16 || base.type == IM_DT_INT16_BE)    bindings[1] = base;
    else if (base.type == IM_DT_FLOAT16)  bindings[2] = base;
    else if (base.type == IM_DT_FLOAT32)  bindings[3] = base;

    if      (blend.type == IM_DT_INT8)      bindings[4] = blend;
    else if (blend.type == IM_DT_INT16 || blend.type == IM_DT_INT16_BE)     bindings[5] = blend;
    else if (blend.type == IM_DT_FLOAT16)   bindings[6] = blend;
    else if (blend.type == IM_DT_FLOAT32)   bindings[7] = blend;

    std::vector<vk_constant_type> constants(15);
    constants[0].i = blend.w;
    constants[1].i = blend.h;
    constants[2].i = blend.c;
    constants[3].i = blend.color_format;
    constants[4].i = blend.type;
    constants[5].i = base.w;
    constants[6].i = base.h;
    constants[7].i = base.c;
    constants[8].i = base.color_format;
    constants[9].i = base.type;
    constants[10].i = mode;
    constants[11].i = offset.x;
    constants[12].i = offset.y;
    constants[13].f = opacity;
    constants[14].i = 0;
    cmd->record_pipeline(pipe, bindings, constants, base);
}

double Blend_vulkan::forward(const ImMat& blend, ImMat& base, ImBlendingMode mode, float opacity, ImPoint offset)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }

    if (offset.x >= base.w || offset.y >= base.h || offset.x <= -base.w || offset.y <= -base.h)
        return ret;

    VkMat dst_gpu;
    if (base.device == IM_DD_VULKAN)
    {
        dst_gpu = base;
    }
    else if (base.device == IM_DD_CPU)
    {
        cmd->record_clone(base, dst_gpu, opt);
    }

    VkMat src_gpu;
    if (blend.device == IM_DD_VULKAN)
    {
        src_gpu = blend;
    }
    else if (blend.device == IM_DD_CPU)
    {
        cmd->record_clone(blend, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, mode, opacity, offset);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (base.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, base, opt);
    else if (base.device == IM_DD_VULKAN)
        base = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#endif
    cmd->reset();
    base.copy_attribute(blend);
    return ret;
}

void Blend_vulkan::upload_param(const VkMat& blend, const VkMat& mask, VkMat& base, ImBlendingMode mode, float opacity, float feather, ImPoint offset)
{
    // make mask with feather
    VkMat vk_feather;
    if (feather > 1.0)
    {
        fea->forward(mask, vk_feather, feather);
    }
    else
        vk_feather = mask;

    std::vector<VkMat> bindings(8);
    if      (blend.type == IM_DT_INT8)     bindings[0] = blend;
    else if (blend.type == IM_DT_INT16 || blend.type == IM_DT_INT16_BE)    bindings[1] = blend;
    else if (blend.type == IM_DT_FLOAT16)  bindings[2] = blend;
    else if (blend.type == IM_DT_FLOAT32)  bindings[3] = blend;

    if      (vk_feather.type == IM_DT_INT8)      bindings[4] = vk_feather;
    else if (vk_feather.type == IM_DT_INT16 || vk_feather.type == IM_DT_INT16_BE)     bindings[5] = vk_feather;
    else if (vk_feather.type == IM_DT_FLOAT16)   bindings[6] = vk_feather;
    else if (vk_feather.type == IM_DT_FLOAT32)   bindings[7] = vk_feather;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = blend.w;
    constants[1].i = blend.h;
    constants[2].i = blend.c;
    constants[3].i = blend.color_format;
    constants[4].i = blend.type;
    constants[5].i = vk_feather.w;
    constants[6].i = vk_feather.h;
    constants[7].i = vk_feather.c;
    constants[8].i = vk_feather.color_format;
    constants[9].i = vk_feather.type;
    cmd->record_pipeline(pipe_mask_merge, bindings, constants, vk_feather);

    std::vector<VkMat> blend_bindings(8);
    if      (base.type == IM_DT_INT8)     blend_bindings[0] = base;
    else if (base.type == IM_DT_INT16 || base.type == IM_DT_INT16_BE)    blend_bindings[1] = base;
    else if (base.type == IM_DT_FLOAT16)  blend_bindings[2] = base;
    else if (base.type == IM_DT_FLOAT32)  blend_bindings[3] = base;

    if      (blend.type == IM_DT_INT8)      blend_bindings[4] = blend;
    else if (blend.type == IM_DT_INT16 || blend.type == IM_DT_INT16_BE)     blend_bindings[5] = blend;
    else if (blend.type == IM_DT_FLOAT16)   blend_bindings[6] = blend;
    else if (blend.type == IM_DT_FLOAT32)   blend_bindings[7] = blend;

    std::vector<vk_constant_type> blend_constants(15);
    blend_constants[0].i = blend.w;
    blend_constants[1].i = blend.h;
    blend_constants[2].i = blend.c;
    blend_constants[3].i = blend.color_format;
    blend_constants[4].i = blend.type;
    blend_constants[5].i = base.w;
    blend_constants[6].i = base.h;
    blend_constants[7].i = base.c;
    blend_constants[8].i = base.color_format;
    blend_constants[9].i = base.type;
    blend_constants[10].i = mode;
    blend_constants[11].i = offset.x;
    blend_constants[12].i = offset.y;
    blend_constants[13].f = opacity;
    blend_constants[14].i = 1;
    cmd->record_pipeline(pipe, blend_bindings, blend_constants, base);
}

double Blend_vulkan::forward(const ImMat& blend, const ImMat& mask, ImMat& base, ImBlendingMode mode, float opacity, float feather, ImPoint offset)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !pipe_mask_merge || !fea || !cmd)
    {
        return ret;
    }

    if (offset.x >= base.w || offset.y >= base.h || offset.x <= -base.w || offset.y <= -base.h)
        return ret;

    VkMat dst_gpu;
    if (base.device == IM_DD_VULKAN)
    {
        dst_gpu = base;
    }
    else if (base.device == IM_DD_CPU)
    {
        cmd->record_clone(base, dst_gpu, opt);
    }

    VkMat src_gpu;
    if (blend.device == IM_DD_VULKAN)
    {
        src_gpu = blend;
    }
    else if (blend.device == IM_DD_CPU)
    {
        cmd->record_clone(blend, src_gpu, opt);
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

    upload_param(src_gpu, mask_gpu, dst_gpu, mode, opacity, feather, offset);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (base.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, base, opt);
    else if (base.device == IM_DD_VULKAN)
        base = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#endif
    cmd->reset();
    base.copy_attribute(blend);
    return ret;
}
} // namespace ImGui