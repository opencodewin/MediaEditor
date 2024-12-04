#include "ImVulkanShader.h"
#include "imvk_mat_shader.h"

namespace ImGui
{
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void ImVulkanGetVersion(int& major, int& minor, int& patch, int& build)
{
    major = VKSHADER_VERSION_MAJOR;
    minor = VKSHADER_VERSION_MINOR;
    patch = VKSHADER_VERSION_PATCH;
    build = VKSHADER_VERSION_BUILD;
}

void ImVulkanImMatToVkMat(const ImMat &src, VkMat &dst)
{
    Option opt;
    const VulkanDevice* vkdev = nullptr;
    VkAllocator* allocator = (VkAllocator*)dst.allocator;
    if (!allocator || !allocator->vkdev)
    {
        vkdev = get_gpu_device(dst.device_number);
        if (!vkdev) return;
        allocator = vkdev->acquire_blob_allocator();
        opt.blob_vkallocator = allocator;
    }
    else
    {
        vkdev = allocator->vkdev;
        opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    }
    dst.create_like(src, allocator);
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
    VkCompute cmd(vkdev, "ImMatToVkMat");
    cmd.record_clone(src, dst, opt);
    cmd.submit_and_wait();
    vkdev->reclaim_blob_allocator(opt.blob_vkallocator);
    vkdev->reclaim_staging_allocator(opt.staging_vkallocator);
}

void ImVulkanVkMatToImMat(const VkMat &src, ImMat &dst)
{
    Option opt;
    const VkAllocator* allocator = (VkAllocator*)src.allocator;
    const VulkanDevice* vkdev = allocator->vkdev;
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
    VkCompute cmd(vkdev, "VkMatToImMat");
    cmd.record_clone(src, dst, opt);
    cmd.submit_and_wait();
    vkdev->reclaim_blob_allocator(opt.blob_vkallocator);
    vkdev->reclaim_staging_allocator(opt.staging_vkallocator);
}

void ImVulkanVkMatToVkImageMat(const VkMat &src, VkImageMat &dst)
{
    Option opt;
    const VkAllocator* allocator = (VkAllocator*)src.allocator;
    const VulkanDevice* vkdev = allocator->vkdev;
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
    VkCompute cmd(vkdev, "VkMatToVkImageMat");
    cmd.record_upload(src, dst, opt);
    cmd.submit_and_wait();
    vkdev->reclaim_blob_allocator(opt.blob_vkallocator);
    vkdev->reclaim_staging_allocator(opt.staging_vkallocator);
}

void* ImVulkanVkMatMapping(const VkMat &src)
{
    Option opt;
    VkMat dst_staging;
    const VkAllocator* allocator = (VkAllocator*)src.allocator;
    const VulkanDevice* vkdev = allocator->vkdev;
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
    VkCompute cmd(vkdev, "VkMatToVkImageMat");
    cmd.record_download(src, dst_staging, opt);
    cmd.submit_and_wait();
    vkdev->reclaim_blob_allocator(opt.blob_vkallocator);
    vkdev->reclaim_staging_allocator(opt.staging_vkallocator);
    if (!dst_staging.empty())
        return dst_staging.mapped_ptr();
    return nullptr;
}

void ImVulkanShaderInit()
{
    create_gpu_instance();
}

void ImVulkanShaderClear()
{
    destroy_gpu_instance();
}

float ImVulkanPeak(VulkanDevice* vkdev, int loop, int count_mb, int cmd_loop, int storage_type, int arithmetic_type, int packing_type)
{
    const int count = count_mb * 1024 * 1024;
    int elempack = packing_type == 0 ? 1 : packing_type == 1 ? 4 : 8;
    if (!vkdev->info.support_fp16_storage() && storage_type == 2)
    {
        return -233;
    }
    if (!vkdev->info.support_fp16_arithmetic() && arithmetic_type == 1)
    {
        return -233;
    }
    double max_gflops = 0;
    Option opt;
    opt.use_fp16_packed = storage_type == 1;
    opt.use_fp16_storage = storage_type == 2;
    opt.use_fp16_arithmetic = arithmetic_type == 1;
    opt.use_shader_pack8 = packing_type == 2;

    // setup pipeline
    Pipeline pipeline(vkdev);
    {
        int local_size_x = std::min(128, std::max(32, (int)vkdev->info.subgroup_size()));
        pipeline.set_local_size_xyz(local_size_x, 1, 1);
        std::vector<vk_specialization_type> specializations(2);
        specializations[0].i = count;
        specializations[1].i = loop;
        // glsl to spirv
        // -1 for omit the tail '\0'
        std::vector<uint32_t> spirv;
        int ret = 0;
        if (packing_type == 0)
        {
            ret = compile_spirv_module(glsl_p1_data, opt, spirv);
        }
        if (packing_type == 1)
        {
            ret = compile_spirv_module(glsl_p4_data, opt, spirv);
        }
        if (packing_type == 2)
        {
            ret = compile_spirv_module(glsl_p8_data, opt, spirv);
        }
        if (ret == 0)
            pipeline.create(spirv.data(), spirv.size() * 4, specializations);
    }

    VkAllocator* allocator = vkdev->acquire_blob_allocator();

    // prepare storage
    {
        VkMat a;
        VkMat b;
        VkMat c;
        {
            if (opt.use_fp16_packed || opt.use_fp16_storage)
            {
                a.create(count, (size_t)(2u * elempack), elempack, allocator);
                b.create(count, (size_t)(2u * elempack), elempack, allocator);
                c.create(count, (size_t)(2u * elempack), elempack, allocator);
            }
            else
            {
                a.create(count, (size_t)(4u * elempack), elempack, allocator);
                b.create(count, (size_t)(4u * elempack), elempack, allocator);
                c.create(count, (size_t)(4u * elempack), elempack, allocator);
            }
        }

        // encode command
        VkCompute cmd(vkdev, "ImVulkanPeak");
        for (int i = 0; i < cmd_loop; i++)
        {
            {
                std::vector<VkMat> bindings(3);
                bindings[0] = a;
                bindings[1] = b;
                bindings[2] = c;
                std::vector<vk_constant_type> constants(0);
                cmd.record_pipeline(&pipeline, bindings, constants, c);
            }

            // time this
            {
                double t0 = GetSysCurrentTime();

                int ret = cmd.submit_and_wait();
                if (ret != 0)
                {
                    vkdev->reclaim_blob_allocator(allocator);
                    return -1;
                }

                double time = GetSysCurrentTime() - t0;
                const double mac = (double)count * (double)loop * 8 * elempack * 2;
                double gflops = mac / time / 1000000000;
                max_gflops += gflops;
            }
            cmd.flash();
        }
        cmd.reset();
    }
    vkdev->reclaim_blob_allocator(allocator);
    return max_gflops / cmd_loop;
}
} // namespace ImGui
