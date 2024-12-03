#include "Substract_mean_normalize.h"
#include "imvk_command.h"
#include "Substract_mean_normalize_shader.h"

namespace ImGui 
{
Substract_Mean_Normalize_vulkan::Substract_Mean_Normalize_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Substract_Mean_Normalize");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(Filter_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }
    
    cmd->reset();
}

Substract_Mean_Normalize_vulkan::~Substract_Mean_Normalize_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Substract_Mean_Normalize_vulkan::upload_param(const VkMat& src, VkMat& dst, std::vector<float> mean_vals, std::vector<float> norm_vals)
{
    std::vector<VkMat> bindings(6);
    if      (src.type == IM_DT_INT8)     bindings[0] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    bindings[1] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[2] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[3] = src;

    if      (dst.type == IM_DT_FLOAT16)  bindings[4] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[5] = dst;

    std::vector<vk_constant_type> constants(18);
    constants[0].i = src.w;
    constants[1].i = src.h;
    constants[2].i = src.c;
    constants[3].i = src.color_format;
    constants[4].i = src.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.cstep;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    constants[10].f = mean_vals[0];
    constants[11].f = mean_vals[1];
    constants[12].f = mean_vals[2];
    constants[13].f = mean_vals[3];
    constants[14].f = norm_vals[0];
    constants[15].f = norm_vals[1];
    constants[16].f = norm_vals[2];
    constants[17].f = norm_vals[3];
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double  Substract_Mean_Normalize_vulkan::forward(const ImMat& bottom_blob, ImMat& top_blob, std::vector<float> mean_vals, std::vector<float> norm_vals)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }
    auto color_format = top_blob.color_format;
    int channels = IM_ISALPHA(color_format) ? 4 : IM_ISRGB(color_format) ? 3 : IM_ISMONO(color_format) ? 1 : 4;
    VkMat dst_gpu;
    dst_gpu.create_type(bottom_blob.w, bottom_blob.h, channels, top_blob.type, opt.blob_vkallocator);
    dst_gpu.color_format = color_format;

    VkMat src_gpu;
    if (bottom_blob.device == IM_DD_VULKAN)
    {
        src_gpu = bottom_blob;
    }
    else if (bottom_blob.device == IM_DD_CPU)
    {
        cmd->record_clone(bottom_blob, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, mean_vals, norm_vals);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (top_blob.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, top_blob, opt);
    else if (top_blob.device == IM_DD_VULKAN)
        top_blob = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#endif
    cmd->reset();
    top_blob.copy_attribute(bottom_blob);
    return ret;
}
} // namespace ImGui
