#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include "Logger.h"
#include "SubtitleTrack.h"
#include "FFUtils.h"

using namespace std;
using namespace Logger;
using namespace DataLayer;

const string c_imguiIniPath = "subrdrtest.ini";
const string c_bookmarkPath = "bookmark.ini";

static SubtitleTrackHolder g_subtrack;
static ImTextureID g_imageTid;

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "SubtitleReaderTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 960;
    property.height = 540;
}

void Application_SetupContext(ImGuiContext* ctx)
{
}

void Application_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);

    if (!InitializeSubtitleLibrary())
        Log(Error) << "FAILED to initialize the subtitle library!" << endl;

#ifdef USE_BOOKMARK
    // load bookmarks
    ifstream docFile(c_bookmarkPath, ios::in);
    if (docFile.is_open())
    {
        stringstream strStream;
        strStream << docFile.rdbuf(); //read the file
        ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
        docFile.close();
    }
#endif

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = c_imguiIniPath.c_str();
}

void Application_Finalize(void** handle)
{
    if (g_imageTid)
    {
        ImGui::ImDestroyTexture(g_imageTid);
        g_imageTid = nullptr;
    }

#ifdef USE_BOOKMARK
    // save bookmarks
    ofstream configFileWriter(c_bookmarkPath, ios::out);
    if (!configFileWriter.bad())
    {
        configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
        configFileWriter.close();
    }
#endif

    ReleaseSubtitleLibrary();
}

bool Application_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "字幕文件(*.srt *.txt *.ass){.srt,.txt,.ass,.SRT,.TXT,.ASS},.*";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开字幕文件", 
                                                    filters, 
                                                    "/workspace/MediaFiles/", 
                                                    1, 
                                                    nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                    ImGuiFileDialogFlags_Modal);
        }

        static int SelectedSubtitleIndex = -1;
        if (g_subtrack)
        {
            if (ImGui::BeginTabBar("SubtitleViewTabs", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("List"))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {10, 10});
                    ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    if (ImGui::BeginTable("#SubtitleList", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV))
                    {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed);
                        ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();

                        ImGuiListClipper clipper;
                        clipper.Begin(g_subtrack->ClipCount());
                        while (clipper.Step())
                        {
                            g_subtrack->SeekToIndex(clipper.DisplayStart);
                            SubtitleClipHolder hSubClip = g_subtrack->GetCurrClip();
                            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                char timeColText[128];
                                snprintf(timeColText, sizeof(timeColText), "%s (+%ld)", MillisecToString(hSubClip->StartTime()).c_str(), hSubClip->Duration());
                                bool isSelected = SelectedSubtitleIndex == row;
                                if (ImGui::Selectable(timeColText, &isSelected, selectableFlags))
                                {
                                    SelectedSubtitleIndex = row;
                                }
                                // ImGui::TextColored({0.6, 0.6, 0.6, 1.}, "%s (+%ld)", MillisecToString(hSubClip->StartTime()).c_str(), hSubClip->Duration());
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextUnformatted(hSubClip->Text().c_str());

                                hSubClip = g_subtrack->GetNextClip();
                            }
                        }
                        ImGui::EndTable();
                    }
                    ImGui::PopStyleVar();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Render"))
                {
                    g_subtrack->SeekToIndex(1);
                    SubtitleClipHolder hSupClip = g_subtrack->GetCurrClip();
                    if (hSupClip)
                    {
                        SubtitleImage subImage = hSupClip->Image();
                        if (subImage.Valid())
                        {
                            if (ImGui::BeginChild("#SubtitleImage", {0, 0}, false))
                            {
                                ImGui::ImMat vmat = subImage.Image();
                                ImGui::ImMatToTexture(vmat, g_imageTid);
                                auto dispalySize = ImGui::GetWindowSize();
                                if (dispalySize.x*vmat.h > dispalySize.y*vmat.w)
                                {
                                    dispalySize.x = dispalySize.y*vmat.w/vmat.h;
                                }
                                else
                                {
                                    dispalySize.y = dispalySize.x*vmat.h/vmat.w;
                                }
                                if (g_imageTid) ImGui::Image(g_imageTid, dispalySize);
                                ImGui::EndChild();
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        ImGui::End();
    }

    // open file dialog
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_subtrack = SubtitleTrack::BuildFromFile(0, filePathName);
            g_subtrack->SetFrameSize(1920, 1080);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        app_done = true;
    }
    if (app_will_quit)
    {
        app_done = true;
    }

    return app_done;
}
