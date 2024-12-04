#pragma once
#include <imgui.h>
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include <immat.h>

class CustomShader
{
public:
    CustomShader(const std::string shader, int gpu = -1, bool fp16 = true);
    ~CustomShader();

    double filter(const ImGui::ImMat& src, const ImGui::ImMat& src2, ImGui::ImMat& dst, std::vector<float>& params);

private:
    ImGui::VulkanDevice* vkdev {nullptr};
    ImGui::Option opt;
    ImGui::Pipeline* pipe {nullptr};
    ImGui::VkCompute * cmd {nullptr};
private:
    void upload_param(const ImGui::VkMat& src, const ImGui::VkMat& src2, ImGui::VkMat& dst, std::vector<float>& params);
};