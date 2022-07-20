#include <sstream>
#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include "Logger.h"
#include "SubtitleTrack.h"
#include "FFUtils.h"
#include "FontManager.h"

using namespace std;
using namespace Logger;
using namespace DataLayer;
using namespace FM;

const string c_imguiIniPath = "subrdrtest.ini";
const string c_bookmarkPath = "bookmark.ini";

static SubtitleTrackHolder g_subtrack;
static ImTextureID g_imageTid;
static unordered_map<string, vector<FontDescriptorHolder>> g_fontTable;
static vector<string> g_fontFamilies;
static int g_fontFamilySelIdx = 0;
static int g_fontStyleSelIdx = 0;
static int g_fontSelChanged = false;

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
    GetSubtitleTrackLogger()
        ->SetShowLevels(DEBUG);

    if (!InitializeSubtitleLibrary())
        Log(Error) << "FAILED to initialize the subtitle library!" << endl;

#ifdef USE_BOOKMARK
    // load bookmarks
    ifstream docFile(c_bookmarkPath, ios::in);
    if (docFile.is_open())
    {
        std::stringstream strStream;
        strStream << docFile.rdbuf(); //read the file
        ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
        docFile.close();
    }
#endif

    g_fontTable = FM::GroupFontsByFamily(FM::GetAvailableFonts());
    list<string> fontFamilies;
    for (auto& item : g_fontTable)
        fontFamilies.push_back(item.first);
    fontFamilies.sort();
    for (auto& item : fontFamilies)
        g_fontFamilies.push_back(item);

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
        auto btnSize = ImGui::GetItemRectSize();

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
                                snprintf(timeColText, sizeof(timeColText), "%s (+%lld)", MillisecToString(hSubClip->StartTime()).c_str(), hSubClip->Duration());
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
                    auto wndSize = ImGui::GetWindowSize();
                    int bottomControlLines = 2;

                    static uint32_t s_showSubIdx = 0;
                    static float s_showSubPos = 0;
                    SubtitleClipHolder hSupClip = g_subtrack->GetCurrClip();
                    if (hSupClip)
                    {
                        SubtitleImage subImage = hSupClip->Image();
                        if (subImage.Valid())
                        {
                            auto uiStyle = ImGui::GetStyle();
                            auto csPos = ImGui::GetCursorPos();
                            if (ImGui::BeginChild("#SubtitleImage",
                                {0, wndSize.y-csPos.y-(btnSize.y+uiStyle.ItemSpacing.y)*bottomControlLines-uiStyle.WindowPadding.y}))
                            {
                                ImGui::ImMat vmat = subImage.Image();
                                ImGui::ImMatToTexture(vmat, g_imageTid);
                                auto dispalySize = ImGui::GetWindowSize();
                                if (dispalySize.x*vmat.h > dispalySize.y*vmat.w)
                                    dispalySize.x = dispalySize.y*vmat.w/vmat.h;
                                else
                                    dispalySize.y = dispalySize.x*vmat.h/vmat.w;
                                if (g_imageTid) ImGui::Image(g_imageTid, dispalySize);
                                ImGui::EndChild();
                            }
                        }
                    }
                    ImGui::EndTabItem();

                    ImGui::PushItemWidth(wndSize.x*0.35);
                    const char* previewValue = g_fontFamilySelIdx >= g_fontFamilies.size() ? nullptr : g_fontFamilies[g_fontFamilySelIdx].c_str();
                    if (ImGui::BeginCombo("Font family", previewValue))
                    {
                        for (int i = 0; i < g_fontFamilies.size(); i++)
                            if (ImGui::Selectable(g_fontFamilies[i].c_str(), i == g_fontFamilySelIdx))
                            {
                                g_fontFamilySelIdx = i;
                                g_fontStyleSelIdx = 0;
                                g_fontSelChanged = true;
                            }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    vector<FontDescriptorHolder> styles;
                    if (g_fontFamilySelIdx < g_fontFamilies.size())
                        styles = g_fontTable[g_fontFamilies[g_fontFamilySelIdx]];
                    previewValue = g_fontStyleSelIdx >= styles.size() ? nullptr : styles[g_fontStyleSelIdx]->Style().c_str();
                    if (ImGui::BeginCombo("Style", previewValue))
                    {
                        for (int i = 0; i < styles.size(); i++)
                            if (ImGui::Selectable(styles[i]->Style().c_str(), i == g_fontStyleSelIdx))
                            {
                                g_fontStyleSelIdx = i;
                                g_fontSelChanged = true;
                            }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ImGui::SameLine();
                    ImGui::BeginDisabled(!g_fontSelChanged);
                    if (ImGui::Button("Update font"))
                    {
                        g_fontSelChanged = false;
                        g_subtrack->SetFont(g_fontTable[g_fontFamilies[g_fontFamilySelIdx]][g_fontStyleSelIdx]->PostscriptName());
                    }
                    ImGui::EndDisabled();

                    ImGui::BeginGroup();
                    ImGui::BeginDisabled(s_showSubIdx == 0);
                    if (ImGui::Button("Prev"))
                    {
                        s_showSubIdx--;
                        hSupClip = g_subtrack->GetPrevClip();
                        s_showSubPos = (float)hSupClip->StartTime()/1000;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(s_showSubIdx >= g_subtrack->ClipCount()-1);
                    if (ImGui::Button("Next"))
                    {
                        s_showSubIdx++;
                        hSupClip = g_subtrack->GetNextClip();
                        s_showSubPos = (float)hSupClip->StartTime()/1000;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine(0, 10);
                    std::string durstr = MillisecToString(g_subtrack->Duration());
                    if (ImGui::SliderFloat(durstr.c_str(), &s_showSubPos, 0, (float)g_subtrack->Duration()/1000, "%.3f", 0))
                    {
                        g_subtrack->SeekToTime((int64_t)(s_showSubPos*1000));
                        hSupClip = g_subtrack->GetCurrClip();
                        int32_t idx = g_subtrack->GetClipIndex(hSupClip);
                        if (idx >= 0)
                            s_showSubIdx = (uint32_t)idx;
                    }
                    ImGui::EndGroup();
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
            g_subtrack->SetBackgroundColor({0.1, 0.1, 0.1, 1});
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
