#include "Harris_vulkan.h"
#include "Filter2DS_shader.h"
#include "Harris_shader.h"
#include "ImVulkanShader.h"

namespace ImGui
{
Harris_vulkan::Harris_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Harris");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(PrewittFilter_data, opt, spirv_data) == 0)
    {
        pipe_prewitt = new Pipeline(vkdev);
        pipe_prewitt->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(NMSFilter_data, opt, spirv_data) == 0)
    {
        pipe_nms = new Pipeline(vkdev);
        pipe_nms->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(HarrisFilter_data, opt, spirv_data) == 0)
    {
        pipe_harris = new Pipeline(vkdev);
        pipe_harris->create(spirv_data.data(), spirv_data.size() * 4, specializations);
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

Harris_vulkan::~Harris_vulkan()
{
    if (vkdev)
    {
        if (pipe_harris) { delete pipe_harris; pipe_harris = nullptr; }
        if (pipe_prewitt) { delete pipe_prewitt; pipe_prewitt = nullptr; }
        if (pipe_nms) { delete pipe_nms; pipe_nms = nullptr; }
        if (pipe_column) { delete pipe_column; pipe_column = nullptr; }
        if (pipe_row) { delete pipe_row; pipe_row = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Harris_vulkan::prepare_kernel()
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

void Harris_vulkan::upload_param(const VkMat& src, VkMat& dst, int _blurRadius, float edgeStrength, float threshold, float harris, float sensitivity)
{
    if (blurRadius != _blurRadius)
    {
        blurRadius = _blurRadius;
        prepare_kernel();
    }
    VkMat vk_prewitt;
    vk_prewitt.create_type(dst.w, dst.h, 4, IM_DT_FLOAT16, opt.blob_vkallocator);

    std::vector<VkMat> prewitt_bindings(8);
    if      (vk_prewitt.type == IM_DT_INT8)     prewitt_bindings[0] = vk_prewitt;
    else if (vk_prewitt.type == IM_DT_INT16 || vk_prewitt.type == IM_DT_INT16_BE)    prewitt_bindings[1] = vk_prewitt;
    else if (vk_prewitt.type == IM_DT_FLOAT16)  prewitt_bindings[2] = vk_prewitt;
    else if (vk_prewitt.type == IM_DT_FLOAT32)  prewitt_bindings[3] = vk_prewitt;

    if      (src.type == IM_DT_INT8)     prewitt_bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    prewitt_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  prewitt_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  prewitt_bindings[7] = src;
    std::vector<vk_constant_type> prewitt_constants(11);
    prewitt_constants[0].i = src.w;
    prewitt_constants[1].i = src.h;
    prewitt_constants[2].i = src.c;
    prewitt_constants[3].i = src.color_format;
    prewitt_constants[4].i = src.type;
    prewitt_constants[5].i = vk_prewitt.w;
    prewitt_constants[6].i = vk_prewitt.h;
    prewitt_constants[7].i = vk_prewitt.c;
    prewitt_constants[8].i = vk_prewitt.color_format;
    prewitt_constants[9].i = vk_prewitt.type;
    prewitt_constants[10].f = edgeStrength;
    cmd->record_pipeline(pipe_prewitt, prewitt_bindings, prewitt_constants, vk_prewitt);

    VkMat vk_column;
    vk_column.create_like(vk_prewitt, opt.blob_vkallocator);
    std::vector<VkMat> column_bindings(9);
    if      (vk_column.type == IM_DT_INT8)     column_bindings[0] = vk_column;
    else if (vk_column.type == IM_DT_INT16 || vk_column.type == IM_DT_INT16_BE)    column_bindings[1] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT16)  column_bindings[2] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT32)  column_bindings[3] = vk_column;

    if      (vk_prewitt.type == IM_DT_INT8)     column_bindings[4] = vk_prewitt;
    else if (vk_prewitt.type == IM_DT_INT16 || vk_prewitt.type == IM_DT_INT16_BE)    column_bindings[5] = vk_prewitt;
    else if (vk_prewitt.type == IM_DT_FLOAT16)  column_bindings[6] = vk_prewitt;
    else if (vk_prewitt.type == IM_DT_FLOAT32)  column_bindings[7] = vk_prewitt;
    column_bindings[8] = vk_kernel;
    std::vector<vk_constant_type> column_constants(14);
    column_constants[0].i = vk_prewitt.w;
    column_constants[1].i = vk_prewitt.h;
    column_constants[2].i = vk_prewitt.c;
    column_constants[3].i = vk_prewitt.color_format;
    column_constants[4].i = vk_prewitt.type;
    column_constants[5].i = vk_column.w;
    column_constants[6].i = vk_column.h;
    column_constants[7].i = vk_column.c;
    column_constants[8].i = vk_column.color_format;
    column_constants[9].i = vk_column.type;
    column_constants[10].i = xksize;
    column_constants[11].i = yksize;
    column_constants[12].i = xanchor;
    column_constants[13].i = yanchor;
    cmd->record_pipeline(pipe_column, column_bindings, column_constants, vk_column);

    VkMat vk_blur;
    vk_blur.create_like(vk_prewitt, opt.blob_vkallocator);

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
    cmd->record_pipeline(pipe_row, row_bindings, row_constants, vk_blur);

    VkMat vk_harris;
    vk_harris.create_type(dst.w, dst.h, vk_harris.type, opt.blob_vkallocator);
    std::vector<VkMat> harris_bindings(8);
    if      (vk_harris.type == IM_DT_INT8)     harris_bindings[0] = vk_harris;
    else if (vk_harris.type == IM_DT_INT16 || vk_harris.type == IM_DT_INT16_BE)    harris_bindings[1] = vk_harris;
    else if (vk_harris.type == IM_DT_FLOAT16)  harris_bindings[2] = vk_harris;
    else if (vk_harris.type == IM_DT_FLOAT32)  harris_bindings[3] = vk_harris;

    if      (vk_blur.type == IM_DT_INT8)     harris_bindings[4] = vk_blur;
    else if (vk_blur.type == IM_DT_INT16 || vk_blur.type == IM_DT_INT16_BE)    harris_bindings[5] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT16)  harris_bindings[6] = vk_blur;
    else if (vk_blur.type == IM_DT_FLOAT32)  harris_bindings[7] = vk_blur;

    std::vector<vk_constant_type> harris_constants(12);
    harris_constants[0].i = vk_blur.w;
    harris_constants[1].i = vk_blur.h;
    harris_constants[2].i = vk_blur.c;
    harris_constants[3].i = vk_blur.color_format;
    harris_constants[4].i = vk_blur.type;
    harris_constants[5].i = vk_harris.w;
    harris_constants[6].i = vk_harris.h;
    harris_constants[7].i = vk_harris.c;
    harris_constants[8].i = vk_harris.color_format;
    harris_constants[9].i = vk_harris.type;
    harris_constants[10].f = harris;
    harris_constants[11].f = sensitivity;
    cmd->record_pipeline(pipe_harris, harris_bindings, harris_constants, vk_harris);

    std::vector<VkMat> nms_bindings(12);
    if      (dst.type == IM_DT_INT8)     nms_bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    nms_bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  nms_bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  nms_bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     nms_bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    nms_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  nms_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  nms_bindings[7] = src;

    if      (vk_harris.type == IM_DT_INT8)     nms_bindings[8] = vk_harris;
    else if (vk_harris.type == IM_DT_INT16 || vk_harris.type == IM_DT_INT16_BE)    nms_bindings[9] = vk_harris;
    else if (vk_harris.type == IM_DT_FLOAT16)  nms_bindings[10] = vk_harris;
    else if (vk_harris.type == IM_DT_FLOAT32)  nms_bindings[11] = vk_harris;

    std::vector<vk_constant_type> nms_constants(16);
    nms_constants[0].i = src.w;
    nms_constants[1].i = src.h;
    nms_constants[2].i = src.c;
    nms_constants[3].i = src.color_format;
    nms_constants[4].i = src.type;
    nms_constants[5].i = vk_harris.w;
    nms_constants[6].i = vk_harris.h;
    nms_constants[7].i = vk_harris.c;
    nms_constants[8].i = vk_harris.color_format;
    nms_constants[9].i = vk_harris.type;
    nms_constants[10].i = dst.w;
    nms_constants[11].i = dst.h;
    nms_constants[12].i = dst.c;
    nms_constants[13].i = dst.color_format;
    nms_constants[14].i = dst.type;
    nms_constants[15].f = threshold;
    cmd->record_pipeline(pipe_nms, nms_bindings, nms_constants, dst);
}

double Harris_vulkan::scope(const ImMat& src, ImMat& dst, int _blurRadius, float edgeStrength, float threshold, float harris, float sensitivity)
{
    double ret = 0.0;
    if (!vkdev || !pipe_harris || !cmd)
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

    upload_param(src_gpu, dst_gpu, _blurRadius, edgeStrength, threshold, harris, sensitivity);

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
    dst.flags |= IM_MAT_FLAGS_CUSTOM_UPDATED;
    return ret;
}
} // namespace ImGui
