#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <imgui_knob.h>
#include <iostream>
#include <sstream>
#include <fstream>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#include "MediaPlayer.h"
#include "Log.h"
#include "GstToolkit.h"

#define ICON_STEP_NEXT      "\uf051"

static std::string ini_file = "Media_Player.ini";
static std::string bookmark_path = "bookmark.ini";
static ImTextureID g_texture = 0;
MediaPlayer g_player;
#if IMGUI_VULKAN_SHADER
ImGui::ColorConvert_vulkan * m_yuv2rgb {nullptr};
#endif
// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Player";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.width = 1280;
    property.height = 720;
}

void Application_Initialize(void** handle)
{
    gst_init(nullptr, nullptr);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
#ifdef USE_BOOKMARK
	// load bookmarks
	std::ifstream docFile(bookmark_path, std::ios::in);
	if (docFile.is_open())
	{
		std::stringstream strStream;
		strStream << docFile.rdbuf();//read the file
		ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
		docFile.close();
	}
#endif
#if IMGUI_VULKAN_SHADER
    m_yuv2rgb = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
#endif
}

void Application_Finalize(void** handle)
{
#if IMGUI_VULKAN_SHADER
    if (m_yuv2rgb) { delete m_yuv2rgb; m_yuv2rgb = nullptr; }
#endif
    if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
    if (g_player.isOpen())
        g_player.close();
#ifdef USE_BOOKMARK
	// save bookmarks
	std::ofstream configFileWriter(bookmark_path, std::ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif
    gst_deinit();
}

bool Application_Frame(void * handle)
{
    static bool show_ctrlbar = true;
    static bool show_log_window = false; 
    static bool force_software = false;
    static float play_speed = 1.0f;
    static bool muted = false;
    static bool full_screen = false;
    static double volume = 0;
    static int ctrlbar_hide_count = 0;
    bool done = false;
    auto& io = ImGui::GetIO();

    g_player.update();

    // Show PlayControl panel
    if (g_player.isOpen() && (show_ctrlbar && io.FrameCountSinceLastInput))
    {
        ctrlbar_hide_count++;
        if (ctrlbar_hide_count >= 200)
        {
            ctrlbar_hide_count = 0;
            show_ctrlbar = false;
        }
    }
    if (io.FrameCountSinceLastInput == 0)
    {
        ctrlbar_hide_count = 0;
        show_ctrlbar = true;
    }

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.9f);
    ImVec2 panel_size(io.DisplaySize.x - 20.0, 120);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5);
    if (show_ctrlbar && ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        int i = ImGui::FindWindowByName("Control")->Size.x;
        ImGui::Indent((i - 32.0f) * 0.4f);
        ImVec2 size = ImVec2(32.0f, 32.0f); // Size of the image we want to make visible
        ImFont* font = ImGui::GetFont();
        float org_scale = font->Scale;
        // add open button
        ImGui::SetWindowFontScale(org_scale * 1.2);
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN, size))
        {
            //ImGui::ShowTooltipOnHover("Open Media File.");
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.avi){.mp4,.mov,.mkv,.avi,.MP4,.MOV,.MKV,.AVI},.*";
			ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", filters, ".", 1, nullptr, ImGuiFileDialogFlags_ShowBookmark);
        }
        ImGui::ShowTooltipOnHover("Open Media File.");
        // add play button
        ImGui::SameLine();
        ImGui::SetWindowFontScale(org_scale * 1.5);
        if (ImGui::Button(g_player.isOpen() ? (g_player.isPlaying() ? ICON_FAD_PAUSE : ICON_FAD_PLAY) : ICON_FAD_PLAY, size))
        {
            if (g_player.isPlaying()) g_player.play(false);
            else g_player.play(true);
        }
        ImGui::ShowTooltipOnHover("Toggle Play/Pause.");
        // add step next button
        ImGui::SameLine();
        if (ImGui::Button(ICON_STEP_NEXT, size))
        {
            if (g_player.isOpen()) g_player.step();
        }
        ImGui::ShowTooltipOnHover("Step Next Frame.");
        // add mute button
        ImGui::SameLine();
        if (ImGui::Button(g_player.isOpen() ? muted ? ICON_FA5_VOLUME_MUTE : ICON_FA5_VOLUME_UP : ICON_FA5_VOLUME_UP, size))
        {
            muted = !muted;
            if (muted)
            {
                volume = g_player.volume();
                g_player.set_volume(0);
            }
            else
            {
                g_player.set_volume(volume);
            }
        }
        ImGui::ShowTooltipOnHover("Toggle Audio Mute.");
        // add about button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_TAG, size))
        {
            ImGui::OpenPopup("##about", ImGuiPopupFlags_AnyPopup);
        }
        ImGui::ShowTooltipOnHover("Show About.");

        ImGui::SetWindowFontScale(org_scale);
        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("##about", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("ImGUI Media Player(GStreamer)");
            ImGui::Separator();
            ImGui::Text("Dicky 2021");
            ImGui::Separator();
            int i = ImGui::GetCurrentWindow()->ContentSize.x;
            ImGui::Indent((i - 40.0f) * 0.5f);
            if (ImGui::Button("OK", ImVec2(40, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        if (ImGui::SliderFloat("Speed", &play_speed, -2.f, 2.f, "%.1f", ImGuiSliderFlags_NoInput))
        {
            if (g_player.isPlaying()) g_player.setPlaySpeed(play_speed);
        }
        ImGui::PopItemWidth();
        // add software decode button
        ImGui::SameLine();
        if (ImGui::ToggleButton("SW", &force_software, size))
        {
            g_player.setSoftwareDecodingForced(force_software);
        }
        ImGui::ShowTooltipOnHover("Software decoder");
        // add show log button
        ImGui::SameLine();
        ImGui::ToggleButton(ICON_FA5_LIST_UL, &show_log_window, size);
        ImGui::ShowTooltipOnHover("Show Log");
        // add button end

        ImGui::Unindent((i - 32.0f) * 0.4f);
        ImGui::Separator();
        // add audio meter bar
        static int left_stack = 0;
        static int left_count = 0;
        static int right_stack = 0;
        static int right_count = 0;
        if (g_player.isOpen())
        {
            int l_level = g_player.audio_level(0);
            int r_level = g_player.audio_level(1);
            ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x, 10), &l_level, 0, 96, 200, &left_stack, &left_count);
            ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x, 10), &r_level, 0, 96, 200, &right_stack, &right_count);
        }
        else
        {
            int zero_channel_level = 0;
            ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x, 10), &zero_channel_level, 0, 96, 200);
            ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x, 10), &zero_channel_level, 0, 96, 200);
        }
        ImGui::Separator();

        // add slider bar
        if (g_player.isOpen())
        {
            auto info = g_player.media();
            float total_time = info.end / 1e+9;
            auto gst_time = g_player.position();
            float time = gst_time != GST_CLOCK_TIME_NONE ? gst_time / 1e+9 : 0;
            float oldtime = time;
            static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoLabel;
            if (ImGui::SliderFloat("time", &time, 0, total_time, "%.2f", flags))
            {
                GstClockTime seek_time = time * 1e+9;
                g_player.seek(seek_time);
            }
            ImGui::SameLine();
            int hours = time / 60 / 60; time -= hours * 60 * 60;
            int mins = time / 60; time -= mins * 60;
            int secs = time; time -= secs;
            int ms = time * 1000;
            ImGui::Text("%02d:%02d:%02d.%03d", hours, mins, secs, ms);

            ImGui::SameLine();
            float ftime = total_time * 1000.0f;
            hours = ftime / 1000 / 60 / 60; ftime -= hours * 60 * 60 * 1000;
            mins = ftime / 1000 / 60; ftime -= mins * 60 * 1000;
            secs = ftime / 1000; ftime -= secs * 1000;
            ms = ftime;
            ImGui::Text("/ %02d:%02d:%02d.%03d", hours, mins, secs, ms);

            ImGui::SameLine();
            ImGui::Text("[%.3fms %.1ffps]", 1000.0f / io.Framerate, io.Framerate);
        }
        else
        {
            // draw empty bar
            float time = 0;
            ImGui::SliderFloat("time", &time, 0, 0, "%.2f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoLabel);
        }
        ImGui::End();
    }

    // handle key event
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space), false))
    {
        if (g_player.isPlaying()) g_player.play(false);
        else g_player.play(true);
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        if (g_player.isOpen()) g_player.close();
        done = true;
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos -= 1e+9;
        if (g_pos < 0) g_pos = 0;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos += 1e+9;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos -= 5e+9;
        if (g_pos < 0) g_pos = 0;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos += 5e+9;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z), true))
    {
        g_player.step();
    }

    // handle screen double click event
    if (ImGui::IsMouseDoubleClicked(0) && !ImGui::IsAnyItemHovered())
    {
        full_screen = !full_screen;
        Application_FullScreen(full_screen);
    }

    // Message Boxes
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_player.open(filePathName);
            g_player.play(true);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Show Log Window
    if (show_log_window)
    {
        Log::ShowLogWindow(&show_log_window);
    }

    // Video Texture Render
    ImGui::ImMat vmat = g_player.videoMat();
    if (!vmat.empty())
    {
#if IMGUI_VULKAN_SHADER
        int video_depth = vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
        int video_shift = vmat.depth != 0 ? vmat.depth : vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
#ifdef VIDEO_FORMAT_RGBA
        ImGui::ImGenerateOrUpdateTexture(g_texture, vmat.w, vmat.h, 4, (const unsigned char *)vmat.data);
#else
        ImGui::VkMat im_RGB; im_RGB.type = IM_DT_INT8;
        m_yuv2rgb->YUV2RGBA(vmat, im_RGB, vmat.color_format, vmat.color_space, vmat.color_range, video_depth, video_shift);
        ImGui::ImGenerateOrUpdateTexture(g_texture, im_RGB.w, im_RGB.h, im_RGB.c, im_RGB.buffer_offset() , (const unsigned char *)im_RGB.buffer());
#endif
#endif
    }
    if (g_texture)
    {
        ImVec2 window_size = io.DisplaySize;
        bool bViewisLandscape = window_size.x >= window_size.y ? true : false;
        bool bRenderisLandscape = g_player.width() >= g_player.height() ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? window_size.y : window_size.x;
        float adj_h = bNeedChangeScreenInfo ? window_size.x : window_size.y;
        float adj_x = adj_h * g_player.aspectRatio();
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        float offset_x = (window_size.x - adj_x) / 2.0;
        float offset_y = (window_size.y - adj_y) / 2.0;
        ImGui::GetBackgroundDrawList ()->AddImage (
            (void *)g_texture,
            ImVec2 (offset_x, offset_y),
            ImVec2 (offset_x + adj_x, offset_y + adj_y),
            ImVec2 (0, 0),
            ImVec2 (1, 1)
        );
    }
    return done;
}