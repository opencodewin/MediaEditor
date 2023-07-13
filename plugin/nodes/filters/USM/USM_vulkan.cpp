#include "USM_vulkan.h"
#include "Filter2DS_shader.h"
#include "USM_shader.h"
#include "ImVulkanShader.h"

namespace ImGui
{
USM_vulkan::USM_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "USM");
    
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(USMFilter_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(FilterColumn_data, opt, spirv_data) == 0)
    {
        pipe_column = new Pipeline(vkdev);
        pipe_column->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(FilterRow_data, opt, spirv_data) == 0)
    {
        pipe_row = new Pipeline(vkdev);
        pipe_row->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    cmd->reset();

    prepare_kernel();
}

USM_vulkan::~USM_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (pipe_column) { delete pipe_column; pipe_column = nullptr; }
        if (pipe_row) { delete pipe_row; pipe_row = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void USM_vulkan::prepare_kernel()
{
    int ksize = blurRadius * 2 + 1;
    if (sigma <= 0.0f) 
    {
        sigma = ((ksize - 1) * 0.5 - 1) * 0.3 + 0.8;
    }
    double scale = 1.0f / (sigma * sigma * 2.0);
    double sum = 0.0;

    kernel.create(ksize, size_t(4u), 1);
    for (int i = 0; i < ksize; i++) 
    {
        int x = i - (ksize - 1) / 2;
        kernel.at<float>(i) = exp(-scale * (x * x));
        sum += kernel.at<float>(i);
    }

    sum = 1.0 / sum;
    kernel *= (float)(sum);
    VkTransfer tran(vkdev);
    tran.record_upload(kernel, vk_kernel, opt, false);
    tran.submit_and_wait();

    xksize = yksize = ksize;
    xanchor = yanchor = blurRadius;
}

void USM_vulkan::upload_param(const VkMat& src, VkMat& dst, float _sigma, float amount, float threshold)
{
    if (sigma != _sigma)
    {
        sigma = _sigma;
        prepare_kernel();
    }

    VkMat vk_column;
    vk_column.create_type(dst.w, dst.h, dst.c, IM_DT_FLOAT16, opt.blob_vkallocator);

    VkMat vk_blur;
    vk_blur.create_type(dst.w, dst.h, dst.c, IM_DT_FLOAT16, opt.blob_vkallocator);

    std::vector<vk_constant_type> column_constants(14);
    column_constants[0].i = src.w;
    column_constants[1].i = src.h;
    column_constants[2].i = src.c;
    column_constants[3].i = src.color_format;
    column_constants[4].i = src.type;
    column_constants[5].i = vk_column.w;
    column_constants[6].i = vk_column.h;
    column_constants[7].i = vk_column.c;
    column_constants[8].i = vk_column.color_format;
    column_constants[9].i = vk_column.type;
    column_constants[10].i = xksize;
    column_constants[11].i = yksize;
    column_constants[12].i = xanchor;
    column_constants[13].i = yanchor;

    std::vector<VkMat> column_bindings(9);
    if      (vk_column.type == IM_DT_INT8)     column_bindings[0] = vk_column;
    else if (vk_column.type == IM_DT_INT16)    column_bindings[1] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT16)  column_bindings[2] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT32)  column_bindings[3] = vk_column;

    if      (src.type == IM_DT_INT8)     column_bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    column_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  column_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  column_bindings[7] = src;
    column_bindings[8] = vk_kernel;
    cmd->record_pipeline(pipe_column, column_bindings, column_constants, vk_column);

    std::vector<vk_constant_type> row_constants(14);
    row_constants[0].i = vk_column.w;
    row_constants[1].i = vk_column.h;
    row_constants[2].i = vk_column.c;
    row_constants[3].i = vk_column.color_format;
    row_constants[4].i = vk_column.type;
    row_constants[5].i = vk_blur.w;
    row_constants[6].i = vk_blur.h;
    row_constants[7].i = vk_blur.c;
    row_constants[8].i = vk_blur.color_format;
    row_constants[9].i = vk_blur.type;
    row_constants[10].i = xksize;
    row_constants[11].i = yksize;
    row_constants[12].i = xanchor;
    row_constants[13].i = yanchor;
    std::vector<VkMat> row_bindings(9);
    if      (vk_blur.type == IM_DT_INT8)     row_bindings[0] = vk_blur;
    else if (vk_blur.type == IM_DT_INT16 || vk_blur.type == IM_DT_INT16_BE)    row_bindings[1] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT16)  row_bindings[2] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT32)  row_bindings[3] = vk_blur;

    if      (vk_column.type == IM_DT_INT8)     row_bindings[4] = vk_column;
    else if (vk_column.type == IM_DT_INT16 || vk_column.type == IM_DT_INT16_BE)    row_bindings[5] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT16)  row_bindings[6] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT32)  row_bindings[7] = vk_column;
    row_bindings[8] = vk_kernel;
    cmd->record_pipeline(pipe_row, row_bindings, row_constants, vk_blur);

    std::vector<VkMat> usm_bindings(12);
    if      (dst.type == IM_DT_INT8)     usm_bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    usm_bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  usm_bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  usm_bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     usm_bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    usm_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  usm_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  usm_bindings[7] = src;

    if      (vk_blur.type == IM_DT_INT8)     usm_bindings[8] = vk_blur;
    else if (vk_blur.type == IM_DT_INT16 || vk_blur.type == IM_DT_INT16_BE)    usm_bindings[9] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT16)  usm_bindings[10] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT32)  usm_bindings[11] = vk_blur;

    std::vector<vk_constant_type> usm_constants(17);
    usm_constants[0].i = src.w;
    usm_constants[1].i = src.h;
    usm_constants[2].i = src.c;
    usm_constants[3].i = src.color_format;
    usm_constants[4].i = src.type;
    usm_constants[5].i = vk_blur.w;
    usm_constants[6].i = vk_blur.h;
    usm_constants[7].i = vk_blur.c;
    usm_constants[8].i = vk_blur.color_format;
    usm_constants[9].i = vk_blur.type;
    usm_constants[10].i = dst.w;
    usm_constants[11].i = dst.h;
    usm_constants[12].i = dst.c;
    usm_constants[13].i = dst.color_format;
    usm_constants[14].i = dst.type;
    usm_constants[15].f = amount;
    usm_constants[16].f = threshold;
    cmd->record_pipeline(pipe, usm_bindings, usm_constants, dst);
}

double USM_vulkan::filter(const ImMat& src, ImMat& dst, float sigma, float amount, float threshold)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !pipe_column || !pipe_row || !cmd)
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

    upload_param(src_gpu, dst_gpu, sigma, amount, threshold);

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
