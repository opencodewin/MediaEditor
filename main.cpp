#include <application.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include "ImSequencer.h"
#include <sstream>

using namespace ImSequencer;

static std::string bookmark_path = "bookmark.ini";

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 1.0);
}

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Editor";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.width = 1680;
    property.height = 1024;
}

void Application_Initialize(void** handle)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
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
}

void Application_Finalize(void** handle)
{
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
    const float media_icon_size = 128; 
    const float tool_icon_size = 32;
    static bool show_about = false;
    static int selectedEntry = -1;
    static int firstFrame = 0;
    static int lastFrame = 0;
    static bool expanded = true;
    static int currentFrame = 0;
    static MediaSequence sequence;
    static bool play = false;
    static std::vector<SequenceItem *> media_items;
    sequence.mFrameMin = 0;
    sequence.mFrameMax = 2000;
    ImGuiFileDialogFlags fflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_DisableCreateDirectoryButton;
    const std::string ffilters = "Video files (*.mp4 *.mov *.mkv *.avi *.ts){.mp4,.mov,.mkv,.avi,.ts},Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},All File(*.*){.*}";
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui::Begin("Content", nullptr, flags);

    if (show_about)
    {
        ImGui::OpenPopup("##about", ImGuiPopupFlags_AnyPopup);
    }
    if (ImGui::BeginPopupModal("##about", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Media Editor Demo(ImGui)");
        ImGui::Separator();
        ImGui::Text("  Dicky 2021");
        ImGui::Separator();
        int i = ImGui::GetCurrentWindow()->ContentSize.x;
        ImGui::Indent((i - 40.0f) * 0.5f);
        if (ImGui::Button("OK", ImVec2(40, 0))) { show_about = false; ImGui::CloseCurrentPopup(); }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.f, 0.f, 0.f, 0.f));
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
        static float size_media_bank_w = 0.2;
        static float size_main_w = 0.8;
        ImGui::PushID("##Bank_Main");
        float bank_width = size_media_bank_w * main_window_size.x;
        float main_width = size_main_w * main_window_size.x;
        Splitter(true, 4.0f, &bank_width, &main_width, media_icon_size + tool_icon_size, 96);
        size_media_bank_w = bank_width / main_window_size.x;
        size_main_w = main_width / main_window_size.x;
        ImGui::PopID();
        
        static bool bank_expanded = true;
        ImVec2 bank_pos(4, 0);
        ImVec2 bank_size(bank_width - 4, main_window_size.y - 4);
        ImGui::SetNextWindowPos(bank_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Bank_Window", bank_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            // full background
            ImVec2 bank_window_size = ImGui::GetWindowSize();
            // make media bank area
            ImVec2 area_pos = ImVec2(tool_icon_size + 4, 0);
            ImGui::SetNextWindowPos(area_pos, ImGuiCond_Always);
            if (ImGui::BeginChild("##Bank_content", bank_window_size - ImVec2(tool_icon_size + 4, 0), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                auto wmin = area_pos;
                auto wmax = wmin + ImGui::GetContentRegionAvail();
                draw_list->AddRectFilled(wmin, wmax, 0xFF121212, 16.0, ImDrawFlags_RoundCornersAll);
                // Show Media Icons
                
                float x_offset = (ImGui::GetContentRegionAvail().x - media_icon_size) / 2;
                for (auto item : media_items)
                {
                    ImGui::Dummy(ImVec2(0, 24));
                    if (x_offset > 0)
                    {
                        ImGui::Dummy(ImVec2(x_offset, 0)); ImGui::SameLine();
                    }
                    ImGui::Button(item->mName.c_str(), ImVec2(media_icon_size, media_icon_size));
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        ImGui::SetDragDropPayload("Media_drag_drop", item, sizeof(SequenceItem));
                        ImGui::TextUnformatted(item->mName.c_str());
                        ImGui::EndDragDropSource();
                    }
                    ImGui::ShowTooltipOnHover("%s", item->mPath.c_str());
                }
                ImGui::EndChild();
            }
            // add tool bar
            ImGui::SetCursorPos(ImVec2(0,0));
            if (ImGui::Button(ICON_IGFD_ADD "##AddMedia", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Open Media Source
                ImGuiFileDialog::Instance()->OpenModal("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Media File", 
                                                        ffilters.c_str(),
                                                        ".",
                                                        1, 
                                                        IGFDUserDatas("Media Source"), 
                                                        fflags);
            }
            if (ImGui::Button(ICON_FA_WHMCS "##Configure", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Show Setting
            }
            if (ImGui::Button(ICON_FA5_INFO_CIRCLE "##ABout", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Show About
                show_about = true;
            }

            // Demo end
            ImGui::EndChild();
        }

        ImVec2 main_sub_pos(bank_width + 8, 0);
        ImVec2 main_sub_size(main_width - 8, main_window_size.y - 4);
        ImGui::SetNextWindowPos(main_sub_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Top_Right_Window", main_sub_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            // full background
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            auto wmin = main_sub_pos;
            auto wmax = wmin + ImGui::GetContentRegionAvail();
            draw_list->AddRectFilled(wmin, wmax, 0xFF000000, 16.0, ImDrawFlags_RoundCornersAll);

            ImGui::TextUnformatted("top_right");
            // TODO:: Add video proview

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
    ImGui::PopStyleColor();
    ImGui::End();

    // File Dialog
    ImVec2 minSize = ImVec2(0, 300);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (ImGuiFileDialog::Instance()->Display("##MediaEditFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
        if (userDatas.compare("Media Source") == 0)
        {
            auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
            SequenceItem * item = new SequenceItem(file_name, file_path, 0, 100, true);
            media_items.push_back(item);
        }
        ImGuiFileDialog::Instance()->Close();
    }
    return false;
}