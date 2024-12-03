#include "Waveform_vulkan.h"
#include "Waveform_shader.h"
#include "ImVulkanShader.h"

namespace ImGui
{
Waveform_vulkan::Waveform_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Waveform");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;
    if (compile_spirv_module(Waveform_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->set_optimal_local_size_xyz(8, 8, 1);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    if (compile_spirv_module(Zero_data, opt, spirv_data) == 0)
    {
        pipe_zero = new Pipeline(vkdev);
        pipe_zero->set_optimal_local_size_xyz(8, 8, 1);
        pipe_zero->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    if (compile_spirv_module(ConvInt2Mat_data, opt, spirv_data) == 0)
    {
        pipe_conv = new Pipeline(vkdev);
        pipe_conv->set_optimal_local_size_xyz(8, 8, 1);
        pipe_conv->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    cmd->reset();
}

Waveform_vulkan::~Waveform_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (pipe_zero) { delete pipe_zero; pipe_zero = nullptr; }
        if (pipe_conv) { delete pipe_conv; pipe_conv = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Waveform_vulkan::upload_param(const ImGui::VkMat& src, ImGui::VkMat& dst, float fintensity, bool separate, bool show_y)
{
    ImGui::VkMat dst_gpu_int32;
    dst_gpu_int32.create_type(dst.w, dst.h, dst.c, IM_DT_INT32, opt.blob_vkallocator);
    
    std::vector<VkMat> zero_bindings(1);
    zero_bindings[0] = dst_gpu_int32;
    std::vector<vk_constant_type> zero_constants(3);
    zero_constants[0].i = dst_gpu_int32.w;
    zero_constants[1].i = dst_gpu_int32.h;
    zero_constants[2].i = dst_gpu_int32.c;
    cmd->record_pipeline(pipe_zero, zero_bindings, zero_constants, dst_gpu_int32);
    
    std::vector<VkMat> bindings(5);
    if      (src.type == IM_DT_INT8)     bindings[0] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    bindings[1] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[2] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[3] = src;
    bindings[4] = dst_gpu_int32;
    std::vector<vk_constant_type> constants(12);
    constants[0].i = src.w;
    constants[1].i = src.h;
    constants[2].i = src.c;
    constants[3].i = src.color_format;
    constants[4].i = src.type;
    constants[5].i = dst_gpu_int32.w;
    constants[6].i = dst_gpu_int32.h;
    constants[7].i = dst_gpu_int32.c;
    constants[8].i = dst_gpu_int32.color_format;
    constants[9].i = dst_gpu_int32.type;
    constants[10].i = separate ? 1 : 0;
    constants[11].i = show_y ? 1 : 0;
    cmd->record_pipeline(pipe, bindings, constants, src);

    std::vector<VkMat> conv_bindings(2);
    conv_bindings[0] = dst_gpu_int32;
    conv_bindings[1] = dst;

    std::vector<vk_constant_type> conv_constants(8);
    conv_constants[0].i = dst_gpu_int32.w;
    conv_constants[1].i = dst_gpu_int32.h;
    conv_constants[2].i = dst_gpu_int32.c;
    conv_constants[3].i = dst.c;
    conv_constants[4].i = dst.color_format;
    conv_constants[5].f = fintensity;
    conv_constants[6].i = separate ? 1 : 0;
    conv_constants[7].i = show_y ? 1 : 0;
    cmd->record_pipeline(pipe_conv, conv_bindings, conv_constants, dst);
}

double Waveform_vulkan::scope(const ImGui::ImMat& src, ImGui::ImMat& dst, int level, float fintensity, bool separate, bool show_y)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !pipe_zero || !pipe_conv || !cmd)
    {
        return ret;
    }
    VkMat dst_gpu;
    dst_gpu.create_type(src.w, level, 4, IM_DT_INT8, opt.blob_vkallocator);

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

    upload_param(src_gpu, dst_gpu, fintensity, separate, show_y);

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
    //cmd->benchmark_print();
#endif
    cmd->reset();
    dst.copy_attribute(src);
    dst.flags |= IM_MAT_FLAGS_CUSTOM_UPDATED;
    return ret;
}
} // namespace ImGui
