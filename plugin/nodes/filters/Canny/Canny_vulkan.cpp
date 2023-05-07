#include "Canny_vulkan.h"
#include "Filter2DS_shader.h"
#include "Canny_shader.h"
#include "ImVulkanShader.h"

namespace ImGui
{
Canny_vulkan::Canny_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Canny");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(DSobelFilter_data, opt, spirv_data) == 0)
    {
        pipe_dsobel = new Pipeline(vkdev);
        pipe_dsobel->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(NMSFilter_data, opt, spirv_data) == 0)
    {
        pipe_nms = new Pipeline(vkdev);
        pipe_nms->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(CannyFilter_data, opt, spirv_data) == 0)
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

Canny_vulkan::~Canny_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (pipe_dsobel) { delete pipe_dsobel; pipe_dsobel = nullptr; }
        if (pipe_nms) { delete pipe_nms; pipe_nms = nullptr; }
        if (pipe_column) { delete pipe_column; pipe_column = nullptr; }
        if (pipe_row) { delete pipe_row; pipe_row = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Canny_vulkan::prepare_kernel()
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

void Canny_vulkan::upload_param(const VkMat& src, VkMat& dst, int _blurRadius, float minThreshold, float maxThreshold)
{
    if (blurRadius != _blurRadius)
    {
        blurRadius = _blurRadius;
        prepare_kernel();
    }

    // need float16/float32 as middle buffer only
    VkMat vk_bobel;
    vk_bobel.create_type(dst.w, dst.h, 3, IM_DT_FLOAT16, opt.blob_vkallocator);
    VkMat vk_nms;
    vk_nms.create_type(dst.w, dst.h, IM_DT_FLOAT16, opt.blob_vkallocator);
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
    else if (src.type == IM_DT_INT16)    column_bindings[5] = src;
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
    else if (vk_blur.type == IM_DT_INT16)    row_bindings[1] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT16)  row_bindings[2] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT32)  row_bindings[3] = vk_blur;

    if      (vk_column.type == IM_DT_INT8)     row_bindings[4] = vk_column;
    else if (vk_column.type == IM_DT_INT16)    row_bindings[5] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT16)  row_bindings[6] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT32)  row_bindings[7] = vk_column;
    row_bindings[8] = vk_kernel;
    cmd->record_pipeline(pipe_row, row_bindings, row_constants, vk_blur);

    std::vector<VkMat> sobel_bindings(8);
    if      (vk_bobel.type == IM_DT_INT8)     sobel_bindings[0] = vk_bobel;
    else if (vk_bobel.type == IM_DT_INT16)    sobel_bindings[1] = vk_bobel;
    else if (vk_bobel.type == IM_DT_FLOAT16)  sobel_bindings[2] = vk_bobel;
    else if (vk_bobel.type == IM_DT_FLOAT32)  sobel_bindings[3] = vk_bobel;

    if      (vk_blur.type == IM_DT_INT8)     sobel_bindings[4] = vk_blur;
    else if (vk_blur.type == IM_DT_INT16)    sobel_bindings[5] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT16)  sobel_bindings[6] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT32)  sobel_bindings[7] = vk_blur;

    std::vector<vk_constant_type> sobel_constants(11);
    sobel_constants[0].i = vk_blur.w;
    sobel_constants[1].i = vk_blur.h;
    sobel_constants[2].i = vk_blur.c;
    sobel_constants[3].i = vk_blur.color_format;
    sobel_constants[4].i = vk_blur.type;
    sobel_constants[5].i = vk_bobel.w;
    sobel_constants[6].i = vk_bobel.h;
    sobel_constants[7].i = vk_bobel.c;
    sobel_constants[8].i = vk_bobel.color_format;
    sobel_constants[9].i = vk_bobel.type;
    sobel_constants[10].f = 1.f;
    cmd->record_pipeline(pipe_dsobel, sobel_bindings, sobel_constants, vk_bobel);

    std::vector<VkMat> nms_bindings(8);
    if      (vk_nms.type == IM_DT_INT8)     nms_bindings[0] = vk_nms;
    else if (vk_nms.type == IM_DT_INT16)    nms_bindings[1] = vk_nms;
    else if (vk_nms.type == IM_DT_FLOAT16)  nms_bindings[2] = vk_nms;
    else if (vk_nms.type == IM_DT_FLOAT32)  nms_bindings[3] = vk_nms;

    if      (vk_bobel.type == IM_DT_INT8)     nms_bindings[4] = vk_bobel;
    else if (vk_bobel.type == IM_DT_INT16)    nms_bindings[5] = vk_bobel;
    else if (vk_bobel.type == IM_DT_FLOAT16)  nms_bindings[6] = vk_bobel;
    else if (vk_bobel.type == IM_DT_FLOAT32)  nms_bindings[7] = vk_bobel;
    std::vector<vk_constant_type> nms_constants(12);
    nms_constants[0].i = vk_bobel.w;
    nms_constants[1].i = vk_bobel.h;
    nms_constants[2].i = vk_bobel.c;
    nms_constants[3].i = vk_bobel.color_format;
    nms_constants[4].i = vk_bobel.type;
    nms_constants[5].i = vk_nms.w;
    nms_constants[6].i = vk_nms.h;
    nms_constants[7].i = vk_nms.c;
    nms_constants[8].i = vk_nms.color_format;
    nms_constants[9].i = vk_nms.type;
    nms_constants[10].f = minThreshold;
    nms_constants[11].f = maxThreshold;
    cmd->record_pipeline(pipe_nms, nms_bindings, nms_constants, vk_nms);

    std::vector<VkMat> canny_bindings(12);
    if      (dst.type == IM_DT_INT8)     canny_bindings[0] = dst;
    else if (dst.type == IM_DT_INT16)    canny_bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  canny_bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  canny_bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     canny_bindings[4] = src;
    else if (src.type == IM_DT_INT16)    canny_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  canny_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  canny_bindings[7] = src;

    if      (vk_nms.type == IM_DT_INT8)     canny_bindings[8] = vk_nms;
    else if (vk_nms.type == IM_DT_INT16)    canny_bindings[9] = vk_nms;
    else if (vk_nms.type == IM_DT_FLOAT16)  canny_bindings[10] = vk_nms;
    else if (vk_nms.type == IM_DT_FLOAT32)  canny_bindings[11] = vk_nms;

    std::vector<vk_constant_type> canny_constants(15);
    canny_constants[0].i = src.w;
    canny_constants[1].i = src.h;
    canny_constants[2].i = src.c;
    canny_constants[3].i = src.color_format;
    canny_constants[4].i = src.type;
    canny_constants[5].i = dst.w;
    canny_constants[6].i = dst.h;
    canny_constants[7].i = dst.c;
    canny_constants[8].i = dst.color_format;
    canny_constants[9].i = dst.type;
    canny_constants[10].i = vk_nms.w;
    canny_constants[11].i = vk_nms.h;
    canny_constants[12].i = vk_nms.c;
    canny_constants[13].i = vk_nms.color_format;
    canny_constants[14].i = vk_nms.type;
    cmd->record_pipeline(pipe, canny_bindings, canny_constants, dst);
}

double Canny_vulkan::filter(const ImMat& src, ImMat& dst, int _blurRadius, float minThreshold, float maxThreshold)
{
    double ret = 0.0;
    if (!vkdev || !pipe || !pipe_dsobel || !pipe_nms || !pipe_column || !pipe_row || !cmd)
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

    upload_param(src_gpu, dst_gpu, _blurRadius, minThreshold, maxThreshold);

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
