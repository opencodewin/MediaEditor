#include <application.h>
#include <imgui_helper.h>
#include "ImSequencer.h"

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 1.0f);
}

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Editor";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.width = 1440;
    property.height = 960;
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
    static int selectedEntry = -1;
    static int firstFrame = 0;
    static int lastFrame = 0;
    static bool expanded = true;
    static int currentFrame = 0;
    static ImSequencer::MediaSequence sequence;
    static bool play = false;
    static ImSequencer::MediaSequence::SequenceItem media0_item("Media1", 0, 200, true);
    sequence.mFrameMin = 0;
    sequence.mFrameMax = 1000;

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Content", nullptr, flags);

    ImVec2 window_size = ImGui::GetWindowSize();
    static float size_main_h = 0.75;
    static float size_timeline_h = 0.25;
    static float old_size_timeline_h = size_timeline_h;

    ImGui::PushID("##Main_Timeline");
    float main_height = size_main_h * window_size.y;
    float timeline_height = size_timeline_h * window_size.y;
    Splitter(false, 4.0f, &main_height, &timeline_height, 32, 32);
    size_main_h = main_height / window_size.y;
    size_timeline_h = timeline_height / window_size.y;
    ImGui::PopID();
    ImVec2 main_pos(4, 0);
    ImVec2 main_size(window_size.x, main_height + 4);
    ImGui::SetNextWindowPos(main_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Top_Panel", main_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 main_window_size = ImGui::GetWindowSize();
        static float size_media_bank_w = 0.25;
        static float size_main_w = 0.75;
        ImGui::PushID("##Bank_Main");
        float bank_width = size_media_bank_w * main_window_size.x;
        float main_width = size_main_w * main_window_size.x;
        Splitter(true, 4.0f, &bank_width, &main_width, 32, 32);
        size_media_bank_w = bank_width / main_window_size.x;
        size_main_w = main_width / main_window_size.x;
        ImGui::PopID();
        
        ImVec2 bank_pos(4, 0);
        ImVec2 bank_size(bank_width - 4, main_window_size.y - 4);
        ImGui::SetNextWindowPos(bank_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Bank_Window", bank_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextUnformatted("top_left");
            ImGui::Button("Media0", ImVec2(48, 48));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("Media_drag_drop", &media0_item, sizeof(media0_item));
                ImGui::TextUnformatted("This is a drag and drop source");
                ImGui::EndDragDropSource();
            }
            ImGui::EndChild();
        }

        ImVec2 main_sub_pos(bank_width + 8, 0);
        ImVec2 main_sub_size(main_width - 8, main_window_size.y - 4);
        ImGui::SetNextWindowPos(main_sub_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Top_Right_Window", main_sub_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::TextUnformatted("top_right");
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    
    ImVec2 panel_pos(4, size_main_h * window_size.y + 12);
    ImVec2 panel_size(window_size.x, size_timeline_h * window_size.y - 12);
    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    bool _expanded = expanded;
    if (ImGui::BeginChild("##Sequencor", panel_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImSequencer::Sequencer(&sequence, &currentFrame, &_expanded, &selectedEntry, &firstFrame, &lastFrame, ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_CHANGE_FRAME | ImSequencer::SEQUENCER_DEL);
        if (selectedEntry != -1)
        {
            //const ImSequencer::MediaSequence::SequenceItem &item = sequence.m_Items[selectedEntry];
            //ImGui::Text("I am a %s, please edit me", item.mName.c_str());
        }
        ImGui::EndChild();
        if (expanded != _expanded)
        {
            if (!_expanded)
            {
                old_size_timeline_h = size_timeline_h;
                size_timeline_h = 40.0f / window_size.y;
                size_main_h = 1 - size_timeline_h;
            }
            else
            {
                size_timeline_h = old_size_timeline_h;
                size_main_h = 1.0f - size_timeline_h;
            }
            expanded = _expanded;
        }
    }
    ImGui::End();
    return false;
}