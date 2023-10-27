#include <stdio.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <application.h>
#include <ImGuiFileDialog.h>
#include <immat.h>
#include <potracelib.h>
#include <backend.h>
#include <bitmap.h>

#if IMGUI_ICONS
#define ICON_RESET u8"\ue042"
#else
#define ICON_RESET "r"
#endif

std::string image_file_dis = "*.png *.gif *.jpg *.jpeg *.tiff *.webp";
std::string image_file_suffix = ".png,.gif,.jpg,.jpeg,.tiff,.webp";
std::string image_filter = "Image files (" + image_file_dis + "){" + image_file_suffix + "}" + "," + ".*";
const char * turnpolicys[] = { "black", "white", "left", "right", "minority", "majority", "random" };

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2 pos, ImVec2 size, float& offset_x, float& offset_y, float& tf_x, float& tf_y, bool bLandscape = true, bool out_border = false, const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1))
{
    if (texture)
    {        
        ImGuiIO& io = ImGui::GetIO();
        float texture_width = ImGui::ImGetTextureWidth(texture);
        float texture_height = ImGui::ImGetTextureHeight(texture);
        float aspectRatioTexture = texture_width / texture_height;
        float aspectRatioView = size.x / size.y;
        bool bTextureisLandscape = aspectRatioTexture > 1.f ? true : false;
        bool bViewisLandscape = aspectRatioView > 1.f ? true : false;
        float adj_w = 0, adj_h = 0;
        if ((bViewisLandscape && bTextureisLandscape) || (!bViewisLandscape && !bTextureisLandscape))
        {
            if (aspectRatioTexture >= aspectRatioView)
            {
                adj_w = size.x;
                adj_h = adj_w / aspectRatioTexture;
            }
            else
            {
                adj_h = size.y;
                adj_w = adj_h * aspectRatioTexture;
            }
        }
        else if (bViewisLandscape && !bTextureisLandscape)
        {
            adj_h = size.y;
            adj_w = adj_h * aspectRatioTexture;
        }
        else if (!bViewisLandscape && bTextureisLandscape)
        {
            adj_w = size.x;
            adj_h = adj_w / aspectRatioTexture;
        }
        tf_x = (size.x - adj_w) / 2.0;
        tf_y = (size.y - adj_h) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        draw_list->AddRectFilled(ImVec2(offset_x, offset_y), ImVec2(offset_x + adj_w, offset_y + adj_h), IM_COL32_BLACK);
        
        draw_list->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_w, offset_y + adj_h),
            uvMin,
            uvMax
        );
        
        tf_x = offset_x + adj_w;
        tf_y = offset_y + adj_h;

        ImVec2 scale_range = ImVec2(2.0 , 8.0);
        static float texture_zoom = scale_range.x;
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
        bool zoom = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        if (zoom && ImGui::IsItemHovered())
        {
            ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
            ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
            float region_sz = 320.0f / texture_zoom;
            float scale_w = texture_width / (tf_x - offset_x);
            float scale_h = texture_height / (tf_y - offset_y);
            float pos_x = (io.MousePos.x - offset_x) * scale_w;
            float pos_y = (io.MousePos.y - offset_y) * scale_h;
            float region_x = pos_x - region_sz * 0.5f;
            float region_y = pos_y - region_sz * 0.5f;
            if (region_x < 0.0f) { region_x = 0.0f; }
            else if (region_x > texture_width - region_sz) { region_x = texture_width - region_sz; }
            if (region_y < 0.0f) { region_y = 0.0f; }
            else if (region_y > texture_height - region_sz) { region_y = texture_height - region_sz; }
            ImGui::SetNextWindowBgAlpha(1.0);
            if (ImGui::BeginTooltip())
            {
                ImGui::Text("(%.2fx)", texture_zoom);
                ImVec2 uv0 = ImVec2((region_x) / texture_width, (region_y) / texture_height);
                ImVec2 uv1 = ImVec2((region_x + region_sz) / texture_width, (region_y + region_sz) / texture_height);
                ImGui::Image(texture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
                ImGui::EndTooltip();
            }
            if (io.MouseWheel < -FLT_EPSILON)
            {
                texture_zoom *= 0.9;
                if (texture_zoom < scale_range.x) texture_zoom = scale_range.x;
            }
            else if (io.MouseWheel > FLT_EPSILON)
            {
                texture_zoom *= 1.1;
                if (texture_zoom > scale_range.y) texture_zoom = scale_range.y;
            }
        }
    }
}

// Application Framework Functions
static void Potrace_SetupContext(ImGuiContext *ctx, bool in_splash)
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

static void Potrace_Finalize(void **handle)
{
}

inline void SVGPane(const char* vFilter, IGFDUserDatas vUserDatas, bool* vCantContinue)
{
    ImGui::RadioButton("Flat as Single", &info.grouping, 0);
    ImGui::RadioButton("Auto Group", &info.grouping, 1);
    ImGui::RadioButton("Group Related",&info.grouping, 2);
}

static bool Potrace_Frame(void *handle, bool app_will_quit)
{
    bool app_done = false;
    // static potrace_bitmap_t * bm = nullptr;
    static ImGui::ImMat m_mat;
    static ImGui::ImMat m_gray;
    static ImTextureID m_texture = 0;
    static ImTextureID m_bm_texture = 0;
    static std::string m_file_path;

    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;

    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Potrace Test", nullptr, flags);

    auto process_image = [&](std::string output_path = std::string())
    {
        potrace_bitmap_t *bitmap = nullptr;
        potrace_state_t *st = nullptr;
        if (m_mat.empty() || m_gray.empty())
            return;
        auto gray_dup = m_gray.lowpass(info.lambda_low);
        if (info.lambda_high > 0)
            gray_dup = gray_dup.highpass(info.lambda_high);

        bitmap = bm_new(gray_dup.w, gray_dup.h);
        if (!bitmap)
            return;
        for (int y = 0; y < gray_dup.h; y++)
        {
            for (int x = 0; x < gray_dup.w; x++)
            {
                BM_UPUT(bitmap, x, gray_dup.h - y, gray_dup.at<uint8_t>(x, y) > info.blacklevel * 255 ? 0 : 1);
            }
        }
        imginfo_t imginfo;
        if (m_bm_texture && output_path.empty())
        {
            ImGui::ImDestroyTexture(m_bm_texture);
            m_bm_texture = 0;
        }
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
            backend_lookup("svg", &info.backend);

        imginfo.pixwidth = bitmap->w;
        imginfo.pixheight = bitmap->h;
        imginfo.channels = 4;
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
                ImGui::ImMatToTexture(mat, m_bm_texture);
        }
        else if (info.backend)
        {
            FILE * fout = fopen(output_path.c_str(), "wb");
            if (fout)
            {
                info.backend->page_f(fout, st->plist, &imginfo);
                fclose(fout);
            }
        }
        potrace_state_free(st);
        bm_free(bitmap);
    };
    // control panel
    ImGui::BeginChild("##Potrace_Config", ImVec2(400, ImGui::GetWindowHeight() - 60), true);
    {
        auto draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Open File...", ImVec2(160, 32)))
        {
            ImGuiFileDialog::Instance()->OpenDialog("##PotraceFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose File",
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
        ImGui::SameLine();

        ImGui::BeginDisabled(m_mat.empty());
        
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Save File...", ImVec2(160, 32)))
        {
            ImGuiFileDialog::Instance()->OpenDialogWithPane("##PotraceFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save File",
                                                            ".svg", ".", "", 
                                                            std::bind(&SVGPane, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
                                                            , 200, 1, IGFDUserDatas("SVGPane"),
                                                            ImGuiFileDialogFlags_ShowBookmark |
                                                            ImGuiFileDialogFlags_CaseInsensitiveExtention |
                                                            ImGuiFileDialogFlags_Modal |
                                                            ImGuiFileDialogFlags_ConfirmOverwrite);
        }
        ImGui::EndDisabled();

        ImGui::PushItemWidth(200);
        ImGui::Separator();

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
            process_image();
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
            process_image();
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
            process_image();
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
            process_image();
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
            process_image();
        }

        bool longcurve = info.param->opticurve == 0;
        if (ImGui::Checkbox("Curve optimization", &longcurve))
        {
            info.param->opticurve = longcurve ? 0 : 1;
            process_image();
        }

        ImGui::TextUnformatted("Scaling and placement options:");

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
            process_image();
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
            process_image();
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
            process_image();
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
            process_image();
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
            process_image();
        }

        // Tighten
        bool tight = info.tight == 1;
        if (ImGui::Checkbox("Tighten the bounding", &tight))
        {
            info.tight = tight ? 1 : 0;
            process_image();
        }

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
            process_image();
        }

        bool invert = info.invert == 1;
        if (ImGui::Checkbox("Invert input", &invert))
        {
            info.invert = invert ? 1 : 0;
            process_image();
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
            process_image();
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
            process_image();
        }

        bool draw_dot = info.draw_dot == 1;
        if (ImGui::Checkbox("Draw dot", &draw_dot))
        {
            info.draw_dot = draw_dot ? 1 : 0;
            process_image();
        }

        ImGui::PopItemWidth();
        ImGui::Separator();

        if (m_texture)
        {
            ShowVideoWindow(draw_list, m_texture, ImGui::GetCursorScreenPos(), ImVec2(window_size.x, window_size.x / 2), offset_x, offset_y, tf_x, tf_y);
        }
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
            ShowVideoWindow(draw_list, m_bm_texture, window_pos, window_size, offset_x, offset_y, tf_x, tf_y);
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
                    if (m_texture)
                    {
                        ImGui::ImDestroyTexture(m_texture);
                        m_texture = 0;
                    }
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
                    process_image();
                }
            }
            if (userDatas.compare("SVGPane") == 0)
            {
                process_image(file_path);
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
    property.name = "Potrace Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    // property.power_save = false;
    property.width = 1680;
    property.height = 900;
    property.application.Application_SetupContext = Potrace_SetupContext;
    property.application.Application_Finalize = Potrace_Finalize;
    property.application.Application_Frame = Potrace_Frame;
    init_info();
}