#include "Filter2DS_vulkan.h"
#include "Filter2DS_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
Filter2DS_vulkan::Filter2DS_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "Filter2DS");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

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
    
    if (compile_spirv_module(FilterColumnMono_data, opt, spirv_data) == 0)
    {
        pipe_column_mono = new Pipeline(vkdev);
        pipe_column_mono->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(FilterRowMono_data, opt, spirv_data) == 0)
    {
        pipe_row_mono = new Pipeline(vkdev);
        pipe_row_mono->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }
    cmd->reset();
}

Filter2DS_vulkan::~Filter2DS_vulkan()
{
    if (vkdev)
    {
        if (pipe_column) { delete pipe_column; pipe_column = nullptr; }
        if (pipe_row) { delete pipe_row; pipe_row = nullptr; }
        if (pipe_column_mono) { delete pipe_column_mono; pipe_column_mono = nullptr; }
        if (pipe_row_mono) { delete pipe_row_mono; pipe_row_mono = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void Filter2DS_vulkan::upload_param(const VkMat& src, VkMat& dst) const
{
    std::vector<vk_constant_type> constants(14);
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
    constants[10].i = xksize;
    constants[11].i = yksize;
    constants[12].i = xanchor;
    constants[13].i = yanchor;

    VkMat vk_column;
    vk_column.create_like(dst, opt.blob_vkallocator);

    std::vector<VkMat> column_bindings(9);
    if      (dst.type == IM_DT_INT8)     column_bindings[0] = vk_column;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    column_bindings[1] = vk_column;
    else if (dst.type == IM_DT_FLOAT16)  column_bindings[2] = vk_column;
    else if (dst.type == IM_DT_FLOAT32)  column_bindings[3] = vk_column;

    if      (src.type == IM_DT_INT8)      column_bindings[4] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)     column_bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)   column_bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)   column_bindings[7] = src;
    column_bindings[8] = vk_kernel;
    if (src.c == 1)
        cmd->record_pipeline(pipe_column_mono, column_bindings, constants, vk_column);
    else
        cmd->record_pipeline(pipe_column, column_bindings, constants, vk_column);

    constants[0].i = vk_column.w;
    constants[1].i = vk_column.h;
    constants[2].i = vk_column.c;
    constants[3].i = vk_column.color_format;
    constants[4].i = vk_column.type;

    std::vector<VkMat> row_bindings(9);
    if      (dst.type == IM_DT_INT8)     row_bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    row_bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  row_bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  row_bindings[3] = dst;

    if      (vk_column.type == IM_DT_INT8)      row_bindings[4] = vk_column;
    else if (vk_column.type == IM_DT_INT16)     row_bindings[5] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT16)   row_bindings[6] = vk_column;
    else if (vk_column.type == IM_DT_FLOAT32)   row_bindings[7] = vk_column;
    row_bindings[8] = vk_kernel;
    if (src.c == 1)
        cmd->record_pipeline(pipe_row_mono, row_bindings, constants, dst);
    else
        cmd->record_pipeline(pipe_row, row_bindings, constants, dst);
}

double Filter2DS_vulkan::filter(const ImMat& src, ImMat& dst) const
{
    double ret = 0.0;
    if (!vkdev || !pipe_column || !pipe_row || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_like(src, opt.blob_vkallocator);

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

    upload_param(src_gpu, dst_gpu);

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
