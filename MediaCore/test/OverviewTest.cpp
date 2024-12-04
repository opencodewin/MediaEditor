#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <ImGuiFileDialog.h>
#include <cmath>
#include <string>
#include <sstream>
#include "Overview.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

static Overview::Holder g_movr;
// static MediaOverview::Holder g_movr2;
static uint32_t g_ssCount = 12;
static vector<ImTextureID> g_snapshotTids;
ImVec2 g_v2SsDisplaySize(0, 0);
const string c_imguiIniPath = "movr_test.ini";
const string c_bookmarkPath = "bookmark.ini";
static bool g_isImageSequence = false;
static MediaParser::Holder g_mediaParser;

// Application Framework Functions
static void MediaOverview_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(INFO);
    Overview::GetLogger()
        ->SetShowLevels(DEBUG);

#ifdef USE_PLACES_FEATURE
	// load bookmarks
	ifstream docFile(c_bookmarkPath, ios::in);
	if (docFile.is_open())
	{
		stringstream strStream;
		strStream << docFile.rdbuf(); //read the file
		ImGuiFileDialog::Instance()->DeserializePlaces(strStream.str());
		docFile.close();
	}
#endif

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = c_imguiIniPath.c_str();

    g_snapshotTids.resize(g_ssCount);
    for (auto& tid : g_snapshotTids)
        tid = nullptr;
    g_movr = Overview::CreateInstance();
    // g_movr->SetSnapshotSize(320, 180);
    g_movr->EnableHwAccel(true);
    g_movr->SetSnapshotResizeFactor(0.1, 0.1);
    // g_movr->SetSnapshotResizeFactor(0.5f, 0.5f);
    // g_movr2 = CreateMediaOverview();
    // g_movr2->SetSnapshotSize(320, 180);
    // g_movr2->SetFixedAggregateSamples(1);

    HwaccelManager::GetDefaultInstance()->Init();
}

static void MediaOverview_Finalize(void** handle)
{
    g_movr = nullptr;
    // g_movr2 = nullptr;
    for (auto& tid : g_snapshotTids)
    {
        ImGui::ImDestroyTexture(&tid);
    }
#ifdef USE_PLACES_FEATURE
	// save bookmarks
	ofstream configFileWriter(c_bookmarkPath, ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializePlaces();
		configFileWriter.close();
	}
#endif
}

static bool MediaOverview_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi *.mxf){.mp4,.mov,.mkv,.webm,.avi,.mxf,.MP4,.MOV,.MKV,.WEBM,.AVI,.MXF},.*";
            IGFD::FileDialogConfig config;
			config.path = "~/Videos/";
            config.countSelectionMax = 1;
			config.flags = ImGuiFileDialogFlags_OpenFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", 
                                                    filters,
                                                    config);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Open image sequence", &g_isImageSequence);

        ImGui::Spacing();

        Overview::Waveform::Holder hWaveform = g_movr->GetWaveform();
        double startPos = 0;
        double windowSize = 0;
        if (hWaveform)
        {
            int sampleSize = hWaveform->pcm[0].size();
            int startOff = startPos == 0 ? 0 : (int)(startPos/hWaveform->aggregateDuration);
            if (startOff >= sampleSize) startOff = 0;
            int windowLen = windowSize == 0 ? sampleSize : (int)(windowSize/hWaveform->aggregateDuration);
            if (startOff+windowLen > sampleSize) windowLen = sampleSize-startOff;
            float verticalMax = abs(hWaveform->maxSample);
            if (verticalMax < abs(hWaveform->minSample))
                verticalMax = abs(hWaveform->minSample);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f,0.f, 1.f));
            ImGui::PlotLinesEx("Waveform", hWaveform->pcm[0].data()+startOff, windowLen, 0, nullptr, -verticalMax, verticalMax, ImVec2(io.DisplaySize.x, 160), sizeof(float), false);
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // hWaveform = g_movr2->GetWaveform();
        // if (hWaveform)
        // {
        //     int sampleSize = hWaveform->pcm[0].size();
        //     int startOff = startPos == 0 ? 0 : (int)(startPos/hWaveform->aggregateDuration);
        //     if (startOff >= sampleSize) startOff = 0;
        //     int windowLen = windowSize == 0 ? sampleSize : (int)(windowSize/hWaveform->aggregateDuration);
        //     if (startOff+windowLen > sampleSize) windowLen = sampleSize-startOff;
        //     ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f,0.f, 1.f));
        //     ImGui::PlotLinesEx("Waveform", hWaveform->pcm[0].data()+startOff, windowLen, 0, nullptr, -1.f, 1.f, ImVec2(io.DisplaySize.x, 160), sizeof(float), false);
        //     ImGui::PopStyleColor();
        // }

        // ImGui::Spacing();

        vector<ImGui::ImMat> snapshots;
        if (!g_movr->GetSnapshots(snapshots))
            snapshots.clear();

        if (snapshots.size() > g_snapshotTids.size())
        {
            int addcnt = snapshots.size()-g_snapshotTids.size();
            for (int i = 0; i < addcnt; i++)
                g_snapshotTids.push_back(nullptr);
        }
        for (int i = 0; i < snapshots.size(); i++)
        {
            const auto& vmat = snapshots[i];
            if (g_v2SsDisplaySize.x == 0.0 && !vmat.empty())
            {
                g_v2SsDisplaySize.x = floor(io.DisplaySize.x/(g_ssCount+1));
                g_v2SsDisplaySize.y = g_v2SsDisplaySize.x*vmat.h/vmat.w;
            }

            ImGui::BeginGroup();
            string tag = TimestampToString(vmat.time_stamp);
            bool valid = true;
            if (vmat.empty())
            {
                valid = false;
                tag += "(loading)";
            }
            if (valid &&
                ((vmat.color_format != IM_CF_RGBA && vmat.color_format != IM_CF_ABGR) ||
                vmat.type != IM_DT_INT8 ||
                (vmat.device != IM_DD_CPU && vmat.device != IM_DD_VULKAN)))
            {
                Log(Error) << "WRONG snapshot format!" << endl;
                valid = false;
                tag += "(bad format)";
            }
            if (valid)
            {
                ImGui::ImMatToTexture(vmat, g_snapshotTids[i]);
                if (g_snapshotTids[i]) ImGui::Image(g_snapshotTids[i], g_v2SsDisplaySize);
            }
            else
            {
                ImGui::Dummy(g_v2SsDisplaySize);
            }
            ImGui::TextUnformatted(tag.c_str());
            ImGui::EndGroup();
            ImGui::SameLine();
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
            g_movr->Close();
            // g_movr2->Close();
            for (auto& tid : g_snapshotTids)
            {
                ImGui::ImDestroyTexture(&tid);
            }
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_mediaParser = MediaParser::CreateInstance();
            if (g_isImageSequence)
                g_mediaParser->OpenImageSequence({25, 1}, filePathName, ".+_([[:digit:]]{1,})\\.png", false);
            else
            {
                g_mediaParser->Open(filePathName);
                g_mediaParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            }
            g_movr->Open(g_mediaParser, g_ssCount);
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

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "MediaOverviewTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
    property.application.Application_Initialize = MediaOverview_Initialize;
    property.application.Application_Finalize = MediaOverview_Finalize;
    property.application.Application_Frame = MediaOverview_Frame;
}
