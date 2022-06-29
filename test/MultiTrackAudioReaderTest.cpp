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
using namespace DataLayer;
using Clock = chrono::steady_clock;

static atomic_int64_t g_idIndex{1};

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
        lock_guard<mutex> lk(m_amatLock);
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
                ImGui::ImMat amat;
                if (!m_audrdr->ReadAudioSamples(amat))
                    return 0;
                g_audPos = amat.time_stamp;
                m_amat = amat;
                m_readPosInAmat = 0;
            }
        }
        return buffSize;
    }

    void Flush() override
    {
        lock_guard<mutex> lk(m_amatLock);
        m_amat.release();
        m_readPosInAmat = 0;
    }

    bool GetTimestampMs(int64_t& ts) override
    {
        return false;
    }

private:
    MultiTrackAudioReader* m_audrdr;
    ImGui::ImMat m_amat;
    uint32_t m_readPosInAmat{0};
    std::mutex m_amatLock;
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

static uint32_t s_addClipOptSelIdx = 0;
static double s_addClipStart = 0;
static double s_addClipStartOffset = 0;
static double s_addClipEndOffset = 0;
static uint32_t s_remTrackOptSelIdx = 0;
static uint32_t s_movClipTrackSelIdx = 0;
static uint32_t s_movClipSelIdx = 0;
static double s_changeClipStart = 0;
static double s_changeClipStartOffset = 0;
static double s_changeClipEndOffset = 0;

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
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", 
                                                    filters, 
                                                    "/mnt/data2/video/hd/", 
                                                    1, 
                                                    nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                    ImGuiFileDialogFlags_Modal);
        }

        ImGui::SameLine(0, 20);

        vector<string> trackNames;
        for (uint32_t i = 0; i < g_mtAudReader->TrackCount(); i++)
        {
            ostringstream oss;
            oss << "track#" << i+1;
            trackNames.push_back(oss.str());
        }

        vector<string> addClipOpts(trackNames);
        addClipOpts.push_back("new track");
        if (s_addClipOptSelIdx >= addClipOpts.size())
            s_addClipOptSelIdx = addClipOpts.size()-1;
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("AddClipOptions");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##AddClipOptions", addClipOpts[s_addClipOptSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < addClipOpts.size(); i++)
            {
                string& item = addClipOpts[i];
                const bool isSelected = s_addClipOptSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_addClipOptSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 20);
        ImGui::TextUnformatted("Start");
        ImGui::SameLine();
        ImGui::InputDouble("##Start", &s_addClipStart);
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

        vector<string> selectTrackOpts(trackNames);
        if (selectTrackOpts.empty())
            selectTrackOpts.push_back("<No track>");
        bool noTrack = trackNames.empty();

        ImGui::PushItemWidth(100);
        if (ImGui::BeginCombo("##RemTrackOptions", selectTrackOpts[s_remTrackOptSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < selectTrackOpts.size(); i++)
            {
                string& item = selectTrackOpts[i];
                const bool isSelected = s_remTrackOptSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_remTrackOptSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 20);

        if (noTrack)
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        if (ImGui::Button("Remove Track"))
        {
            g_mtAudReader->RemoveTrackByIndex(s_remTrackOptSelIdx);
            s_remTrackOptSelIdx = 0;
            g_audrnd->Flush();
        }
        if (noTrack)
            ImGui::PopItemFlag();

        ImGui::Spacing();

        ImGui::PushItemWidth(100);
        if (ImGui::BeginCombo("##MovClipSelTrackOptions", selectTrackOpts[s_movClipTrackSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < selectTrackOpts.size(); i++)
            {
                string& item = selectTrackOpts[i];
                const bool isSelected = s_movClipTrackSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_movClipTrackSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine(0, 10);

        vector<string> clipNames;
        if (!noTrack)
        {
            AudioTrackHolder hTrack = g_mtAudReader->GetTrackByIndex(s_movClipTrackSelIdx);
            auto clipIter = hTrack->ClipListBegin();
            while (clipIter != hTrack->ClipListEnd())
            {
                ostringstream oss;
                oss << "Clip#" << (*clipIter)->Id();
                clipNames.push_back(oss.str());
                clipIter++;
            }
        }
        bool noClip = false;
        vector<string> clipSelOpts(clipNames);
        if (clipSelOpts.empty())
        {
            clipSelOpts.push_back("<no clip>");
            noClip = true;
        }
        if (s_movClipSelIdx >= clipSelOpts.size())
            s_movClipSelIdx = clipSelOpts.size()-1;
        if (ImGui::BeginCombo("##MovClipSelClipOptions", clipSelOpts[s_movClipSelIdx].c_str()))
        {
            AudioTrackHolder hTrack = g_mtAudReader->GetTrackByIndex(s_movClipTrackSelIdx);
            auto clipIter = hTrack->ClipListBegin();
            for (uint32_t i = 0; i < clipSelOpts.size(); i++)
            {
                string& item = clipSelOpts[i];
                const bool isSelected = s_movClipSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_movClipSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                clipIter++;
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
        ImGui::SameLine(0, 20);

        if (noClip)
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        if (ImGui::Button("Remove Clip"))
        {
            AudioTrackHolder hTrack = g_mtAudReader->GetTrackByIndex(s_movClipTrackSelIdx);
            hTrack->RemoveClipByIndex(s_movClipSelIdx);
            g_mtAudReader->Refresh();
            s_movClipSelIdx = 0;
            g_audrnd->Flush();
        }
        if (noClip)
            ImGui::PopItemFlag();

        ImGui::SameLine(0, 20);

        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("tloff");
        ImGui::SameLine();
        ImGui::InputDouble("##tloff", &s_changeClipStart);
        ImGui::SameLine(0, 10);

        ImGui::PopItemWidth();
        if (noClip)
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        if (ImGui::Button("Move Clip"))
        {
            AudioTrackHolder hTrack = g_mtAudReader->GetTrackByIndex(s_movClipTrackSelIdx);
            AudioClipHolder hClip = hTrack->GetClipByIndex(s_movClipSelIdx);
            hTrack->MoveClip(hClip->Id(), (int64_t)(s_changeClipStart*1000));
            g_mtAudReader->Refresh();
        }
        if (noClip)
            ImGui::PopItemFlag();

        ImGui::SameLine(0, 10);
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("off0");
        ImGui::SameLine();
        ImGui::InputDouble("##off0", &s_changeClipStartOffset);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("off1");
        ImGui::SameLine();
        ImGui::InputDouble("##off1", &s_changeClipEndOffset);
        ImGui::SameLine(0, 10);
        ImGui::PopItemWidth();

        if (noClip)
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        if (ImGui::Button("Change Clip Range"))
        {
            AudioTrackHolder hTrack = g_mtAudReader->GetTrackByIndex(s_movClipTrackSelIdx);
            AudioClipHolder hClip = hTrack->GetClipByIndex(s_movClipSelIdx);
            hTrack->ChangeClipRange(hClip->Id(), (int64_t)(s_changeClipStartOffset*1000), (int64_t)(s_changeClipEndOffset*1000));
            g_mtAudReader->Refresh();
        }
        if (noClip)
            ImGui::PopItemFlag();

        ImGui::Spacing();

        ImGui::TextUnformatted("Audio Tracks:");
        uint32_t audTrackIdx = 1;
        for (auto track = g_mtAudReader->TrackListBegin(); track != g_mtAudReader->TrackListEnd(); track++)
        {
            ostringstream oss;
            oss << "Track#" << audTrackIdx++ << "{ 'clips': [";
            for (auto clIter = (*track)->ClipListBegin(); clIter != (*track)->ClipListEnd();)
            {
                oss << "Clip#" << (*clIter)->Id() << ":{'tlOff':" << (*clIter)->Start()
                    << ", 'off0':" << (*clIter)->StartOffset() << ", 'off1':" << (*clIter)->EndOffset()
                    << ", 'dur':" << (*clIter)->Duration() << "}";
                clIter++;
                if (clIter != (*track)->ClipListEnd())
                    oss << ", ";
            }
            oss << "], 'overlaps': [";
            for (auto ovIter = (*track)->OverlapListBegin(); ovIter != (*track)->OverlapListEnd();)
            {
                oss << "Overlap#" << (*ovIter)->Id() << ":{'start':" << (*ovIter)->Start()
                    << ", 'dur':" << (*ovIter)->Duration() << "}";
                ovIter++;
                if (ovIter != (*track)->OverlapListEnd())
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
            if (s_addClipOptSelIdx == g_mtAudReader->TrackCount())
            {
                int64_t trackId = g_idIndex++;
                if (!g_mtAudReader->AddTrack(trackId))
                {
                    Log(Error) << "FAILED to 'AddTrack'! Message is '" << g_mtAudReader->GetError() << "'." << endl;
                }
            }
            AudioTrackHolder hTrack = g_mtAudReader->GetTrackByIndex(s_addClipOptSelIdx);
            MediaParserHolder hParser = CreateMediaParser();
            if (!hParser->Open(filePathName))
                throw std::runtime_error(hParser->GetError());
            int64_t clipId = g_idIndex++;
            hTrack->AddNewClip(
                clipId, hParser,
                (int64_t)(s_addClipStart*1000), (int64_t)(s_addClipStartOffset*1000), (int64_t)(s_addClipEndOffset*1000));
            g_mtAudReader->Refresh();
            s_addClipOptSelIdx = g_mtAudReader->TrackCount();
            s_addClipStart = 0;
            s_addClipStartOffset = 0;
            s_addClipEndOffset = 0;
            g_audrnd->Flush();
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
