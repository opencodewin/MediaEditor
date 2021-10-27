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
#include "MediaPlayer.h"
#define ICON_STEP_NEXT      "\uf051"
#define ICON_SPEED_MINUS_2  "\ue3cc"
#define ICON_SPEED_MINUS_1  "\ue3cb"
#define ICON_SPEED_ZERO     "\ue3cf"
#define ICON_SPEED_PLUS_1   "\ue3cd"
#define ICON_SPEED_PLUS_2   "\ue3ce"

static std::string ini_file = "Media_Player.ini";
static std::string bookmark_path = "bookmark.ini";
MediaPlayer g_player;

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
    gst_init(nullptr, nullptr);
}

void Application_Finalize(void** handle)
{
    if (g_player.isOpen())
        g_player.close();
    gst_deinit();
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

bool Application_Frame(void * handle)
{
    static bool force_software = false;
    bool done = false;
    auto& io = ImGui::GetIO();
    g_player.update();
    // Show PlayControl panel
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.9f);
    ImVec2 panel_size(io.DisplaySize.x - 20.0, 120);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5);
    if (ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
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
        if (ImGui::Button(/*!is ? ICON_FA5_VOLUME_UP : is->muted ? ICON_FA5_VOLUME_MUTE : ICON_FA5_VOLUME_UP*/ICON_FA5_VOLUME_UP, size))
        {
            //if (is) toggle_mute(is);
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
            ImGui::Text("ImGUI Media Player");
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
        // add speed -2 button
        ImGui::SameLine();
        if (ImGui::Button(ICON_SPEED_MINUS_2, size))
        {
            if (g_player.isPlaying()) g_player.setPlaySpeed(0.25);
        }
        ImGui::ShowTooltipOnHover("1/4 Speed Play");
        // add speed -1 button
        ImGui::SameLine();
        if (ImGui::Button(ICON_SPEED_MINUS_1, size))
        {
            if (g_player.isPlaying()) g_player.setPlaySpeed(0.5);
        }
        ImGui::ShowTooltipOnHover("1/2 Speed Play");
        // add speed normal button
        ImGui::SameLine();
        if (ImGui::Button(ICON_SPEED_ZERO, size))
        {
            if (g_player.isPlaying()) g_player.setPlaySpeed(1.0);
        }
        ImGui::ShowTooltipOnHover("Normal Speed Play");
        // add speed +1 button
        ImGui::SameLine();
        if (ImGui::Button(ICON_SPEED_PLUS_1, size))
        {
            if (g_player.isPlaying()) g_player.setPlaySpeed(2.0);
        }
        ImGui::ShowTooltipOnHover("2x Speed Play");
        // add speed +2 button
        ImGui::SameLine();
        if (ImGui::Button(ICON_SPEED_PLUS_2, size))
        {
            if (g_player.isPlaying()) g_player.setPlaySpeed(4.0);
        }
        ImGui::ShowTooltipOnHover("4x Speed Play");
        // add software decode button
        ImGui::SameLine();
        if (ImGui::ToggleButton("SW", &force_software, size))
        {
            g_player.setSoftwareDecodingForced(force_software);
        }
        ImGui::ShowTooltipOnHover("Software decoder");
        // add button end

        ImGui::Unindent((i - 32.0f) * 0.4f);
        ImGui::Separator();
        // add audio meter bar
        static int left_stack = 0;
        static int left_count = 0;
        static int right_stack = 0;
        static int right_count = 0;
        //if (is)
        //{
        //    ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x, 10), &is->audio_left_channel_level, 0, 96, 200, &left_stack, &left_count);
        //    ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x, 10), &is->audio_right_channel_level, 0, 96, 200, &right_stack, &right_count);
        //}
        //else
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
            ImGui::Text("[%.3fms %.1ffps]", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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

    // Message Boxes
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_player.open(filePathName);
            g_player.play(true);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (g_player.texture())
    {
        ImGui::GetBackgroundDrawList ()->AddImage (
            (void *)g_player.texture(),
            ImVec2 (0, 0),
            io.DisplaySize,
            ImVec2 (0, 0),
            ImVec2 (1, 1)
        );
    }
    return done;
}