#pragma once
#include "immat.h"
#include "Filter2DS_vulkan.h"
#include <string>

namespace ImGui
{
class VKSHADER_API GaussianBlur_vulkan : public Filter2DS_vulkan
{
public:
    GaussianBlur_vulkan(int gpu = 0);
    ~GaussianBlur_vulkan();
    void SetParam(int _blurRadius, float _sigma);

private:
    int blurRadius {3};
    float sigma {0.0f};
    void prepare_kernel();
};
} // namespace ImGui

