#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API AlphaBlending_vulkan
{
public:
    AlphaBlending_vulkan(int gpu = -1);
    ~AlphaBlending_vulkan();

    // alpha blending src1 with src2 to dst, which dst is same size as src2, algorithm is 
    // src2.rgb * (1 - src1.a) + src1.rgb * src1.a, dst alpha is src2.a
    // src1 will copy to dst at (x,y), -src1.w < x src2.w -src1.h < y < src2.h 
    double blend(const ImMat& src1, const ImMat& src2, ImMat& dst, int x = 0, int y = 0) const;

    // alpha blending src1 with src2 to dst, which dst is same size as src2, algorithm is
    // src2.rgb * (1 - alpha) + src1.rgb * src1.a * aplha, dst alpha is src2.a
    double blend(const ImMat& src1, const ImMat& src2, ImMat& dst, float alpha, int x = 0, int y = 0) const;

    // alpha blending src1 and src2 using alpha from argument 'alpha',
    // dst(x,y) = src2(x,y)+src1(x-offx,y-offy)*alpha
    // argument 'alpha' must be IM_DT_FLOAT32, channel == 1
    void blend(const ImMat& src1, const ImMat& src2, const ImMat& alpha, ImMat& dst, int offx = 0, int offy = 0) const;

    // overlay picture 'overlayImg' on top of 'baseImg' with opacity computation,
    // color_mix_alpha = overlayImg(x-offx,y-offy).alpha * overlayAlpha
    // dst(x,y).rgb = baseImg(x,y).rgb * (1-color_mix_alpha) + overlayImg(x-offx,y-offy).rgb * color_mix_alpha
    // dst(x,y).alpha = 1 - (1 - baseImg(x,y).alpha) * (1 - color_mix_alpha)
    bool overlay(const ImMat& baseImg, const ImMat& overlayImg, ImMat& dst, float overlayAlpha, int offx = 0, int offy = 0) const;

    // alpha blending src to dst using alpha from argument 'alpha',
    // dst(x,y) = src(x-offx,y-offy)*alpha
    // argument 'alpha' must be IM_DT_FLOAT32, channel == 1
    void blendto(const ImMat& src, const ImMat& alpha, ImMat& dst, int offx = 0, int offy = 0) const;

    // alpha mask src with alpha from argument 'alpha',
    // dst(x,y) = src(x,y) + alpha
    // argument 'alpha' must be IM_DT_FLOAT32, channel == 1
    void mask(const ImMat& src, const ImMat& alpha, ImMat& dst) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    Pipeline * pipe_alpha     {nullptr};
    Pipeline * pipe_alpha_mat {nullptr};
    Pipeline * pipe_alpha_mat_to {nullptr};
    Pipeline * pipe_alpha_mat_mask {nullptr};
    Pipeline * pipe_overlay   {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, int x, int y) const;
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float alpha, int x, int y) const;
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, const VkMat& alpha, int x, int y) const;
    void upload_param_overlay(const VkMat& src1, const VkMat& src2, VkMat& dst, float alpha, int x, int y) const;
    void upload_param_to(const VkMat& src, const VkMat& alpha, VkMat& dst, int x, int y) const;
    void upload_param_mask(const VkMat& src, const VkMat& alpha, VkMat& dst) const;
};
} // namespace ImGui 