#include <imgui.h>
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
#include "AudioRender.h"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using namespace MediaCore;
using Clock = chrono::steady_clock;

static atomic_int64_t g_idIndex{1};

static MultiTrackAudioReader::Holder g_mtAudReader;
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
    SimplePcmStream(MultiTrackAudioReader::Holder audrdr) : m_audrdr(audrdr) {}

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
                bool eof;
                if (!m_audrdr->ReadAudioSamples(amat, eof))
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
    MultiTrackAudioReader::Holder m_audrdr;
    ImGui::ImMat m_amat;
    uint32_t m_readPosInAmat{0};
    std::mutex m_amatLock;
};
static SimplePcmStream* g_pcmStream = nullptr;


// Application Framework Functions
static void MultiTrackAudioReader_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    MediaReader::GetDefaultLogger()
        ->SetShowLevels(INFO);
    MultiTrackAudioReader::GetLogger()
        ->SetShowLevels(DEBUG);
    AudioTrack::GetLogger()
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

    g_mtAudReader = MultiTrackAudioReader::CreateInstance();
    g_mtAudReader->Configure(c_audioRenderChannels, c_audioRenderSampleRate, "flt");
    g_mtAudReader->Start();

    g_pcmStream = new SimplePcmStream(g_mtAudReader);
    g_audrnd = AudioRender::CreateInstance();
    g_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, g_pcmStream);
}

static void MultiTrackAudioReader_Finalize(void** handle)
{
    if (g_audrnd)
    {
        g_audrnd->CloseDevice();
        AudioRender::ReleaseInstance(&g_audrnd);
    }
    if (g_pcmStream)
    {
        delete g_pcmStream;
        g_pcmStream = nullptr;
    }
    g_mtAudReader = nullptr;

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

static uint32_t s_addClipOptSelIdx = 0;
static double s_addClipStart = 0;
static double s_addClipStartOffset = 0;
static double s_addClipEndOffset = 0;
static uint32_t s_remTrackOptSelIdx = 0;
static uint32_t s_clipOpTrackSelIdx = 0;
static uint32_t s_clipOpClipSelIdx = 0;
static double s_changeClipStart = 0;
static double s_changeClipStartOffset = 0;
static double s_changeClipEndOffset = 0;
static bool isOpenMasterWindow = false;
static bool isOpenTrackWindow = false;
static int64_t currTrackId = 0;

void DrawAudioEffectSettingsWindow(AudioEffectFilter::Holder aeFilter)
{
    ostringstream labeloss;
    string label;
    float value, valMin, valMax;

    // Volume
    if (aeFilter->HasFilter(AudioEffectFilter::VOLUME))
    {
        ImGui::Spacing();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Volume settings");
        valMin = 0; valMax = 1.5;
        labeloss.str(""); labeloss << "Vol";
        label = labeloss.str();
        AudioEffectFilter::VolumeParams volParams = aeFilter->GetVolumeParams();
        value = volParams.volume;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            volParams.volume = value;
            aeFilter->SetVolumeParams(&volParams);
        }
        ImGui::EndGroup();
    }

    // Equalizer
    if (aeFilter->HasFilter(AudioEffectFilter::EQUALIZER))
    {
        ImGui::Spacing();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Equalizer settings");
        AudioEffectFilter::EqualizerBandInfo bandInfo = aeFilter->GetEqualizerBandInfo();
        valMin = -12; valMax = 12;
        for (int i = 0; i < bandInfo.bandCount; i++)
        {
            if (i > 0)
                ImGui::SameLine();
            labeloss.str(""); labeloss << bandInfo.centerFreqList[i];
            label = labeloss.str();
            AudioEffectFilter::EqualizerParams equalizerParams = aeFilter->GetEqualizerParamsByIndex(i);
            value = equalizerParams.gain;
            if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
            {
                equalizerParams.gain = value;
                aeFilter->SetEqualizerParamsByIndex(&equalizerParams, i);
            }
        }
        ImGui::EndGroup();
    }

    // Compressor
    if (aeFilter->HasFilter(AudioEffectFilter::COMPRESSOR))
    {
        ImGui::Spacing();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Compressor settings");
        AudioEffectFilter::CompressorParams compressorParams = aeFilter->GetCompressorParams();
        labeloss.str(""); labeloss << "Thresh";
        label = labeloss.str();
        valMin = 0.00097563; valMax = 1;
        value = compressorParams.threshold;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            compressorParams.threshold = value;
            aeFilter->SetCompressorParams(&compressorParams);
        }
        ImGui::SameLine();
        labeloss.str(""); labeloss << "Ratio";
        label = labeloss.str();
        valMin = 1; valMax = 20;
        value = compressorParams.ratio;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            compressorParams.ratio = value;
            aeFilter->SetCompressorParams(&compressorParams);
        }
        ImGui::SameLine();
        labeloss.str(""); labeloss << "LvlIn";
        label = labeloss.str();
        valMin = 0.015625; valMax = 64;
        value = compressorParams.levelIn;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            compressorParams.levelIn = value;
            aeFilter->SetCompressorParams(&compressorParams);
        }
        ImGui::EndGroup();
    }

    // Gate
    if (aeFilter->HasFilter(AudioEffectFilter::GATE))
    {
        ImGui::Spacing();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Gate settings");
        AudioEffectFilter::GateParams gateParams = aeFilter->GetGateParams();
        labeloss.str(""); labeloss << "Thresh";
        label = labeloss.str();
        valMin = 0; valMax = 1;
        value = gateParams.threshold;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            gateParams.threshold = value;
            aeFilter->SetGateParams(&gateParams);
        }
        ImGui::SameLine();
        labeloss.str(""); labeloss << "Ratio";
        label = labeloss.str();
        valMin = 1; valMax = 9000;
        value = gateParams.ratio;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            gateParams.ratio = value;
            aeFilter->SetGateParams(&gateParams);
        }
        ImGui::EndGroup();
    }

    // Limiter
    if (aeFilter->HasFilter(AudioEffectFilter::LIMITER))
    {
        ImGui::Spacing();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Limiter settings");
        AudioEffectFilter::LimiterParams limiterParams = aeFilter->GetLimiterParams();
        labeloss.str(""); labeloss << "limit";
        label = labeloss.str();
        value = limiterParams.limit;
        valMin = 0.0625; valMax = 1;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            limiterParams.limit = value;
            aeFilter->SetLimiterParams(&limiterParams);
        }
        ImGui::EndGroup();
    }

    // Pan
    if (aeFilter->HasFilter(AudioEffectFilter::PAN))
    {
        ImGui::Spacing();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Pan settings");
        AudioEffectFilter::PanParams panParams = aeFilter->GetPanParams();
        valMin = 0; valMax = 1;
        labeloss.str(""); labeloss << "X";
        label = labeloss.str();
        value = panParams.x;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            panParams.x = value;
            aeFilter->SetPanParams(&panParams);
        }
        ImGui::SameLine();
        labeloss.str(""); labeloss << "Y";
        label = labeloss.str();
        value = panParams.y;
        if (ImGui::VSliderFloat(label.c_str(), ImVec2(24, 96), &value, valMin, valMax, "%.1f"))
        {
            panParams.y = value;
            aeFilter->SetPanParams(&panParams);
        }
        ImGui::EndGroup();
    }
}

static bool MultiTrackAudioReader_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        // control line #1
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
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("Start");
        ImGui::SameLine();
        ImGui::InputDouble("##Start", &s_addClipStart);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("ClipStartOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##ClipStartOffset", &s_addClipStartOffset);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("ClipEndOffset");
        ImGui::SameLine();
        ImGui::InputDouble("##ClipEndOffset", &s_addClipEndOffset);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp3 *.m4a *.wav *.mp4 *.mov *.mkv *.webm *.avi){.mp3,.m4a,.wav,.mp4,.mov,.mkv,.webm,.avi,.MP3,.M4A,.WAV.MP4,.MOV,.MKV,WEBM,.AVI},.*";
            IGFD::FileDialogConfig config;
			config.path = "/mnt/data2/video/hd/";
            config.countSelectionMax = 1;
			config.flags = ImGuiFileDialogFlags_OpenFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开音频文件", 
                                                    filters, 
                                                    config);
        }

        // control line #2
        ImGui::Spacing();
        vector<string> selectTrackOpts(trackNames);
        if (selectTrackOpts.empty())
            selectTrackOpts.push_back("<No track>");
        bool noTrack = trackNames.empty();
        ImGui::PushItemWidth(100);
        ImGui::PushItemWidth(100);
        if (ImGui::BeginCombo("##MovClipSelTrackOptions", selectTrackOpts[s_clipOpTrackSelIdx].c_str()))
        {
            for (uint32_t i = 0; i < selectTrackOpts.size(); i++)
            {
                string& item = selectTrackOpts[i];
                const bool isSelected = s_clipOpTrackSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_clipOpTrackSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 10);
        vector<string> clipNames;
        if (!noTrack)
        {
            AudioTrack::Holder hTrack = g_mtAudReader->GetTrackByIndex(s_clipOpTrackSelIdx);
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
        if (s_clipOpClipSelIdx >= clipSelOpts.size())
            s_clipOpClipSelIdx = clipSelOpts.size()-1;
        if (ImGui::BeginCombo("##MovClipSelClipOptions", clipSelOpts[s_clipOpClipSelIdx].c_str()))
        {
            AudioTrack::Holder hTrack = g_mtAudReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            auto clipIter = hTrack->ClipListBegin();
            for (uint32_t i = 0; i < clipSelOpts.size(); i++)
            {
                string& item = clipSelOpts[i];
                const bool isSelected = s_clipOpClipSelIdx == i;
                if (ImGui::Selectable(item.c_str(), isSelected))
                    s_clipOpClipSelIdx = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                clipIter++;
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noClip);
        if (ImGui::Button("Remove Clip"))
        {
            AudioTrack::Holder hTrack = g_mtAudReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            hTrack->RemoveClipByIndex(s_clipOpClipSelIdx);
            g_mtAudReader->Refresh();
            s_clipOpClipSelIdx = 0;
            g_audrnd->Flush();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 20);
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("tloff");
        ImGui::SameLine();
        ImGui::InputDouble("##tloff", &s_changeClipStart);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noClip);
        if (ImGui::Button("Move Clip"))
        {
            AudioTrack::Holder hTrack = g_mtAudReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            AudioClip::Holder hClip = hTrack->GetClipByIndex(s_clipOpClipSelIdx);
            hTrack->MoveClip(hClip->Id(), (int64_t)(s_changeClipStart*1000));
            g_mtAudReader->Refresh();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 20);
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("off0");
        ImGui::SameLine();
        ImGui::InputDouble("##off0", &s_changeClipStartOffset);
        ImGui::SameLine(0, 10);
        ImGui::TextUnformatted("off1");
        ImGui::SameLine();
        ImGui::InputDouble("##off1", &s_changeClipEndOffset);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noClip);
        if (ImGui::Button("Change Clip Range"))
        {
            AudioTrack::Holder hTrack = g_mtAudReader->GetTrackByIndex(s_clipOpTrackSelIdx);
            AudioClip::Holder hClip = hTrack->GetClipByIndex(s_clipOpClipSelIdx);
            hTrack->ChangeClipRange(hClip->Id(), (int64_t)(s_changeClipStartOffset*1000), (int64_t)(s_changeClipEndOffset*1000));
            g_mtAudReader->Refresh();
        }
        ImGui::EndDisabled();

        // control line #3
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

        // control line #4
        ImGui::Spacing();
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
        ImGui::SameLine(0, 10);
        ImGui::BeginDisabled(noTrack);
        if (ImGui::Button("Remove Track"))
        {
            g_mtAudReader->RemoveTrackByIndex(s_remTrackOptSelIdx);
            s_remTrackOptSelIdx = 0;
            g_audrnd->Flush();
        }
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Dummy({10, 10});

        float currPos = g_audPos;
        int64_t dur = g_mtAudReader->Duration();
        if (ImGui::SliderFloat("Seek Pos", &currPos, 0, (float)dur/1000, "%.3f"))
        {
            if (g_isPlay)
                g_audrnd->Pause();
            g_audrnd->Flush();
            g_mtAudReader->SeekTo((int64_t)(currPos*1000));
            if (g_isPlay)
                g_audrnd->Resume();
        }

        ImGui::Spacing();
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

        // Audio effect setting window open buttons
        ImGui::Spacing();
        ImGui::BeginGroup();
        if (ImGui::Button("Master")) isOpenMasterWindow = true;
        if (isOpenMasterWindow)
        {
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Master audio effect settings", &isOpenMasterWindow);
            DrawAudioEffectSettingsWindow(g_mtAudReader->GetAudioEffectFilter());
            ImGui::End();
        }
        audTrackIdx = 1;
        for (auto track = g_mtAudReader->TrackListBegin(); track != g_mtAudReader->TrackListEnd(); track++)
        {
            ImGui::SameLine();
            oss.str(""); oss << "Track#" << audTrackIdx;
            if (ImGui::Button(oss.str().c_str()))
            {
                isOpenTrackWindow = true;
                currTrackId = audTrackIdx;
            }
            if (isOpenTrackWindow && audTrackIdx == currTrackId)
            {
                oss.str(""); oss << "Track#" << audTrackIdx << " audio effect settings";
                ImGui::SetNextWindowPos({0, 0});
                ImGui::SetNextWindowSize(io.DisplaySize);
                string wintitle = oss.str();
                ImGui::Begin(wintitle.c_str(), &isOpenTrackWindow);
                DrawAudioEffectSettingsWindow((*track)->GetAudioEffectFilter());
                ImGui::End();
            }
            audTrackIdx++;
        }
        ImGui::EndGroup();
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
            AudioTrack::Holder hTrack = g_mtAudReader->GetTrackByIndex(s_addClipOptSelIdx);
            MediaParser::Holder hParser = MediaParser::CreateInstance();
            if (!hParser->Open(filePathName))
                throw std::runtime_error(hParser->GetError());
            int64_t clipId = g_idIndex++;
            int64_t clipStart = (int64_t)(s_addClipStart*1000);
            int64_t clipEnd = clipStart+(int64_t)(hParser->GetMediaInfo()->duration*1000);
            hTrack->AddNewClip(
                clipId, hParser,
                clipStart, clipEnd, (int64_t)(s_addClipStartOffset*1000), (int64_t)(s_addClipEndOffset*1000));
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

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "MultiTrackAudioReaderTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;

    property.application.Application_Initialize = MultiTrackAudioReader_Initialize;
    property.application.Application_Finalize = MultiTrackAudioReader_Finalize;
    property.application.Application_Frame = MultiTrackAudioReader_Frame;
}