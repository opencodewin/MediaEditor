#include "DeBand_vulkan.h"
#include "DeBand_shader.h"
#include "ImVulkanShader.h"

static inline float frand(int x, int y)
{
    const float r = sinf(x * 12.9898 + y * 78.233) * 43758.545;

    return r - floorf(r);
}

namespace ImGui 
{
DeBand_vulkan::DeBand_vulkan(int width, int height, int channels, int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "DeBand");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(DeBand_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
    }

    cmd->reset();

    in_width = width;
    in_height = height;
    in_channels = channels;
    xpos_cpu.create_type(in_width, in_height, IM_DT_INT32);
    ypos_cpu.create_type(in_width, in_height, IM_DT_INT32);
    precalc_pos();
}

DeBand_vulkan::~DeBand_vulkan()
{
    if (vkdev)
    {
        if (pipe) { delete pipe; pipe = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void DeBand_vulkan::precalc_pos(void)
{
    MutexLockGuard lock(param_lock);
    int * xpos_data = (int * )xpos_cpu.data;
    int * ypos_data = (int * )ypos_cpu.data;
    for (int y = 0; y < in_height; y++) 
    {
        for (int x = 0; x < in_width; x++) 
        {
            const float r = frand(x, y);
            const float dir = direction < 0 ? -direction : r * direction;
            const int dist = range < 0 ? -range : r * range;

            xpos_data[y * in_width + x] = cosf(dir) * dist;
            ypos_data[y * in_width + x] = sinf(dir) * dist;
        }
    }
    if (xpos.empty())
    {
        VkTransfer tran_xpos(vkdev);
        tran_xpos.record_upload(xpos_cpu, xpos, opt);
        tran_xpos.submit_and_wait();
    }
    if (ypos.empty())
    {
        VkTransfer tran_ypos(vkdev);
        tran_ypos.record_upload(ypos_cpu, ypos, opt);
        tran_ypos.submit_and_wait();
    }
}

void DeBand_vulkan::SetParam(int _range, float _direction)
{
    if (range != _range ||
        direction != _direction)
    {
        range = _range;
        direction = _direction;
        precalc_pos();
    }
}

void DeBand_vulkan::upload_param(const VkMat& src, VkMat& dst, float threshold, bool blur)
{
    std::vector<VkMat> bindings(10);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (src.type == IM_DT_INT8)     bindings[4] = src;
    else if (src.type == IM_DT_INT16)    bindings[5] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[6] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[7] = src;
    MutexLockGuard lock(param_lock);
    bindings[8] = xpos;
    bindings[9] = ypos;
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
    constants[10].f = threshold;
    constants[11].i = blur ? 1 : 0;
    cmd->record_pipeline(pipe, bindings, constants, dst);
}

double DeBand_vulkan::filter(const ImMat& src, ImMat& dst, float threshold, bool blur)
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

    upload_param(src_gpu, dst_gpu, threshold, blur);

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