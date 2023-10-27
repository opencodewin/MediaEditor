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

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2 &pos, ImVec2 &size, float &offset_x, float &offset_y, float &tf_x, float &tf_y, float aspectRatio = 0.f, ImVec2 start = ImVec2(0.f, 0.f), ImVec2 end = ImVec2(1.f, 1.f), bool bLandscape = true)
{
    if (texture)
    {
        if (aspectRatio < FLT_EPSILON)
        {
            aspectRatio = (float)ImGui::ImGetTextureWidth(texture) / (float)ImGui::ImGetTextureHeight(texture);
        }
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
        bool bViewisLandscape = size.x >= size.y ? true : false;
        bViewisLandscape |= bLandscape;
        bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? size.y : size.x;
        float adj_h = bNeedChangeScreenInfo ? size.x : size.y;
        float adj_x = adj_h * aspectRatio;
        float adj_y = adj_h;
        if (adj_x > adj_w)
        {
            adj_y *= adj_w / adj_x;
            adj_x = adj_w;
        }
        tf_x = (size.x - adj_x) / 2.0;
        tf_y = (size.y - adj_y) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        draw_list->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_x, offset_y + adj_y),
            start,
            end);
        tf_x = offset_x + adj_x;
        tf_y = offset_y + adj_y;
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

    auto process_image = [&]()
    {
#if 1
        potrace_bitmap_t *bitmap = nullptr;
        potrace_state_t *st = nullptr;
        if (m_mat.empty() || m_gray.empty())
            return;
        bitmap = bm_new(m_gray.w, m_gray.h);
        if (!bitmap)
            return;
        for (int y = 0; y < m_gray.h; y++)
        {
            for (int x = 0; x < m_gray.w; x++)
            {
                BM_UPUT(bitmap, x, m_gray.h - y, m_gray.at<uint8_t>(x, y) > info.blacklevel * 255 ? 0 : 1);
            }
        }
        imginfo_t imginfo;
        if (m_bm_texture)
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
            return;
        }
        backend_lookup("mem", &info.backend);
        imginfo.pixwidth = bitmap->w;
        imginfo.pixheight = bitmap->h;
        imginfo.channels = 4;
        int out_width = 0;
        int out_height = 0;
        calc_dimensions(&imginfo, st->plist, &out_width, &out_height);
        ImGui::ImMat mat(out_width, out_height, 4, 1u, 4);
        if (page_mem(mat.data, st->plist, &imginfo))
        {
            return;
        }
        potrace_state_free(st);
        if (!mat.empty())
            ImGui::ImMatToTexture(mat, m_bm_texture);
        bm_free(bitmap);
#else
        if (!m_gray.empty())
            ImGui::ImMatToTexture(m_gray, m_bm_texture);
#endif
    };
    // control panel
    ImGui::BeginChild("##Potrace_Config", ImVec2(400, ImGui::GetWindowHeight() - 60), true);
    {
        auto draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::PushItemWidth(200);
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Open File...", ImVec2(280, 32)))
        {
            ImGuiFileDialog::Instance()->OpenDialog("##PotraceFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose File",
                                                    image_filter.c_str(),
                                                    m_file_path.empty() ? "." : m_file_path,
                                                    1,
                                                    nullptr,
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                        ImGuiFileDialogFlags_CaseInsensitiveExtention |
                                                        ImGuiFileDialogFlags_DisableCreateDirectoryButton |
                                                        ImGuiFileDialogFlags_Modal);
        }
        ImGui::ShowTooltipOnHover("File Path:%s", m_file_path.c_str());
        ImGui::Separator();

        ImGui::TextUnformatted("Algorithm options:");
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

        ImGui::PopItemWidth();
        ImGui::Separator();

        if (m_texture)
        {
            auto display_pos = ImGui::GetCursorScreenPos() + ImVec2(10, 0);
            auto width = (float)ImGui::ImGetTextureWidth(m_texture);
            auto height = (float)ImGui::ImGetTextureHeight(m_texture);
            float adj_width = window_size.x - 20;
            float adj_height = adj_width * (height / width);
            auto display_size = ImVec2(adj_width, adj_height);
            ShowVideoWindow(draw_list, m_texture, display_pos, display_size, offset_x, offset_y, tf_x, tf_y);
        }
    }
    ImGui::EndChild();

    // run panel
    ImGui::SameLine();
    ImGui::BeginChild("##Potrace_Result", ImVec2(ImGui::GetWindowWidth() - 400 - 30, ImGui::GetWindowHeight() - 60), true);
    {
        auto draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        if (m_bm_texture)
        {
            auto width = (float)ImGui::ImGetTextureWidth(m_bm_texture);
            auto height = (float)ImGui::ImGetTextureHeight(m_bm_texture);
            float adj_height = window_size.y - 40;
            float adj_width = adj_height * (width / height);
            auto display_size = ImVec2(adj_width, adj_height);
            auto display_pos = window_pos + ImVec2(window_size.x / 8, 0);
            ShowVideoWindow(draw_list, m_bm_texture, display_pos, display_size, offset_x, offset_y, tf_x, tf_y);
        }
    }
    ImGui::EndChild();

    // File Dialog
    ImVec2 minSize = ImVec2(600, 400);
    ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (ImGuiFileDialog::Instance()->Display("##PotraceFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        m_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
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