#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <string>
#include <sstream>
#include "MediaOverview.h"
#include "SnapshotGenerator.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

static MediaOverview* g_movr = nullptr;
static SnapshotGeneratorHolder g_ssgen;
static SnapshotGenerator::ViewerHolder g_ssvw1;
static double g_windowPos = 0.f;
static double g_windowSize = 300.f;
static double g_windowFrames = 14.0f;
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

void Application_SetupContext(ImGuiContext* ctx)
{
}

void Application_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    GetMediaParserLogger()
        ->SetShowLevels(DEBUG);
    GetSnapshotGeneratorLogger()
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
    g_movr = CreateMediaOverview();
    g_movr->SetSnapshotSize(320, 180);
    g_ssgen = CreateSnapshotGenerator();
    g_ssgen->SetSnapshotResizeFactor(0.5f, 0.5f);
    g_ssgen->SetCacheFactor(3);
    g_ssvw1 = g_ssgen->CreateViewer(0);
}

void Application_Finalize(void** handle)
{
    g_ssgen->ReleaseViewer(g_ssvw1);
    g_ssgen = nullptr;
    ReleaseMediaOverview(&g_movr);
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
    g_snapImageSize.x = io.DisplaySize.x/(g_windowFrames+1);
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
        ImGui::SameLine();
        if (ImGui::Button("Refresh snapwnd configuration"))
            g_ssgen->ConfigSnapWindow(g_windowSize, g_windowFrames, true);

        ImGui::Spacing();

        float pos = g_windowPos;
        float minPos = (float)g_ssgen->GetVideoMinPos()/1000.f;
        float vidDur = (float)g_ssgen->GetVideoDuration()/1000.f;
        if (ImGui::SliderFloat("Position", &pos, minPos, minPos+vidDur, "%.3f"))
        {
            g_windowPos = pos;
        }

        float wndSize = g_windowSize;
        float minWndSize = (float)g_ssgen->GetMinWindowSize();
        float maxWndSize = (float)g_ssgen->GetMaxWindowSize();
        if (ImGui::SliderFloat("WindowSize", &wndSize, minWndSize, maxWndSize, "%.3f"))
            g_windowSize = wndSize;
        if (ImGui::IsItemDeactivated())
            g_ssgen->ConfigSnapWindow(g_windowSize, g_windowFrames);

        ImGui::Spacing();

        vector<SnapshotGenerator::ImageHolder> snapshots;
        if (!g_ssvw1->GetSnapshots(pos, snapshots))
            snapshots.clear();
        else
            g_ssvw1->UpdateSnapshotTexture(snapshots);

        float startPos = snapshots.size() > 0 ? (float)snapshots[0]->mTimestampMs/1000 : minPos;
        int snapshotCnt = (int)ceil(g_windowFrames);
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
                string tag = MillisecToString(snapshots[i]->mTimestampMs);
                if (!snapshots[i]->mTextureReady)
                {
                    ImGui::Dummy(g_snapImageSize);
                    tag += "(loading)";
                }
                else
                {
                    ImGui::Image(*(snapshots[i]->mTextureHolder), g_snapImageSize);
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
            g_ssgen->Close();
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            // g_movr->Open(filePathName, 10);
            // g_movr->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            // g_ssgen->Open(g_movr->GetMediaParser());
            g_ssgen->Open(filePathName);
            g_windowPos = (float)g_ssgen->GetVideoMinPos()/1000.f;
            g_windowSize = (float)g_ssgen->GetVideoDuration()/10000.f;
            g_ssgen->ConfigSnapWindow(g_windowSize, g_windowFrames);
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
