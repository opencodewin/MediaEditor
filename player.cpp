#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <imgui_knob.h>
#include <sstream>
#include <fstream>

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
}

void Application_Finalize(void** handle)
{

}

bool Application_Frame(void * handle)
{
    bool done = false;
    auto& io = ImGui::GetIO();
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
        if (ImGui::Button(/*!is ? ICON_FAD_PLAY : is->paused ? ICON_FAD_PLAY : ICON_FAD_PAUSE*/ICON_FAD_PLAY, size))
        {
            //if (is) toggle_pause(is);
        }
        ImGui::ShowTooltipOnHover("Toggle Play/Pause.");
        // add mute button
        ImGui::SameLine();
        if (ImGui::Button(/*!is ? ICON_FA5_VOLUME_UP : is->muted ? ICON_FA5_VOLUME_MUTE : ICON_FA5_VOLUME_UP*/ICON_FA5_VOLUME_UP, size))
        {
            //if (is) toggle_mute(is);
        }
        ImGui::ShowTooltipOnHover("Toggle Audio Mute.");
        // add info button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_INDENT, size))
        {
            //if (is) show_info = !show_info;
        }
        ImGui::ShowTooltipOnHover("Media Info.");

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
        /*
        if (is && is->total_time > 0)
        {
            float time = get_master_clock(is);
            if (time == NAN) time = 0;
            float oldtime = time;
            static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoLabel;
            if (ImGui::SliderFloat("time", &time, 0, is->total_time, "%.2f", flags))
            {
                float incr = time - oldtime;
                if (fabs(incr) > 1.0)
                {
                    if (is->ic->start_time != AV_NOPTS_VALUE && time < is->ic->start_time / (double)AV_TIME_BASE)
                        time = is->ic->start_time / (double)AV_TIME_BASE;
                    stream_seek(is, (int64_t)(time * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
                }
            }
            ImGui::SameLine();
            int hours = time / 60 / 60; time -= hours * 60 * 60;
            int mins = time / 60; time -= mins * 60;
            int secs = time; time -= secs;
            int ms = time * 1000;
            ImGui::Text("%02d:%02d:%02d.%03d", hours, mins, secs, ms);

            ImGui::SameLine();
            float ftime = is->total_time * 1000.0f;
            hours = ftime / 1000 / 60 / 60; ftime -= hours * 60 * 60 * 1000;
            mins = ftime / 1000 / 60; ftime -= mins * 60 * 1000;
            secs = ftime / 1000; ftime -= secs * 1000;
            ms = ftime;
            ImGui::Text("/ %02d:%02d:%02d.%03d", hours, mins, secs, ms);

            ImGui::SameLine();
            ImGui::Text("[%.3fms %.1ffps]", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        else
        */
        {
            // draw empty bar
            float time = 0;
            ImGui::SliderFloat("time", &time, 0, 0, "%.2f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoLabel);
        }
        /*
        if (show_status)
        {
            ImGui::Separator();
            // add stats bar
            if (is && !is->stats_string.empty())
                ImGui::Text("%s", is->stats_string.c_str());
            else
                ImGui::Text("no media");
        }
        */
        ImGui::End();
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
            /*
            if (is != nullptr)
            {
                if (is) stream_close(is);
                is = nullptr;
            }
            is = stream_open(filePathName.c_str(), nullptr);
            if (is == nullptr)
            {
                ImGui::OpenPopup("Open Error?");
            }
            */
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SetNextWindowPos(modal_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Open Error?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("无法打开媒体文件");
        ImGui::Separator();

        if (ImGui::Button("确定", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }


    return done;
}