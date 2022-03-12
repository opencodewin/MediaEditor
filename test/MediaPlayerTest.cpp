#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <imgui_widget.h>
#include "ImGuiToolkit.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
// #include <Lut3D.h>
#endif
#include "Log.h"
#include "AudioRender.hpp"
#include "MediaPlayer.h"

static std::string ini_file = "Media_Player.ini";
static std::string bookmark_path = "bookmark.ini";
static ImTextureID g_texture = 0;
AudioRender* g_audrnd = nullptr;
MediaPlayer* g_player = nullptr;
#if IMGUI_VULKAN_SHADER
ImGui::ColorConvert_vulkan * m_yuv2rgb {nullptr};
// ImGui::LUT3D_vulkan *        m_lut3d {nullptr};
#endif

#define CheckPlayerError(funccall) \
    if (!(funccall)) \
    { \
        std::cerr << g_player->GetError() << std::endl; \
    }

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Player";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
}

void Application_SetupContext(ImGuiContext* ctx)
{
}

void Application_Initialize(void** handle)
{
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
    g_audrnd = CreateAudioRender();
#if !IMGUI_APPLICATION_PLATFORM_SDL2
    if (!g_audrnd->Initialize())
        std::cerr << g_audrnd->GetError() << std::endl;
#endif

    g_player = CreateMediaPlayer();
    CheckPlayerError(g_player->SetAudioRender(g_audrnd));
    // g_player->SetPreferHwDecoder(false);
    // g_player->SetPlayMode(MediaPlayer::PlayMode::AUDIO_ONLY);
    // g_player->SetPlayMode(MediaPlayer::PlayMode::VIDEO_ONLY);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
#if IMGUI_VULKAN_SHADER
    m_yuv2rgb = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
#endif
}

void Application_Finalize(void** handle)
{
#if IMGUI_VULKAN_SHADER
    if (m_yuv2rgb) { delete m_yuv2rgb; m_yuv2rgb = nullptr; }
    // if (m_lut3d) { delete m_lut3d; m_lut3d = nullptr; }
#endif
    if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }

    ReleaseMediaPlayer(&g_player);
    ReleaseAudioRender(&g_audrnd);
#ifdef USE_BOOKMARK
	// save bookmarks
	std::ofstream configFileWriter(bookmark_path, std::ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif
}

bool Application_Frame(void * handle, bool app_will_quit)
{
    static bool show_ctrlbar = true;
    static bool show_log_window = false; 
    static bool force_software = false;
    static bool convert_hdr = false;
    static bool has_hdr = false;
    static bool muted = false;
    static bool full_screen = false;
    static int ctrlbar_hide_count = 0;
    bool app_done = false;
    auto& io = ImGui::GetIO();
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    Log::Render();

    // Show PlayControl panel
    if (g_player->IsOpened() && (show_ctrlbar && io.FrameCountSinceLastInput))
    {
        ctrlbar_hide_count++;
        if (ctrlbar_hide_count >= 100)
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

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.85f);
    ImVec2 panel_size(io.DisplaySize.x - 40.0, 160);
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
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi){.mp4,.mov,.mkv,.webm,.avi,.MP4,.MOV,.MKV,WEBM,.AVI},.*";
			ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", filters, "/mnt/data2/video/hd/", 1, nullptr, ImGuiFileDialogFlags_ShowBookmark);
        }
        ImGui::ShowTooltipOnHover("Open Media File.");
//         // add open camera button
//         ImGui::SameLine();
//         if (ImGui::Button(ICON_FA5_VIDEO, size))
//         {
//             if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
//             if (g_audio_dev) { SDL_ClearQueuedAudio(g_audio_dev); SDL_CloseAudioDevice(g_audio_dev); g_audio_dev = 0; }
// #if IMGUI_VULKAN_SHADER
//             if (m_lut3d) { delete m_lut3d; m_lut3d = nullptr; }
//             has_hdr = false;
//             convert_hdr = false;
// #endif
//             g_player->open("camera");
//             g_player->play(true);
//         }
//         ImGui::ShowTooltipOnHover("Open Camera.");
        // add play button
        ImGui::SameLine();
        ImGui::SetWindowFontScale(org_scale * 1.5);
        if (ImGui::Button(g_player->IsOpened() ? (g_player->IsPlaying() ? ICON_FAD_PAUSE : ICON_FAD_PLAY) : ICON_FAD_PLAY, size))
        {
            if (g_player->IsPlaying())
                g_player->Pause();
            else
                g_player->Play();
        }
        ImGui::ShowTooltipOnHover("Toggle Play/Pause.");
        // // add step next button
        // ImGui::SameLine();
        // if (ImGui::Button(ICON_FA5_STEP_FORWARD, size))
        // {
        //     if (g_player->IsOpened()) g_player->step();
        // }
        // ImGui::ShowTooltipOnHover("Step Next Frame.");
        // add mute button
        ImGui::SameLine();
        if (ImGui::Button(g_player->IsOpened() ? muted ? ICON_FA5_VOLUME_MUTE : ICON_FA5_VOLUME_UP : ICON_FA5_VOLUME_UP, size))
        {
            muted = !muted;
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
        float play_speed = g_player->GetPlaySpeed();
        if (ImGui::SliderFloat("Speed", &play_speed, -2.f, 2.f, "%.1f", ImGuiSliderFlags_NoInput))
        {
            if (g_player->IsOpened())
            {
                g_player->SetPlaySpeed(play_speed);
            }
        }
        ImGui::PopItemWidth();
        // add software decode button
        ImGui::SameLine();
        if (ImGui::ToggleButton("SW", &force_software, size * 0.75))
        {
            g_player->SetPreferHwDecoder(!force_software);
        }
        ImGui::ShowTooltipOnHover("Software decoder");
        // // add HDR decode button
        // ImGui::SameLine();
        // if (ImGui::ToggleButton(has_hdr ? "HDR" : "SDR", &convert_hdr, size * 0.75))
        // {
        //     if (!has_hdr) convert_hdr = false;
        // }
        // ImGui::ShowTooltipOnHover("HDR decoder");
        // add show log button
        ImGui::SameLine();
        ImGui::ToggleButton(ICON_FA5_LIST_UL, &show_log_window, size * 0.75);
        ImGui::ShowTooltipOnHover("Show Log");
        // add button end

        // show time info
        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine();
        ImGui::Text("%s/%s", ImGuiToolkit::MillisecToString(g_player->GetPlayPos()).c_str(), ImGuiToolkit::MillisecToString(g_player->GetDuration()).c_str());

        ImGui::Unindent((i - 32.0f) * 0.4f);
        ImGui::Separator();

        float padding = style.FramePadding.x * 16;
        // add audio meter bar
        static int left_stack = 0;
        static int left_count = 0;
        static int right_stack = 0;
        static int right_count = 0;
        if (g_player->IsOpened())
        {
            // int l_level = g_player->audio_level(0);
            // int r_level = g_player->audio_level(1);
            int l_level = 1, r_level = 1;
            ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x - padding, 10), &l_level, 0, 96, 200, &left_stack, &left_count);
            ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x - padding, 10), &r_level, 0, 96, 200, &right_stack, &right_count);
        }
        else
        {
            int zero_channel_level = 0;
            ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x - padding, 10), &zero_channel_level, 0, 96, 200);
            ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x - padding, 10), &zero_channel_level, 0, 96, 200);
        }
        ImGui::Separator();
        auto timescale_width = io.DisplaySize.x - padding;
        if (g_player->IsOpened())
        {
            uint64_t pos = g_player->GetPlayPos();
            uint64_t duration = g_player->GetDuration();
            uint64_t step = duration/500;
            if (ImGuiToolkit::TimelineSlider("##timeline", &pos, duration, step, timescale_width))
            {
                // g_player->Seek(pos);
                // g_player->Seek(pos, true);
                g_player->SeekAsync(pos);
            }
            else if (g_player->IsSeeking())
            {
                g_player->QuitSeekAsync();
            }
        }
        else
        {
            uint64_t seek_t = 0;
            ImGuiToolkit::TimelineSlider("##timeline", &seek_t, 0, 0, timescale_width);
        }
        ImGui::Separator();

        // // add buffer extent
        // float v_extent = g_player->video_buffer_extent();
        // ImGui::UvMeter("##video_extent", ImVec2(200, 6), &v_extent, 0, 1.0, 200); ImGui::ShowTooltipOnHover("VB extent:%.1f%%", v_extent * 100);
        // float a_extent = g_player->audio_buffer_extent();
        // ImGui::UvMeter("##audio_extent", ImVec2(200, 6), &a_extent, 0, 1.0, 200); ImGui::ShowTooltipOnHover("AB extent:%.1f%%", a_extent * 100);
        // // add buffer extent end
        ImGui::End();
    }

    // handle key event
    if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space), false))
    {
        if (g_player->IsPlaying())
            g_player->Pause();
        else 
            g_player->Play();
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        if (g_player->IsOpened()) g_player->Close();
        app_done = true;
    }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos -= 5e+9;
    //     if (g_pos < 0) g_pos = 0;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos += 5e+9;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos -= 1e+10;
    //     if (g_pos < 0) g_pos = 0;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos += 1e+10;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z), true))
    // {
    //     g_player->step();
    // }

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
            g_player->Close();
            if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
// #if IMGUI_VULKAN_SHADER
//             if (m_lut3d) { delete m_lut3d; m_lut3d = nullptr; }
//             has_hdr = false;
//             convert_hdr = false;
// #endif
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_player->Open(filePathName);
            g_player->Play();
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Show Log Window
    if (show_log_window)
    {
        Log::ShowLogWindow(&show_log_window);
    }

    // Video Texture Render
    if (g_player->IsOpened())
    {
        ImGui::ImMat vmat = g_player->GetVideo();
        if (!vmat.empty())
        {
#if IMGUI_VULKAN_SHADER
            int video_depth = vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
            int video_shift = vmat.depth != 0 ? vmat.depth : vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
#ifdef VIDEO_FORMAT_RGBA
            ImGui::ImGenerateOrUpdateTexture(g_texture, vmat.w, vmat.h, 4, (const unsigned char *)vmat.data);
#else
            ImGui::VkMat in_RGB; in_RGB.type = IM_DT_INT8;
            in_RGB.color_format = IM_CF_ABGR;
            m_yuv2rgb->ConvertColorFormat(vmat, in_RGB);
            // if (vmat.color_space == IM_CS_BT2020) has_hdr = true; else has_hdr = false;
            // int lut_mode = vmat.flags & IM_MAT_FLAGS_VIDEO_HDR_HLG ? HDRHLG_SDR709 : vmat.flags & IM_MAT_FLAGS_VIDEO_HDR_PQ ? HDRPQ_SDR709 : NO_DEFAULT;
            ImGui::VkMat im_RGB; im_RGB.type = IM_DT_INT8;
            // if (has_hdr && convert_hdr && lut_mode != NO_DEFAULT)
            // {
            //     // Convert HDR to SDR
            //     if (!m_lut3d)
            //     {
            //         m_lut3d = new ImGui::LUT3D_vulkan(lut_mode, IM_INTERPOLATE_TRILINEAR, ImGui::get_default_gpu_index());
            //     }
            //     if (m_lut3d)
            //     {
            //         m_lut3d->filter(in_RGB, im_RGB);
            //     }
            //     else
            //         im_RGB = in_RGB;
            // }
            // else
                im_RGB = in_RGB;
            ImGui::ImGenerateOrUpdateTexture(g_texture, im_RGB.w, im_RGB.h, im_RGB.c, im_RGB.buffer_offset(), (const unsigned char *)im_RGB.buffer());
#endif
#else
#ifdef VIDEO_FORMAT_RGBA
            ImGui::ImGenerateOrUpdateTexture(g_texture, vmat.w, vmat.h, 4, (const unsigned char *)vmat.data);
#endif
#endif
        }
        else
        {
            std::cout << "Empty video ImMat at " << ImGuiToolkit::MillisecToString(g_player->GetPlayPos()) << std::endl;
        }
    }
    if (g_texture)
    {
        ImVec2 window_size = io.DisplaySize;
        bool bViewisLandscape = window_size.x >= window_size.y ? true : false;
        // bool bRenderisLandscape = g_player->width() >= g_player->height() ? true : false;
        bool bRenderisLandscape = true;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? window_size.y : window_size.x;
        float adj_h = bNeedChangeScreenInfo ? window_size.x : window_size.y;
        float adj_x = adj_w;
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
    if (app_will_quit)
    {
        app_done = true;
    }
    return app_done;
}
