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
static float g_scale = 1.f;
static float g_scaleX = 1.f;
static float g_scaleY = 1.f;
static float g_spacing = 1.f;
static float g_angle = 0;
static float g_outlineWidth = 1;
static int g_alignment = 2;
static int g_marginV = 0;
static int g_marginH = 0;
static int g_marginR = 0;
static bool g_fontItalic = false;
static bool g_fontBold = false;
static bool g_fontUnderLine = false;
static bool g_fontStrikeOut = false;
static int g_clipTime[2];

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "SubtitleReaderTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 960;
    property.height = 720;
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

    g_clipTime[0] = 0;
    g_clipTime[1] = 0;

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
        ImGui::SameLine(0, 20);
        if (ImGui::Button("New empty track"))
        {
            g_subtrack = SubtitleTrack::NewEmptyTrack(0);
            g_subtrack->SetFrameSize(1920, 1080);
            g_subtrack->SetBackgroundColor({0.2, 0.2, 0.2, 1});
        }

        auto btnSize = ImGui::GetItemRectSize();
        auto wndSize = ImGui::GetWindowSize();
        auto& uiStyle = ImGui::GetStyle();

        static int s_selectedSubtitleIndex = -1;
        static char s_subtitleEdit[2048];
        static bool s_subtitleEditChanged = false;
        static ImVec4 s_primaryColor(1,1,1,1);
        static ImVec4 s_outlineColor(0,0,0,1);
        if (g_subtrack)
        {
            static int s_currTabIdx = 0;
            if (ImGui::BeginTabBar("SubtitleViewTabs", ImGuiTabBarFlags_None))
            {
                int bottomControlLines = 1;
                if (ImGui::BeginTabItem("List"))
                {
                    if (s_currTabIdx != 0)
                    {
                        s_currTabIdx = 0;
                    }
                    auto csPos = ImGui::GetCursorPos();
                    float editInputHeight = 50;
                    float tableHeight = wndSize.y-csPos.y-(btnSize.y+uiStyle.ItemSpacing.y)*bottomControlLines-editInputHeight-uiStyle.ItemSpacing.y-uiStyle.WindowPadding.y;
                    SubtitleClipHolder hSelectedClip;
                    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {10, 10});
                    ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    if (ImGui::BeginTable("#SubtitleList", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV, {0, tableHeight}))
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
                                bool isSelected = s_selectedSubtitleIndex == row;
                                if (isSelected) hSelectedClip = hSubClip;
                                if (ImGui::Selectable(timeColText, &isSelected, selectableFlags))
                                {
                                    s_selectedSubtitleIndex = row;
                                    hSelectedClip = hSubClip;
                                    int copySize = hSubClip->Text().size();
                                    if (copySize >= sizeof(s_subtitleEdit))
                                        copySize = sizeof(s_subtitleEdit)-1;
                                    memcpy(s_subtitleEdit, hSubClip->Text().c_str(), copySize);
                                    s_subtitleEdit[copySize] = 0;
                                    s_subtitleEditChanged = false;
                                    g_clipTime[0] = hSubClip->StartTime();
                                    g_clipTime[1] = hSubClip->Duration();
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

                    // Control line #1
                    if (ImGui::InputInt2("Subtitle Time", g_clipTime))
                    {

                    }
                    ImGui::SameLine(0, 20);

                    ImGui::BeginDisabled(!hSelectedClip || (hSelectedClip->StartTime() == g_clipTime[0] && hSelectedClip->Duration() == g_clipTime[1]));
                    if (ImGui::Button("Update time"))
                    {
                        g_subtrack->ChangeClipTime(hSelectedClip, g_clipTime[0], g_clipTime[1]);
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();

                    int64_t insertAt = g_clipTime[0]+g_clipTime[1];
                    if (ImGui::Button("Insert after"))
                    {
                        g_subtrack->NewClip(insertAt, 1000);
                    }

                    // Control line #SubEdit
                    if (ImGui::InputTextMultiline("##SubtitleEditInput", s_subtitleEdit, sizeof(s_subtitleEdit), {0, editInputHeight}, ImGuiInputTextFlags_AllowTabInput))
                    {
                        s_subtitleEditChanged = true;
                    }
                    ImGui::SameLine(0, 20);
                    ImGui::BeginDisabled(!s_subtitleEditChanged);
                    if (ImGui::Button("Update"))
                    {
                        g_subtrack->ChangeText(s_selectedSubtitleIndex, string(s_subtitleEdit));
                        s_subtitleEditChanged = false;
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Render"))
                {
                    static uint32_t s_showSubIdx = 0;
                    static float s_showSubPos = 0;
                    if (s_currTabIdx != 1)
                    {
                        s_currTabIdx = 1;
                        g_subtrack->SeekToIndex(s_showSubIdx);
                    }
                    int bottomControlLines = 6;

                    SubtitleClipHolder hSupClip = g_subtrack->GetCurrClip();
                    if (hSupClip)
                    {
                        SubtitleImage subImage = hSupClip->Image();
                        if (subImage.Valid())
                        {
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

                    // Control Line #1
                    ImGui::PushItemWidth(wndSize.x*0.25);
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
                    ImGui::SameLine(0, 30);
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
                    if (g_fontSelChanged)
                    {
                        auto& font = g_fontTable[g_fontFamilies[g_fontFamilySelIdx]][g_fontStyleSelIdx];
                        g_subtrack->SetFont(font->Family());
                        g_subtrack->SetBold((int)font->Weight());
                        g_subtrack->SetItalic(font->Italic() ? 1 : 0);
                        g_fontSelChanged = false;
                    }
                    ImGui::SameLine(0, 30);
                    if (ImGui::SliderFloat("Scale", &g_scale, 0.2, 3, "%.1f"))
                    {
                        g_subtrack->SetScale((double)g_scale);
                    }

                    // Control Line #2
                    if (ImGui::SliderFloat("Spacing", &g_spacing, 0.5, 5, "%.1f"))
                    {
                        g_subtrack->SetSpacing((double)g_spacing);
                    }
                    ImGui::SameLine(0, 30);
                    if (ImGui::SliderFloat("Angle", &g_angle, 0, 360, "%.1f"))
                    {
                        g_subtrack->SetAngle((double)g_angle);
                    }
                    ImGui::SameLine(0, 30);
                    if (ImGui::SliderFloat("Outline width", &g_outlineWidth, 0, 3, "%.1f"))
                    {
                        g_subtrack->SetOutlineWidth((double)g_outlineWidth);
                    }

                    // Control Line #3
                    if (ImGui::SliderInt("Alignment", &g_alignment, 1, 3, "%d"))
                    {
                        g_subtrack->SetAlignment(g_alignment);
                    }
                    ImGui::SameLine(0, 30);
                    if (ImGui::SliderFloat("ScaleX", &g_scaleX, 0.5, 3, "%.1f"))
                    {
                        g_subtrack->SetScaleX(g_scaleX);
                    }
                    ImGui::SameLine(0, 30);
                    if (ImGui::SliderFloat("ScaleY", &g_scaleY, 0.5, 3, "%.1f"))
                    {
                        g_subtrack->SetScaleY(g_scaleY);
                    }
                    // Control Line #4
                    if (ImGui::SliderInt("MarginV", &g_marginV, -300, 300, "%d"))
                    {
                        g_subtrack->SetMarginV(g_marginV);
                    }
                    ImGui::SameLine(0, 30);
                    if (ImGui::SliderInt("MarginH", &g_marginH, -300, 300, "%d"))
                    {
                        g_subtrack->SetMarginH(g_marginH);
                    }
                    ImGui::PopItemWidth();

                    // Control Line #5
                    if (ImGui::Checkbox("Italic", &g_fontItalic))
                    {
                        g_subtrack->SetItalic(g_fontItalic ? 1 : 0);
                    }
                    ImGui::SameLine(0, 20);
                    if (ImGui::Checkbox("UnderLine", &g_fontUnderLine))
                    {
                        g_subtrack->SetUnderLine(g_fontUnderLine);
                    }
                    ImGui::SameLine(0, 20);
                    if (ImGui::Checkbox("StrikeOut", &g_fontStrikeOut))
                    {
                        g_subtrack->SetStrikeOut(g_fontStrikeOut);
                    }
                    ImGui::SameLine(0, 20);
                    ImGui::TextUnformatted("Primary color:");
                    ImGui::SameLine(0);
                    if (ImGui::ColorEdit4("FontColor##Primary", (float*)&s_primaryColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
                    {
                        g_subtrack->SetPrimaryColor(SubtitleClip::Color(s_primaryColor.x, s_primaryColor.y, s_primaryColor.z, s_primaryColor.w));
                    }
                    ImGui::SameLine(0, 20);
                    ImGui::TextUnformatted("Outline color:");
                    ImGui::SameLine(0);
                    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&s_outlineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
                    {
                        g_subtrack->SetOutlineColor(SubtitleClip::Color(s_outlineColor.x, s_outlineColor.y, s_outlineColor.z, s_outlineColor.w));
                    }
                    
                    // Control Line #6
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
            if (g_subtrack)
            {
                g_subtrack->SetFrameSize(1920, 1080);
                g_subtrack->SetBackgroundColor({0.2, 0.2, 0.2, 1});
                // g_subtrack->EnableFullSizeOutput(false);
            }
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
