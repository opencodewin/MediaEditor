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
#include "MediaSnapshot.h"
#include "FFUtils.h"

using namespace std;

static MediaSnapshot* g_msrc = nullptr;
static float g_windowPos = 0.f;
static float g_windowSize = 300.f;
static float g_windowFrames = 10.0f;
static vector<ImTextureID> g_snapshotTids;
ImVec2 g_snapImageSize;
const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";
#if IMGUI_VULKAN_SHADER
ImGui::ColorConvert_vulkan * m_yuv2rgb {nullptr};
#endif

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
#if IMGUI_VULKAN_SHADER
    m_yuv2rgb = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
#endif

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = c_imguiIniPath.c_str();

    size_t ssCnt = (size_t)ceil(g_windowFrames)+1;
    g_snapshotTids.reserve(ssCnt);
    for (auto& tid : g_snapshotTids)
        tid = nullptr;
    g_msrc = CreateMediaSnapshot();
}

void Application_Finalize(void** handle)
{
    ReleaseMediaSnapshot(&g_msrc);
    for (auto& tid : g_snapshotTids)
    {
        if (tid)
            ImGui::ImDestroyTexture(tid);
        tid = nullptr;
    }
#if IMGUI_VULKAN_SHADER
    if (m_yuv2rgb) { delete m_yuv2rgb; m_yuv2rgb = nullptr; }
#endif
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
        float minPos = (float)g_msrc->GetVidoeMinPos()/1000.f;
        float vidDur = (float)g_msrc->GetVidoeDuration()/1000.f;
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
                if (!vmat.empty())
                {
                    int video_depth = vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
                    int video_shift = vmat.depth != 0 ? vmat.depth : vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
                    ImGui::VkMat in_RGB; in_RGB.type = IM_DT_INT8;
                    m_yuv2rgb->YUV2RGBA(vmat, in_RGB, vmat.color_format, vmat.color_space, vmat.color_range, video_depth, video_shift);
                    ImGui::ImGenerateOrUpdateTexture(g_snapshotTids[i], in_RGB.w, in_RGB.h, in_RGB.c, in_RGB.buffer_offset(), (const unsigned char *)in_RGB.buffer());
                    ImGui::Image(g_snapshotTids[i], g_snapImageSize);
                    ImGui::TextUnformatted(TimestampToString(vmat.time_stamp).c_str());
                }
                else
                {
                    ImGui::Dummy(g_snapImageSize);
                    ImGui::TextUnformatted("loading");
                }
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
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_msrc->Open(filePathName);
            g_windowPos = (float)g_msrc->GetVidoeMinPos()/1000.f;
            g_windowSize = (float)g_msrc->GetVidoeDuration()/10000.f;
            g_msrc->ConfigSnapWindow(g_windowSize, g_windowFrames);
        }
        ImGuiFileDialog::Instance()->Close();
        cout << "ImGuiFileDialog::Instance()->Close()" << endl;
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        done = true;
    }

    return done;
}
