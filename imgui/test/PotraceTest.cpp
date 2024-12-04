#include <stdio.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <application.h>
#include <ImGuiFileDialog.h>
#include <immat.h>
#include <potracelib.h>
#include <backend.h>
#include <bitmap.h>
#include <libdither.h>

#if IMGUI_ICONS
#define ICON_RESET u8"\ue042"
#else
#define ICON_RESET "r"
#endif

const char * turnpolicys[] = { "black", "white", "left", "right", "minority", "majority", "random" };

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
static void Potrace_SetupContext(ImGuiContext *ctx, void* handle, bool in_splash)
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

static void Potrace_Finalize(void **handle)
{
}

inline void SVGPane(const char* vFilter, const char* currentPath, IGFDUserDatas vUserDatas, bool* vCantContinue, bool* bOK)
{
    ImGui::RadioButton("Flat as Single", &info.grouping, 0);
    ImGui::RadioButton("Auto Group", &info.grouping, 1);
    ImGui::RadioButton("Group Related",&info.grouping, 2);
}

struct dither_info
{
    bool m_dither_upscale = false;
    int m_dither_type = 0;
    int m_dot_diffusion_type = 0;
    int m_err_diffusion_type = 0;
    int m_ord_dithering_type = 0;
    int m_verr_diffusion_type = 0;
    int m_riemersma_type = 0;
    int m_pattern_type = 0;
    int m_dot_lippens_type = 0;
    int m_grid_width = 4;
    int m_grid_height = 4;
    int m_grid_min_pixels = 0;
    bool m_grid_alt_algorithm = true;
    float m_err_diffusion_sigma = 0.f;
    bool m_err_diffusion_serpentine = true;
    float m_ord_diffusion_sigma = 0.f;
    int m_ord_diffusion_step = 55;
    float m_ord_diffusion_a = 52.9829189f;
    float m_ord_diffusion_b = 0.06711056f;
    float m_ord_diffusion_c = 0.00583715;
    bool m_verr_diffusion_serpentine = true;
    float m_threshold_thres = 0.5f;
    bool m_threshold_auto = true;
    float m_threshold_noise = 0.55f;
    int m_dbs_formula = 0;
    bool m_kacker_allebach_random = true;
    bool m_riemersma_modified = true;
};

static void process_image(const ImGui::ImMat& gray, const bool dither_filter, const dither_info& dither, ImTextureID& bm_texture, std::string output_path = std::string())
{
    potrace_bitmap_t *bitmap = nullptr;
    potrace_state_t *st = nullptr;
    if (gray.empty())
        return;
    
    auto gray_dup = gray.clone();
    if (info.lambda_high > 0)
        gray_dup = gray_dup.highpass(info.lambda_high);
    else if (info.lambda_low > 0)
        gray_dup = gray_dup.lowpass(info.lambda_low);

    if (dither_filter)
    {
        ImGui::ImMat result;
        result.create_type(gray_dup.w, gray_dup.h, 1, IM_DT_INT8);
        switch (dither.m_dither_type)
        {
            case 0 : 
                grid_dither(gray_dup, dither.m_grid_width, dither.m_grid_height, dither.m_grid_min_pixels, dither.m_grid_alt_algorithm, result);
            break;
            case 1 : 
                dot_diffusion_dither(gray_dup, (DD_TYPE)dither.m_dot_diffusion_type, result);
            break;
            case 2 : 
                error_diffusion_dither(gray_dup, (ED_TYPE)dither.m_err_diffusion_type, dither.m_err_diffusion_serpentine, dither.m_err_diffusion_sigma, result);
            break;
            case 3 : 
                ordered_dither(gray_dup, (OD_TYPE)dither.m_ord_dithering_type, dither.m_ord_diffusion_sigma, result, ImGui::ImMat(), dither.m_ord_diffusion_step, ImVec4(dither.m_ord_diffusion_a, dither.m_ord_diffusion_b, dither.m_ord_diffusion_c, 0));
            break;
            case 4 : 
                variable_error_diffusion_dither(gray_dup, (VD_TYPE)dither.m_verr_diffusion_type, dither.m_verr_diffusion_serpentine, result);
            break;
            case 5 : 
                threshold_dither(gray_dup, dither.m_threshold_auto ? auto_threshold(gray_dup) : dither.m_threshold_thres, dither.m_threshold_noise, result);
            break;
            case 6 : 
                dbs_dither(gray_dup, dither.m_dbs_formula, result);
            break;
            case 7 : 
                kallebach_dither(gray_dup, dither.m_kacker_allebach_random, result);
            break;
            case 8 : 
                riemersma_dither(gray_dup, (RD_TYPE)dither.m_riemersma_type, dither.m_riemersma_modified, result);
            break;
            case 9 : 
                pattern_dither(gray_dup, (PD_TYPE)dither.m_pattern_type, result);
            break;
            case 10 : 
                dotlippens_dither(gray_dup, (LP_TYPE)dither.m_dot_lippens_type, result);
            break;
            default : break;
        }
        gray_dup = result;
        if (dither.m_dither_upscale) gray_dup = gray_dup.resize(gray_dup.w * 2, gray_dup.h * 2);
        info.param->turdsize = 0;
        info.param->alphamax = 0;
    }

    bitmap = bm_new(gray_dup.w, gray_dup.h);
    if (!bitmap)
        return;
    for (int y = 0; y < gray_dup.h; y++)
    {
        for (int x = 0; x < gray_dup.w; x++)
        {
            BM_UPUT(bitmap, x, gray_dup.h - y, gray_dup.at<uint8_t>(x, y) > (dither_filter ? 0.999 : info.blacklevel) * 255 ? 0 : 1);
        }
    }
    imginfo_t imginfo;
    if (bm_texture && output_path.empty()) ImGui::ImDestroyTexture(&bm_texture);
    if (info.invert)
    {
        bm_invert(bitmap);
    }
    st = potrace_trace(info.param, bitmap);
    if (!st || st->status != POTRACE_STATUS_OK)
    {
        bm_free(bitmap);
        return;
    }

    if (output_path.empty())
        backend_lookup("mem", &info.backend);
    else
    {
        backend_lookup("svg", &info.backend);
    }

    imginfo.pixwidth = bitmap->w;
    imginfo.pixheight = bitmap->h;
    imginfo.channels = 4;
    imginfo.invert = info.invert_background;
    int out_width = 0;
    int out_height = 0;
    calc_dimensions(&imginfo, st->plist, &out_width, &out_height);

    if (output_path.empty())
    {
        ImGui::ImMat mat(out_width, out_height, 4, 1u, 4);
        if (page_mem(mat.data, st->plist, &imginfo))
        {
            potrace_state_free(st);
            bm_free(bitmap);
            return;
        }
        if (!mat.empty())
            ImGui::ImMatToTexture(mat, bm_texture);
    }
    else if (info.backend)
    {
        FILE * fout = fopen(output_path.c_str(), "wb");
        if (fout)
        {
            info.opaque = info.invert_background ? 1 : 0;
            info.backend->page_f(fout, st->plist, &imginfo);
            fclose(fout);
        }
    }
    potrace_state_free(st);
    bm_free(bitmap);
}

static bool Potrace_Frame(void *handle, bool app_will_quit)
{
    bool app_done = false;
    static ImGui::ImMat m_mat;
    static ImGui::ImMat m_gray;
    static ImTextureID m_texture = 0;
    static ImTextureID m_bm_texture = 0;
    static std::string m_file_path;

    static bool m_dither_filter = false;
    static dither_info dither;

    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Potrace Test", nullptr, flags);

    // control panel
    ImGui::BeginChild("##Potrace_Config", ImVec2(500, ImGui::GetWindowHeight() - 60), true);
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
            ImGuiFileDialog::Instance()->OpenDialog("##PotraceFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose File",
                                                    image_filter.c_str(),
                                                    config);
        }
        ImGui::ShowTooltipOnHover("File Path:%s", m_file_path.c_str());
        ImGui::SameLine();

        ImGui::BeginDisabled(m_mat.empty());
        
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Save File...", ImVec2(160, 32)))
        {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.sidePane = std::bind(&SVGPane, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
            config.sidePaneWidth = 200;
            config.userDatas = IGFDUserDatas("SVGPane");
            config.flags = ImGuiFileDialogFlags_SaveFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("##PotraceFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save File",
                                                    ".svg",
                                                    config);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Generator", ImVec2(120, 32)))
        {
            const int range = 200;
            const int range2 = 100;
            m_mat = ImGui::ImMat(2048, 2048, 4, 1u, 4);
            for (int i = 0; i < 20; i++)
            {
                ImVec2 center;
                float radius = rand() % range2 + (1024 - range2 - 200);
                float thiness = rand()/double(RAND_MAX) * 2.f + 0.5f;
                center.x = rand() % range + (1024 - range2);
                center.y = rand() % range + (1024 - range2);
                m_mat.draw_circle(center.x, center.y, radius, thiness, ImPixel(1,1,1,1));
            }
            ImGui::ImDestroyTexture(&m_texture);
            ImGui::ImDestroyTexture(&m_bm_texture);
            ImGui::ImMatToTexture(m_mat, m_texture);
            m_gray.create_type(m_mat.w, m_mat.h, IM_DT_INT8);
            for (int y = 0; y < m_mat.h; y++)
            {
                for (int x = 0; x < m_mat.w; x++)
                {
                    float R = m_mat.at<uint8_t>(x, y, 0);
                    float G = m_mat.at<uint8_t>(x, y, 1);
                    float B = m_mat.at<uint8_t>(x, y, 2);
                    float gray = R * 0.299 + G * 0.587 + B * 0.114;
                    m_gray.at<uint8_t>(x, y) = (uint8_t)gray;
                }
            }
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        ImGui::Separator();

        if (m_texture) ImGui::ImShowVideoWindow(draw_list, m_texture, ImGui::GetCursorScreenPos(), ImVec2(window_size.x, window_size.x / 3));
        else ImGui::InvisibleButton("##dummy image", ImVec2(window_size.x, window_size.x / 3));
        ImGui::Separator();

        ImGui::PushItemWidth(200);
        ImGui::TextUnformatted("Algorithm options:");
        int turnpolicy = info.param->turnpolicy;
        if (ImGui::Button(ICON_RESET "##reset_turnpolicy"))
        {
            turnpolicy = POTRACE_TURNPOLICY_MINORITY;
        }
        ImGui::SameLine();
        ImGui::Combo("Ambiguities policy", &turnpolicy, turnpolicys, IM_ARRAYSIZE(turnpolicys));
        if (turnpolicy != info.param->turnpolicy)
        {
            info.param->turnpolicy = turnpolicy;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        int turdsize = info.param->turdsize;
        if (ImGui::Button(ICON_RESET "##reset_turdsize"))
        {
            turdsize = 2;
        }
        ImGui::SameLine();
        ImGui::SliderInt("Suppress speckles", &turdsize, 0, 100);
        if (turdsize != info.param->turdsize)
        {
            info.param->turdsize = turdsize;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        float alphamax = info.param->alphamax;
        if (ImGui::Button(ICON_RESET "##reset_alphamax"))
        {
            alphamax = 1;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Corner threshold", &alphamax, 0.f, 1.5f, "%.2f");
        if (alphamax != info.param->alphamax)
        {
            info.param->alphamax = alphamax;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        float unit = info.unit;
        if (ImGui::Button(ICON_RESET "##reset_unit"))
        {
            unit = 10.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Quantize output", &unit, 0.f, 100.f, "%.0f");
        if (unit != info.unit)
        {
            info.unit = unit;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        float opttolerance = info.param->opttolerance;
        if (ImGui::Button(ICON_RESET "##reset_opttolerance"))
        {
            opttolerance = 0.2f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Optimization tolerance", &opttolerance, 0.f, 1.f, "%.2f");
        if (opttolerance != info.param->opttolerance)
        {
            info.param->opttolerance = opttolerance;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        bool longcurve = info.param->opticurve == 0;
        if (ImGui::Checkbox("Curve optimization", &longcurve))
        {
            info.param->opticurve = longcurve ? 0 : 1;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Scaling and placement options:");
        bool invert_background = info.invert_background;
        if (ImGui::Checkbox("Invert Background", &invert_background))
        {
            info.invert_background = invert_background;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        // Left margin
        float l_margin = info.lmar_d.x == UNDEF ? 0 : info.lmar_d.x;
        if (!m_mat.empty())
            l_margin = l_margin / (float)m_mat.w;
        else if (l_margin > 1.0)
            l_margin = 1.0;
        else if (l_margin < -1.0)
            l_margin = -1.0;
        float old_l = l_margin;
        if (ImGui::Button(ICON_RESET "##reset_l_margin"))
        {
            l_margin = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Left margin", &l_margin, -1.f, 1.f, "%.2f");
        if (l_margin != old_l)
        {
            info.lmar_d.x = l_margin == 0 ? UNDEF : l_margin * (m_mat.empty() ? 1 : m_mat.w);
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        // Right margin
        float r_margin = info.rmar_d.x == UNDEF ? 0 : info.rmar_d.x;
        if (!m_mat.empty())
            r_margin = r_margin / (float)m_mat.w;
        else if (r_margin > 1.0)
            r_margin = 1.0;
        else if (r_margin < -1.0)
            r_margin = -1.0;
        float old_r = r_margin;
        if (ImGui::Button(ICON_RESET "##reset_r_margin"))
        {
            r_margin = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Right margin", &r_margin, -1.f, 1.f, "%.2f");
        if (r_margin != old_r)
        {
            info.rmar_d.x = r_margin == 0 ? UNDEF : r_margin * (m_mat.empty() ? 1 : m_mat.w);
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        // Top margin
        float t_margin = info.tmar_d.x == UNDEF ? 0 : info.tmar_d.x;
        if (!m_mat.empty())
            t_margin = t_margin / (float)m_mat.h;
        else if (t_margin > 1.0)
            t_margin = 1.0;
        else if (t_margin < -1.0)
            t_margin = -1.0;
        float old_t = t_margin;
        if (ImGui::Button(ICON_RESET "##reset_t_margin"))
        {
            t_margin = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Top margin", &t_margin, -1.f, 1.f, "%.2f");
        if (t_margin != old_t)
        {
            info.tmar_d.x = t_margin == 0 ? UNDEF : t_margin * (m_mat.empty() ? 1 : m_mat.h);
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        // Bottom margin
        float b_margin = info.bmar_d.x == UNDEF ? 0 : info.bmar_d.x;
        if (!m_mat.empty())
            b_margin = b_margin / (float)m_mat.h;
        else if (b_margin > 1.0)
            b_margin = 1.0;
        else if (b_margin < -1.0)
            b_margin = -1.0;
        float old_b = b_margin;
        if (ImGui::Button(ICON_RESET "##reset_b_margin"))
        {
            b_margin = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Bottom margin", &b_margin, -1.f, 1.f, "%.2f");
        if (b_margin != old_b)
        {
            info.bmar_d.x = b_margin == 0 ? UNDEF : b_margin * (m_mat.empty() ? 1 : m_mat.h);
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        // Angle
        float angle = info.angle;
        if (ImGui::Button(ICON_RESET "##reset_angle"))
        {
            angle = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Rotate counterclockwise", &angle, -180.f, 180.f, "%.1f");
        if (angle != info.angle)
        {
            info.angle = angle;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        // Tighten
        bool tight = info.tight == 1;
        if (ImGui::Checkbox("Tighten the bounding", &tight))
        {
            info.tight = tight ? 1 : 0;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Frontend options:");
        float blacklevel = info.blacklevel;
        if (ImGui::Button(ICON_RESET "##reset_blacklevel"))
        {
            blacklevel = 0.5f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Black/White cutoff", &blacklevel, 0.f, 1.f, "%.2f");
        if (blacklevel != info.blacklevel)
        {
            info.blacklevel = blacklevel;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        bool invert = info.invert == 1;
        if (ImGui::Checkbox("Invert input", &invert))
        {
            info.invert = invert ? 1 : 0;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        float lambda_low = info.lambda_low;
        if (ImGui::Button(ICON_RESET "##reset_lowpass_lambda"))
        {
            lambda_low = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Lowpass lambda", &lambda_low, 0.f, 10.f, "%.1f");
        if (lambda_low != info.lambda_low)
        {
            info.lambda_low = lambda_low;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        float lambda_high = info.lambda_high;
        if (ImGui::Button(ICON_RESET "##reset_highpass_lambda"))
        {
            lambda_high = 0.f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Highpass lambda", &lambda_high, 0.f, 10.f, "%.1f");
        if (lambda_high != info.lambda_high)
        {
            info.lambda_high = lambda_high;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        bool draw_dot = info.draw_dot == 1;
        if (ImGui::Checkbox("Draw dot", &draw_dot))
        {
            info.draw_dot = draw_dot ? 1 : 0;
            process_image(m_gray, m_dither_filter, dither, m_bm_texture);
        }

        ImGui::Separator();
        bool changed = false;
        changed |= ImGui::Checkbox("Dither Filters", &m_dither_filter);
        changed |= ImGui::Checkbox("Dither Upscale", &dither.m_dither_upscale);
        ImGui::BeginDisabled(!m_dither_filter);
        changed |= ImGui::Combo("Type", &dither.m_dither_type, Dither_items, IM_ARRAYSIZE(Dither_items));
        switch (dither.m_dither_type)
        {
            case 0 : 
            {
                changed |= ImGui::SliderInt("Grid width", &dither.m_grid_width, 1, 20);
                changed |= ImGui::SliderInt("Grid height", &dither.m_grid_height, 1, 20);
                changed |= ImGui::SliderInt("Min pixels", &dither.m_grid_min_pixels, 0, 20);
                changed |= ImGui::Checkbox("Alt Algorithm", &dither.m_grid_alt_algorithm);
            }
            break;
            case 1 : 
                changed |= ImGui::Combo("Dot Diffusion Type", &dither.m_dot_diffusion_type, Dot_Diffusion_items, IM_ARRAYSIZE(Dot_Diffusion_items));
            break;
            case 2 : 
                changed |= ImGui::Combo("Error Diffusion Type", &dither.m_err_diffusion_type, Error_Diffusion_items, IM_ARRAYSIZE(Error_Diffusion_items));
                changed |= ImGui::SliderFloat("Jitter##error", &dither.m_err_diffusion_sigma, 0.f, 1.f, "%.2f");
                changed |= ImGui::Checkbox("Serpentine", &dither.m_err_diffusion_serpentine);
            break;
            case 3 : 
                changed |= ImGui::Combo("Ordered Dithering Type", &dither.m_ord_dithering_type, Ordered_Dithering_item, IM_ARRAYSIZE(Ordered_Dithering_item));
                changed |= ImGui::SliderFloat("Jitter##order", &dither.m_ord_diffusion_sigma, 0.f, 1.f, "%.2f");
                if (dither.m_ord_dithering_type >= 40 && dither.m_ord_dithering_type < 43)
                {
                    changed |= ImGui::SliderInt("Step", &dither.m_ord_diffusion_step, 1, 100);
                    if (dither.m_ord_dithering_type == 42)
                    {
                        changed |= ImGui::SliderFloat("A", &dither.m_ord_diffusion_a, 0.f, 100.f, "%.3f");
                        changed |= ImGui::SliderFloat("B", &dither.m_ord_diffusion_b, 0.f, 1.f, "%.3f");
                        changed |= ImGui::SliderFloat("C", &dither.m_ord_diffusion_c, 0.f, 1.f, "%.3f");
                    }
                }
            break;
            case 4 : 
                changed |= ImGui::Combo("Variable Error Diffusion Type", &dither.m_verr_diffusion_type, Variable_Error_Diffusion_items, IM_ARRAYSIZE(Variable_Error_Diffusion_items));
                changed |= ImGui::Checkbox("Serpentine", &dither.m_verr_diffusion_serpentine);
            break;
            case 5 : 
                changed |= ImGui::SliderFloat("Noise", &dither.m_threshold_noise, 0.f, 1.f, "%.3f");
                changed |= ImGui::Checkbox("Auto threshold", &dither.m_threshold_auto);
                ImGui::BeginDisabled(dither.m_threshold_auto);
                changed |= ImGui::SliderFloat("Threshold", &dither.m_threshold_thres, 0.f, 1.f, "%.3f");
                ImGui::EndDisabled();
            break;
            case 6 : 
                changed |= ImGui::SliderInt("Formula", &dither.m_dbs_formula, 1, 7);
            break;
            case 7 : 
                changed |= ImGui::Checkbox("Random", &dither.m_kacker_allebach_random);
            break;
            case 8 : 
                changed |= ImGui::Combo("Riemersma Dithering Type", &dither.m_riemersma_type, Riemersma_Dithering_items, IM_ARRAYSIZE(Riemersma_Dithering_items));
                changed |= ImGui::Checkbox("Modified", &dither.m_riemersma_modified);
            break;
            case 9 : 
                changed |= ImGui::Combo("Pattern Dithering Type", &dither.m_pattern_type, Pattern_items, IM_ARRAYSIZE(Pattern_items));
            break;
            case 10 : 
                changed |= ImGui::Combo("Dot Lippens Type", &dither.m_dot_lippens_type, Dot_Lippens_items, IM_ARRAYSIZE(Dot_Lippens_items));
            break;
            default : break;
        }
        ImGui::EndDisabled();
        if (changed) process_image(m_gray, m_dither_filter, dither, m_bm_texture);

        ImGui::PopItemWidth();
    }
    ImGui::EndChild();

    // run panel
    ImGui::SameLine();
    ImGui::BeginChild("##Potrace_Result", ImVec2(ImGui::GetWindowWidth() - 500 - 30, ImGui::GetWindowHeight() - 60), true);
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
    if (ImGuiFileDialog::Instance()->Display("##PotraceFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
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
                    ImGui::ImDestroyTexture(&m_bm_texture);
                    ImGui::ImMatToTexture(m_mat, m_texture);
                    int width = m_mat.w & 0xFFFFFFFC;
                    m_gray.create_type(width, m_mat.h, IM_DT_INT8);
                    for (int y = 0; y < m_mat.h; y++)
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
                    process_image(m_gray, m_dither_filter, dither, m_bm_texture);
                }
            }
            if (userDatas.compare("SVGPane") == 0)
            {
                process_image(m_gray, m_dither_filter, dither, m_bm_texture, file_path);
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
    property.name = "Potrace Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    // property.power_save = false;
    property.width = 1680;
    property.height = 1024;
    property.application.Application_SetupContext = Potrace_SetupContext;
    property.application.Application_Finalize = Potrace_Finalize;
    property.application.Application_Frame = Potrace_Frame;
    init_info();
}