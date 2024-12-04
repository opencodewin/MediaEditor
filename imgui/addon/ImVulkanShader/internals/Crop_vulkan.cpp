#include "Crop_vulkan.h"
#include "Crop_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
Crop_vulkan::Crop_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Crop");

    std::vector<vk_specialization_type> specializations(0);

    if (compile_spirv_module(CropShader_data, opt, spirv_data) == 0)
    {
        pipe_crop = new Pipeline(vkdev);
        pipe_crop->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(CropToShader_data, opt, spirv_data) == 0)
    {
        pipe_cropto = new Pipeline(vkdev);
        pipe_cropto->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    
    cmd->reset();
}

Crop_vulkan::~Crop_vulkan()
{
    if (vkdev)
    {
        if (pipe_crop) { delete pipe_crop; pipe_crop = nullptr; }
        if (pipe_cropto) { delete pipe_cropto; pipe_cropto = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Crop_vulkan::upload_param(const VkMat& src, VkMat& dst, int _x, int _y, int _w, int _h) const
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

    std::vector<vk_constant_type> constants(12);
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
    constants[10].i = _x;
    constants[11].i = _y;
    cmd->record_pipeline(pipe_crop, bindings, constants, dst);
}

double Crop_vulkan::crop(const ImMat& src, ImMat& dst, int _x, int _y, int _w, int _h) const
{
    double ret = 0.0;
    if (!vkdev || !pipe_crop || !cmd)
    {
        return ret;
    }
    VkMat dst_gpu;
    dst_gpu.create_type(_w, _h, 4, dst.type, opt.blob_vkallocator);

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

    upload_param(src_gpu, dst_gpu, _x, _y, _w, _h);

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

void Crop_vulkan::upload_param(const VkMat& src, VkMat& dst, int _x, int _y, int _w, int _h, int _dx, int _dy, bool fill) const
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

    std::vector<vk_constant_type> constants(17);
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
    constants[10].i = _x;
    constants[11].i = _y;
    constants[12].i = _w;
    constants[13].i = _h;
    constants[14].i = _dx;
    constants[15].i = _dy;
    constants[16].i = fill ? 1 : 0;
    cmd->record_pipeline(pipe_cropto, bindings, constants, dst);
}

double Crop_vulkan::cropto(const ImMat& src, ImMat& dst, int _x, int _y, int _w, int _h, int _dx, int _dy) const
{
    double ret = 0.0;
    bool fill = true;
    if (!vkdev || !pipe_cropto || !cmd)
    {
        return ret;
    }
    
    VkMat dst_gpu;
    if (dst.data)
    {
        if (dst.device == IM_DD_VULKAN)
            dst_gpu = dst;
        else if (dst.device == IM_DD_CPU)
            cmd->record_clone(dst, dst_gpu, opt);
        fill = false;
    }
    else
    {
        if (dst.w > 0 && dst.h > 0)
            dst_gpu.create_type(dst.w, dst.h, 4, dst.type, opt.blob_vkallocator);
        else
            dst_gpu.create_type(_w, _h, 4, dst.type, opt.blob_vkallocator);
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

    upload_param(src_gpu, dst_gpu, _x, _y, _w, _h, _dx, _dy, fill);

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
