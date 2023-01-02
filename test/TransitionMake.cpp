#include <stdio.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <application.h>
#include <ImGuiFileDialog.h>
//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include <stb_image_write.h>
#include <immat.h>
#include <ImVulkanShader.h>
#include "AlphaBlending_vulkan.h"
#include "BookFlip_vulkan.h"
#include "Bounce_vulkan.h"
#include "BowTie_vulkan.h"
#include "Burn_vulkan.h"
#include "BurnOut_vulkan.h"
#include "ButterflyWave_vulkan.h"
#include "CannabisLeaf_vulkan.h"
#include "CircleBlur_vulkan.h"
#include "CircleCrop_vulkan.h"
#include "ColorPhase_vulkan.h"
#include "ColourDistance_vulkan.h"
#include "CrazyParametric_vulkan.h"
#include "Crosshatch_vulkan.h"
#include "CrossWarp_vulkan.h"
#include "CrossZoom_vulkan.h"
#include "Cube_vulkan.h"
#include "DirectionalWarp_vulkan.h"
#include "DoomScreen_vulkan.h"
#include "Doorway_vulkan.h"
#include "Dreamy_vulkan.h"
#include "DreamyZoom_vulkan.h"
#include "Flyeye_vulkan.h"
#include "GlitchDisplace_vulkan.h"
#include "GlitchMemories_vulkan.h"
#include "GridFlip_vulkan.h"
#include "Heart_vulkan.h"
#include "Hexagonalize_vulkan.h"
#include "KaleidoScope_vulkan.h"
#include "LuminanceMelt_vulkan.h"
#include "Morph_vulkan.h"
#include "Mosaic_vulkan.h"
#include "MultiplyBlend_vulkan.h"
#include "PageCurl_vulkan.h"
#include "Perlin_vulkan.h"
#include "Pinwheel_vulkan.h"
#include "Pixelize_vulkan.h"
#include "Polar_vulkan.h"
#include "PolkaDots_vulkan.h"
#include "Radial_vulkan.h"
#include "RandomSquares_vulkan.h"
#include "Ripple_vulkan.h"
#include "RotateScale_vulkan.h"
#include "SimpleZoom_vulkan.h"
#include "SquaresWire_vulkan.h"
#include "Squeeze_vulkan.h"
#include "StereoViewer_vulkan.h"
#include "Swap_vulkan.h"
#include "Swirl_vulkan.h"
#include "WaterDrop_vulkan.h"
#include "Wind_vulkan.h"
#include "WindowBlinds_vulkan.h"
#include "WindowSlice_vulkan.h"
#include "ZoomInCircles_vulkan.h"
#include <CopyTo_vulkan.h>
#include <unistd.h>
std::string g_source_1;
std::string g_source_2;
std::string g_dist;
std::string g_source_name_1;
std::string g_source_name_2;
std::string g_dist_name;

ImGui::ImMat g_mat_1;
ImGui::ImMat g_mat_2;
ImGui::ImMat g_mat_d;
ImTextureID g_texture_1 = nullptr;
ImTextureID g_texture_2 = nullptr;
ImTextureID g_texture_d = nullptr;

static const char* fusion_items[] = {
    "AlphaBlending",
    "BookFlip",
    "Bounce",
    "BowTie",
    "Burn",
    "BurnOut",
    "ButterflyWave",
    "CannabisLeaf",
    "CircleBlur",
    "CircleCrop",
    "ColorPhase",
    "ColourDistance",
    "CrazyParametric",
    "Crosshatch",
    "CrossWarp",
    "CrossZoom",
    "Cube",
    "DirectionalWarp",
    "DoomScreen",
    "Doorway",
    "Dreamy",
    "DreamyZoom",
    "Flyeye",
    "GlitchDisplace",
    "GlitchMemories",
    "GridFlip",
    "Heart",
    "Hexagonalize",
    "KaleidoScope",
    "LuminanceMelt",
    "Morph",
    "Mosaic",
    "MultiplyBlend",
    "PageCurl",
    "Perlin",
    "Pinwheel",
    "Pixelize",
    "Polar",
    "PolkaDots",
    "Radial",
    "RandomSquares",
    "Ripple",
    "RotateScale",
    "SimpleZoom",
    "SquaresWire",
    "Squeeze",
    "StereoViewer",
    "Swap",
    "Swirl",
    "WaterDrop",
    "Wind",
    "WindowBlinds",
    "WindowSlice",
    "ZoomInCircles"
};

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2& pos, ImVec2& size, float& offset_x, float& offset_y, float& tf_x, float& tf_y, bool bLandscape = true)
{
    if (texture)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
        bool bViewisLandscape = size.x >= size.y ? true : false;
        bViewisLandscape |= bLandscape;
        float aspectRatio = (float)ImGui::ImGetTextureWidth(texture) / (float)ImGui::ImGetTextureHeight(texture);
        bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? size.y : size.x;
        float adj_h = bNeedChangeScreenInfo ? size.x : size.y;
        float adj_x = adj_h * aspectRatio;
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        tf_x = (size.x - adj_x) / 2.0;
        tf_y = (size.y - adj_y) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        draw_list->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_x, offset_y + adj_y),
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
        tf_x = offset_x + adj_x;
        tf_y = offset_y + adj_y;
    }
}

static void load_image(std::string path, ImGui::ImMat & mat)
{
    int width = 0, height = 0, component = 0;
    uint8_t * data = nullptr;
    data = stbi_load(path.c_str(), &width, &height, &component, 4);
    if (data)
    {
        mat.release();
        mat.create_type(width, height, component, data, IM_DT_INT8);
    }
}
// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Transition Maker";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1680;
    property.height = 900;

    // get opt
    if (property.argc > 1 && property.argv)
    {
        int o = -1;
        const char *option_str = "1:2:o:";
        while ((o = getopt(property.argc, property.argv, option_str)) != -1)
        {
            switch (o)
            {
                case '1': g_source_1 = std::string(optarg); break;
                case '2': g_source_2 = std::string(optarg); break;
                case 'o': g_dist = std::string(optarg); break;
                default: break;
            }
        }
        if (!g_source_1.empty())
        {
            auto pos = g_source_1.find_last_of("\\/");
            if (pos != std::string::npos)
                g_source_name_1 = g_source_1.substr(pos + 1);
            load_image(g_source_1, g_mat_1);
        }
        if (!g_source_2.empty())
        {
            auto pos = g_source_2.find_last_of("\\/");
            if (pos != std::string::npos)
                g_source_name_2 = g_source_2.substr(pos + 1);
            load_image(g_source_2, g_mat_2);
        }
        if (!g_dist.empty())
        {
            auto pos = g_dist.find_last_of("\\/");
            if (pos != std::string::npos)
                g_dist_name = g_dist.substr(pos + 1);
        }
    }
}

void Application_SetupContext(ImGuiContext* ctx)
{
#ifdef USE_BOOKMARK
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

void Application_Initialize(void** handle)
{

}

void Application_Finalize(void** handle)
{
    if (g_texture_1) ImGui::ImDestroyTexture(g_texture_1);
    if (g_texture_2) ImGui::ImDestroyTexture(g_texture_2);
    if (g_texture_d) ImGui::ImDestroyTexture(g_texture_d);
}

bool Application_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();
    static int fusion_index = 0;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

    const char *filters = "Image Files(*.png){.png,.PNG},.*";
    ImVec2 minSize = ImVec2(600, 800);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowBookmark;
    if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Source One File"))
            ImGuiFileDialog::Instance()->OpenDialog("##MediaSourceDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Source One File", 
                                                    filters, 
                                                    g_source_1.empty() ? "." : g_source_1,
                                                    1, IGFDUserDatas("Source 1"), vflags);
    ImGui::SameLine(0);
    ImGui::TextUnformatted(g_source_name_1.c_str());

    if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Source Two File"))
            ImGuiFileDialog::Instance()->OpenDialog("##MediaSourceDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Source Two File", 
                                                    filters, 
                                                    g_source_2.empty() ? "." : g_source_2,
                                                    1, IGFDUserDatas("Source 2"), vflags);
    ImGui::SameLine(0);
    ImGui::TextUnformatted(g_source_name_2.c_str());

    if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Dist File"))
            ImGuiFileDialog::Instance()->OpenDialog("##MediaDistDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Dist File", 
                                                    filters, 
                                                    g_dist.empty() ? "." : g_dist,
                                                    1, IGFDUserDatas("Dist"), vflags | ImGuiFileDialogFlags_ConfirmOverwrite);
    ImGui::SameLine(0);
    ImGui::TextUnformatted(g_dist_name.c_str());

    ImGui::Combo("Fusion", &fusion_index, fusion_items, IM_ARRAYSIZE(fusion_items));

    if (!g_mat_1.empty() && !g_texture_1) ImGui::ImMatToTexture(g_mat_1, g_texture_1);
    if (!g_mat_2.empty() && !g_texture_2) ImGui::ImMatToTexture(g_mat_2, g_texture_2);

    auto draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 FirstVideoPos = window_pos + ImVec2(4, 4);
    ImVec2 FirstVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y / 4);
    ImRect FirstVideoRect(FirstVideoPos, FirstVideoPos + FirstVideoSize);
    ImVec2 SecondVideoPos = window_pos + ImVec2(window_size.x / 2 + 4, 4);
    ImVec2 SecondVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y / 4);
    ImRect SecondVideoRect(SecondVideoPos, SecondVideoPos + SecondVideoSize);

    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;
    // Draw source 1
    if (g_texture_1)
    {
        ShowVideoWindow(draw_list, g_texture_1, FirstVideoPos, FirstVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
    }

    // Draw source 2
    if (g_texture_2)
    {
        ShowVideoWindow(draw_list, g_texture_2, SecondVideoPos, SecondVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
    }

    // handle file open
    if (ImGuiFileDialog::Instance()->Display("##MediaSourceDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk() == true)
        {
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("Source 1") == 0)
            {
                g_source_1 = ImGuiFileDialog::Instance()->GetFilePathName();
                g_source_name_1 = ImGuiFileDialog::Instance()->GetCurrentFileName();
                load_image(g_source_1, g_mat_1);
                if (g_texture_1) { ImGui::ImDestroyTexture(g_texture_1); g_texture_1 = nullptr; }
            }
            else if (userDatas.compare("Source 2") == 0)
            {
                g_source_2 = ImGuiFileDialog::Instance()->GetFilePathName();
                g_source_name_2 = ImGuiFileDialog::Instance()->GetCurrentFileName();
                load_image(g_source_2, g_mat_2);
                if (g_texture_2) { ImGui::ImDestroyTexture(g_texture_2); g_texture_2 = nullptr; }
            }
        }
        // close
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("##MediaDistDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk() == true)
        {
            g_dist = ImGuiFileDialog::Instance()->GetFilePathName();
            g_dist_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
        }
        // close
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::End();
    if (app_will_quit)
    {
        app_done = true;
    }
    return app_done;
}
/*
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
*/