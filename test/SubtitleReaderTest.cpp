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
using namespace MediaCore;
using namespace FM;

const string c_imguiIniPath = "subrdrtest.ini";
const string c_bookmarkPath = "bookmark.ini";

static SubtitleTrackHolder g_subtrack;
static ImTextureID g_imageTid;
static unordered_map<string, vector<FontDescriptorHolder>> g_fontTable;
static vector<string> g_fontFamilies;
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
    property.height = 840;
}

void Application_SetupContext(ImGuiContext* ctx)
{
}

void Application_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    GetSubtitleTrackLogger()
        ->SetShowLevels(VERBOSE);

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


static uint32_t s_showSubIdx = 0;
static float s_showSubPos = 0;

static void UIComponent_TrackStyle(SubtitleClipHolder hSupClip)
{
    auto wndSize = ImGui::GetWindowSize();
    auto& style = g_subtrack->DefaultStyle();
    // Control Line #1
    ImGui::PushItemWidth(wndSize.x*0.25);
    string fontName = style.Font();
    const char* previewValue = fontName.c_str();
    if (ImGui::BeginCombo("Font", previewValue))
    {
        for (int i = 0; i < g_fontFamilies.size(); i++)
        {
            bool isSelected = g_fontFamilies[i] == fontName;
            if (ImGui::Selectable(g_fontFamilies[i].c_str(), isSelected))
            {
                g_subtrack->SetFont(g_fontTable[g_fontFamilies[i]][0]->Family());
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 30);
    int bold = style.Bold();
    if (ImGui::SliderInt("Bold", &bold, 0, 2, "%d"))
    {
        g_subtrack->SetBold(bold);
    }
    ImGui::SameLine(0, 30);
    int italic = style.Italic();
    if (ImGui::SliderInt("Italic", &italic, 0, 2, "%d"))
    {
        g_subtrack->SetItalic(italic);
    }

    // Control Line #2
    float spacing = (float)style.Spacing();
    if (ImGui::SliderFloat("Spacing", &spacing, 0, 5, "%.1f"))
    {
        g_subtrack->SetSpacing((double)spacing);
    }
    ImGui::SameLine(0, 30);
    float angle = (float)style.Angle();
    if (ImGui::SliderFloat("Angle", &angle, 0, 360, "%.1f"))
    {
        g_subtrack->SetAngle((double)angle);
    }
    ImGui::SameLine(0, 30);
    float outlineWidth = (float)style.OutlineWidth();
    if (ImGui::SliderFloat("Outline width", &outlineWidth, 0, 10, "%.1f"))
    {
        g_subtrack->SetOutlineWidth((double)outlineWidth);
    }

    // Control Line #3
    float scaleX = (float)style.ScaleX();
    if (ImGui::SliderFloat("ScaleX", &scaleX, 0.5, 3, "%.1f"))
    {
        g_subtrack->SetScaleX(scaleX);
    }
    ImGui::SameLine(0, 30);
    float scaleY = (float)style.ScaleY();
    if (ImGui::SliderFloat("ScaleY", &scaleY, 0.5, 3, "%.1f"))
    {
        g_subtrack->SetScaleY(scaleY);
    }
    ImGui::SameLine(0, 30);
    float shadowDepth = (float)style.ShadowDepth();
    if (ImGui::SliderFloat("Shadow depth", &shadowDepth, -10, 10, "%.1f"))
    {
        g_subtrack->SetShadowDepth((double)shadowDepth);
    }

    // Control Line #4
    int offsetV = style.OffsetV();
    if (ImGui::SliderInt("Offset V", &offsetV, -300, 300, "%d"))
    {
        g_subtrack->SetOffsetV(offsetV);
    }
    ImGui::SameLine(0, 30);
    int offsetH = style.OffsetH();
    if (ImGui::SliderInt("Offset H", &offsetH, -300, 300, "%d"))
    {
        g_subtrack->SetOffsetH(offsetH);
    }
    ImGui::SameLine(0, 30);
    int borderStyle = style.BorderStyle();
    if (ImGui::SliderInt("Border style", &borderStyle, -1, 5, "%d"))
    {
        g_subtrack->SetBorderStyle(borderStyle);
    }

    // Control Line #5
    int alignment = style.Alignment();
    if (ImGui::SliderInt("Alignment", &alignment, 1, 9, "%d"))
    {
        g_subtrack->SetAlignment(alignment);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 20);
    bool underline = style.UnderLine();
    if (ImGui::Checkbox("UnderLine", &underline))
    {
        g_subtrack->SetUnderLine(underline);
    }
    ImGui::SameLine(0, 20);
    bool strikeout = style.StrikeOut();
    if (ImGui::Checkbox("StrikeOut", &strikeout))
    {
        g_subtrack->SetStrikeOut(strikeout);
    }

    // Control Line #6
    ImGui::TextUnformatted("Primary color:");
    ImGui::SameLine(0);
    ImVec4 primaryColor = style.PrimaryColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Primary", (float*)&primaryColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        g_subtrack->SetPrimaryColor(primaryColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Secondary color:");
    ImGui::SameLine(0);
    ImVec4 secondaryColor = style.SecondaryColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Secondary", (float*)&secondaryColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        g_subtrack->SetSecondaryColor(secondaryColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Outline color:");
    ImGui::SameLine(0);
    ImVec4 outlineColor = style.OutlineColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&outlineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        g_subtrack->SetOutlineColor(outlineColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Shadow color:");
    ImGui::SameLine(0);
    ImVec4 backColor = style.BackColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Back", (float*)&backColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        g_subtrack->SetBackColor(backColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Background color:");
    ImGui::SameLine(0);
    ImVec4 bgColor = style.BackgroundColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Background", (float*)&bgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        g_subtrack->SetBackgroundColor(bgColor);
}

static void UIComponent_ClipStyle(SubtitleClipHolder hSubClip)
{
    auto wndSize = ImGui::GetWindowSize();
    // Control Line #1
    ImGui::PushItemWidth(wndSize.x*0.25);
    string fontName = hSubClip->Font();
    const char* previewValue = fontName.c_str();
    if (ImGui::BeginCombo("Font", previewValue))
    {
        for (int i = 0; i < g_fontFamilies.size(); i++)
        {
            bool isSelected = g_fontFamilies[i] == fontName;
            if (ImGui::Selectable(g_fontFamilies[i].c_str(), isSelected))
            {
                hSubClip->SetFont(g_fontTable[g_fontFamilies[i]][0]->Family());
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 30);
    int bold = hSubClip->Bold() ? 1 : 0;
    if (ImGui::SliderInt("Bold", &bold, 0, 1, "%d"))
    {
        hSubClip->SetBold(bold > 0);
    }
    ImGui::SameLine(0, 30);
    int italic = hSubClip->Italic() ? 1 : 0;
    if (ImGui::SliderInt("Italic", &italic, 0, 1, "%d"))
    {
        hSubClip->SetItalic(italic > 0);
    }

    // Control Line #2
    float spacing = (float)hSubClip->Spacing();
    if (ImGui::SliderFloat("Spacing", &spacing, 0, 5, "%.1f"))
    {
        hSubClip->SetSpacing((double)spacing);
    }
    ImGui::SameLine(0, 30);
    float angle = (float)hSubClip->RotationZ();
    if (ImGui::SliderFloat("Angle", &angle, 0, 360, "%.1f"))
    {
        hSubClip->SetRotationZ((double)angle);
    }
    ImGui::SameLine(0, 30);
    float borderWidth = hSubClip->BorderWidth();
    if (ImGui::SliderFloat("Border width", &borderWidth, 0, 10, "%.1f"))
    {
        hSubClip->SetBorderWidth((double)borderWidth);
    }

    // Control Line #3
    float scaleX = (float)hSubClip->ScaleX();
    if (ImGui::SliderFloat("ScaleX", &scaleX, 0.5, 3, "%.1f"))
    {
        hSubClip->SetScaleX(scaleX);
    }
    ImGui::SameLine(0, 30);
    float scaleY = (float)hSubClip->ScaleY();
    if (ImGui::SliderFloat("ScaleY", &scaleY, 0.5, 3, "%.1f"))
    {
        hSubClip->SetScaleY(scaleY);
    }
    ImGui::SameLine(0, 30);
    float shadowDepth = (float)hSubClip->ShadowDepth();
    if (ImGui::SliderFloat("Shadow depth", &shadowDepth, -10, 10, "%.1f"))
    {
        hSubClip->SetShadowDepth((double)shadowDepth);
    }

    // Control Line #4
    int offsetV = hSubClip->OffsetV();
    if (ImGui::SliderInt("Offset V", &offsetV, -300, 300, "%d"))
    {
        hSubClip->SetOffsetV(offsetV);
    }
    ImGui::SameLine(0, 30);
    int offsetH = hSubClip->OffsetH();
    if (ImGui::SliderInt("Offset H", &offsetH, -300, 300, "%d"))
    {
        hSubClip->SetOffsetH(offsetH);
    }

    // Control Line #5
    int alignment = hSubClip->Alignment();
    if (ImGui::SliderInt("Alignment", &alignment, 1, 9, "%d"))
    {
        hSubClip->SetAlignment(alignment);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 20);
    bool underline = hSubClip->UnderLine();
    if (ImGui::Checkbox("UnderLine", &underline))
    {
        hSubClip->SetUnderLine(underline);
    }
    ImGui::SameLine(0, 20);
    bool strikeout = hSubClip->StrikeOut();
    if (ImGui::Checkbox("StrikeOut", &strikeout))
    {
        hSubClip->SetStrikeOut(strikeout);
    }

    // Control Line #6
    ImGui::TextUnformatted("Primary color:");
    ImGui::SameLine(0);
    ImVec4 primaryColor = hSubClip->PrimaryColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Primary", (float*)&primaryColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        hSubClip->SetPrimaryColor(primaryColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Secondary color:");
    ImGui::SameLine(0);
    ImVec4 secondaryColor = hSubClip->SecondaryColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Secondary", (float*)&secondaryColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        hSubClip->SetSecondaryColor(secondaryColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Outline color:");
    ImGui::SameLine(0);
    ImVec4 outlineColor = hSubClip->OutlineColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&outlineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        hSubClip->SetOutlineColor(outlineColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Shadow color:");
    ImGui::SameLine(0);
    ImVec4 backColor = hSubClip->BackColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Shadow", (float*)&backColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        hSubClip->SetBackColor(backColor);
    ImGui::SameLine(0, 20);
    ImGui::TextUnformatted("Background color:");
    ImGui::SameLine(0);
    ImVec4 bgColor = hSubClip->BackgroundColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Background", (float*)&bgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar))
        hSubClip->SetBackgroundColor(bgColor);
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
            g_subtrack->SetBackgroundColor(SubtitleColor{0.2, 0.2, 0.2, 1});
        }

        auto btnSize = ImGui::GetItemRectSize();
        auto wndSize = ImGui::GetWindowSize();
        auto& uiStyle = ImGui::GetStyle();

        static int s_selectedSubtitleIndex = -1;
        static char s_subtitleEdit[2048];
        static bool s_subtitleEditChanged = false;
        if (g_subtrack)
        {
            static int s_currTabIdx = 0;
            if (ImGui::BeginTabBar("SubtitleViewTabs", ImGuiTabBarFlags_None))
            {
                int bottomControlLines = 1;
                // TAB 'List'
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
                        hSelectedClip->SetText(s_subtitleEdit);
                        s_subtitleEditChanged = false;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine(0, 20);
                    ImGui::BeginDisabled(!hSelectedClip);
                    if (ImGui::Button("Delete clip"))
                    {
                        g_subtrack->DeleteClip(hSelectedClip);
                        s_showSubIdx = g_subtrack->GetCurrIndex();
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }
                // TAB 'Render'
                if (ImGui::BeginTabItem("Render"))
                {
                    if (s_currTabIdx != 1)
                    {
                        s_currTabIdx = 1;
                        g_subtrack->SeekToIndex(s_showSubIdx);
                    }
                    bottomControlLines = 9;

                    SubtitleClipHolder hSubClip = g_subtrack->GetCurrClip();
                    if (hSubClip)
                    {
                        SubtitleImage subImage = hSubClip->Image();
                        SubtitleImage::Rect dispRect;
                        if (subImage.Valid())
                        {
                            auto csPos = ImGui::GetCursorPos();
                            if (ImGui::BeginChild("#SubtitleImage",
                                {0, wndSize.y-csPos.y-(btnSize.y+uiStyle.ItemSpacing.y)*bottomControlLines-uiStyle.WindowPadding.y}))
                            {
                                ImGui::ImMat vmat = subImage.Vmat();
                                ImGui::ImMatToTexture(vmat, g_imageTid);
                                auto dispalySize = ImGui::GetWindowSize();
                                if (dispalySize.x*vmat.h > dispalySize.y*vmat.w)
                                    dispalySize.x = dispalySize.y*vmat.w/vmat.h;
                                else
                                    dispalySize.y = dispalySize.x*vmat.h/vmat.w;
                                if (g_imageTid) ImGui::Image(g_imageTid, dispalySize);
                                ImGui::EndChild();
                            }
                            dispRect = subImage.Area();
                        }

                        bool useTrackStyle = hSubClip->IsUsingTrackStyle();
                        if (ImGui::Checkbox("Use track style", &useTrackStyle))
                        {
                            hSubClip->EnableUsingTrackStyle(useTrackStyle);
                        }
                        ImGui::SameLine(0, 20);
                        ImGui::TextColored({0.7,0.9,0.7,1}, "{%4d,%4d,%4d,%4d}", dispRect.x, dispRect.y, dispRect.w, dispRect.h);
                        ImGui::SameLine(0, 20);
                        bool isFullSizeOutput = g_subtrack->IsFullSizeOutput();
                        if (ImGui::Checkbox("Full size output", &isFullSizeOutput))
                        {
                            g_subtrack->EnableFullSizeOutput(isFullSizeOutput);
                        }

                        if (ImGui::BeginTabBar("SubtitleStyleTabs", ImGuiTabBarFlags_None))
                        {
                            if (ImGui::BeginTabItem("Track style"))
                            {
                                UIComponent_TrackStyle(hSubClip);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Clip style"))
                            {
                                UIComponent_ClipStyle(hSubClip);
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }

                        // Control Line #6
                        ImGui::BeginGroup();
                        ImGui::BeginDisabled(s_showSubIdx == 0);
                        if (ImGui::Button("Prev"))
                        {
                            s_showSubIdx--;
                            hSubClip = g_subtrack->GetPrevClip();
                            s_showSubPos = (float)hSubClip->StartTime()/1000;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        ImGui::BeginDisabled(s_showSubIdx >= g_subtrack->ClipCount()-1);
                        if (ImGui::Button("Next"))
                        {
                            s_showSubIdx++;
                            hSubClip = g_subtrack->GetNextClip();
                            s_showSubPos = (float)hSubClip->StartTime()/1000;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine(0, 10);
                        if (ImGui::SliderFloat("##PosSlider", &s_showSubPos, 0, (float)g_subtrack->Duration()/1000, "%.3f", 0))
                        {
                            g_subtrack->SeekToTime((int64_t)(s_showSubPos*1000));
                            hSubClip = g_subtrack->GetCurrClip();
                            s_showSubIdx = g_subtrack->GetCurrIndex();
                        }

                        string timestr = hSubClip ? MillisecToString(hSubClip->StartTime()) : "-:--:--.---";
                        string durstr = MillisecToString(g_subtrack->Duration());
                        string posstr = timestr+"/"+durstr;
                        ImGui::SameLine(0, 10);
                        ImGui::TextUnformatted(posstr.c_str());
                        ImGui::EndGroup();
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
            if (g_subtrack)
            {
                g_subtrack->SetFrameSize(1920, 1080);
                g_subtrack->SetBackgroundColor(SubtitleColor{0.2, 0.2, 0.2, 1});
                g_subtrack->SetAlignment(5);
                // g_subtrack->SaveAs("~/test_encsub.sRt");
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
