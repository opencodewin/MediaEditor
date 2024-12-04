#include <sstream>
#include "ColorConvert_vulkan.h"
#include "ColorConvert_shader.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
extern const ImMat * color_table[2][2][4];
extern const ImMat * xyz_color_table[2][2][8];
ColorConvert_vulkan::ColorConvert_vulkan(int gpu)
{
    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = false;
    opt.use_fp16_storage = false;   // fp16 has accuracy issue for int16 convert
#endif
    cmd = new VkCompute(vkdev, "ColorConvert");
    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;

    if (compile_spirv_module(YUV2RGB_data, opt, spirv_data) == 0)
    {
        pipeline_yuv_rgb = new Pipeline(vkdev);
        pipeline_yuv_rgb->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(Y_U_V2RGB_data, opt, spirv_data) == 0)
    {
        pipeline_y_u_v_rgb = new Pipeline(vkdev);
        pipeline_y_u_v_rgb->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(RGB2YUV_data, opt, spirv_data) == 0)
    {
        pipeline_rgb_yuv = new Pipeline(vkdev);
        pipeline_rgb_yuv->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(GRAY2RGB_data, opt, spirv_data) == 0)
    {
        pipeline_gray_rgb = new Pipeline(vkdev);
        pipeline_gray_rgb->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(Conv_data, opt, spirv_data) == 0)
    {
        pipeline_conv = new Pipeline(vkdev);
        pipeline_conv->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(RGB2LAB_data, opt, spirv_data) == 0)
    {
        pipeline_rgb_lab = new Pipeline(vkdev);
        pipeline_rgb_lab->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(LAB2RGB_data, opt, spirv_data) == 0)
    {
        pipeline_lab_rgb = new Pipeline(vkdev);
        pipeline_lab_rgb->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(RGB2HSL_data, opt, spirv_data) == 0)
    {
        pipeline_rgb_hsl = new Pipeline(vkdev);
        pipeline_rgb_hsl->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(HSL2RGB_data, opt, spirv_data) == 0)
    {
        pipeline_hsl_rgb = new Pipeline(vkdev);
        pipeline_hsl_rgb->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(RGB2HSV_data, opt, spirv_data) == 0)
    {
        pipeline_rgb_hsv = new Pipeline(vkdev);
        pipeline_rgb_hsv->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(HSV2RGB_data, opt, spirv_data) == 0)
    {
        pipeline_hsv_rgb = new Pipeline(vkdev);
        pipeline_hsv_rgb->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    cmd->reset();
}

ColorConvert_vulkan::~ColorConvert_vulkan()
{
    if (vkdev)
    {
        if (pipeline_yuv_rgb) { delete pipeline_yuv_rgb; pipeline_yuv_rgb = nullptr; }
        if (pipeline_rgb_yuv) { delete pipeline_rgb_yuv; pipeline_rgb_yuv = nullptr; }
        if (pipeline_gray_rgb) { delete pipeline_gray_rgb; pipeline_gray_rgb = nullptr; }
        if (pipeline_conv) { delete pipeline_conv; pipeline_conv = nullptr; }
        if (pipeline_y_u_v_rgb) { delete pipeline_y_u_v_rgb; pipeline_y_u_v_rgb = nullptr; }
        if (pipeline_rgb_lab) { delete pipeline_rgb_lab; pipeline_rgb_lab = nullptr; }
        if (pipeline_lab_rgb) { delete pipeline_lab_rgb; pipeline_lab_rgb = nullptr; }
        if (pipeline_rgb_hsl) { delete pipeline_rgb_hsl; pipeline_rgb_hsl = nullptr; }
        if (pipeline_hsl_rgb) { delete pipeline_hsl_rgb; pipeline_hsl_rgb = nullptr; }
        if (pipeline_rgb_hsv) { delete pipeline_rgb_hsv; pipeline_rgb_hsv = nullptr; }
        if (pipeline_hsv_rgb) { delete pipeline_hsv_rgb; pipeline_hsv_rgb = nullptr; }

        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

double ColorConvert_vulkan::ConvertColorFormat(const ImMat& srcMat, ImMat& dstMat, ImInterpolateMode type)
{
    double ret = -1.0;
    if (!vkdev || !pipeline_gray_rgb || !pipeline_yuv_rgb || !pipeline_rgb_yuv || !pipeline_conv)
        return ret;
    if (dstMat.color_format < IM_CF_BGR)
    {
        std::ostringstream oss;
        oss << "Argument 'dstMat' has UNSUPPORTED color format " << srcMat.color_format << "!";
        mErrMsg = oss.str();
        return ret;
    }

    auto color_range = dstMat.color_range;
    auto color_space = dstMat.color_space;
    dstMat.copy_attribute(srcMat);
    // keep dstMat color info
    dstMat.color_range = color_range;
    dstMat.color_space = color_space;

    int srcClrCatg = GetColorFormatCategory(srcMat.color_format);
    int dstClrCatg = GetColorFormatCategory(dstMat.color_format);
    // source is RGB
    if (srcClrCatg == 1)
    {
        // TODO: add other rgb format support
        // only support input format ABGR/ARGB/RGBA/BGRA.
        if (srcMat.color_format != IM_CF_ABGR && srcMat.color_format != IM_CF_ARGB &&
            srcMat.color_format != IM_CF_RGBA && srcMat.color_format != IM_CF_BGRA &&
            srcMat.color_format != IM_CF_RGB && srcMat.color_format != IM_CF_BGR &&
            dstClrCatg != 1)
        {
            mErrMsg = "Only support rgb input format ABGR/ARGB/RGBA/BGRA!";
            return ret;
        }
    }
    // destination is RGB
    if (dstClrCatg == 1)
    {
        // only support output format ABGR/ARGB/RGBA/BGRA.
        if (dstMat.color_format != IM_CF_ABGR && dstMat.color_format != IM_CF_ARGB &&
            dstMat.color_format != IM_CF_RGBA && dstMat.color_format != IM_CF_BGRA)
        {
            mErrMsg = "Only support rgb output format ABGR/ARGB/RGBA/BGRA!";
            return ret;
        }
        dstMat.color_range = IM_CR_FULL_RANGE;
    }

    // prepare source vulkan mat
    VkMat srcVkMat;
    if (srcMat.device == IM_DD_VULKAN)
        srcVkMat = srcMat;
    else
        cmd->record_clone(srcMat, srcVkMat, opt);

    // prepare destination vulkan mat
    VkMat dstVkMat;
    int dst_width = dstMat.w > 0 ? dstMat.w : srcMat.w;
    int dst_height = dstMat.h > 0 ? dstMat.h : srcMat.h;
    if (dstMat.device == IM_DD_VULKAN)
    {
        dstVkMat.create_type(dst_width, dst_height, GetChannelCountByColorFormat(dstMat.color_format), dstMat.type, opt.blob_vkallocator);
        dstVkMat.color_format = dstMat.color_format;
        dstVkMat.color_range = dstMat.color_range;
        dstVkMat.color_space = dstMat.color_space;
        dstMat = dstVkMat;
    }
    else
    {
        ImMat tmp;
        tmp.create_type(dst_width, dst_height, GetChannelCountByColorFormat(dstMat.color_format), dstMat.type);
        tmp.color_format = dstMat.color_format;
        tmp.color_range = dstMat.color_range;
        tmp.color_space = dstMat.color_space;
        dstMat = tmp;
        dstVkMat.create_like(dstMat, opt.blob_vkallocator);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    auto ret_update = UploadParam(srcVkMat, dstVkMat, type);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    if (!ret_update)
    {
        return ret;
    }

    if (dstMat.device == IM_DD_CPU)
        cmd->record_clone(dstVkMat, dstMat, opt);
    else if (dstMat.device == IM_DD_VULKAN_IMAGE)
    {
        VkImageMat* pVkiMat = dynamic_cast<VkImageMat*>(&dstMat);
        cmd->record_buffer_to_image(dstVkMat, *pVkiMat, opt);
    }

    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();
    return ret;
}

bool ColorConvert_vulkan::UploadParam(const VkMat& src, VkMat& dst, ImInterpolateMode type)
{
    int srcClrCatg = GetColorFormatCategory(src.color_format);
    int dstClrCatg = GetColorFormatCategory(dst.color_format);
    bool resize = false;
    if (dst.w != 0 && dst.h != 0 && (dst.w != src.w || dst.h != src.h))
        resize = true;
    if (srcClrCatg < 0 || dstClrCatg < 0)
    {
        std::ostringstream oss;
        oss << "Unknown color format category! 'src.color_format' is " << src.color_format
            << ", 'dst.color_format' is " << dst.color_format << ".";
        mErrMsg = oss.str();
        return false;
    }

    // GRAY -> RGB
    if (srcClrCatg == 0 && dstClrCatg == 1)
    {
        int bitDepth = src.depth != 0 ? src.depth : src.type == IM_DT_INT8 ? 8 : src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE ? 16 : 8;

        std::vector<VkMat> bindings(8);
        if      (dst.type == IM_DT_INT8)    bindings[0] = dst;
        else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)   bindings[1] = dst;
        else if (dst.type == IM_DT_FLOAT16) bindings[2] = dst;
        else if (dst.type == IM_DT_FLOAT32) bindings[3] = dst;
        if      (src.type == IM_DT_INT8)    bindings[4] = src;
        else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)   bindings[5] = src;
        else if (src.type == IM_DT_FLOAT16) bindings[6] = src;
        else if (src.type == IM_DT_FLOAT32) bindings[7] = src;

        std::vector<vk_constant_type> constants(11);
        constants[ 0].i = src.w;
        constants[ 1].i = src.h;
        constants[ 2].i = src.c;
        constants[ 3].i = src.color_format;
        constants[ 4].i = src.type;
        constants[ 5].i = dst.w;
        constants[ 6].i = dst.h;
        constants[ 7].i = dst.c;
        constants[ 8].i = dst.color_format;
        constants[ 9].i = dst.type;
        constants[10].f = (float)((1 << bitDepth) - 1);

        cmd->record_pipeline(pipeline_gray_rgb, bindings, constants, dst);
    }
    // YUV -> RGB
    else if (srcClrCatg == 2 && dstClrCatg == 1)
    {
        VkMat vkCscCoefs;
        const ImMat cscCoefs = *color_table[0][src.color_range][src.color_space];
        cmd->record_clone(cscCoefs, vkCscCoefs, opt);
        int bitDepth = src.depth != 0 ? src.depth : src.type == IM_DT_INT8 ? 8 : src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE ? 16 : 8;

        std::vector<VkMat> bindings(9);
        if      (dst.type == IM_DT_INT8)    bindings[0] = dst;
        else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)   bindings[1] = dst;
        else if (dst.type == IM_DT_FLOAT16) bindings[2] = dst;
        else if (dst.type == IM_DT_FLOAT32) bindings[3] = dst;
        if      (src.type == IM_DT_INT8)    bindings[4] = src;
        else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)   bindings[5] = src;
        else if (src.type == IM_DT_FLOAT16) bindings[6] = src;
        else if (src.type == IM_DT_FLOAT32) bindings[7] = src;
        bindings[8] = vkCscCoefs;

        std::vector<vk_constant_type> constants(18);
        constants[0].i = src.w;
        constants[1].i = src.h;
        constants[2].i = dst.c;
        constants[3].i = src.color_format;
        constants[4].i = src.type;
        constants[5].i = src.color_space;
        constants[6].i = src.color_range;
        constants[7].f = (float)((1 << bitDepth) - 1);
        constants[8].i = src.dw;
        constants[9].i = src.dh;
        constants[10].i = dst.w;
        constants[11].i = dst.h;
        constants[12].i = dst.c;
        constants[13].i = dst.color_format;
        constants[14].i = dst.type;
        constants[15].i = resize ? 1 : 0;
        constants[16].i = type;
        constants[17].i = false;

        cmd->record_pipeline(pipeline_yuv_rgb, bindings, constants, dst);
    }
    // RGB -> YUV
    else if (srcClrCatg == 1 && dstClrCatg == 2)
    {
        VkMat vkCscCoefs;
        const ImMat cscCoefs = *color_table[1][dst.color_range][dst.color_space];
        cmd->record_clone(cscCoefs, vkCscCoefs, opt);
        int bitDepth = dst.depth != 0 ? dst.depth : dst.type == IM_DT_INT8 ? 8 : dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE ? 16 : 8;

        std::vector<VkMat> bindings(9);
        if      (dst.type == IM_DT_INT8)    bindings[0] = dst;
        else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)   bindings[1] = dst;
        else if (dst.type == IM_DT_FLOAT16) bindings[2] = dst;
        else if (dst.type == IM_DT_FLOAT32) bindings[3] = dst;
        if      (src.type == IM_DT_INT8)    bindings[4] = src;
        else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)   bindings[5] = src;
        else if (src.type == IM_DT_FLOAT16) bindings[6] = src;
        else if (src.type == IM_DT_FLOAT32) bindings[7] = src;
        bindings[8] = vkCscCoefs;

        std::vector<vk_constant_type> constants(13);
        constants[ 0].i = src.w;
        constants[ 1].i = src.h;
        constants[ 2].i = src.c;
        constants[ 3].i = src.color_format;
        constants[ 4].i = src.type;
        constants[ 5].i = src.color_space;
        constants[ 6].i = src.color_range;
        constants[ 7].i = dst.cstep;
        constants[ 8].i = dst.color_format;
        constants[ 9].i = dst.type;
        constants[10].i = dst.color_space;
        constants[11].i = dst.color_range;
        constants[12].f = (float)((1 << bitDepth) - 1);

        cmd->record_pipeline(pipeline_rgb_yuv, bindings, constants, dst);
    }
    // conversion in same color format category
    else if (srcClrCatg == dstClrCatg)
    {
        std::vector<VkMat> bindings(8);
        if      (dst.type == IM_DT_INT8)    bindings[0] = dst;
        else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)   bindings[1] = dst;
        else if (dst.type == IM_DT_FLOAT16) bindings[2] = dst;
        else if (dst.type == IM_DT_FLOAT32) bindings[3] = dst;
        if      (src.type == IM_DT_INT8)    bindings[4] = src;
        else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)   bindings[5] = src;
        else if (src.type == IM_DT_FLOAT16) bindings[6] = src;
        else if (src.type == IM_DT_FLOAT32) bindings[7] = src;

        std::vector<vk_constant_type> constants(10);
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

        cmd->record_pipeline(pipeline_conv, bindings, constants, dst);
    }
    else
    {
        std::ostringstream oss;
        oss << "UNSUPPORTED color format conversion! From " << src.color_format << " to " << dst.color_format << ".";
        mErrMsg = oss.str();
        return false;
    }

    return true;
}

// YUV to RGBA functions
void ColorConvert_vulkan::upload_param(const VkMat& Im_YUV, VkMat& dst, ImInterpolateMode type, ImColorFormat color_format, ImColorSpace color_space, ImColorRange color_range, int video_depth, bool mirror) const
{
    VkMat matrix_y2r_gpu;
    const ImMat conv_mat_y2r = *color_table[0][color_range][color_space];
    cmd->record_clone(conv_mat_y2r, matrix_y2r_gpu, opt);
    std::vector<VkMat> bindings(9);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (Im_YUV.type == IM_DT_INT8)    bindings[4] = Im_YUV;
    else if (Im_YUV.type == IM_DT_INT16 || Im_YUV.type == IM_DT_INT16_BE)   bindings[5] = Im_YUV;
    else if (Im_YUV.type == IM_DT_FLOAT16) bindings[6] = Im_YUV;
    else if (Im_YUV.type == IM_DT_FLOAT32) bindings[7] = Im_YUV;

    bindings[8] = matrix_y2r_gpu;

    bool resize = false;
    if (dst.w != 0 && dst.h != 0 && (dst.w != Im_YUV.w || dst.h != Im_YUV.h))
        resize = true;
    int bitDepth = Im_YUV.depth != 0 ? Im_YUV.depth : Im_YUV.type == IM_DT_INT8 ? 8 : Im_YUV.type == IM_DT_INT16 || Im_YUV.type == IM_DT_INT16_BE ? 16 : 8;
    std::vector<vk_constant_type> constants(18);
    constants[0].i = Im_YUV.w;
    constants[1].i = Im_YUV.h;
    constants[2].i = dst.c;
    constants[3].i = Im_YUV.color_format;
    constants[4].i = Im_YUV.type;
    constants[5].i = Im_YUV.color_space;
    constants[6].i = Im_YUV.color_range;
    constants[7].f = (float)((1 << bitDepth) - 1);
    constants[8].i = Im_YUV.dw;
    constants[9].i = Im_YUV.dh;
    constants[10].i = dst.w;
    constants[11].i = dst.h;
    constants[12].i = dst.c;
    constants[13].i = dst.color_format;
    constants[14].i = dst.type;
    constants[15].i = resize ? 1 : 0;
    constants[16].i = type;
    constants[17].i = mirror ? 1 : 0;
    cmd->record_pipeline(pipeline_yuv_rgb, bindings, constants, dst);
}

double ColorConvert_vulkan::YUV2RGBA(const ImMat& im_YUV, ImMat & im_RGBA, ImInterpolateMode type, bool mirror) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_yuv_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int dst_width = im_RGBA.w > 0 ? im_RGBA.w : im_YUV.w;
    int dst_height = im_RGBA.h > 0 ? im_RGBA.h : im_YUV.h;
    dst_gpu.create_type(dst_width, dst_height, 4, im_RGBA.type, opt.blob_vkallocator);
    dst_gpu.elempack = dst_gpu.c;
    dst_gpu.color_format = im_RGBA.color_format;
    dst_gpu.color_range = IM_CR_FULL_RANGE;
    dst_gpu.color_space = im_YUV.color_space;

    VkMat src_gpu;
    if (im_YUV.device == IM_DD_VULKAN)
    {
        src_gpu = im_YUV;
    }
    else if (im_YUV.device == IM_DD_CPU)
    {
        cmd->record_clone(im_YUV, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, type, im_YUV.color_format, im_YUV.color_space, im_YUV.color_range, im_YUV.depth, mirror);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGBA.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGBA, opt);
    else if (im_RGBA.device == IM_DD_VULKAN)
        im_RGBA = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    im_RGBA.copy_attribute(im_YUV);
    return ret;
}

double ColorConvert_vulkan::YUV2RGB(const ImMat& im_YUV, ImMat & im_RGB, ImInterpolateMode type, bool mirror) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_yuv_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int dst_width = im_RGB.w > 0 ? im_RGB.w : im_YUV.w;
    int dst_height = im_RGB.h > 0 ? im_RGB.h : im_YUV.h;
    dst_gpu.create_type(dst_width, dst_height, 3, im_RGB.type, opt.blob_vkallocator);
    dst_gpu.elempack = dst_gpu.c;
    dst_gpu.color_format = im_RGB.color_format;
    dst_gpu.color_range = IM_CR_FULL_RANGE;
    dst_gpu.color_space = im_YUV.color_space;

    VkMat src_gpu;
    if (im_YUV.device == IM_DD_VULKAN)
    {
        src_gpu = im_YUV;
    }
    else if (im_YUV.device == IM_DD_CPU)
    {
        cmd->record_clone(im_YUV, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, type, im_YUV.color_format, im_YUV.color_space, im_YUV.color_range, im_YUV.depth, mirror);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGB, opt);
    else if (im_RGB.device == IM_DD_VULKAN)
        im_RGB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    im_RGB.copy_attribute(im_YUV);
    return ret;
}

void ColorConvert_vulkan::upload_param(const VkMat& Im_Y, const VkMat& Im_U, const VkMat& Im_V, VkMat& dst, ImInterpolateMode type, bool mirror) const
{
    VkMat matrix_y2r_gpu;
    const ImMat conv_mat_y2r = *color_table[0][Im_Y.color_range][Im_Y.color_space];
    cmd->record_clone(conv_mat_y2r, matrix_y2r_gpu, opt);
    std::vector<VkMat> bindings(17);
    if      (dst.type == IM_DT_INT8)     bindings[ 0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[ 1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[ 2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[ 3] = dst;

    if      (Im_Y.type == IM_DT_INT8)    bindings[ 4] = Im_Y;
    else if (Im_Y.type == IM_DT_INT16 || Im_Y.type == IM_DT_INT16_BE)   bindings[ 5] = Im_Y;
    else if (Im_Y.type == IM_DT_FLOAT16) bindings[ 6] = Im_Y;
    else if (Im_Y.type == IM_DT_FLOAT32) bindings[ 7] = Im_Y;

    if      (Im_U.type == IM_DT_INT8)    bindings[ 8] = Im_U;
    else if (Im_U.type == IM_DT_INT16 || Im_U.type == IM_DT_INT16_BE)   bindings[ 9] = Im_U;
    else if (Im_U.type == IM_DT_FLOAT16) bindings[10] = Im_U;
    else if (Im_U.type == IM_DT_FLOAT32) bindings[11] = Im_U;

    if      (Im_V.type == IM_DT_INT8)    bindings[12] = Im_V;
    else if (Im_V.type == IM_DT_INT16 || Im_V.type == IM_DT_INT16_BE)   bindings[13] = Im_V;
    else if (Im_V.type == IM_DT_FLOAT16) bindings[14] = Im_V;
    else if (Im_V.type == IM_DT_FLOAT32) bindings[15] = Im_V;

    bindings[16] = matrix_y2r_gpu;

    bool resize = false;
    if (dst.w != 0 && dst.h != 0 && (dst.w != Im_Y.w || dst.h != Im_Y.h))
        resize = true;
    if (Im_Y.dw != Im_Y.w || Im_Y.dh != Im_Y.h)
        resize = true;

    int bitDepth = Im_Y.depth != 0 ? Im_Y.depth : Im_Y.type == IM_DT_INT8 ? 8 : Im_Y.type == IM_DT_INT16 || Im_Y.type == IM_DT_INT16_BE ? 16 : 8;
    std::vector<vk_constant_type> constants(20);
    constants[0].i = Im_Y.w;
    constants[1].i = Im_Y.h;
    constants[2].i = dst.c;
    constants[3].i = Im_Y.color_format;
    constants[4].i = Im_Y.type;
    constants[5].i = Im_Y.color_space;
    constants[6].i = Im_Y.color_range;
    constants[7].f = (float)((1 << bitDepth) - 1);
    constants[8].i = Im_Y.dw;
    constants[9].i = Im_Y.dh;
    constants[10].i = dst.w;
    constants[11].i = dst.h;
    constants[12].i = dst.c;
    constants[13].i = dst.color_format;
    constants[14].i = dst.type;
    constants[15].i = resize ? 1 : 0;
    constants[16].i = type;
    constants[17].i = Im_U.w;
    constants[18].i = Im_V.w;
    constants[19].i = mirror ? 1 : 0;
    cmd->record_pipeline(pipeline_y_u_v_rgb, bindings, constants, dst);
}

double ColorConvert_vulkan::YUV2RGBA(const ImMat& im_Y, const ImMat& im_U, const ImMat& im_V, ImMat & im_RGBA, ImInterpolateMode type, bool mirror) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_y_u_v_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int dst_width = im_RGBA.w > 0 ? im_RGBA.w : (im_Y.dw > 0 ? im_Y.dw : im_Y.w);
    int dst_height = im_RGBA.h > 0 ? im_RGBA.h : (im_Y.dh > 0 ? im_Y.dh : im_Y.h);
    dst_gpu.create_type(dst_width, dst_height, 4, im_RGBA.type, opt.blob_vkallocator);
    dst_gpu.elempack = dst_gpu.c;
    dst_gpu.color_format = im_RGBA.color_format;
    dst_gpu.color_range = IM_CR_FULL_RANGE;
    dst_gpu.color_space = im_Y.color_space;

    VkMat src_y_gpu, src_u_gpu, src_v_gpu;
    if (im_Y.device == IM_DD_VULKAN)
    {
        src_y_gpu = im_Y;
    }
    else if (im_Y.device == IM_DD_CPU)
    {
        cmd->record_clone(im_Y, src_y_gpu, opt);
    }

    if (im_U.device == IM_DD_VULKAN)
    {
        src_u_gpu = im_U;
    }
    else if (im_U.device == IM_DD_CPU)
    {
        cmd->record_clone(im_U, src_u_gpu, opt);
    }
    if (!im_V.empty())
    {
        if (im_V.device == IM_DD_VULKAN)
        {
            src_v_gpu = im_V;
        }
        else if (im_V.device == IM_DD_CPU)
        {
            cmd->record_clone(im_V, src_v_gpu, opt);
        }
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_y_gpu, src_u_gpu, src_v_gpu, dst_gpu, type, mirror);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGBA.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGBA, opt);
    else if (im_RGBA.device == IM_DD_VULKAN)
        im_RGBA = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
    //cmd->benchmark_print();
#else
    ret = 1.f;
#endif
    cmd->reset();

    im_RGBA.copy_attribute(im_Y);
    return ret;
}

double ColorConvert_vulkan::YUV2RGB(const ImMat& im_Y, const ImMat& im_U, const ImMat& im_V, ImMat & im_RGB, ImInterpolateMode type, bool mirror) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_y_u_v_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int dst_width = im_RGB.w > 0 ? im_RGB.w : (im_Y.dw > 0 ? im_Y.dw : im_Y.w);
    int dst_height = im_RGB.h > 0 ? im_RGB.h : (im_Y.dh > 0 ? im_Y.dh : im_Y.h);
    dst_gpu.create_type(dst_width, dst_height, 3, im_RGB.type, opt.blob_vkallocator);
    dst_gpu.elempack = dst_gpu.c;
    dst_gpu.color_format = im_RGB.color_format;
    dst_gpu.color_range = IM_CR_FULL_RANGE;
    dst_gpu.color_space = im_Y.color_space;

    VkMat src_y_gpu, src_u_gpu, src_v_gpu;
    if (im_Y.device == IM_DD_VULKAN)
    {
        src_y_gpu = im_Y;
    }
    else if (im_Y.device == IM_DD_CPU)
    {
        cmd->record_clone(im_Y, src_y_gpu, opt);
    }

    if (im_U.device == IM_DD_VULKAN)
    {
        src_u_gpu = im_U;
    }
    else if (im_U.device == IM_DD_CPU)
    {
        cmd->record_clone(im_U, src_u_gpu, opt);
    }
    if (!im_V.empty())
    {
        if (im_V.device == IM_DD_VULKAN)
        {
            src_v_gpu = im_V;
        }
        else if (im_V.device == IM_DD_CPU)
        {
            cmd->record_clone(im_V, src_v_gpu, opt);
        }
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_y_gpu, src_u_gpu, src_v_gpu, dst_gpu, type, mirror);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGB, opt);
    else if (im_RGB.device == IM_DD_VULKAN)
        im_RGB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
    //cmd->benchmark_print();
#else
    ret = 1.f;
#endif
    cmd->reset();

    im_RGB.copy_attribute(im_Y);
    return ret;
}

// RGBA to YUV functions
void ColorConvert_vulkan::upload_param(const VkMat& Im_RGB, VkMat& dst, ImColorFormat color_format, ImColorSpace color_space, ImColorRange color_range, int depth) const
{
    VkMat matrix_r2y_gpu;
    int bitDepth = depth != 0 ? depth : Im_RGB.type == IM_DT_INT8 ? 8 : Im_RGB.type == IM_DT_INT16 || Im_RGB.type == IM_DT_INT16_BE ? 16 : 8;
    const ImMat conv_mat_r2y = *color_table[1][color_range][color_space];
    cmd->record_clone(conv_mat_r2y, matrix_r2y_gpu, opt);
    std::vector<VkMat> bindings(9);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (Im_RGB.type == IM_DT_INT8)    bindings[4] = Im_RGB;
    else if (Im_RGB.type == IM_DT_INT16 || Im_RGB.type == IM_DT_INT16_BE)   bindings[5] = Im_RGB;
    else if (Im_RGB.type == IM_DT_FLOAT16) bindings[6] = Im_RGB;
    else if (Im_RGB.type == IM_DT_FLOAT32) bindings[7] = Im_RGB;

    bindings[8] = matrix_r2y_gpu;
    std::vector<vk_constant_type> constants(13);
    constants[0].i = Im_RGB.w;
    constants[1].i = Im_RGB.h;
    constants[2].i = Im_RGB.c;
    constants[3].i = Im_RGB.color_format;
    constants[4].i = Im_RGB.type;
    constants[5].i = Im_RGB.color_space;
    constants[6].i = Im_RGB.color_range;
    constants[7].i = dst.cstep;
    constants[8].i = color_format;
    constants[9].i = dst.type;
    constants[10].i = color_space;
    constants[11].i = color_range;
    constants[12].f = (float)((1 << bitDepth) - 1);
    cmd->record_pipeline(pipeline_rgb_yuv, bindings, constants, dst);
}

double ColorConvert_vulkan::RGBA2YUV(const ImMat& im_RGB, ImMat & im_YUV) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_rgb_yuv || !cmd)
    {
        return ret;
    }
    VkMat dst_gpu;
    dst_gpu.create_type(im_RGB.w, im_RGB.h, 4, im_YUV.type, opt.blob_vkallocator);
    im_YUV.copy_attribute(im_RGB);

    VkMat src_gpu;
    if (im_RGB.device == IM_DD_VULKAN)
    {
        src_gpu = im_RGB;
    }
    else if (im_RGB.device == IM_DD_CPU)
    {
        cmd->record_clone(im_RGB, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, im_YUV.color_format, im_YUV.color_space, im_YUV.color_range, im_YUV.depth);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_YUV.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_YUV, opt);
    else if (im_YUV.device == IM_DD_VULKAN)
        im_YUV = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();
    return ret;
}

// Gray to RGBA functions
void ColorConvert_vulkan::upload_param(const VkMat& Im, VkMat& dst, ImColorSpace color_space, ImColorRange color_range, int video_depth, int video_shift) const
{
    std::vector<VkMat> bindings(8);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (Im.type == IM_DT_INT8)      bindings[4] = Im;
    else if (Im.type == IM_DT_INT16 || Im.type == IM_DT_INT16_BE)     bindings[5] = Im;
    else if (Im.type == IM_DT_FLOAT16)   bindings[6] = Im;
    else if (Im.type == IM_DT_FLOAT32)   bindings[7] = Im;

    std::vector<vk_constant_type> constants(11);
    constants[0].i = Im.w;
    constants[1].i = Im.h;
    constants[2].i = Im.c;
    constants[3].i = IM_CF_GRAY;
    constants[4].i = Im.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.c;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    constants[10].f = (float)((1 << video_shift) - 1);
    cmd->record_pipeline(pipeline_gray_rgb, bindings, constants, dst);
}

double ColorConvert_vulkan::GRAY2RGBA(const ImMat& im, ImMat & im_RGB, ImColorSpace color_space, ImColorRange color_range, int video_depth, int video_shift) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_gray_rgb || !cmd)
    {
        return ret;
    }
    VkMat dst_gpu;
    dst_gpu.create_type(im.w, im.h, 4, im_RGB.type, opt.blob_vkallocator);
    im_RGB.copy_attribute(im);

    VkMat src_gpu;
    if (im.device == IM_DD_VULKAN)
    {
        src_gpu = im;
    }
    else if (im.device == IM_DD_CPU)
    {
        cmd->record_clone(im, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, color_space, color_range, video_depth, video_shift);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGB, opt);
    else if (im_RGB.device == IM_DD_VULKAN)
        im_RGB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();
    return ret;
}

// Conv Functions
void ColorConvert_vulkan::upload_param(const VkMat& Im, VkMat& dst) const
{
    std::vector<VkMat> bindings(8);
    if      (dst.type == IM_DT_INT8)     bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings[3] = dst;

    if      (Im.type == IM_DT_INT8)      bindings[4] = Im;
    else if (Im.type == IM_DT_INT16 || Im.type == IM_DT_INT16_BE)     bindings[5] = Im;
    else if (Im.type == IM_DT_FLOAT16)   bindings[6] = Im;
    else if (Im.type == IM_DT_FLOAT32)   bindings[7] = Im;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = Im.w;
    constants[1].i = Im.h;
    constants[2].i = Im.c;
    constants[3].i = Im.color_format;
    constants[4].i = Im.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.c;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    cmd->record_pipeline(pipeline_conv, bindings, constants, dst);
}

double ColorConvert_vulkan::Conv(const ImMat& im, ImMat & om) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_conv || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(im.dw > 0 ? im.dw : im.w, im.dh > 0 ? im.dh : im.h, om.c > 0 ? om.c : im.c, om.type, opt.blob_vkallocator);
    dst_gpu.elempack = dst_gpu.c;

    VkMat src_gpu;
    if (im.device == IM_DD_VULKAN)
    {
        src_gpu = im;
    }
    else if (im.device == IM_DD_CPU)
    {
        cmd->record_clone(im, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (om.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, om, opt);
    else if (om.device == IM_DD_VULKAN)
        om = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    om.copy_attribute(im);
    return ret;
}

// RGB <-> LAB functions
void ColorConvert_vulkan::upload_param(const VkMat& Im, VkMat& dst, ImColorXYZSystem s, int reference_white) const
{
    VkMat vkCscCoefs;
    Pipeline * pipeline = nullptr;
    if (dst.color_format == IM_CF_LAB)
    {
        // RGB -> LAB
        const ImMat cscCoefs = *xyz_color_table[0][reference_white][s];
        cmd->record_clone(cscCoefs, vkCscCoefs, opt);
        pipeline = pipeline_rgb_lab;
    }
    else if (Im.color_format == IM_CF_LAB)
    {
        // LAB-> RGB
        const ImMat cscCoefs = *xyz_color_table[1][reference_white][s];
        cmd->record_clone(cscCoefs, vkCscCoefs, opt);
        pipeline = pipeline_lab_rgb;
    }
    
    if (!pipeline || vkCscCoefs.empty()) return;
    std::vector<VkMat> bindings(9);
    if      (dst.type == IM_DT_INT8)    bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)   bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16) bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32) bindings[3] = dst;
    if      (Im.type == IM_DT_INT8)    bindings[4] = Im;
    else if (Im.type == IM_DT_INT16 || Im.type == IM_DT_INT16_BE)   bindings[5] = Im;
    else if (Im.type == IM_DT_FLOAT16) bindings[6] = Im;
    else if (Im.type == IM_DT_FLOAT32) bindings[7] = Im;
    bindings[8] = vkCscCoefs;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = Im.w;
    constants[1].i = Im.h;
    constants[2].i = Im.c;
    constants[3].i = Im.color_format;
    constants[4].i = Im.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.c;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    cmd->record_pipeline(pipeline, bindings, constants, dst);
}

double ColorConvert_vulkan::RGB2LAB(const ImMat& im_RGB, ImMat& im_LAB, ImColorXYZSystem s, int reference_white) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_rgb_lab || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(im_RGB.w, im_RGB.h, 3, IM_DT_FLOAT32, opt.blob_vkallocator);
    dst_gpu.color_format = IM_CF_LAB;

    VkMat src_gpu;
    if (im_RGB.device == IM_DD_VULKAN)
    {
        src_gpu = im_RGB;
    }
    else if (im_RGB.device == IM_DD_CPU)
    {
        cmd->record_clone(im_RGB, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, s, reference_white);

    #ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_LAB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_LAB, opt);
    else if (im_LAB.device == IM_DD_VULKAN)
        im_LAB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    return ret;
}

double ColorConvert_vulkan::LAB2RGB(const ImMat& im_LAB, ImMat& im_RGB, ImColorXYZSystem s, int reference_white) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_lab_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int channel = im_RGB.color_format == IM_CF_BGR || im_RGB.color_format == IM_CF_RGB ? 3 : 4;
    dst_gpu.create_type(im_LAB.w, im_LAB.h, channel, im_RGB.type, opt.blob_vkallocator);
    dst_gpu.color_format = im_RGB.color_format;

    VkMat src_gpu;
    if (im_LAB.device == IM_DD_VULKAN)
    {
        src_gpu = im_LAB;
    }
    else if (im_LAB.device == IM_DD_CPU)
    {
        cmd->record_clone(im_LAB, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, s, reference_white);

    #ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGB, opt);
    else if (im_RGB.device == IM_DD_VULKAN)
        im_RGB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    return ret;
}

// RGB <-> HSL/HSV functions
void ColorConvert_vulkan::upload_param(const VkMat& Im, VkMat& dst, bool b_hsl) const
{
    VkMat vkCscCoefs;
    Pipeline * pipeline = nullptr;
    if (dst.color_format == IM_CF_HSL)
    {
        pipeline = pipeline_rgb_hsl;
    }
    else if (Im.color_format == IM_CF_HSL)
    {
        pipeline = pipeline_hsl_rgb;
    }
    else if (dst.color_format == IM_CF_HSV)
    {
        pipeline = pipeline_rgb_hsv;
    }
    else if (Im.color_format == IM_CF_HSV)
    {
        pipeline = pipeline_hsv_rgb;
    }
    if (!pipeline) return;

    std::vector<VkMat> bindings(8);
    if      (dst.type == IM_DT_INT8)    bindings[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)   bindings[1] = dst;
    else if (dst.type == IM_DT_FLOAT16) bindings[2] = dst;
    else if (dst.type == IM_DT_FLOAT32) bindings[3] = dst;
    if      (Im.type == IM_DT_INT8)    bindings[4] = Im;
    else if (Im.type == IM_DT_INT16 || Im.type == IM_DT_INT16_BE)   bindings[5] = Im;
    else if (Im.type == IM_DT_FLOAT16) bindings[6] = Im;
    else if (Im.type == IM_DT_FLOAT32) bindings[7] = Im;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = Im.w;
    constants[1].i = Im.h;
    constants[2].i = Im.c;
    constants[3].i = Im.color_format;
    constants[4].i = Im.type;
    constants[5].i = dst.w;
    constants[6].i = dst.h;
    constants[7].i = dst.c;
    constants[8].i = dst.color_format;
    constants[9].i = dst.type;
    cmd->record_pipeline(pipeline, bindings, constants, dst);
}

double ColorConvert_vulkan::RGB2HSL(const ImMat& im_RGB, ImMat& im_HSL) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_rgb_hsl || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(im_RGB.w, im_RGB.h, 3, IM_DT_FLOAT32, opt.blob_vkallocator);
    dst_gpu.color_format = IM_CF_HSL;

    VkMat src_gpu;
    if (im_RGB.device == IM_DD_VULKAN)
    {
        src_gpu = im_RGB;
    }
    else if (im_RGB.device == IM_DD_CPU)
    {
        cmd->record_clone(im_RGB, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, true);

    #ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_HSL.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_HSL, opt);
    else if (im_HSL.device == IM_DD_VULKAN)
        im_HSL = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    return ret;
}

double ColorConvert_vulkan::HSL2RGB(const ImMat& im_HSL, ImMat& im_RGB) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_hsl_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int channel = im_RGB.color_format == IM_CF_BGR || im_RGB.color_format == IM_CF_RGB ? 3 : 4;
    dst_gpu.create_type(im_HSL.w, im_HSL.h, channel, im_RGB.type, opt.blob_vkallocator);
    dst_gpu.color_format = im_RGB.color_format;

    VkMat src_gpu;
    if (im_HSL.device == IM_DD_VULKAN)
    {
        src_gpu = im_HSL;
    }
    else if (im_HSL.device == IM_DD_CPU)
    {
        cmd->record_clone(im_HSL, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, true);

    #ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGB, opt);
    else if (im_RGB.device == IM_DD_VULKAN)
        im_RGB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    return ret;
}

double ColorConvert_vulkan::RGB2HSV(const ImMat& im_RGB, ImMat& im_HSV) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_rgb_hsv || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(im_RGB.w, im_RGB.h, 3, IM_DT_FLOAT32, opt.blob_vkallocator);
    dst_gpu.color_format = IM_CF_HSL;

    VkMat src_gpu;
    if (im_RGB.device == IM_DD_VULKAN)
    {
        src_gpu = im_RGB;
    }
    else if (im_RGB.device == IM_DD_CPU)
    {
        cmd->record_clone(im_RGB, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, false);

    #ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_HSV.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_HSV, opt);
    else if (im_HSV.device == IM_DD_VULKAN)
        im_HSV = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    return ret;
}

double ColorConvert_vulkan::HSV2RGB(const ImMat& im_HSV, ImMat& im_RGB) const
{
    double ret = -1.f;
    if (!vkdev || !pipeline_hsv_rgb || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    int channel = im_RGB.color_format == IM_CF_BGR || im_RGB.color_format == IM_CF_RGB ? 3 : 4;
    dst_gpu.create_type(im_HSV.w, im_HSV.h, channel, im_RGB.type, opt.blob_vkallocator);
    dst_gpu.color_format = im_RGB.color_format;

    VkMat src_gpu;
    if (im_HSV.device == IM_DD_VULKAN)
    {
        src_gpu = im_HSV;
    }
    else if (im_HSV.device == IM_DD_CPU)
    {
        cmd->record_clone(im_HSV, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, false);

    #ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (im_RGB.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, im_RGB, opt);
    else if (im_RGB.device == IM_DD_VULKAN)
        im_RGB = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
#else
    ret = 1.f;
#endif
    cmd->reset();

    return ret;
}

} // namespace ImGui 
