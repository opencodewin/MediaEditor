#include "ImVulkanShader.h"
#include "OpacityFilter_vulkan.h"
#include "OpacityFilter_shader.h"

using namespace std;

namespace ImGui
{
OpacityFilter_vulkan::OpacityFilter_vulkan(int gpuIdx)
{
    m_pVkDev = get_gpu_device(gpuIdx);
    m_tShdOpt.blob_vkallocator = m_pVkDev->acquire_blob_allocator();
    m_tShdOpt.staging_vkallocator = m_pVkDev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    m_tShdOpt.use_fp16_arithmetic = true;
    m_tShdOpt.use_fp16_storage = true;
#endif
    m_pVkCpt = new VkCompute(m_pVkDev, "OpacityFilter");

    vector<vk_specialization_type> specializations(0);
    vector<uint32_t> aSpirvData;

    if (compile_spirv_module(MASK_OPACITY_SHADER, m_tShdOpt, aSpirvData) == 0)
    {
        m_pPipeMaskOpacity = new Pipeline(m_pVkDev);
        m_pPipeMaskOpacity->create(aSpirvData.data(), aSpirvData.size()*4, specializations);
        aSpirvData.clear();
    }
    if (compile_spirv_module(MASK_OPACITY_INPLACE_SHADER, m_tShdOpt, aSpirvData) == 0)
    {
        m_pPipeMaskOpacityInplace = new Pipeline(m_pVkDev);
        m_pPipeMaskOpacityInplace->create(aSpirvData.data(), aSpirvData.size()*4, specializations);
        aSpirvData.clear();
    }
    m_pVkCpt->reset();
}

OpacityFilter_vulkan::~OpacityFilter_vulkan()
{
    if (m_pVkDev)
    {
        if (m_pPipeMaskOpacity) { delete m_pPipeMaskOpacity; m_pPipeMaskOpacity = nullptr; }
        if (m_pPipeMaskOpacityInplace) { delete m_pPipeMaskOpacityInplace; m_pPipeMaskOpacityInplace = nullptr; }
        if (m_pVkCpt) { delete m_pVkCpt; m_pVkCpt = nullptr; }
        if (m_tShdOpt.blob_vkallocator) { m_pVkDev->reclaim_blob_allocator(m_tShdOpt.blob_vkallocator); m_tShdOpt.blob_vkallocator = nullptr; }
        if (m_tShdOpt.staging_vkallocator) { m_pVkDev->reclaim_staging_allocator(m_tShdOpt.staging_vkallocator); m_tShdOpt.staging_vkallocator = nullptr; }
    }
}

ImMat OpacityFilter_vulkan::maskOpacity(const ImMat& src, const ImMat& mask, float opacity, bool inverse, bool inplace) const
{
    if (!m_pVkDev || !m_pPipeMaskOpacity || !m_pPipeMaskOpacityInplace || !m_pVkCpt)
        throw runtime_error("Vulkan items are INVALID!");
    if (src.empty())
        return src;
    if (src.c != 4 || !IM_ISRGB(src.color_format))
        throw runtime_error("'src' must be RGB of 4 channels!");
    if (src.w != mask.w || src.h != mask.h)
        throw runtime_error("'mask' must have the same size with 'src'!");
    if (mask.c != 1)
        throw runtime_error("'mask' must has 1 channel data!");

    VkMat srcGpu;
    if (src.device == IM_DD_VULKAN)
        srcGpu = src;
    else if (src.device == IM_DD_CPU)
        m_pVkCpt->record_clone(src, srcGpu, m_tShdOpt);
    else
        throw runtime_error("Unsupported 'src' mat device type!");

    VkMat maskGpu;
    if (mask.device == IM_DD_VULKAN)
        maskGpu = mask;
    else if (mask.device == IM_DD_CPU)
        m_pVkCpt->record_clone(mask, maskGpu, m_tShdOpt);
    else
        throw runtime_error("Unsupported 'mask' mat device type!");

    VkMat dstGpu;
    if (inplace)
        dstGpu = srcGpu;
    else
    {
        dstGpu.create_like(srcGpu, m_tShdOpt.blob_vkallocator);
        m_pVkCpt->record_clone(srcGpu, dstGpu, m_tShdOpt);
    }

    VkMat dispatcher(src.w, src.h, 1, nullptr, 1, nullptr);

    vector<vk_constant_type> constants(10);
    constants[0].i = srcGpu.w;
    constants[1].i = srcGpu.h;
    constants[2].i = srcGpu.c;
    constants[3].i = srcGpu.color_format;
    constants[4].i = srcGpu.type;
    constants[5].i = maskGpu.c;
    constants[6].i = maskGpu.color_format;
    constants[7].i = maskGpu.type;
    constants[8].i = inverse ? 1 : 0;
    constants[9].f = opacity;
    if (inplace)
    {
        // upload parameter
        vector<VkMat> bindings(8);
        if      (dstGpu.type == IM_DT_INT8)                                     bindings[0] = dstGpu;
        else if (dstGpu.type == IM_DT_INT16 || dstGpu.type == IM_DT_INT16_BE)   bindings[1] = dstGpu;
        else if (dstGpu.type == IM_DT_FLOAT16)                                  bindings[2] = dstGpu;
        else if (dstGpu.type == IM_DT_FLOAT32)                                  bindings[3] = dstGpu;

        if      (maskGpu.type == IM_DT_INT8)                                    bindings[4] = maskGpu;
        else if (maskGpu.type == IM_DT_INT16 || maskGpu.type == IM_DT_INT16_BE) bindings[5] = maskGpu;
        else if (maskGpu.type == IM_DT_FLOAT16)                                 bindings[6] = maskGpu;
        else if (maskGpu.type == IM_DT_FLOAT32)                                 bindings[7] = maskGpu;

        m_pVkCpt->record_pipeline(m_pPipeMaskOpacityInplace, bindings, constants, dispatcher);
    }
    else
    {
        // upload parameter
        vector<VkMat> bindings(12);
        if      (dstGpu.type == IM_DT_INT8)                                     bindings[0]  = dstGpu;
        else if (dstGpu.type == IM_DT_INT16 || dstGpu.type == IM_DT_INT16_BE)   bindings[1]  = dstGpu;
        else if (dstGpu.type == IM_DT_FLOAT16)                                  bindings[2]  = dstGpu;
        else if (dstGpu.type == IM_DT_FLOAT32)                                  bindings[3]  = dstGpu;

        if      (srcGpu.type == IM_DT_INT8)                                     bindings[4]  = srcGpu;
        else if (srcGpu.type == IM_DT_INT16 || srcGpu.type == IM_DT_INT16_BE)   bindings[5]  = srcGpu;
        else if (srcGpu.type == IM_DT_FLOAT16)                                  bindings[6]  = srcGpu;
        else if (srcGpu.type == IM_DT_FLOAT32)                                  bindings[7]  = srcGpu;

        if      (maskGpu.type == IM_DT_INT8)                                    bindings[8]  = maskGpu;
        else if (maskGpu.type == IM_DT_INT16 || maskGpu.type == IM_DT_INT16_BE) bindings[9]  = maskGpu;
        else if (maskGpu.type == IM_DT_FLOAT16)                                 bindings[10] = maskGpu;
        else if (maskGpu.type == IM_DT_FLOAT32)                                 bindings[11] = maskGpu;

        m_pVkCpt->record_pipeline(m_pPipeMaskOpacity, bindings, constants, dispatcher);
    }

    // download
    ImMat dst = dstGpu;
    if (inplace && src.device == IM_DD_CPU)
    {
        dst = src;
        m_pVkCpt->record_clone(dstGpu, dst, m_tShdOpt);
    }
    m_pVkCpt->submit_and_wait();
    m_pVkCpt->reset();
    return dst;
}
}