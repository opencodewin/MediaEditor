#include <stdio.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <immat.h>
#include <libdither.h>
#include <application.h>

#if IMGUI_ICONS
#define ICON_RESET u8"\ue042"
#else
#define ICON_RESET "r"
#endif

const char* Dot_Diffusion_items[] = {
    "Knuth",
    "Mini-Knuth",
    "Optimized Knuth",
    "Mese and Vaidyanathan 8x8",
    "Mese and Vaidyanathan 16x16",
    "Guo Liu 8x8",
    "Guo Liu 16x16",
    "Spiral",
    "Inverted Spiral"
};

const char* Error_Diffusion_items[] = {
    "Xot",
    "Diagonal Diffusion",
    "Floyd Steinberg",
    "Shiau Fan 3",
    "Shiau Fan 2",
    "Shiau Fan 1",
    "Stucki",
    "1 Dimensional",
    "2 Dimensional",
    "Fake Floyd Steinberg",
    "Jarvis-Judice-Ninke",
    "Atkinson",
    "Burkes",
    "Sierra 3",
    "Sierra 2-Row",
    "Sierra Lite",
    "Steve Pigeon",
    "Robert Kist",
    "Stevenson-Arce"
};

const char* Ordered_Dithering_item[] = {
    "Blue Noise",
    "Bayer 2x2",
    "Bayer 3x3",
    "Bayer 4x4",
    "Bayer 8x8",
    "Bayer 16x16",
    "Bayer 32x32",
    "Dispersed Dots 1",
    "Dispersed Dots 2",
    "Ulichney Void Dispersed Dots",
    "Non-Rectangular 1",
    "Non-Rectangular 2",
    "Non-Rectangular 3",
    "Non-Rectangular 4",
    "Ulichney Bayer 5x5",
    "Ulichney",
    "Clustered Dot 1",
    "Clustered Dot 2",
    "Clustered Dot 3",
    "Clustered Dot 4",
    "Clustered Dot 5",
    "Clustered Dot 6",
    "Clustered Dot 7",
    "Clustered Dot 8",
    "Clustered Dot 9",
    "Clustered Dot 10",
    "Clustered Dot 11",
    "Central White Point",
    "Balanced Central White Point",
    "Diagonal Ordered",
    "Ulichney Clustered Dot",
    "ImageMagick 5x5 Circle",
    "ImageMagick 6x6 Circle",
    "ImageMagick 7x7 Circle",
    "ImageMagick 4x4 45-degrees",
    "ImageMagick 6x6 45-degrees",
    "ImageMagick 8x8 45-degrees",
    "ImageMagick 4x4",
    "ImageMagick 6x6",
    "ImageMagick 8x8",
    "Variable 2x2 Matix",
    "Variable 4x4 Matix",
    "Interleaved Gradient Noise",
    //"Blue Noise image based"
};

const char* Variable_Error_Diffusion_items[] = {
    "Ostromoukhov",
    "Zhou Fang"
};

const char* Riemersma_Dithering_items[] = {
    "Hilbert curve",
    "Hilbert Mod curve",
    "Peano curve",
    "Fass-0 curve",
    "Fass-1 curve",
    "Fass-2 curve",
    "Gosper curve",
    "Fass Spiral"
};

const char* Pattern_items[] = {
    "2x2 pattern",
    "3x3 pattern v1",
    "3x3 pattern v2",
    "3x3 pattern v3",
    "4x4 pattern",
    "5x2 pattern"
};

const char* Dot_Lippens_items[] = {
    "v1",
    "v2",
    "v3",
    "Guo Liu 16x16",
    "Mese and Vaidyanathan 16x16",
    "Knuth"
};

const char* Dither_items[] = {
    "Grid Dithering",
    "Dot Diffusion",
    "Error Diffusion",
    "Ordered Dithering",
    "Variable Error Diffusion",
    "Thresholding",
    "Direct Binary Search",
    "Kacker and Allebach",
    "Riemersma Dithering",
    "Pattern Dithering",
    "Dot Lippens"
};

// Application Framework Functions
static void Dither_SetupContext(ImGuiContext *ctx, void * handle, bool in_splash)
{
#ifdef USE_PLACES_FEATURE
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, const char *name) -> void *
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, void *entry, const char *line) -> void
    {
        IGFD::FileDialog *dialog = (IGFD::FileDialog *)entry;
        if (dialog) dialog->DeserializePlaces(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *out_buf)
    {
        ImGuiContext &g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializePlaces();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

static void Dither_Initialize(void **handle)
{
    auto dither_version = libdither_version();
    auto dither_mat = dither_test_image();
    fprintf(stderr, "Dither version:%s\n", dither_version);
}

static void Dither_Finalize(void **handle)
{
}

static bool Dither_Frame(void *handle, bool app_will_quit)
{
    bool app_done = false;
    static std::string m_file_path;
    static int m_dither_type = 0;
    static int m_dot_diffusion_type = 0;
    static int m_err_diffusion_type = 0;
    static int m_ord_dithering_type = 0;
    static int m_verr_diffusion_type = 0;
    static int m_riemersma_type = 0;
    static int m_pattern_type = 0;
    static int m_dot_lippens_type = 0;

    static int m_grid_width = 4;
    static int m_grid_height = 4;
    static int m_grid_min_pixels = 0;
    static bool m_grid_alt_algorithm = true;

    static float m_err_diffusion_sigma = 0.f;
    static bool m_err_diffusion_serpentine = true;

    static float m_ord_diffusion_sigma = 0.f;
    static int m_ord_diffusion_step = 55;
    static float m_ord_diffusion_a = 52.9829189f;
    static float m_ord_diffusion_b = 0.06711056f;
    static float m_ord_diffusion_c = 0.00583715;

    static bool m_verr_diffusion_serpentine = true;

    static float m_threshold_thres = 0.5f;
    static bool m_threshold_auto = true;
    static float m_threshold_noise = 0.55f;

    static int m_dbs_formula = 1;

    static bool m_kacker_allebach_random = true;

    static bool m_riemersma_modified = true;

    static ImGui::ImMat m_mat;
    static ImGui::ImMat m_gray;
    static ImGui::ImMat m_result;
    static ImTextureID m_texture = 0;
    static ImTextureID m_bm_texture = 0;

    auto process_image = [&](std::string output_path = std::string())
    {
        ImGui::ImDestroyTexture(&m_bm_texture);
        m_result.create_type(m_gray.w, m_gray.h, 1, IM_DT_INT8);
        switch (m_dither_type)
        {
            case 0 : 
                grid_dither(m_gray, m_grid_width, m_grid_height, m_grid_min_pixels, m_grid_alt_algorithm, m_result);
            break;
            case 1 : 
                dot_diffusion_dither(m_gray, (DD_TYPE)m_dot_diffusion_type, m_result);
            break;
            case 2 : 
                error_diffusion_dither(m_gray, (ED_TYPE)m_err_diffusion_type, m_err_diffusion_serpentine, m_err_diffusion_sigma, m_result);
            break;
            case 3 : 
                ordered_dither(m_gray, (OD_TYPE)m_ord_dithering_type, m_ord_diffusion_sigma, m_result, ImGui::ImMat(), m_ord_diffusion_step, ImVec4(m_ord_diffusion_a, m_ord_diffusion_b, m_ord_diffusion_c, 0));
            break;
            case 4 : 
                variable_error_diffusion_dither(m_gray, (VD_TYPE)m_verr_diffusion_type, m_verr_diffusion_serpentine, m_result);
            break;
            case 5 : 
                threshold_dither(m_gray, m_threshold_auto ? auto_threshold(m_gray) : m_threshold_thres, m_threshold_noise, m_result);
            break;
            case 6 : 
                dbs_dither(m_gray, m_dbs_formula, m_result);
            break;
            case 7 : 
                kallebach_dither(m_gray, m_kacker_allebach_random, m_result);
            break;
            case 8 : 
                riemersma_dither(m_gray, (RD_TYPE)m_riemersma_type, m_riemersma_modified, m_result);
            break;
            case 9 : 
                pattern_dither(m_gray, (PD_TYPE)m_pattern_type, m_result);
            break;
            case 10 : 
                dotlippens_dither(m_gray, (LP_TYPE)m_dot_lippens_type, m_result);
            break;
            default : break;
        }
        if (!m_result.empty())
        {
#ifdef __APPLE__
            ImGui::ImMat mat_RGB(m_result.w, m_result.h, 4, 1u, 4);
            for (int y = 0; y < m_result.h; y++)
            {
                for (int x = 0; x < m_result.w; x++)
                {
                    unsigned char val = m_result.at<unsigned char>(x, y);
                    mat_RGB.at<unsigned char>(x, y, 0) =
                    mat_RGB.at<unsigned char>(x, y, 1) =
                    mat_RGB.at<unsigned char>(x, y, 2) = val;
                    mat_RGB.at<unsigned char>(x, y, 3) = 0xFF;
                }
            }
            ImGui::ImMatToTexture(mat_RGB, m_bm_texture);
#else
            ImGui::ImMatToTexture(m_result, m_bm_texture);
#endif
        }
    };

    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Dither Test", nullptr, flags);
    // control panel
    ImGui::BeginChild("##Dither_Config", ImVec2(400, ImGui::GetWindowHeight() - 60), true);
    {
        auto draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Open File...", ImVec2(160, 32)))
        {
            IGFD::FileDialogConfig config;
            config.path = m_file_path.empty() ? "." : m_file_path;
            config.countSelectionMax = 1;
            config.userDatas = IGFDUserDatas("OpenImage");
            config.flags = ImGuiFileDialogFlags_OpenFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("##DitherFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose File",
                                                    image_filter.c_str(),
                                                    config);
        }
        ImGui::ShowTooltipOnHover("File Path:%s", m_file_path.c_str());
        ImGui::Separator();

        if (m_texture) ImGui::ImShowVideoWindow(draw_list, m_texture, ImGui::GetCursorScreenPos(), ImVec2(window_size.x, window_size.x / 2));
        else ImGui::InvisibleButton("##dummy image", ImVec2(window_size.x, window_size.x / 2));

        ImGui::Separator();
        ImGui::BeginDisabled(m_mat.empty());
        if (ImGui::Combo("Type", &m_dither_type, Dither_items, IM_ARRAYSIZE(Dither_items)))
        {
            process_image();
        }
        bool changed = false;
        switch (m_dither_type)
        {
            case 0 : 
            {
                changed |= ImGui::SliderInt("Grid width", &m_grid_width, 1, 20);
                changed |= ImGui::SliderInt("Grid height", &m_grid_height, 1, 20);
                changed |= ImGui::SliderInt("Min pixels", &m_grid_min_pixels, 0, 20);
                changed |= ImGui::Checkbox("Alt Algorithm", &m_grid_alt_algorithm);
            }
            break;
            case 1 : 
                changed |= ImGui::Combo("Dot Diffusion Type", &m_dot_diffusion_type, Dot_Diffusion_items, IM_ARRAYSIZE(Dot_Diffusion_items));
            break;
            case 2 : 
                changed |= ImGui::Combo("Error Diffusion Type", &m_err_diffusion_type, Error_Diffusion_items, IM_ARRAYSIZE(Error_Diffusion_items));
                changed |= ImGui::SliderFloat("Jitter##error", &m_err_diffusion_sigma, 0.f, 1.f, "%.2f");
                changed |= ImGui::Checkbox("Serpentine", &m_err_diffusion_serpentine);
            break;
            case 3 : 
                changed |= ImGui::Combo("Ordered Dithering Type", &m_ord_dithering_type, Ordered_Dithering_item, IM_ARRAYSIZE(Ordered_Dithering_item));
                changed |= ImGui::SliderFloat("Jitter##order", &m_ord_diffusion_sigma, 0.f, 1.f, "%.2f");
                if (m_ord_dithering_type >= 40 && m_ord_dithering_type < 43)
                {
                    changed |= ImGui::SliderInt("Step", &m_ord_diffusion_step, 1, 100);
                    if (m_ord_dithering_type == 42)
                    {
                        changed |= ImGui::SliderFloat("A", &m_ord_diffusion_a, 0.f, 100.f, "%.3f");
                        changed |= ImGui::SliderFloat("B", &m_ord_diffusion_b, 0.f, 1.f, "%.3f");
                        changed |= ImGui::SliderFloat("C", &m_ord_diffusion_c, 0.f, 1.f, "%.3f");
                    }
                }
            break;
            case 4 : 
                changed |= ImGui::Combo("Variable Error Diffusion Type", &m_verr_diffusion_type, Variable_Error_Diffusion_items, IM_ARRAYSIZE(Variable_Error_Diffusion_items));
                changed |= ImGui::Checkbox("Serpentine", &m_verr_diffusion_serpentine);
            break;
            case 5 : 
                changed |= ImGui::SliderFloat("Noise", &m_threshold_noise, 0.f, 1.f, "%.3f");
                changed |= ImGui::Checkbox("Auto threshold", &m_threshold_auto);
                ImGui::BeginDisabled(m_threshold_auto);
                changed |= ImGui::SliderFloat("Threshold", &m_threshold_thres, 0.f, 1.f, "%.3f");
                ImGui::EndDisabled();
            break;
            case 6 : 
                changed |= ImGui::SliderInt("Formula", &m_dbs_formula, 1, 7);
            break;
            case 7 : 
                changed |= ImGui::Checkbox("Random", &m_kacker_allebach_random);
            break;
            case 8 : 
                changed |= ImGui::Combo("Riemersma Dithering Type", &m_riemersma_type, Riemersma_Dithering_items, IM_ARRAYSIZE(Riemersma_Dithering_items));
                changed |= ImGui::Checkbox("Modified", &m_riemersma_modified);
            break;
            case 9 : 
                changed |= ImGui::Combo("Pattern Dithering Type", &m_pattern_type, Pattern_items, IM_ARRAYSIZE(Pattern_items));
            break;
            case 10 : 
                changed |= ImGui::Combo("Dot Lippens Type", &m_dot_lippens_type, Dot_Lippens_items, IM_ARRAYSIZE(Dot_Lippens_items));
            break;
            default : break;
        }
        if (changed) process_image();
        ImGui::EndDisabled();
    }

    ImGui::EndChild();

    // run panel
    ImGui::SameLine();
    ImGui::BeginChild("##Potrace_Result", ImVec2(ImGui::GetWindowWidth() - 400 - 30, ImGui::GetWindowHeight() - 60), true);
    {
        auto draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetContentRegionAvail();
        if (m_bm_texture)
        {
            ImGui::ImShowVideoWindow(draw_list, m_bm_texture, window_pos, window_size / 4);
            std::string dialog_id = "##TextureFileDlgKey" + std::to_string((long long)m_bm_texture);
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem((std::string(ICON_FA_IMAGE) + " Save Texture to File").c_str()))
                {
                    IGFD::FileDialogConfig config;
                    config.path = ".";
                    config.countSelectionMax = 1;
                    config.flags = ImGuiFileDialogFlags_SaveFile_Default;
                    ImGuiFileDialog::Instance()->OpenDialog(dialog_id.c_str(), ICON_IGFD_FOLDER_OPEN " Choose File", 
                                                            image_filter.c_str(),
                                                            config);
                }
                ImGui::EndPopup();
            }
            ImVec2 minSize = ImVec2(600, 300);
            ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
            if (ImGuiFileDialog::Instance()->Display(dialog_id.c_str(), ImGuiWindowFlags_NoCollapse, minSize, maxSize))
            {
                if (ImGuiFileDialog::Instance()->IsOk() == true)
                {
                    std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                    ImGui::ImTextureToFile(m_bm_texture, file_path);
                }
                ImGuiFileDialog::Instance()->Close();
            }
        }
    }
    ImGui::EndChild();

    // File Dialog
    ImVec2 minSize = ImVec2(600, 400);
    ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (ImGuiFileDialog::Instance()->Display("##DitherFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("OpenImage") == 0)
            {
                m_file_path = file_path;
                ImGui::ImLoadImageToMat(m_file_path.c_str(), m_mat);
                if (!m_mat.empty())
                {
                    ImGui::ImDestroyTexture(&m_texture);
                    ImGui::ImMatToTexture(m_mat, m_texture);
                    int width = m_mat.w & 0xFFFFFFFC;
                    m_gray.create_type(width, m_mat.h, IM_DT_INT8);
                    for (int y = 0; y < m_mat.h ; y++)
                    {
                        for (int x = 0; x < width; x++)
                        {
                            float R = m_mat.at<uint8_t>(x, y, 0);
                            float G = m_mat.at<uint8_t>(x, y, 1);
                            float B = m_mat.at<uint8_t>(x, y, 2);
                            float gray = R * 0.299 + G * 0.587 + B * 0.114;
                            m_gray.at<uint8_t>(x, y) = (uint8_t)gray;
                        }
                    }
                    process_image();
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (app_will_quit)
    {
        ImGui::ImDestroyTexture(&m_texture);
        ImGui::ImDestroyTexture(&m_bm_texture);
        app_done = true;
    }
    ImGui::End();
    return app_done;
}

void Application_Setup(ApplicationWindowProperty &property)
{
    property.name = "Dither Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    // property.power_save = false;
    property.width = 1680;
    property.height = 900;
    property.application.Application_SetupContext = Dither_SetupContext;
    property.application.Application_Initialize = Dither_Initialize;
    property.application.Application_Finalize = Dither_Finalize;
    property.application.Application_Frame = Dither_Frame;
}