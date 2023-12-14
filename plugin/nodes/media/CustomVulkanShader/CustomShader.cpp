#include "ImVulkanShader.h"
#include "CustomShader.h"

CustomShader::CustomShader(const std::string shader, int gpu, bool fp16)
{
    vkdev = ImGui::get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
    opt.use_image_storage = false;
    opt.use_fp16_arithmetic = fp16;
    opt.use_fp16_storage = fp16;
    cmd = new ImGui::VkCompute(vkdev, "Shader Editor");
    std::vector<ImGui::vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (ImGui::compile_spirv_module(shader.data(), opt, spirv_data) == 0)
    {
        pipe = new ImGui::Pipeline(vkdev);
        pipe->set_optimal_local_size_xyz(16, 16, 1);
        if (pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations) != 0)
        {
            delete pipe;
            pipe = nullptr;
        }
    }

    cmd->reset();
}

CustomShader::~CustomShader()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void CustomShader::upload_param(const ImGui::VkMat& src, ImGui::VkMat& dst, std::vector<float>& params)
{
    std::vector<ImGui::VkMat> bindings(8);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     bindings[4] = src;
    else if (src.type == IM_DT_INT16)    bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[7] = src;

    std::vector<ImGui::vk_constant_type> constants(10 + params.size());
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
    for (int i = 10; i < constants.size(); i++)
        constants[i].f = params[i - 10];
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double CustomShader::filter(const ImGui::ImMat& src, ImGui::ImMat& dst, std::vector<float>& params)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }
    ImGui::VkMat dst_gpu;
    int width = dst.w;
    int height = dst.h;
    if (width == 0 || height == 0)
    {
        width = src.w;
        height = src.h;
    }
    dst_gpu.create_type(width, height, 4, dst.type, opt.blob_vkallocator);

    ImGui::VkMat src_gpu;
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

    upload_param(src_gpu, dst_gpu, params);

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
