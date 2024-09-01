#pragma once
#include "immat.h"
#include "Filter2D_vulkan.h"
#include <string>

namespace ImGui
{
class VKSHADER_API  Laplacian_vulkan : public Filter2D_vulkan
{
public:
    Laplacian_vulkan(int gpu = 0);
    ~Laplacian_vulkan();
    void SetParam(int _Strength);

private:
    int Strength {2};
    void prepare_kernel();
};
} // namespace ImGui
