#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <string>
#include <sstream>
#include "MediaOverview.h"
#include "MediaSnapshot.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

static MediaOverview* g_movr = nullptr;
static MediaSnapshot* g_msrc = nullptr;
static double g_windowPos = 0.f;
static double g_windowSize = 300.f;
static double g_windowFrames = 14.0f;
static vector<ImTextureID> g_snapshotTids;
ImVec2 g_snapImageSize;
const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "MediaSnapshotTest";
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

    size_t ssCnt = (size_t)ceil(g_windowFrames)+1;
    g_snapshotTids.reserve(ssCnt);
    for (auto& tid : g_snapshotTids)
        tid = nullptr;
    g_movr = CreateMediaOverview();
    g_movr->SetSnapshotSize(320, 180);
    g_msrc = CreateMediaSnapshot();
    g_msrc->SetSnapshotResizeFactor(0.5f, 0.5f);
}

void Application_Finalize(void** handle)
{
    ReleaseMediaSnapshot(&g_msrc);
    ReleaseMediaOverview(&g_movr);
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

bool Application_Frame(void * handle)
{
    bool done = false;
    auto& io = ImGui::GetIO();
    g_snapImageSize.x = io.DisplaySize.x/(g_windowFrames+1);
    g_snapImageSize.y = g_snapImageSize.x*9/16;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi){.mp4,.mov,.mkv,.webm,.avi,.MP4,.MOV,.MKV,WEBM,.AVI},.*";
            ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", filters, "/mnt/data2/video/hd/", 1, nullptr, ImGuiFileDialogFlags_ShowBookmark);
        }

        ImGui::Spacing();

        float pos = g_windowPos;
        float minPos = (float)g_msrc->GetVideoMinPos()/1000.f;
        float vidDur = (float)g_msrc->GetVideoDuration()/1000.f;
        if (ImGui::SliderFloat("Position", &pos, minPos, minPos+vidDur, "%.3f"))
        {
            g_windowPos = pos;
        }

        float wndSize = g_windowSize;
        float minWndSize = (float)g_msrc->GetMinWindowSize();
        float maxWndSize = (float)g_msrc->GetMaxWindowSize();
        if (ImGui::SliderFloat("WindowSize", &wndSize, minWndSize, maxWndSize, "%.3f"))
            g_windowSize = wndSize;
        if (ImGui::IsItemDeactivated())
            g_msrc->ConfigSnapWindow(g_windowSize, g_windowFrames);

        ImGui::Spacing();

        vector<ImGui::ImMat> snapshots;
        if (!g_msrc->GetSnapshots(snapshots, pos))
            snapshots.clear();

        float startPos = snapshots.size() > 0 ? snapshots[0].time_stamp : minPos;
        int snapshotCnt = (int)ceil(g_windowFrames);
        if (snapshotCnt > g_snapshotTids.size())
        {
            int addcnt = snapshotCnt-g_snapshotTids.size();
            for (int i = 0; i < addcnt; i++)
                g_snapshotTids.push_back(nullptr);
        }
        for (int i = 0; i < snapshotCnt; i++)
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
                    Log(ERROR) << "WRONG snapshot format!" << endl;
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
            g_msrc->Close();
            for (auto& tid : g_snapshotTids)
            {
                if (tid)
                    ImGui::ImDestroyTexture(tid);
                tid = nullptr;
            }
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_movr->Open(filePathName, 10);
            g_movr->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            g_msrc->Open(g_movr->GetMediaParser());
            // g_msrc->Open(filePathName);
            g_windowPos = (float)g_msrc->GetVideoMinPos()/1000.f;
            g_windowSize = (float)g_msrc->GetVideoDuration()/10000.f;
            g_msrc->ConfigSnapWindow(g_windowSize, g_windowFrames);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        done = true;
    }

    return done;
}
