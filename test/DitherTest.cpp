#include <stdio.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <application.h>
#include <ImGuiFileDialog.h>
#include <immat.h>
#include "libdither.h"

#if IMGUI_ICONS
#define ICON_RESET u8"\ue042"
#else
#define ICON_RESET "r"
#endif

std::string image_file_dis = "*.bmp,*.png *.gif *.jpg *.jpeg *.tiff *.webp";
std::string image_file_suffix = ".bmp,.png,.gif,.jpg,.jpeg,.tiff,.webp";
std::string image_filter = "Image files (" + image_file_dis + "){" + image_file_suffix + "}" + "," + ".*";

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
static void Dither_SetupContext(ImGuiContext *ctx, bool in_splash)
{
#ifdef USE_BOOKMARK
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
        dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *out_buf)
    {
        ImGuiContext &g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

static void Dither_Finalize(void **handle)
{
}

static void Grid_Ditherer(DitherImage* input, int width, int height, int min_pixel, bool alt, ImGui::ImMat& output)
{
    if (!input) return;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    grid_dither(input, width, height, min_pixel, alt, (uint8_t *)output.data);
}

static void Dot_Diffusion(DitherImage* input, int type, ImGui::ImMat& output)
{
    DotClassMatrix* dcm = nullptr;
    DotDiffusionMatrix* ddm = nullptr;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    switch(type)
    {
        case 0 :
            ddm = get_default_diffusion_matrix();
            dcm = get_knuth_class_matrix();
            break;
        case 1 :
            ddm = get_default_diffusion_matrix();
            dcm = get_mini_knuth_class_matrix();
            break;
        case 2 : 
            ddm = get_default_diffusion_matrix();
            dcm = get_optimized_knuth_class_matrix();
            break;
        case 3 : 
            ddm = get_default_diffusion_matrix();
            dcm = get_mese_8x8_class_matrix();
            break;
        case 4 : 
            ddm = get_default_diffusion_matrix();
            dcm = get_mese_16x16_class_matrix();
            break;
        case 5 : 
            ddm = get_guoliu8_diffusion_matrix();
            dcm = get_guoliu_8x8_class_matrix();
            break;
        case 6 : 
            ddm = get_guoliu16_diffusion_matrix();
            dcm = get_guoliu_16x16_class_matrix();
            break;
        case 7:
            ddm = get_guoliu8_diffusion_matrix();
            dcm = get_spiral_class_matrix();
            break;
        case 8:
            ddm = get_guoliu8_diffusion_matrix();
            dcm = get_spiral_inverted_class_matrix();
            break;
        default: break;
    }
    dot_diffusion_dither(input, ddm, dcm, (uint8_t *)output.data);
    DotClassMatrix_free(dcm);
    DotDiffusionMatrix_free(ddm);
}

static void Error_Diffusion(DitherImage* input, int type, float sigma, bool serpentine, ImGui::ImMat& output)
{
    ErrorDiffusionMatrix* em = nullptr;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    switch (type)
    {
        case 0 : em = get_xot_matrix(); break;
        case 1 : em = get_diagonal_matrix(); break;
        case 2 : em = get_floyd_steinberg_matrix(); break;
        case 3 : em = get_shiaufan3_matrix(); break;
        case 4 : em = get_shiaufan2_matrix(); break;
        case 5 : em = get_shiaufan1_matrix(); break;
        case 6 : em = get_stucki_matrix(); break;
        case 7 : em = get_diffusion_1d_matrix(); break;
        case 8 : em = get_diffusion_2d_matrix(); break;
        case 9 : em = get_fake_floyd_steinberg_matrix(); break;
        case 10 : em = get_jarvis_judice_ninke_matrix(); break;
        case 11 : em = get_atkinson_matrix(); break;
        case 12 : em = get_burkes_matrix(); break;
        case 13 : em = get_sierra_3_matrix(); break;
        case 14 : em = get_sierra_2row_matrix(); break;
        case 15 : em = get_sierra_lite_matrix(); break;
        case 16 : em = get_steve_pigeon_matrix(); break;
        case 17 : em = get_robert_kist_matrix(); break;
        case 18 : em = get_stevenson_arce_matrix(); break;
        default: break;
    }
    error_diffusion_dither(input, em, serpentine, sigma, (uint8_t *)output.data);
    ErrorDiffusionMatrix_free(em);
}

static void Ordered_Diffusion(DitherImage* input, int type, float sigma, int step, float a, float b, float c, ImGui::ImMat& output)
{
    OrderedDitherMatrix* om = nullptr;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    switch (type)
    {
        case 0 : om = get_blue_noise_128x128(); break;
        case 1 : om = get_bayer2x2_matrix(); break;
        case 2 : om = get_bayer3x3_matrix(); break;
        case 3 : om = get_bayer4x4_matrix(); break;
        case 4 : om = get_bayer8x8_matrix(); break;
        case 5 : om = get_bayer16x16_matrix(); break;
        case 6 : om = get_bayer32x32_matrix(); break;
        case 7 : om = get_dispersed_dots_1_matrix(); break;
        case 8 : om = get_dispersed_dots_2_matrix(); break;
        case 9 : om = get_ulichney_void_dispersed_dots_matrix(); break;
        case 10 : om = get_non_rectangular_1_matrix(); break;
        case 11 : om = get_non_rectangular_2_matrix(); break;
        case 12 : om = get_non_rectangular_3_matrix(); break;
        case 13 : om = get_non_rectangular_4_matrix(); break;
        case 14 : om = get_ulichney_bayer_5_matrix(); break;
        case 15 : om = get_ulichney_matrix(); break;
        case 16 : om = get_bayer_clustered_dot_1_matrix(); break;
        case 17 : om = get_bayer_clustered_dot_2_matrix(); break;
        case 18 : om = get_bayer_clustered_dot_3_matrix(); break;
        case 19 : om = get_bayer_clustered_dot_4_matrix(); break;
        case 20 : om = get_bayer_clustered_dot_5_matrix(); break;
        case 21 : om = get_bayer_clustered_dot_6_matrix(); break;
        case 22 : om = get_bayer_clustered_dot_7_matrix(); break;
        case 23 : om = get_bayer_clustered_dot_8_matrix(); break;
        case 24 : om = get_bayer_clustered_dot_9_matrix(); break;
        case 25 : om = get_bayer_clustered_dot_10_matrix(); break;
        case 26 : om = get_bayer_clustered_dot_11_matrix(); break;
        case 27 : om = get_central_white_point_matrix(); break;
        case 28 : om = get_balanced_centered_point_matrix(); break;
        case 29 : om = get_diagonal_ordered_matrix_matrix(); break;
        case 30 : om = get_ulichney_clustered_dot_matrix(); break;
        case 31 : om = get_magic5x5_circle_matrix(); break;
        case 32 : om = get_magic6x6_circle_matrix(); break;
        case 33 : om = get_magic7x7_circle_matrix(); break;
        case 34 : om = get_magic4x4_45_matrix(); break;
        case 35 : om = get_magic6x6_45_matrix(); break;
        case 36 : om = get_magic8x8_45_matrix(); break;
        case 37 : om = get_magic4x4_matrix(); break;
        case 38 : om = get_magic6x6_matrix(); break;
        case 39 : om = get_magic8x8_matrix(); break;
        case 40 : om = get_variable_2x2_matrix(step); break;
        case 41 : om = get_variable_4x4_matrix(step); break;
        case 42 : om = get_interleaved_gradient_noise(step, a, b, c); break;
        //case 43 : om = get_matrix_from_image(); break; // TODO::Dicky
        default: break;
    }
    ordered_dither(input, om, sigma, (uint8_t *)output.data);
    OrderedDitherMatrix_free(om);
}

static void Variable_Error_Diffusion(DitherImage* input, int type, bool serpentine, ImGui::ImMat& output)
{
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    if (type == 0)
        variable_error_diffusion_dither(input, Ostromoukhov, serpentine, (uint8_t *)output.data);
    else
        variable_error_diffusion_dither(input, Zhoufang, serpentine, (uint8_t *)output.data);
}

static void Thresholding(DitherImage* input, bool auto_thres, float threshold, float noise, ImGui::ImMat& output)
{
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    if (auto_thres)
        threshold = auto_threshold(input);
    threshold_dither(input, threshold, noise, (uint8_t *)output.data);
}

static void Direct_Binary_Search(DitherImage* input, int v, ImGui::ImMat& output)
{
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    dbs_dither(input, v, (uint8_t *)output.data);
}

static void Kacker_Allebach(DitherImage* input, bool random, ImGui::ImMat& output)
{
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    kallebach_dither(input, random, (uint8_t *)output.data);
}

static void Riemersma_Dithering(DitherImage* input, int type, bool modified, ImGui::ImMat& output)
{
    RiemersmaCurve* rc = nullptr;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    switch (type)
    {
        case 0: rc = get_hilbert_curve(); break;
        case 1: rc = get_hilbert_mod_curve(); break;
        case 2: rc = get_peano_curve(); break;
        case 3: rc = get_fass0_curve(); break;
        case 4: rc = get_fass1_curve(); break;
        case 5: rc = get_fass2_curve(); break;
        case 6: rc = get_gosper_curve(); break;
        case 7: rc = get_fass_spiral_curve(); break;
        default: break;
    }
    riemersma_dither(input, rc, modified, (uint8_t *)output.data);
}

static void Pattern_Dithering(DitherImage* input, int type, ImGui::ImMat& output)
{
    TilePattern* tp;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    switch (type)
    {
        case 0: tp = get_2x2_pattern(); break;
        case 1: tp = get_3x3_v1_pattern(); break;
        case 2: tp = get_3x3_v2_pattern(); break;
        case 3: tp = get_3x3_v3_pattern(); break;
        case 4: tp = get_4x4_pattern(); break;
        case 5: tp = get_5x2_pattern(); break;
    }
    pattern_dither(input, tp, (uint8_t *)output.data);
}

static void Dot_Lippens(DitherImage* input, int type, ImGui::ImMat& output)
{
    DotClassMatrix* cm;
    DotLippensCoefficients* coe;
    output.create_type(input->width, input->height, 1, IM_DT_INT8);
    switch (type)
    {
        case 0:
            cm = get_dotlippens_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        case 1:
            cm = get_dotlippens_class_matrix();
            coe = get_dotlippens_coefficients2();
            break;
        case 2:
            cm = get_dotlippens_class_matrix();
            coe = get_dotlippens_coefficients3();
            break;
        case 3:
            cm = get_guoliu_16x16_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        case 4:
            cm = get_mese_16x16_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        case 5:
            cm = get_knuth_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        default: break;
    }
    dotlippens_dither(input, cm, coe, (uint8_t *)output.data);
    DotLippensCoefficients_free(coe);
    DotClassMatrix_free(cm);
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

    static int m_dbs_formula = 0;

    static bool m_kacker_allebach_random = true;

    static bool m_riemersma_modified = true;

    static ImGui::ImMat m_mat;
    static ImGui::ImMat m_gray;
    static ImGui::ImMat m_result;
    static DitherImage* dither_image = nullptr;
    static ImTextureID m_texture = 0;
    static ImTextureID m_bm_texture = 0;

    auto process_image = [&](std::string output_path = std::string())
    {
        if (m_bm_texture) { ImGui::ImDestroyTexture(m_bm_texture); m_bm_texture = 0; }
        switch (m_dither_type)
        {
            case 0 : 
            {
                Grid_Ditherer(dither_image, m_grid_width, m_grid_height, m_grid_min_pixels, m_grid_alt_algorithm, m_result);
            }
            break;
            case 1 : 
                Dot_Diffusion(dither_image, m_dot_diffusion_type, m_result);
            break;
            case 2 : 
                Error_Diffusion(dither_image, m_err_diffusion_type, m_err_diffusion_sigma, m_err_diffusion_serpentine, m_result);
            break;
            case 3 : 
                Ordered_Diffusion(dither_image, m_ord_dithering_type, m_ord_diffusion_sigma, m_ord_diffusion_step, m_ord_diffusion_a, m_ord_diffusion_b, m_ord_diffusion_c, m_result);
            break;
            case 4 : 
                Variable_Error_Diffusion(dither_image, m_verr_diffusion_type, m_verr_diffusion_serpentine, m_result);
            break;
            case 5 : 
                Thresholding(dither_image, m_threshold_auto, m_threshold_thres, m_threshold_noise, m_result);
            break;
            case 6 : 
                Direct_Binary_Search(dither_image, m_dbs_formula, m_result);
            break;
            case 7 : 
                Kacker_Allebach(dither_image, m_kacker_allebach_random, m_result);
            break;
            case 8 : 
                Riemersma_Dithering(dither_image, m_riemersma_type, m_riemersma_modified, m_result);
            break;
            case 9 : 
                Pattern_Dithering(dither_image, m_pattern_type, m_result);
            break;
            case 10 : 
                Dot_Lippens(dither_image, m_dot_lippens_type, m_result);
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
            ImGuiFileDialog::Instance()->OpenDialog("##DitherFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose File",
                                                    image_filter.c_str(),
                                                    m_file_path.empty() ? "." : m_file_path,
                                                    1,
                                                    IGFDUserDatas("OpenImage"),
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                    ImGuiFileDialogFlags_CaseInsensitiveExtention |
                                                    ImGuiFileDialogFlags_DisableCreateDirectoryButton |
                                                    ImGuiFileDialogFlags_Modal);
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
            ImGui::ImShowVideoWindow(draw_list, m_bm_texture, window_pos, window_size);
            std::string dialog_id = "##TextureFileDlgKey" + std::to_string((long long)m_bm_texture);
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem((std::string(ICON_FA_IMAGE) + " Save Texture to File").c_str()))
                {
                    ImGuiFileDialog::Instance()->OpenDialog(dialog_id.c_str(), ICON_IGFD_FOLDER_OPEN " Choose File", 
                                                            image_filter.c_str(),
                                                            ".",
                                                            1, 
                                                            nullptr, 
                                                            ImGuiFileDialogFlags_ShowBookmark |
                                                            ImGuiFileDialogFlags_CaseInsensitiveExtention |
                                                            ImGuiFileDialogFlags_ConfirmOverwrite |
                                                            ImGuiFileDialogFlags_Modal);
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
                    if (m_texture)
                    {
                        ImGui::ImDestroyTexture(m_texture);
                        m_texture = 0;
                    }
                    ImGui::ImMatToTexture(m_mat, m_texture);
                    int width = m_mat.w & 0xFFFFFFFC;
                    m_gray.create_type(width, m_mat.h, IM_DT_INT8);
                    if (dither_image) { DitherImage_free(dither_image); dither_image = nullptr; }
                    dither_image = DitherImage_new(width, m_mat.h);
                    for (int y = 0; y < m_mat.h ; y++)
                    {
                        for (int x = 0; x < width; x++)
                        {
                            float R = m_mat.at<uint8_t>(x, y, 0);
                            float G = m_mat.at<uint8_t>(x, y, 1);
                            float B = m_mat.at<uint8_t>(x, y, 2);
                            float gray = R * 0.299 + G * 0.587 + B * 0.114;
                            if (dither_image) DitherImage_set_pixel(dither_image, x, y, R, G, B, true);
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
        if (m_texture)
        {
            ImGui::ImDestroyTexture(m_texture);
            m_texture = 0;
        }
        if (m_bm_texture)
        {
            ImGui::ImDestroyTexture(m_bm_texture);
            m_bm_texture = 0;
        }
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
    property.application.Application_Finalize = Dither_Finalize;
    property.application.Application_Frame = Dither_Frame;
}