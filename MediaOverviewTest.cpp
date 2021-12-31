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

void Application_Initialize(void** handle)
{
    SetDefaultLoggerLevels(DEBUG);

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
    // g_movr->SetSnapshotSize(160, 90);
    g_movr->SetSnapshotResizeFactor(0.5f, 0.5f);
}

void Application_Finalize(void** handle)
{
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
    g_snapImageSize.x = io.DisplaySize.x/(g_ssCount+1);
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
                    Log(ERROR) << "WRONG snapshot format!" << endl;
                    valid = false;
                    tag += "(bad format)";
                }
                if (valid)
                {
                    if (vmat.device == IM_DD_CPU)
                        ImGui::ImGenerateOrUpdateTexture(g_snapshotTids[i], vmat.w, vmat.h, vmat.c, (const unsigned char *)vmat.data);
#if IMGUI_VULKAN_SHADER
                    else
                    {
                        ImGui::VkMat vkmat = vmat;
                        ImGui::ImGenerateOrUpdateTexture(g_snapshotTids[i], vkmat.w, vkmat.h, vkmat.c, vkmat.buffer_offset(), (const unsigned char *)vkmat.buffer());
                    }
#endif
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
            for (auto& tid : g_snapshotTids)
            {
                if (tid)
                    ImGui::ImDestroyTexture(tid);
                tid = nullptr;
            }
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (g_movr->Open(filePathName, g_ssCount))
                g_movr->GetMediaParser()->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        done = true;
    }

    return done;
}
