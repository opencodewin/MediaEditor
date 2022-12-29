#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <immat.h>
#include <ImVulkanShader.h>
#include <CopyTo_vulkan.h>
#include <ZoomInCircles_vulkan.h>

int main(int argc, char** argv)
{
    int width_a = 0, height_a = 0, component_a = 0;
    uint8_t * data_a = nullptr;
    data_a = stbi_load("/Users/dicky/Desktop/A.png", &width_a, &height_a, &component_a, 4);
    int width_b = 0, height_b = 0, component_b = 0;
    uint8_t * data_b = nullptr;
    data_b = stbi_load("/Users/dicky/Desktop/B.png", &width_b, &height_b, &component_b, 4);
    if (!data_a || !data_b)
        return -1;

    ImGui::ZoomInCircles_vulkan m_fusion(0);
    ImGui::CopyTo_vulkan m_copy(0);

    ImGui::ImMat mat_a, mat_b;
    mat_a.create_type(width_a, height_a, component_a, data_a, IM_DT_INT8);
    mat_b.create_type(width_b, height_b, component_b, data_b, IM_DT_INT8);
    ImGui::ImMat result;
    result.create_type(width_a * 4, height_a * 4, 4, IM_DT_INT8);

    ImGui::ImMat mat_t;
    mat_t.type = mat_a.type;
    for (int h = 0; h < 4; h++)
    {
        for (int w = 0; w < 4; w++)
        {
            float progress = (float)(h * 4 + w) / 15.0;
            ImPixel color(0.15f, 0.15f, 0.15f, 1.0f);
            ImPixel color2(0.6f, 0.8f, 1.0f, 1.0f);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
            m_copy.copyTo(mat_t, result, w * width_a, h * height_a);
        }
    }
    
    
    stbi_write_jpg("/Users/dicky/Desktop/C.jpg", result.w, result.h, result.c, result.data, 70);

    return 0;
}
