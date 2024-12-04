#include <application.h>
#include <UI.h>
#include <getopt.h>
#include <string>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#define ENABLE_MULTI_VIEWPORT   1

static bool g_plugin_loading {false};
static bool g_plugin_loaded {false};
static float g_plugin_loading_percentage {0};
static int g_plugin_loading_current_index {0};
static std::string g_plugin_loading_message;
static bool set_context_in_splash = false;
static std::thread * g_loading_plugin_thread {nullptr};

static std::string g_plugin_path = "";

struct BluePrintSettings
{
    BluePrintSettings() {}
    std::string project_path;               // Editor Recently project file path
    bool ShowInfoTooltips {false};
    bool ShowSettingPanel {false};
};

static BluePrintSettings g_blueprint_settings;

static int OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (/*type == BluePrint::BP_CB_Link ||
        type == BluePrint::BP_CB_Unlink ||
        type == BluePrint::BP_CB_NODE_DELETED ||
        type == BluePrint::BP_CB_NODE_APPEND ||*/
        type == BluePrint::BP_CB_NODE_INSERT)
    {
        // need update
        ret = BluePrint::BP_CBR_AutoLink;
    }
    else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
            type == BluePrint::BP_CB_SETTING_CHANGED)
    {
        return BluePrint::BP_CBR_RunAgain;
    }
    return ret;
}

static void LoadPluginThread()
{
    std::vector<std::string> plugin_paths;
    plugin_paths.push_back(g_plugin_path);
    int plugins = BluePrint::BluePrintUI::CheckPlugins(plugin_paths);
    BluePrint::BluePrintUI::LoadPlugins(plugin_paths, g_plugin_loading_current_index, g_plugin_loading_message, g_plugin_loading_percentage, plugins);
    g_plugin_loading_message = "Plugin load finished!!!";
    g_plugin_loading = false;
}

static void BlueprintTest_SetupContext(ImGuiContext* ctx, void * handle, bool in_splash)
{
    if (!ctx)
        return;
    set_context_in_splash = in_splash;
    // Setup BluePrintSetting
    ImGuiSettingsHandler setting_ini_handler;
    setting_ini_handler.TypeName = "BluePrintSetting";
    setting_ini_handler.TypeHash = ImHashStr("BluePrintSetting");
    setting_ini_handler.UserData = handle;
    setting_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return &g_blueprint_settings;
    };
    setting_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        BluePrintSettings * setting = (BluePrintSettings*)entry;
        if (!setting) return;
        int val_int = 0;
        char val_path[1024] = {0};
        if (sscanf(line, "ProjectPath=%[^|\n]", val_path) == 1) { setting->project_path = std::string(val_path); }
        else if (sscanf(line, "ShowInfoTooltips=%d", &val_int) == 1) { setting->ShowInfoTooltips = val_int == 1; }
        else if (sscanf(line, "ShowSettingPanel=%d", &val_int) == 1) { setting->ShowSettingPanel = val_int == 1; }
    };
    setting_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        out_buf->appendf("[%s][##BluePrintSetting]\n", handler->TypeName);
        out_buf->appendf("ProjectPath=%s\n", g_blueprint_settings.project_path.c_str());
        out_buf->appendf("ShowInfoTooltips=%d\n", g_blueprint_settings.ShowInfoTooltips ? 1 : 0);
        out_buf->appendf("ShowSettingPanel=%d\n", g_blueprint_settings.ShowSettingPanel ? 1 : 0);
        out_buf->append("\n");
    };
    setting_ini_handler.ApplyAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler)
    {
        BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)handler->UserData;
        if (UI)
        {
            UI->m_isShowInfoTooltips = g_blueprint_settings.ShowInfoTooltips;
            UI->m_ShowSettingPanel = g_blueprint_settings.ShowSettingPanel;
        }
    };
    ctx->SettingsHandlers.push_back(setting_ini_handler);
#ifdef USE_PLACES_FEATURE
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMarkBPUI";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMarkBPUI");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        if (dialog) dialog->DeserializePlaces(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializePlaces();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

static void BlueprintTest_Initialize(void** handle)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    BluePrint::BluePrintUI * UI = new BluePrint::BluePrintUI();
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    UI->SetCallbacks(callbacks, nullptr);
    *handle = UI;
}

static void BlueprintTest_Finalize(void** handle)
{
    BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)*handle;
    if (!UI)
        return;
    g_blueprint_settings.ShowInfoTooltips = UI->m_isShowInfoTooltips;
    g_blueprint_settings.ShowSettingPanel = UI->m_ShowSettingPanel;
    g_blueprint_settings.project_path = UI->m_Document->m_Path;
    UI->Finalize();
    delete UI;
}

static bool BlueprintTest_Splash_Screen(void* handle, bool& app_will_quit)
{
    static int32_t splash_start_time = ImGui::get_current_time_msec();
    static bool UI_inited = false;
    auto& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    ImGuiCond cond = ImGuiCond_None;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, cond);
    ImGui::Begin("BluePrint Splash", nullptr, flags);
    float dur = (ImGui::get_current_time_msec() - splash_start_time) / 1000.f;
    auto draw_node = [](ImDrawList* draw_list, ImVec2 pos, ImVec2 size, float alpha)
    {
        draw_list->AddRect(pos, pos + ImVec2(size.x, 16), IM_COL32(255, 255, 255, 255 * alpha), 4, ImDrawFlags_RoundCornersTop);
        draw_list->AddRect(pos + ImVec2(0, 16), pos + size, IM_COL32(128, 128, 255, 255 * alpha));
        draw_list->AddRectFilled(pos + ImVec2(0, 16), pos + size, IM_COL32(0, 0, 64, 128 * alpha));
        draw_list->AddCircle(pos + ImVec2(8, 16 + 8), 3, IM_COL32(128, 255, 128, 255 * alpha));
        draw_list->AddCircle(pos + ImVec2(8, 16 + 8 + 8), 3, IM_COL32(128, 255, 128, 255 * alpha));
        draw_list->AddCircle(pos + ImVec2(size.x - 8, 16 + 8), 3, IM_COL32(255, 128, 128, 255 * alpha));
        draw_list->AddCircle(pos + ImVec2(size.x - 8, 16 + 8 + 8), 3, IM_COL32(255, 128, 128, 255 * alpha));
        draw_list->AddCircle(pos + ImVec2(size.x - 8, 16 + 8 + 8 + 8), 3, IM_COL32(255, 128, 128, 255 * alpha));
    };
    auto draw_line = [](ImDrawList* draw_list, ImVec2 pos1, ImVec2 pos2, float alpha)
    {
        ImVec2 sz = pos2 - pos1;
        ImVec2 cp4[4] = { ImVec2(pos1.x, pos1.y), ImVec2(pos1.x + sz.x * 1.1f, pos1.y + sz.y * 0.3f), ImVec2(pos1.x + sz.x - sz.x * 1.1f, pos1.y + sz.y - sz.y * 0.3f), ImVec2(pos2.x, pos2.y) };
        draw_list->AddBezierCubic(cp4[0], cp4[1], cp4[2], cp4[3], IM_COL32(128, 128, 128, 255 * alpha), 2);
    };

    {
        auto draw_list = ImGui::GetWindowDrawList();
        float line_width = dur * io.DisplaySize.x;
        float line_height = dur > 1 ? (dur - 1) * (io.DisplaySize.y - 64) : 0;
        line_height = fmin(line_height, floor((io.DisplaySize.y - 64) / 30) * 30);
        for (int y = 0; y < io.DisplaySize.y - 64; y += 30)
            draw_list->AddLine(ImVec2(0, y), ImVec2(line_width, y), IM_COL32(128, 128, 128, 128));
        for (int x = 0; x < io.DisplaySize.x; x += 30)
            draw_list->AddLine(ImVec2(x, 0), ImVec2(x, line_height), IM_COL32(128, 128, 128, 128));
        if (dur > 2)
        {
            float alpha = fmin((dur - 2) * 2.0, 1.0);
            draw_node(draw_list, ImVec2(20, 20), ImVec2(80, 80), alpha);
        }
        if (dur > 2.5)
        {
            float alpha = fmin((dur - 2.5) * 2.0, 1.0);
            draw_node(draw_list, ImVec2(120, 160), ImVec2(100, 100), alpha);
        }
        if (dur > 3)
        {
            float alpha = fmin((dur - 3) * 2.0, 1.0);
            draw_node(draw_list, ImVec2(300, 80), ImVec2(120, 100), alpha);
        }
        if (dur > 3.5)
        {
            float alpha = fmin((dur - 3.5) * 2.0, 1.0);
            draw_node(draw_list, ImVec2(500, 100), ImVec2(80, 100), alpha);
        }
        if (dur > 4)
        {
            float alpha = fmin((dur - 4) * 2.0, 1.0);
            draw_node(draw_list, ImVec2(660, 160), ImVec2(120, 120), alpha);
        }
        if (dur > 5)
        {
            float alpha = fmin((dur - 5) / 2.0, 1.0);
            draw_line(draw_list, ImVec2(20, 20) + ImVec2(80 - 8, 16 + 8), ImVec2(120, 160) + ImVec2(8, 16 + 8), alpha);
            draw_line(draw_list, ImVec2(120, 160) + ImVec2(100 - 8, 16 + 8), ImVec2(300, 80) + ImVec2(8, 16 + 8), alpha);
            draw_line(draw_list, ImVec2(120, 160) + ImVec2(100 - 8, 16 + 16), ImVec2(300, 80) + ImVec2(8, 16 + 16), alpha);
            draw_line(draw_list, ImVec2(300, 80) + ImVec2(120 - 8, 16 + 8), ImVec2(500, 100) + ImVec2(8, 16 + 8), alpha);
            draw_line(draw_list, ImVec2(500, 100) + ImVec2(80 - 8, 16 + 8), ImVec2(660, 160) + ImVec2(8, 16 + 8), alpha);
            draw_line(draw_list, ImVec2(500, 100) + ImVec2(80 - 8, 16 + 16), ImVec2(660, 160) + ImVec2(8, 16 + 16), alpha);
        }
        if (dur > 3)
        {
            float alpha = fmin((dur - 3) / 4.0, 1.0);
            ImGui::SetWindowFontScale(3.0);
            std::string str = "BluePrint Editor";
            draw_list->AddTextComplex(ImVec2(240, 240), str.c_str(), 3.f, IM_COL32(128, 128, 255, alpha * 255), 0.f, 0, ImVec2(4, 4), IM_COL32(0, 0, 0, 255));
            ImGui::SetWindowFontScale(1.0);
        }
        if (dur > 7)
        {
            int ver_major = 0, ver_minor = 0, ver_patch = 0, ver_build = 0;
            std::string version_string;
            ImVec2 version_size;
            float vxoft, vyoft;
            // imgui version
            ImGui::GetVersion(ver_major, ver_minor, ver_patch, ver_build);
            version_string = "ImGui: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
            version_size = ImGui::CalcTextSize(version_string.c_str());
            vxoft = io.DisplaySize.x - version_size.x - 32;
            vyoft = io.DisplaySize.y - 18 * 3 - 32 - 32;
            ImGui::GetWindowDrawList()->AddText(ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, 255), version_string.c_str());
            // Blurprint SDK version
            BluePrint::GetVersion(ver_major, ver_minor, ver_patch, ver_build);
            version_string = "BluePrint: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
            version_size = ImGui::CalcTextSize(version_string.c_str());
            vxoft = io.DisplaySize.x - version_size.x - 32;
            vyoft = io.DisplaySize.y - 18 * 2 - 32 - 32;
            ImGui::GetWindowDrawList()->AddText(ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, 255), version_string.c_str());
#if IMGUI_VULKAN_SHADER
            // vkshader version
            ImGui::ImVulkanGetVersion(ver_major, ver_minor, ver_patch, ver_build);
            version_string = "VkShader: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
            version_size = ImGui::CalcTextSize(version_string.c_str());
            vxoft = io.DisplaySize.x - version_size.x - 32;
            vyoft = io.DisplaySize.y - 18 * 1 - 32 - 32;
            ImGui::GetWindowDrawList()->AddText(ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, 255), version_string.c_str());
#endif
            // copyright
            std::string copy_str = "Copyright(c) 2023-2024 OpenCodeWin Team";
            auto copy_size = ImGui::CalcTextSize(copy_str.c_str());
            ImGui::GetWindowDrawList()->AddText(ImVec2(io.DisplaySize.x - copy_size.x - 16, io.DisplaySize.y - 32 - 24), IM_COL32(128, 128, 255, 255), copy_str.c_str());
        }
    }
    if (!g_plugin_loaded)
    {
        g_plugin_loading = true;
        g_loading_plugin_thread = new std::thread(LoadPluginThread);
        g_plugin_loaded = true;
    }
    std::string load_str;
    if (g_plugin_loading)
    {
        load_str = "Loading Plugin:" + g_plugin_loading_message;
        auto loading_size = ImGui::CalcTextSize(load_str.c_str());
        float xoft = 4;
        float yoft = io.DisplaySize.y - loading_size.y - 32 - 8;
        ImGui::SetCursorPos(ImVec2(xoft, yoft));
        ImGui::TextUnformatted("Loading Plugin:"); ImGui::SameLine();
        ImGui::Text("%s", g_plugin_loading_message.c_str());
    }
    else
    {
        BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)handle;
        if (!UI_inited && UI)
        {
            UI->Initialize(g_blueprint_settings.project_path.c_str());
            UI_inited = true;
        }
    }
    float percentage = std::min(g_plugin_loading_percentage, 1.0f);
    ImGui::SetCursorPos(ImVec2(4, io.DisplaySize.y - 32));
    ImGui::ProgressBar("##splash_progress", percentage, 0.f, 1.f, "", ImVec2(io.DisplaySize.x - 16, 8), 
                        ImVec4(0.3f, 0.3f, 0.8f, 1.f), ImVec4(0.1f, 0.1f, 0.2f, 1.f), ImVec4(0.f, 0.f, 0.8f, 1.f));
    ImGui::UpdateData();

    ImGui::End();
    return dur > 9 && !g_plugin_loading;
}

static bool BlueprintTest_Frame(void * handle, bool app_will_quit)
{
    static const char* buttons[] = { "Quit", "Cancel", "Save", NULL };
    static ImGui::MsgBox msgbox_event;
    msgbox_event.Init("Quit Editor?", ICON_MD_WARNING, "Current document is modified.\nAre you really sure you want to Quit?", buttons, false);
    BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)handle;
    if (!UI)
        return true;
    auto app_done = UI->Frame() || app_will_quit;
    if (app_done)
    {
        if (UI->m_Document && UI->File_IsModified())
        {
            // need confirm quit
            msgbox_event.Open();
            app_done = false;
        }
    }

    auto msg_ret = msgbox_event.Draw(400);
    if (msg_ret == 1)
    {
        app_done = true;
    }
    else if (msg_ret == 2)
    {
        UI->Resume();
        app_done = false;
    }
    else if (msg_ret == 3)
    {
        UI->File_Save();
        app_done = true;
    }
    return app_done;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    // param commandline args
    static struct option long_options[] = {
        { "plugin_dir", required_argument, NULL, 'p' },
        { 0, 0, 0, 0 }
    };
    if (property.argc > 1 && property.argv)
    {
        int o = -1;
        int option_index = 0;
        while ((o = getopt_long(property.argc, property.argv, "p:", long_options, &option_index)) != -1)
        {
            if (o == -1)
                break;
            switch (o)
            {
                case 'p': g_plugin_path = std::string(optarg); break;
                default: break;
            }
        }
    }

    property.name = "BlueprintEdit";
    property.docking = false;
    property.resizable = false;
    property.full_size = true;
    property.auto_merge = false;
    property.font_scale = 2.0f;
    property.low_reflash = true;
    property.power_save = true;
    property.splash_screen_width = 800;
    property.splash_screen_height = 400;
    property.splash_screen_alpha = 0.95;
    property.application.Application_SetupContext = BlueprintTest_SetupContext;
    property.application.Application_SplashScreen = BlueprintTest_Splash_Screen;
    property.application.Application_Initialize = BlueprintTest_Initialize;
    property.application.Application_Finalize = BlueprintTest_Finalize;
    property.application.Application_Frame = BlueprintTest_Frame;
}