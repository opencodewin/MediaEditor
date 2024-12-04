#include <iostream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <application.h>
#include <imgui_extra_widget.h>
#include <ImGuiFileDialog.h>
#include <imgui_helper.h>
#include "ImMaskCreator.h"
#include "MatMath.h"
#include "MatFilter.h"
#include "MatIo.h"

using namespace std;
using namespace ImGui;

static MaskCreator::Holder g_hMaskCreator;
static bool g_bMaskSizeInited = false;
static bool g_bShowContainBox = false;
static bool g_bFillContour = true;
static ImMat g_mMask;
static ImTextureID g_tidMask = 0;
static int g_iMorphSize = 1;
static char g_acMaskSavePath[256];
static ImVec2 g_v2MousePos(0, 0);
static float g_fTime = 0;
static const float g_fTimeMax = 30.f;

// Application Framework Functions
static void _AppInitialize(void** handle)
{
    g_hMaskCreator = MaskCreator::CreateInstance({1920, 1080});
    g_hMaskCreator->SetLoggerLevel(Logger::DEBUG);
    g_hMaskCreator->SetTickRange(0, 30000);
    strncpy(g_acMaskSavePath, "./mask.png", sizeof(g_acMaskSavePath));
}

static void _AppFinalize(void** handle)
{
    g_hMaskCreator = nullptr;
}

static bool _AppFrame(void* handle, bool closeApp)
{
    bool quitApp = false;
    ImGuiIO& io = GetIO(); (void)io;
    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    ImGui:Begin("##MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    int64_t i64Tick = g_fTime*1000;
    auto wndAvailSize = GetContentRegionAvail();
    ImVec2 mousePos(0, 0);
    if (BeginChild("left", {wndAvailSize.x/2, 0}, true))
    {
        TextUnformatted("Draw Mask Area"); SameLine(0, 20);
        Checkbox("Show contain box", &g_bShowContainBox); SameLine(0, 20);
        bool bEnableKeyFrame = g_hMaskCreator->IsKeyFrameEnabled();
        if (Checkbox("Key Frame", &bEnableKeyFrame))
        {
            g_hMaskCreator->EnableKeyFrames(bEnableKeyFrame);
            if (!bEnableKeyFrame)
                g_fTime = 0;
        } SameLine(0, 20);
        if (Button("Load json"))
        {
            const char *filters = "JSON文件(*.json){.JSON},.*";
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.flags = ImGuiFileDialogFlags_OpenFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("LoadJsonFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开JSON文件", 
                                                    filters,
                                                    config);
        } SameLine(0, 20);
        if (Button("Save json"))
        {
            const char *filters = "JSON文件(*.json){.JSON},.*";
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.flags = ImGuiFileDialogFlags_SaveFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("SaveJsonFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开JSON文件", 
                                                    filters,
                                                    config);
        }

        BeginDisabled(!bEnableKeyFrame);
        TextUnformatted("Time:"); SameLine(0, 10);
        SliderFloat("##time_slider", &g_fTime, 0, g_fTimeMax, "%.03f"); SameLine(0, 10);
        Button("Play");
        EndDisabled();

        ostringstream oss;
        oss << "Hovered point: ";
        auto pHoveredPoint = g_hMaskCreator->GetHoveredPoint();
        if (pHoveredPoint)
        {
            auto pos = pHoveredPoint->GetPos();
            oss << "pos(" << pos.x << ", " << pos.y << ")";
            auto off0 = pHoveredPoint->GetBezierGrabberOffset(0);
            oss << ", bg0(" << off0.x << ", " << off0.y << ")";
            auto off1 = pHoveredPoint->GetBezierGrabberOffset(1);
            oss << ", bg1(" << off1.x << ", " << off1.y << ")";
            auto hoverType = pHoveredPoint->GetHoverType();
            oss << ", ht=" << hoverType;
        }
        else
        {
            oss << "N/A";
        }
        string hoverPointInfo = oss.str();
        TextUnformatted(hoverPointInfo.c_str());

        int64_t i64Tick_ = i64Tick;
        g_hMaskCreator->DrawContourPointKeyFrames(i64Tick_);
        if (i64Tick_ != i64Tick)
        {
            i64Tick = i64Tick_;
            g_fTime = (float)((double)i64Tick_/1000);
        }

        BeginChild("##DrawMaskArea");
        wndAvailSize = GetContentRegionAvail();
        auto cursorPos = GetCursorScreenPos();
        if (!g_bMaskSizeInited)
        {
            g_hMaskCreator->ChangeMaskSize(MatUtils::Size2i(wndAvailSize.x, wndAvailSize.y));
            g_bMaskSizeInited = true;
        }
        mousePos = GetMousePos()-cursorPos;
        g_hMaskCreator->DrawContent(cursorPos, wndAvailSize, true, i64Tick);
        if (g_bShowContainBox)
        {
            ImDrawList* pDrawList = GetWindowDrawList();
            auto _rContBox = g_hMaskCreator->GetContourContainBox();
            ImRect rContBox(_rContBox.x, _rContBox.y, _rContBox.z, _rContBox.w);
            static const ImU32 CONTBOX_COLOR = IM_COL32(250, 250, 250, 255);
            pDrawList->AddLine(rContBox.Min, {rContBox.Max.x, rContBox.Min.y}, CONTBOX_COLOR);
            pDrawList->AddLine({rContBox.Max.x, rContBox.Min.y}, rContBox.Max, CONTBOX_COLOR);
            pDrawList->AddLine(rContBox.Max, {rContBox.Min.x, rContBox.Max.y}, CONTBOX_COLOR);
            pDrawList->AddLine({rContBox.Min.x, rContBox.Max.y}, rContBox.Min, CONTBOX_COLOR);
        }
        EndChild();
    }
    EndChild();

    SameLine();
    if (BeginChild("right", {0, 0}, true))
    {
        TextUnformatted("Show Mask Area");

        static const char* s_acLineTypeOpts[] = { "8-connect", "4-connect", "AA", "CapsuleSDF" };
        static const int s_iLintTypeOptsCnt = sizeof(s_acLineTypeOpts)/sizeof(s_acLineTypeOpts[0]);
        static int s_iLintTypeSelIdx = 0;
        PushItemWidth(200);
        if (BeginCombo("Line Type", s_acLineTypeOpts[s_iLintTypeSelIdx]))
        {
            for (auto i = 0; i < s_iLintTypeOptsCnt; i++)
            {
                const bool bSelected = i == s_iLintTypeSelIdx;
                if (Selectable(s_acLineTypeOpts[i], bSelected))
                    s_iLintTypeSelIdx = i;
                if (bSelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }
        PopItemWidth();
        SameLine(0, 20);
        Checkbox("Fill contour", &g_bFillContour);

        g_mMask = g_hMaskCreator->GetMask(s_iLintTypeSelIdx, g_bFillContour, IM_DT_FLOAT32, 1, 0, i64Tick);
        if (!g_mMask.empty())
        {
            ImGui::ImMat mRgba; mRgba.type = IM_DT_INT8;
            MatUtils::GrayToRgba(mRgba, g_mMask, 255);
            ImGenerateOrUpdateTexture(g_tidMask, mRgba.w, mRgba.h, mRgba.c, (const unsigned char *)mRgba.data);

            // log mask value around mouse position
            // if (mousePos != g_v2MousePos)
            // {
            //     g_v2MousePos = mousePos;
            //     auto v2MouseCoord = mousePos/g_hMaskCreator->GetUiScale();
            //     int coordCenterX = round(v2MouseCoord.x);
            //     int coordCenterY = round(v2MouseCoord.y);
            //     cout << "Mouse pos: (" << mousePos.x << ", " << mousePos.y << ") -> (" << coordCenterX << ", " << coordCenterY << ")" << endl;
            //     int offX = 3, offY = 3;
            //     const auto maskDtype = g_mMask.type;
            //     const bool isMaskDataInteger = maskDtype==IM_DT_INT8 || maskDtype==IM_DT_INT16 || maskDtype==IM_DT_INT16_BE || maskDtype==IM_DT_INT32 || maskDtype==IM_DT_INT64;
            //     for (int j = coordCenterY-offY; j <= coordCenterY+offY; j++)
            //     {
            //         if (j < 0 || j >= g_mMask.h)
            //             continue;
            //         cout << "\t\t";
            //         for (int i = coordCenterX-offX; i <= coordCenterX+offX; i++)
            //         {
            //             if (i < 0 || i >= g_mMask.w)
            //                 continue;
            //             if (maskDtype == IM_DT_INT8)
            //                 cout << setw(6) << (uint32_t)g_mMask.at<uint8_t>(i, j);
            //             else if (maskDtype == IM_DT_INT16)
            //                 cout << setw(6) << g_mMask.at<uint16_t>(i, j);
            //             else if (maskDtype == IM_DT_FLOAT32)
            //                 cout << setw(6) << fixed << g_mMask.at<float>(i, j);
            //             else
            //                 throw runtime_error("INVALID code branch.");
            //             cout << " ";
            //         }
            //         cout << endl;
            //     }
            //     cout << endl;
            // }
        }

        PushItemWidth(200);
        InputText("##SavePath", g_acMaskSavePath, sizeof(g_acMaskSavePath));
        PopItemWidth();
        SameLine(0, 5);
        BeginDisabled(g_mMask.empty());
        if (Button("Save png"))
        {
            MatUtils::SaveAsPng(g_mMask, g_acMaskSavePath);
        }
        EndDisabled();

        auto currPos = GetCursorScreenPos();
        wndAvailSize = GetContentRegionAvail();
        if (g_tidMask)
            Image(g_tidMask, wndAvailSize);
    }
    EndChild();

    End();

    // open file dialog
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("LoadJsonFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            g_hMaskCreator = MaskCreator::LoadFromJson(filePath);
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("SaveJsonFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            g_hMaskCreator->SaveAsJson(filePath);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (closeApp)
        quitApp = true;
    return quitApp;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "ImMaskCreator Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.width = 1280;
    property.height = 720;
    property.application.Application_Initialize = _AppInitialize;
    property.application.Application_Finalize = _AppFinalize;
    property.application.Application_Frame = _AppFrame;
}
