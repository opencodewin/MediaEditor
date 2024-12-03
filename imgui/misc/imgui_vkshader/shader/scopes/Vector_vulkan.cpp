#include "Vector_vulkan.h"
#include "Vector_shader.h"
#include "ImVulkanShader.h"

namespace ImGui
{
Vector_vulkan::Vector_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Vector");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;
    if (compile_spirv_module(Vector_data, opt, spirv_data) == 0)
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

    if (ImGui::compile_spirv_module(Vector_merge_data, opt, spirv_data) == 0)
    {
        pipe_merge = new ImGui::Pipeline(vkdev);
        pipe_merge->set_optimal_local_size_xyz(8, 8, 1);
        pipe_merge->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    cmd->reset();
}

Vector_vulkan::~Vector_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (pipe_zero) { delete pipe_zero; pipe_zero = nullptr; }
        if (pipe_merge) { delete pipe_merge; pipe_merge = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Vector_vulkan::upload_param(const ImGui::VkMat& src, ImGui::VkMat& dst, float intensity)
{
    ImGui::VkMat buffer_gpu;
    buffer_gpu.create_type(size, size, 1, IM_DT_INT32, opt.blob_vkallocator);

    std::vector<VkMat> zero_bindings(1);
    zero_bindings[0] = buffer_gpu;
    std::vector<vk_constant_type> zero_constants(3);
    zero_constants[0].i = buffer_gpu.w;
    zero_constants[1].i = buffer_gpu.h;
    zero_constants[2].i = buffer_gpu.c;
    cmd->record_pipeline(pipe_zero, zero_bindings, zero_constants, buffer_gpu);
    
    std::vector<ImGui::VkMat> bindings(5);
    if      (src.type == IM_DT_INT8)     bindings[0] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    bindings[1] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[2] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[3] = src;
    bindings[4] = buffer_gpu;
    std::vector<ImGui::vk_constant_type> constants(8);
    constants[0].i = src.w;
    constants[1].i = src.h;
    constants[2].i = src.c;
    constants[3].i = src.color_format;
    constants[4].i = src.type;
    constants[5].i = buffer_gpu.w;
    constants[6].i = buffer_gpu.h;
    constants[7].i = buffer_gpu.c;
    cmd->record_pipeline(pipe, bindings, constants, src);

    std::vector<ImGui::VkMat> bindings_merge(5);
    if      (dst.type == IM_DT_INT8)     bindings_merge[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings_merge[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings_merge[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings_merge[3] = dst;

    bindings_merge[4] = buffer_gpu;
    std::vector<ImGui::vk_constant_type> constants_merge(11);
    constants_merge[0].i = buffer_gpu.w;
    constants_merge[1].i = buffer_gpu.h;
    constants_merge[2].i = buffer_gpu.c;
    constants_merge[3].i = buffer_gpu.color_format;
    constants_merge[4].i = buffer_gpu.type;
    constants_merge[5].i = dst.w;
    constants_merge[6].i = dst.h;
    constants_merge[7].i = dst.c;
    constants_merge[8].i = dst.color_format;
    constants_merge[9].i = dst.type;
    constants_merge[10].f = intensity;
    cmd->record_pipeline(pipe_merge, bindings_merge, constants_merge, dst);
}

double Vector_vulkan::scope(const ImGui::ImMat& src, ImGui::ImMat& dst, float intensity)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !pipe_zero || !pipe_merge || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(size, size, 4, IM_DT_INT8, opt.blob_vkallocator);

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

    upload_param(src_gpu, dst_gpu, intensity);

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
