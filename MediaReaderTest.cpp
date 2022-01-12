#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <chrono>
#include "MediaReader.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using Clock = chrono::steady_clock;

static MediaReader* g_mrdr = nullptr;
static double g_playStartPos = 0.f;
static Clock::time_point g_playStartTp;
static bool g_isPlay = false;
static bool g_isLongCacheDur = false;
static const pair<double, double> G_DurTable[] = {
    {  5, 1 },
    { 10, 2 },
};
static ImTextureID g_imageTid;
static ImVec2 g_imageDisplaySize = { 640, 360 };
const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";


// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "MediaReaderTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
}

void Application_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    GetMediaReaderLogger()
        ->SetShowLevels(DEBUG);

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

    g_mrdr = CreateMediaReader();
    g_mrdr->SetSnapshotSize(g_imageDisplaySize.x, g_imageDisplaySize.y);
    // g_mrdr->SetSnapshotResizeFactor(0.5f, 0.5f);
}

void Application_Finalize(void** handle)
{
    ReleaseMediaReader(&g_mrdr);
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
}

bool Application_Frame(void * handle)
{
    bool done = false;
    auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi){.mp4,.mov,.mkv,.webm,.avi,.MP4,.MOV,.MKV,WEBM,.AVI},.*";
            ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", filters, "/mnt/data2/video/hd/", 1, nullptr, ImGuiFileDialogFlags_ShowBookmark);
        }

        ImGui::SameLine();

        const MediaInfo::VideoStream* vstminfo = g_mrdr->GetVideoStream();
        float vidDur = vstminfo ? (float)vstminfo->duration : 0;
        bool isForward = g_mrdr->IsDirectionForward();
        double elapsedTime = chrono::duration_cast<chrono::duration<double>>((Clock::now()-g_playStartTp)).count();
        float playPos = g_isPlay ? (isForward ? g_playStartPos+elapsedTime : g_playStartPos-elapsedTime) : g_playStartPos;
        if (playPos < 0) playPos = 0;
        if (playPos > vidDur) playPos = vidDur;
        string playBtnLabel = g_isPlay ? "Pause" : "Play ";
        if (ImGui::Button(playBtnLabel.c_str()))
        {
            g_isPlay = !g_isPlay;
            if (g_isPlay)
            {
                g_playStartTp = Clock::now();
            }
            else
            {
                g_playStartPos = playPos;
            }
        }

        ImGui::SameLine();

        string dirBtnLabel = isForward ? "Backword" : "Forward";
        if (ImGui::Button(dirBtnLabel.c_str()))
        {
            g_mrdr->SetDirection(!isForward);
            isForward = g_mrdr->IsDirectionForward();
            g_playStartPos = playPos;
            g_playStartTp = Clock::now();
        }

        ImGui::SameLine();

        string cdurBtnLabel = g_isLongCacheDur ? "Short cache duration" : "Long cache duration";
        if (ImGui::Button(cdurBtnLabel.c_str()))
        {
            g_isLongCacheDur = !g_isLongCacheDur;
            if (g_isLongCacheDur)
                g_mrdr->SetCacheDuration(G_DurTable[1].first, G_DurTable[1].second);
            else
                g_mrdr->SetCacheDuration(G_DurTable[0].first, G_DurTable[0].second);
        }

        ImGui::Spacing();

        if (ImGui::SliderFloat("Position", &playPos, 0, vidDur, "%.3f"))
        {
            g_mrdr->SeekTo(playPos);
            g_playStartPos = playPos;
            g_playStartTp = Clock::now();
        }

        ImGui::Spacing();

        ImGui::ImMat vmat;
        if (g_mrdr->ReadFrame(playPos, vmat))
        {
            string imgTag = TimestampToString(vmat.time_stamp);
            bool imgValid = true;
            if (vmat.empty())
            {
                imgValid = false;
                imgTag += "(loading)";
            }
            if (imgValid &&
                ((vmat.color_format != IM_CF_RGBA && vmat.color_format != IM_CF_ABGR) ||
                vmat.type != IM_DT_INT8 ||
                (vmat.device != IM_DD_CPU && vmat.device != IM_DD_VULKAN)))
            {
                Log(ERROR) << "WRONG snapshot format!" << endl;
                imgValid = false;
                imgTag += "(bad format)";
            }
            if (imgValid)
            {
                ImGui::ImMatToTexture(vmat, g_imageTid);
                if (g_imageTid) ImGui::Image(g_imageTid, g_imageDisplaySize);
            }
            else
            {
                ImGui::Dummy(g_imageDisplaySize);
            }
            ImGui::TextUnformatted(imgTag.c_str());
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
            g_mrdr->Close();
            if (g_imageTid)
                ImGui::ImDestroyTexture(g_imageTid);
            g_imageTid = nullptr;
            g_isLongCacheDur = false;
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            // g_movr->Open(filePathName, 10);
            // g_movr->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            // g_mrdr->Open(g_movr->GetMediaParser());
            g_mrdr->Open(filePathName);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        done = true;
    }

    return done;
}
