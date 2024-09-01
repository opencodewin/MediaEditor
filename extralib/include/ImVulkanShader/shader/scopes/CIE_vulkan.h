#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui
{
enum CieSystem : int32_t
{
    XYY = 0,
    UCS,
    LUV,
    NB_CIE
};

enum ColorsSystems : int32_t
{
    NTSCsystem = 0,
    EBUsystem,
    SMPTEsystem,
    SMPTE240Msystem,
    APPLEsystem,
    wRGBsystem,
    CIE1931system,
    Rec709system,
    Rec2020system,
    DCIP3,
    NB_CS
};

class VKSHADER_API CIE_vulkan
{
public:
    CIE_vulkan(int gpu = 0);
    ~CIE_vulkan();

    void SetParam(int _color_system, int _cie, int _size, int _gamuts, float _contrast, bool _correct_gamma);

    double scope(const ImMat& src, ImMat& dst, float intensity = 0.01, bool show_color = true);

public:
    void GetWhitePoint(ColorsSystems cs, float w, float h, float* x, float* y);
    void GetRedPoint(ColorsSystems cs, float w, float h, float* x, float* y);
    void GetBluePoint(ColorsSystems cs, float w, float h, float* x, float* y);
    void GetGreenPoint(ColorsSystems cs, float w, float h, float* x, float* y);

private:
    ImGui::VulkanDevice* vkdev      {nullptr};
    ImGui::Option opt;
    ImGui::VkCompute * cmd          {nullptr};
    ImGui::Pipeline* pipe           {nullptr};
    ImGui::Pipeline* pipe_set       {nullptr};
    ImGui::Pipeline* pipe_merge     {nullptr};

    ImGui::ImMat xyz_matrix;
    ImGui::ImMat xyz_imatrix;
    ImGui::ImMat backgroud;

    ImGui::VkMat xyz_matrix_gpu;
    ImGui::VkMat backgroud_gpu;
    ImGui::VkMat buffer;

    int rgba_map[4] {2, 1, 0, 3};
    int size {512};
    int color_system {Rec709system};
    int cie {XYY};
    int gamuts {Rec2020system};
    float contrast {0.75};
    bool correct_gamma {false};

private:
    void draw_backbroud();
    void upload_param(const VkMat& src, VkMat& dst, float intensity, bool show_color);
};
} // namespace ImGui