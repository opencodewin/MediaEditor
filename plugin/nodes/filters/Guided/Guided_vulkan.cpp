#include "Guided_vulkan.h"
#include "Guided_shader.h"
#include "ImVulkanShader.h"

namespace ImGui
{
Guided_vulkan::Guided_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Guided");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(Guided_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(ToMatting_data, opt, spirv_data) == 0)
    {
        pipe_to_matting = new Pipeline(vkdev);
        pipe_to_matting->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(Matting_data, opt, spirv_data) == 0)
    {
        pipe_matting = new Pipeline(vkdev);
        pipe_matting->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    box1 = new BoxBlur_vulkan(gpu);
    box2 = new BoxBlur_vulkan(gpu);
    box3 = new BoxBlur_vulkan(gpu);
    box4 = new BoxBlur_vulkan(gpu);
    box5 = new BoxBlur_vulkan(gpu);

    cmd->reset();
}

Guided_vulkan::~Guided_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (box1) { delete box1; box1 = nullptr; }
        if (box2) { delete box2; box2 = nullptr; }
        if (box3) { delete box3; box3 = nullptr; }
        if (box4) { delete box4; box4 = nullptr; }
        if (box5) { delete box5; box5 = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Guided_vulkan::upload_param(const VkMat& src, VkMat& dst, int r, float eps)
{
    VkMat vk_box1, vk_box2, vk_box3, vk_box4, vk_box5;
    vk_box1.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_box2.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_box3.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_box4.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_box5.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);

    box1->SetParam(r, r);
    box1->filter(src, vk_box1);

    VkMat vk_tex2, vk_tex3, vk_tex4, vk_tex5;
    vk_tex2.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_tex3.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_tex4.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    vk_tex5.create_type(src.w, src.h, src.c, IM_DT_FLOAT16, opt.blob_vkallocator);
    std::vector<VkMat> to_matting_bindings(7);
    if      (src.type == IM_DT_INT8)     to_matting_bindings[0] = src;
    else if (src.type == IM_DT_INT16)    to_matting_bindings[1] = src;
    else if (src.type == IM_DT_FLOAT16)  to_matting_bindings[2] = src;
    else if (src.type == IM_DT_FLOAT32)  to_matting_bindings[3] = src;
    to_matting_bindings[4] = vk_tex2;
    to_matting_bindings[5] = vk_tex3;
    to_matting_bindings[6] = vk_tex4;
    std::vector<vk_constant_type> to_matting_constants(10);
    to_matting_constants[0].i = src.w;
    to_matting_constants[1].i = src.h;
    to_matting_constants[2].i = src.c;
    to_matting_constants[3].i = src.color_format;
    to_matting_constants[4].i = src.type;
    to_matting_constants[5].i = vk_tex2.w;
    to_matting_constants[6].i = vk_tex2.h;
    to_matting_constants[7].i = vk_tex2.c;
    to_matting_constants[8].i = vk_tex2.color_format;
    to_matting_constants[9].i = vk_tex2.type;
    cmd->record_pipeline(pipe_to_matting, to_matting_bindings, to_matting_constants, vk_tex2);

    box2->SetParam(r, r);
    box2->filter(vk_tex2, vk_box2);

    box3->SetParam(r, r);
    box3->filter(vk_tex3, vk_box3);

    box4->SetParam(r, r);
    box4->filter(vk_tex4, vk_box4);

    std::vector<VkMat> bindings(5);
    bindings[0] = vk_box1;
    bindings[1] = vk_box2;
    bindings[2] = vk_box3;
    bindings[3] = vk_box4;
    bindings[4] = vk_tex5;
    std::vector<vk_constant_type> constants(11);
    constants[0].i = vk_box1.w;
    constants[1].i = vk_box1.h;
    constants[2].i = vk_box1.c;
    constants[3].i = vk_box1.color_format;
    constants[4].i = vk_box1.type;
    constants[5].i = vk_tex5.w;
    constants[6].i = vk_tex5.h;
    constants[7].i = vk_tex5.c;
    constants[8].i = vk_tex5.color_format;
    constants[9].i = vk_tex5.type;
    constants[10].f = eps;
    cmd->record_pipeline(pipe, bindings, constants, vk_tex5);

    box5->SetParam(r, r);
    box5->filter(vk_tex5, vk_box5);

    std::vector<VkMat> matting_bindings(9);
    if      (dst.type == IM_DT_INT8)     matting_bindings[0] = dst;
    else if (dst.type == IM_DT_INT16)    matting_bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  matting_bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  matting_bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     matting_bindings[4] = src;
    else if (src.type == IM_DT_INT16)    matting_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  matting_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  matting_bindings[7] = src;
    matting_bindings[8] = vk_box5;

    std::vector<vk_constant_type> matting_constants(15);
    matting_constants[0].i = src.w;
    matting_constants[1].i = src.h;
    matting_constants[2].i = src.c;
    matting_constants[3].i = src.color_format;
    matting_constants[4].i = src.type;
    matting_constants[5].i = vk_box5.w;
    matting_constants[6].i = vk_box5.h;
    matting_constants[7].i = vk_box5.c;
    matting_constants[8].i = vk_box5.color_format;
    matting_constants[9].i = vk_box5.type;
    matting_constants[10].i = dst.w;
    matting_constants[11].i = dst.h;
    matting_constants[12].i = dst.c;
    matting_constants[13].i = dst.color_format;
    matting_constants[14].i = dst.type;
    cmd->record_pipeline(pipe_matting, matting_bindings, matting_constants, dst);
}

double Guided_vulkan::filter(const ImMat& src, ImMat& dst, int r, float eps)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(src.w, src.h, 4, dst.type, opt.blob_vkallocator);

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

    upload_param(src_gpu, dst_gpu, r, eps);

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
} // namespace ImGui
