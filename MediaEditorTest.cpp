#include <application.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <ImGuiTabWindow.h>
#include "ImSequencer.h"
#include "FFUtils.h"
#include "Logger.h"
#include <sstream>

using namespace ImSequencer;

static std::string bookmark_path = "bookmark.ini";

static const char* ControlPanelTabNames[] = {
    ICON_MEDIA_BANK,
    ICON_MEDIA_TRANSITIONS,
    ICON_MEDIA_FILTERS,
    ICON_MEDIA_OUTPUT
};

static const char* ControlPanelTabTooltips[] = 
{
    "Meida Bank",
    "Transition",
    "Filters",
    "Output"
};

static MediaSequencer * sequencer = nullptr;
static std::vector<SequencerItem *> media_items;

static void ShowMediaBankWindow(ImDrawList *draw_list, float media_icon_size)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);

    // Show Media Icons
    float x_offset = (ImGui::GetContentRegionAvail().x - media_icon_size - 12) / 2;
    for (auto item : media_items)
    {
        ImGui::Dummy(ImVec2(0, 24));
        if (x_offset > 0)
        {
            ImGui::Dummy(ImVec2(x_offset, 0)); ImGui::SameLine();
        }

        auto icon_pos = ImGui::GetCursorScreenPos();
        ImVec2 icon_size = ImVec2(media_icon_size, media_icon_size);
        // Draw Shadow for Icon
        draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(32, 32, 32, 255));
        draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(48, 48, 72, 255));
        draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255));
        
        if (item->mMediaThumbnail)
        {
            auto tex_w = ImGui::ImGetTextureWidth(item->mMediaThumbnail);
            auto tex_h = ImGui::ImGetTextureHeight(item->mMediaThumbnail);
            float aspectRatio = (float)tex_w / (float)tex_h;
            bool bViewisLandscape = icon_size.x >= icon_size.y ? true : false;
            bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
            bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
            float adj_w = bNeedChangeScreenInfo ? icon_size.y : icon_size.x;
            float adj_h = bNeedChangeScreenInfo ? icon_size.x : icon_size.y;
            float adj_x = adj_h * aspectRatio;
            float adj_y = adj_h;
            if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
            float offset_x = (icon_size.x - adj_x) / 2.0;
            float offset_y = (icon_size.y - adj_y) / 2.0;
            ImGui::PushID((void*)(intptr_t)item->mMediaThumbnail);
            const ImGuiID id = ImGui::GetCurrentWindow()->GetID("#image");
            ImGui::PopID();
            ImGui::ImageButtonEx(id, item->mMediaThumbnail, ImVec2(adj_w - offset_x * 2, adj_h - offset_y * 2), 
                                ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec2(offset_x, offset_y),
                                ImVec4(0.0f, 0.0f, 0.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            item->SequencerItemUpdateThumbnail();
            ImGui::Button(item->mName.c_str(), ImVec2(media_icon_size, media_icon_size));
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("Media_drag_drop", item, sizeof(SequencerItem));
            ImGui::TextUnformatted(item->mName.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::ShowTooltipOnHover("%s", item->mPath.c_str());
        if (item->mMedia && item->mMedia->IsOpened())
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            auto has_video = item->mMedia->HasVideo();
            auto has_audio = item->mMedia->HasAudio();
            auto video_width = item->mMedia->GetVideoWidth();
            auto video_height = item->mMedia->GetVideoHeight();
            auto media_length = item->mMedia->GetVidoeDuration() / 1000.f;
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(4, 4));
            std::string type_string = "? ";
            switch (item->mMediaType)
            {
                case SEQUENCER_ITEM_UNKNOWN: break;
                case SEQUENCER_ITEM_VIDEO: type_string = std::string(ICON_FA5_FILE_VIDEO) + " "; break;
                case SEQUENCER_ITEM_AUDIO: type_string = std::string(ICON_FA5_FILE_AUDIO) + " "; break;
                case SEQUENCER_ITEM_PICTURE: type_string = std::string(ICON_FA5_FILE_IMAGE) + " "; break;
                case SEQUENCER_ITEM_TEXT: type_string = std::string(ICON_FA5_FILE_CODE) + " "; break;
                default: break;
            }
            type_string += TimestampToString(media_length);
            ImGui::TextUnformatted(type_string.c_str());
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(media_icon_size - 24, 0));
            if (ImGui::Button( (std::string(ICON_TRASH "##delete_media") + item->mPath).c_str(), ImVec2(24, 24)))
            {
                // TODO::Dicky delete media from bank, also need delete it from sequencer item list
            }
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, media_icon_size - 24));
            if (has_video) { ImGui::Button( (std::string(ICON_MEDIA_VIDEO "##video") + item->mPath).c_str(), ImVec2(24, 24)); ImGui::SameLine(); }
            if (has_audio) { ImGui::Button( (std::string(ICON_MEDIA_AUDIO "##audio") + item->mPath).c_str(), ImVec2(24, 24)); ImGui::SameLine(); }
            if (has_video) { ImGui::Text("%dx%d", video_width, video_height); }
            ImGui::PopStyleColor();
        }
    }
}

static void ShowMediaTransitionWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida Transition");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowMediaFiltersWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida Filters");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static void ShowMediaOutputWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Meida Output");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 1.0, 0.01);
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
    Logger::SetDefaultLoggerLevels(Logger::DEBUG);
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
    sequencer = new MediaSequencer();
}

void Application_Finalize(void** handle)
{
    for (auto item : media_items) delete item;
    if (sequencer) delete sequencer;
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
    const float media_icon_size = 144; 
    const float tool_icon_size = 32;
    static bool show_about = false;
    static int selectedEntry = -1;
    static bool expanded = true;
    static int64_t currentTime = 0;
    static int64_t firstTime = 0;
    static int64_t lastTime = 0;
    static bool play = false;
    ImGuiFileDialogFlags fflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_DisableCreateDirectoryButton;
    const std::string ffilters = "Video files (*.mp4 *.mov *.mkv *.avi *.webm *.ts){.mp4,.mov,.mkv,.avi,.webm,.ts},Audio files (*.wav *.mp3 *.aac *.ogg *.ac3 *.dts){.wav,.mp3,.aac,.ogg,.ac3,.dts},Image files (*.png *.gif *.jpg *.jpeg *.tiff *.webp){.png,.gif,.jpg,.jpeg,.tiff,.webp},All File(*.*){.*}";
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static const int numControlPanelTabs = sizeof(ControlPanelTabNames)/sizeof(ControlPanelTabNames[0]);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
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
        static int ControlPanelIndex = 0;
        ImVec2 main_window_size = ImGui::GetWindowSize();
        static float size_media_bank_w = 0.2;
        static float size_main_w = 0.8;
        ImGui::PushID("##Control_Panel_Main");
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
        if (ImGui::BeginChild("##Control_Panel_Window", bank_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImVec2 bank_window_size = ImGui::GetWindowSize();
            ImGui::TabLabels(numControlPanelTabs, ControlPanelTabNames, ControlPanelIndex, ControlPanelTabTooltips , false, nullptr, nullptr, false, false, nullptr, nullptr);
            // make control panel area
            ImVec2 area_pos = ImVec2(tool_icon_size + 4, 32);
            ImGui::SetNextWindowPos(area_pos, ImGuiCond_Always);
            if (ImGui::BeginChild("##Control_Panel_content", bank_window_size - ImVec2(tool_icon_size + 4, 32), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                auto wmin = area_pos;
                auto wmax = wmin + ImGui::GetContentRegionAvail();
                draw_list->AddRectFilled(wmin, wmax, IM_COL32_BLACK, 8.0, ImDrawFlags_RoundCornersAll);
                switch (ControlPanelIndex)
                {
                    case 0: ShowMediaBankWindow(draw_list, media_icon_size); break;
                    case 1: ShowMediaTransitionWindow(draw_list); break;
                    case 2: ShowMediaFiltersWindow(draw_list); break;
                    case 3: ShowMediaOutputWindow(draw_list); break;
                    default: break;
                }
                ImGui::EndChild();
            }
            // add tool bar
            ImGui::SetCursorPos(ImVec2(0,32));
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
            ImGui::ShowTooltipOnHover("Add new media into bank");
            if (ImGui::Button(ICON_FA_WHMCS "##Configure", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Show Setting
            }
            ImGui::ShowTooltipOnHover("Configure");
            if (ImGui::Button(ICON_FA5_INFO_CIRCLE "##About", ImVec2(tool_icon_size, tool_icon_size)))
            {
                // Show About
                show_about = true;
            }
            ImGui::ShowTooltipOnHover("About Media Editor");

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
            draw_list->AddRectFilled(wmin, wmax, IM_COL32_BLACK, 8.0, ImDrawFlags_RoundCornersAll);

            ImGui::TextUnformatted("top_right");
            // TODO:: Add video proview

            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    
    ImVec2 panel_pos(4, size_main_h * window_size.y + 12);
    ImVec2 panel_size(window_size.x - 4, size_timeline_h * window_size.y - 12);
    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    bool _expanded = expanded;
    if (ImGui::BeginChild("##Sequencor", panel_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImSequencer::Sequencer(sequencer, &currentTime, &_expanded, &selectedEntry, &firstTime, &lastTime, 
                                ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_CHANGE_TIME | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_ADD |
                                ImSequencer::SEQUENCER_LOCK | ImSequencer::SEQUENCER_VIEW | ImSequencer::SEQUENCER_MUTE);
        if (selectedEntry != -1)
        {
            //const ImSequencer::MediaSequencer::SequencerItem &item = sequencer.m_Items[selectedEntry];
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
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("Media Source") == 0)
            {
                auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                auto file_surfix = ImGuiFileDialog::Instance()->GetCurrentFileSurfix();
                int type = SEQUENCER_ITEM_UNKNOWN;
                if (!file_surfix.empty())
                {
                    if ((file_surfix.compare(".mp4") == 0) ||
                        (file_surfix.compare(".mov") == 0) ||
                        (file_surfix.compare(".mkv") == 0) ||
                        (file_surfix.compare(".avi") == 0) ||
                        (file_surfix.compare(".webm") == 0) ||
                        (file_surfix.compare(".ts") == 0))
                        type = SEQUENCER_ITEM_VIDEO;
                    else 
                        if ((file_surfix.compare(".wav") == 0) ||
                            (file_surfix.compare(".mp3") == 0) ||
                            (file_surfix.compare(".aac") == 0) ||
                            (file_surfix.compare(".ac3") == 0) ||
                            (file_surfix.compare(".dts") == 0) ||
                            (file_surfix.compare(".ogg") == 0))
                        type = SEQUENCER_ITEM_AUDIO;
                    else 
                        if ((file_surfix.compare(".jpg") == 0) ||
                            (file_surfix.compare(".jpeg") == 0) ||
                            (file_surfix.compare(".png") == 0) ||
                            (file_surfix.compare(".gif") == 0) ||
                            (file_surfix.compare(".tiff") == 0) ||
                            (file_surfix.compare(".webp") == 0))
                        type = SEQUENCER_ITEM_PICTURE;
                }
                SequencerItem * item = new SequencerItem(file_name, file_path, 0, 100, true, type);
                media_items.push_back(item);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
    return false;
}