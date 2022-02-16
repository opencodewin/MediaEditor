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
#include "MultiTrackAudioReader.h"
#include "AudioRender.hpp"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using Clock = chrono::steady_clock;

static MultiTrackAudioReader* g_mtAudReader = nullptr;
const int c_audioRenderChannels = 2;
const int c_audioRenderSampleRate = 44100;
const AudioRender::PcmFormat c_audioRenderFormat = AudioRender::PcmFormat::FLOAT32;
static AudioRender* g_audrnd = nullptr;
static double g_audPos = 0;
static bool g_isPlay = false;
static bool g_playForward = true;

const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";

class SimplePcmStream : public AudioRender::ByteStream
{
public:
    SimplePcmStream(MultiTrackAudioReader* audrdr) : m_audrdr(audrdr) {}

    uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override
    {
        if (!m_audrdr)
            return 0;
        uint32_t readSize = 0;
        while (readSize < buffSize)
        {
            uint32_t amatTotalDataSize = m_amat.total()*m_amat.elemsize;
            if (m_readPosInAmat < amatTotalDataSize)
            {
                uint32_t copySize = buffSize-readSize;
                if (copySize > amatTotalDataSize)
                    copySize = amatTotalDataSize;
                memcpy(buff+readSize, (uint8_t*)m_amat.data+m_readPosInAmat, copySize);
                readSize += copySize;
                m_readPosInAmat += copySize;
            }
            if (m_readPosInAmat >= amatTotalDataSize)
            {
                bool eof;
                ImGui::ImMat amat;
                if (!m_audrdr->ReadAudioSamples(amat, eof))
                    return 0;
                g_audPos = amat.time_stamp;
                m_amat = amat;
                m_readPosInAmat = 0;
            }
        }
        return buffSize;
    }

private:
    MultiTrackAudioReader* m_audrdr;
    ImGui::ImMat m_amat;
    uint32_t m_readPosInAmat{0};
};
static SimplePcmStream* g_pcmStream = nullptr;


// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "MultiTrackAudioReaderTest";
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
    GetMultiTrackAudioReaderLogger()
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

    g_mtAudReader = CreateMultiTrackAudioReader();
    g_mtAudReader->Configure(c_audioRenderChannels, c_audioRenderSampleRate);
    g_mtAudReader->Start();

    g_pcmStream = new SimplePcmStream(g_mtAudReader);
    g_audrnd = CreateAudioRender();
    g_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, g_pcmStream);
}

void Application_Finalize(void** handle)
{
    if (g_audrnd)
    {
        g_audrnd->CloseDevice();
        ReleaseAudioRender(&g_audrnd);
    }
    if (g_pcmStream)
    {
        delete g_pcmStream;
        g_pcmStream = nullptr;
    }
    ReleaseMultiTrackAudioReader(&g_mtAudReader);

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

static uint32_t s_addClipOptSelId = 0;
static double s_addClipTimeLineOffset = 0;
static double s_addClipStartOffset = 0;
static double s_addClipEndOffset = 0;

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
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi){.mp4,.mov,.mkv,.webm,.avi,.MP4,.MOV,.MKV,WEBM,.AVI},.*";
            ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", filters, "/mnt/data2/video/hd/", 1, nullptr, ImGuiFileDialogFlags_ShowBookmark);
        }
        ImGui::SameLine(0, 20);
        vector<string> addClipOpts;
        for (uint32_t i = 0; i < g_mtAudReader->TrackCount(); i++)
        {
            ostringstream oss;
            oss << "track#" << i+1;
            addClipOpts.push_back(oss.str());
        }
        addClipOpts.push_back("new track");
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("AddClipOptions");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##AddClipOptions", addClipOpts[s_addClipOptSelId].c_str()))
        {
            for (uint32_t i = 0; i < addClipOpts.size(); i++)
            {
                string& item = addClipOpts[i];
                const bool isSelected = s_addClipOptSelId == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_addClipOptSelId = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 20);
        ImGui::TextUnformatted("TimeLineOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##TimeLineOffset", &s_addClipTimeLineOffset);
        ImGui::SameLine(0, 20);
        ImGui::TextUnformatted("ClipStartOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##ClipStartOffset", &s_addClipStartOffset);
        ImGui::SameLine(0, 20);
        ImGui::TextUnformatted("ClipEndOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##ClipEndOffset", &s_addClipEndOffset);
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextUnformatted("Audio Tracks:");
        uint32_t audTrackIdx = 1;
        for (auto track = g_mtAudReader->TrackListBegin(); track != g_mtAudReader->TrackListEnd(); track++)
        {
            ostringstream oss;
            oss << "Track#" << audTrackIdx << ": [";
            for (auto clip = (*track)->ClipListBegin(); clip != (*track)->ClipListEnd();)
            {
                oss << "Clip#" << (*clip)->Id() << ":{ 'timeLineOffset': " << (*clip)->TimeLineOffset()
                    << ", 'startOffset': " << (*clip)->StartOffset() << ", 'endOffset': " << (*clip)->EndOffset()
                    << ", 'duration': " << (*clip)->ClipDuration() << " }";
                clip++;
                if (clip != (*track)->ClipListEnd())
                    oss << ", ";
            }
            oss << "].";
            ImGui::TextUnformatted(oss.str().c_str());
        }

        ImGui::Spacing();
        ImGui::Dummy({10, 10});

        string playBtnLabel = g_isPlay ? "Pause" : "Play ";
        if (ImGui::Button(playBtnLabel.c_str()))
        {
            g_isPlay = !g_isPlay;
            if (g_isPlay)
                g_audrnd->Resume();
            else
                g_audrnd->Pause();
        }

        ImGui::SameLine();

        string dirBtnLabel = g_playForward ? "Backword" : "Forward";
        if (ImGui::Button(dirBtnLabel.c_str()))
        {
            bool notForward = !g_playForward;
            g_mtAudReader->SetDirection(notForward);
            g_playForward = notForward;
        }

        ImGui::Spacing();

        ostringstream oss;
        oss << "Audio pos: " << TimestampToString(g_audPos);
        string audTag = oss.str();
        ImGui::TextUnformatted(audTag.c_str());

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
            if (s_addClipOptSelId == g_mtAudReader->TrackCount())
            {
                if (!g_mtAudReader->AddTrack())
                {
                    Log(Error) << "FAILED to 'AddTrack'! Message is '" << g_mtAudReader->GetError() << "'." << endl;
                }
            }
            AudioTrackHolder hTrack = g_mtAudReader->GetTrack(s_addClipOptSelId);
            hTrack->AddNewClip(filePathName, s_addClipTimeLineOffset, s_addClipStartOffset, s_addClipEndOffset);
            s_addClipOptSelId = g_mtAudReader->TrackCount();
            s_addClipTimeLineOffset = 0;
            s_addClipStartOffset = 0;
            s_addClipEndOffset = 0;
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
