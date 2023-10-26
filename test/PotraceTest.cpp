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
#define ICON_RESET              u8"\ue042"
#else
#define ICON_RESET              "r"
#endif

std::string image_file_dis = "*.png *.gif *.jpg *.jpeg *.tiff *.webp";
std::string image_file_suffix = ".png,.gif,.jpg,.jpeg,.tiff,.webp";
std::string image_filter = "Image files (" + image_file_dis + "){" + image_file_suffix + "}" + "," + ".*";

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2& pos, ImVec2& size, float& offset_x, float& offset_y, float& tf_x, float& tf_y, float aspectRatio = 0.f, ImVec2 start = ImVec2(0.f, 0.f), ImVec2 end = ImVec2(1.f, 1.f), bool bLandscape = true)
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
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        tf_x = (size.x - adj_x) / 2.0;
        tf_y = (size.y - adj_y) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        draw_list->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_x, offset_y + adj_y),
            start,
            end
        );
        tf_x = offset_x + adj_x;
        tf_y = offset_y + adj_y;
    }
}

// Application Framework Functions
static void Potrace_SetupContext(ImGuiContext* ctx, bool in_splash)
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

static void Potrace_Finalize(void** handle)
{
}

static bool Potrace_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    static potrace_bitmap_t * bm = nullptr;
    static ImTextureID m_texture = 0;
    static ImTextureID m_bm_texture = 0;
    static std::string m_file_path;

    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Potrace Test", nullptr, flags);

    auto process_image = [](potrace_bitmap_t * bitmap, ImTextureID* texture)
    {
        potrace_state_t *st = nullptr;
        imginfo_t imginfo;
        if (!bitmap) return;
        if (*texture) { ImGui::ImDestroyTexture(*texture); *texture = 0; }
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
            ImGui::ImMatToTexture(mat, *texture);
    };
    auto update_image = [&](potrace_bitmap_t * bitmap, ImTextureID* texture)
    {
        if (bitmap) process_image(bitmap, texture);
    };
    // control panel
    ImGui::BeginChild("##Potrace_Config", ImVec2(400, ImGui::GetWindowHeight() - 60), true);
    ImGui::PushItemWidth(200);
    if (ImGui::Button( ICON_IGFD_FOLDER_OPEN " Open File...", ImVec2(280,32)))
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
    if (ImGui::Button(ICON_RESET "##reset_turdsize")) { turdsize = 2; }
    ImGui::SameLine();  
    ImGui::SliderInt("Suppress speckles", &turdsize, 0, 100);
    if (turdsize != info.param->turdsize)
    {
        info.param->turdsize = turdsize;
        update_image(bm, &m_bm_texture);
    }

    float alphamax = info.param->alphamax;
    if (ImGui::Button(ICON_RESET "##reset_alphamax")) { alphamax = 1; }
    ImGui::SameLine();  
    ImGui::SliderFloat("Corner threshold", &alphamax, 0.f, 1.5f, "%.2f");
    if (alphamax != info.param->alphamax)
    {
        info.param->alphamax = alphamax;
        update_image(bm, &m_bm_texture);
    }

    float unit = info.unit;
    if (ImGui::Button(ICON_RESET "##reset_unit")) { unit = 10.f; }
    ImGui::SameLine();  
    ImGui::SliderFloat("Quantize output", &unit, 0.f, 100.f, "%.0f");
    if (unit != info.unit)
    {
        info.unit = unit;
        update_image(bm, &m_bm_texture);
    }

    float opttolerance = info.param->opttolerance;
    if (ImGui::Button(ICON_RESET "##reset_opttolerance")) { opttolerance = 0.2f; }
    ImGui::SameLine();  
    ImGui::SliderFloat("Optimization tolerance", &opttolerance, 0.f, 1.f, "%.2f");
    if (opttolerance != info.param->opttolerance)
    {
        info.param->opttolerance = opttolerance;
        update_image(bm, &m_bm_texture);
    }

    bool longcurve = info.param->opticurve == 0;
    if (ImGui::Checkbox("Curve optimization", &longcurve))
    {
        info.param->opticurve = longcurve ? 0 : 1;
        update_image(bm, &m_bm_texture);
    }

    ImGui::TextUnformatted("Scaling and placement options:");
    float angle = info.angle;
    if (ImGui::Button(ICON_RESET "##reset_angle")) { angle = 0.f; }
    ImGui::SameLine();  
    ImGui::SliderFloat("Rotate counterclockwise", &angle, -180.f, 180.f, "%.1f");
    if (angle != info.angle)
    {
        info.angle = angle;
        update_image(bm, &m_bm_texture);
    }

    ImGui::PopItemWidth();
    ImGui::EndChild();

    // run panel
    ImGui::SameLine();
    ImGui::BeginChild("##Potrace_Result", ImVec2(ImGui::GetWindowWidth() - 400 - 30, ImGui::GetWindowHeight() - 60), true);
    auto draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (m_texture)
    {
        auto width = (float)ImGui::ImGetTextureWidth(m_texture);
        auto height = (float)ImGui::ImGetTextureHeight(m_texture);
        float adj_width = window_size.x / 8;
        float adj_height = adj_width * (height / width);
        auto display_size = ImVec2(adj_width, adj_height);
        ShowVideoWindow(draw_list, m_texture, window_pos, display_size, offset_x, offset_y, tf_x, tf_y);
    }
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
    ImGui::EndChild();

    // File Dialog
    ImVec2 minSize = ImVec2(600, 400);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (ImGuiFileDialog::Instance()->Display("##PotraceFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        m_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
        int width = 0, height = 0, component = 0;
        stbi_uc *data = stbi_load(m_file_path.c_str(), &width, &height, &component, 4);
        if (data)
        {
            if (bm) { bm_free(bm); bm = nullptr; }
            bm = bm_new(width, height);
            if (bm)
            {
                unsigned int c;
                for (int y = 0; y < height; y++)
                {
                    for (int x = 0; x < width; x++)
                    {
                        size_t index = 4 * (y * width + x);
                        c = (data[index + 0] & 0xff) + (data[index + 1] & 0xff) + (data[index + 2] & 0xff);
                        BM_UPUT(bm, x, height - y, c > 3 * info.blacklevel * 255 ? 0 : 1);
                    }
                }
                process_image(bm, &m_bm_texture);
            }
            if (m_texture) { ImGui::ImDestroyTexture(m_texture); m_texture = 0; }
            m_texture = ImGui::ImCreateTexture(data, width, height);
            stbi_image_free(data);
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if (app_will_quit)
    {
        if (bm) { bm_free(bm); bm = nullptr; }
        if (m_texture) { ImGui::ImDestroyTexture(m_texture); m_texture = 0; }
        if (m_bm_texture) { ImGui::ImDestroyTexture(m_bm_texture); m_bm_texture = 0; }
        app_done = true;
    }
    ImGui::End();
    return app_done;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "Potrace Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1680;
    property.height = 900;
    property.application.Application_SetupContext = Potrace_SetupContext;
    property.application.Application_Finalize = Potrace_Finalize;
    property.application.Application_Frame = Potrace_Frame;
    init_info();
}