#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <ImGuiFileDialog.h>
#include <string>
#include <sstream>
#include "MediaOverview.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

static MediaOverview* g_movr = nullptr;
// static MediaOverview* g_movr2 = nullptr;
static uint32_t g_ssCount = 12;
static vector<ImTextureID> g_snapshotTids;
ImVec2 g_snapImageSize;
const string c_imguiIniPath = "movr_test.ini";
const string c_bookmarkPath = "bookmark.ini";

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "MediaOverviewTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
}

void Application_SetupContext(ImGuiContext* ctx)
{
}

void Application_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    GetMediaOverviewLogger()
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

    g_snapshotTids.resize(g_ssCount);
    for (auto& tid : g_snapshotTids)
        tid = nullptr;
    g_movr = CreateMediaOverview();
    g_movr->SetSnapshotSize(320, 180);
    // g_movr->SetSnapshotResizeFactor(0.5f, 0.5f);
    // g_movr2 = CreateMediaOverview();
    // g_movr2->SetSnapshotSize(320, 180);
    // g_movr2->SetFixedAggregateSamples(1);
}

void Application_Finalize(void** handle)
{
    ReleaseMediaOverview(&g_movr);
    // ReleaseMediaOverview(&g_movr2);
    for (auto& tid : g_snapshotTids)
    {
        if (tid)
            ImGui::ImDestroyTexture(tid);
        tid = nullptr;
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

bool Application_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();
    g_snapImageSize.x = io.DisplaySize.x/(g_ssCount+1);
    g_snapImageSize.y = g_snapImageSize.x*9/16;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi *.mxf){.mp4,.mov,.mkv,.webm,.avi,.mxf,.MP4,.MOV,.MKV,.WEBM,.AVI,.MXF},.*";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", 
                                                    filters, "~/Videos/", 1, nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_Modal);
        }

        ImGui::Spacing();

        MediaOverview::WaveformHolder hWaveform = g_movr->GetWaveform();
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
            ImGui::BeginGroup();
            if (i >= snapshots.size())
            {
                ImGui::Dummy(g_snapImageSize);
                ImGui::TextUnformatted("No image");
            }
            else
            {
                ImGui::ImMat vmat = snapshots[i];
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
                    if (g_snapshotTids[i]) ImGui::Image(g_snapshotTids[i], g_snapImageSize);
                }
                else
                {
                    ImGui::Dummy(g_snapImageSize);
                }
                ImGui::TextUnformatted(tag.c_str());
            }
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
                if (tid)
                    ImGui::ImDestroyTexture(tid);
                tid = nullptr;
            }
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (g_movr->Open(filePathName, g_ssCount))
                g_movr->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            // g_movr2->Open(g_movr->GetMediaParser());
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
