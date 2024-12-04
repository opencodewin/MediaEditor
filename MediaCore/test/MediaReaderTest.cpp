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
#include <MatUtilsImVecHelper.h>
#include "MediaReader.h"
#include "AudioRender.h"
#include "FFUtils.h"
#include "TextureManager.h"
#include "HwaccelManager.h"
#include "Logger.h"
#include "DebugHelper.h"

using namespace std;
using namespace MediaCore;
using namespace RenderUtils;
using namespace Logger;
using Clock = chrono::steady_clock;

static TextureManager::Holder g_txmgr;
static ManagedTexture::Holder g_tx;
static bool g_isOpening = false;
static bool g_isImageSequence = false;
static MediaParser::Holder g_mediaParser;
static bool g_videoOnly = false;
static bool g_audioOnly = false;
static bool g_useHwAccel = true;
static int32_t g_audioStreamCount = 0;
static int32_t g_chooseAudioIndex = -1;
// video
static MediaReader::Holder g_vidrdr;
static double g_playStartPos = 0.f;
static Clock::time_point g_playStartTp;
static bool g_isPlay = false;
static bool g_bInStepMode = false;
static bool g_isLongCacheDur = false;
static bool g_bIsSeeking = false;
static const pair<double, double> G_DurTable[] = {
    {  5, 1 },
    { 10, 2 },
};
static MatUtils::Size2i g_v2DisplayViewSize = { 800, 450 };
// audio
static MediaReader::Holder g_audrdr;
static AudioRender* g_audrnd = nullptr;
const int c_audioRenderChannels = 2;
const int c_audioRenderSampleRate = 44100;
const AudioRender::PcmFormat c_audioRenderFormat = AudioRender::PcmFormat::FLOAT32;
static double g_audPos = 0;
// dump pcm for debug
bool g_dumpPcm = false;
FILE* g_fpPcmFile = NULL;

const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";

class SimplePcmStream : public AudioRender::ByteStream
{
public:
    SimplePcmStream(MediaReader::Holder audrdr) : m_audrdr(audrdr) {}

    uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override
    {
        if (!m_audrdr)
            return 0;
        uint32_t readSize = buffSize;
        int64_t pos;
        bool eof;
        if (!m_audrdr->ReadAudioSamples(buff, readSize, pos, eof, blocking))
            return 0;
        g_audPos = (double)pos/1000;
        if (g_fpPcmFile)
            fwrite(buff, 1, readSize, g_fpPcmFile);
        return readSize;
    }

    void Flush() override {}

    bool GetTimestampMs(int64_t& ts) override
    {
        return false;
    }

private:
    MediaReader::Holder m_audrdr;
};
static SimplePcmStream* g_pcmStream = nullptr;


// Application Framework Functions
static void MediaReader_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    MediaParser::GetLogger()
        ->SetShowLevels(INFO);
    MediaReader::GetDefaultLogger()
        ->SetShowLevels(INFO);
    g_txmgr = TextureManager::CreateInstance();
    g_txmgr->SetLogLevel(INFO);

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

    g_audrdr = MediaReader::CreateInstance();
    g_audrdr->SetLogLevel(INFO);

    g_pcmStream = new SimplePcmStream(g_audrdr);
    g_audrnd = AudioRender::CreateInstance();
    g_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, g_pcmStream);
    if (g_dumpPcm)
        g_fpPcmFile = fopen("MediaReaderTest_PcmDump.pcm", "wb");

    HwaccelManager::GetDefaultInstance()->Init();
}

static void MediaReader_Finalize(void** handle)
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
    g_vidrdr = nullptr;
    g_audrdr = nullptr;

#ifdef USE_PLACES_FEATURE
	// save bookmarks
	ofstream configFileWriter(c_bookmarkPath, ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializePlaces();
		configFileWriter.close();
	}
#endif

    g_tx = nullptr;
    g_txmgr = nullptr;

    if (g_dumpPcm)
        fclose(g_fpPcmFile);
}

static bool MediaReader_Frame(void * handle, bool app_will_quit)
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

        const bool bHasVideo = g_vidrdr && g_vidrdr->IsOpened();
        const bool bHasAudio = g_audrdr && g_audrdr->IsOpened();
        bool isFileOpened = bHasVideo || bHasAudio;
        ImGui::SameLine();
        ImGui::BeginDisabled(!isFileOpened || g_audioStreamCount < 2);
        ImGui::PushItemWidth(100);
        ostringstream audstmOptTagOss;
        audstmOptTagOss << g_chooseAudioIndex;
        string audstmOptTag = audstmOptTagOss.str();
        if (ImGui::BeginCombo("##AudstmSelTrackOptions", audstmOptTag.c_str()))
        {
            for (int32_t i = 0; i < g_audioStreamCount; i++)
            {
                audstmOptTagOss.str("");
                audstmOptTagOss << i;
                audstmOptTag = audstmOptTagOss.str();
                const bool isSelected = g_chooseAudioIndex == i;
                if (ImGui::Selectable(audstmOptTag.c_str(), isSelected))
                {
                    g_chooseAudioIndex = i;
                    g_audrdr->Stop();
                    g_audrdr->ConfigAudioReader(c_audioRenderChannels, c_audioRenderSampleRate, "flt", g_chooseAudioIndex);
                    g_audrdr->Start();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::EndDisabled();

        bool isForward;
        float playPos;
        float mediaDur;
        if (g_vidrdr && bHasVideo)
        {
            isForward = g_vidrdr->IsDirectionForward();
            const VideoStream* vstminfo = g_vidrdr->GetVideoStream();
            float vidDur = vstminfo ? (float)vstminfo->duration : 0;
            mediaDur = vidDur;
        }
        if (g_bIsSeeking)
        {
            playPos = g_playStartPos;
        }
        else if (bHasAudio)
        {
            if (!g_vidrdr || !bHasVideo)
            {
                isForward = g_audrdr->IsDirectionForward();
                const AudioStream* astminfo = g_audrdr->GetAudioStream();
                float audDur = astminfo ? (float)astminfo->duration : 0;
                mediaDur = audDur;
            }
            playPos = g_isPlay ? g_audPos : g_playStartPos;
        }
        else
        {
            double elapsedTime = chrono::duration_cast<chrono::duration<double>>((Clock::now()-g_playStartTp)).count();
            playPos = g_isPlay ? (isForward ? g_playStartPos+elapsedTime : g_playStartPos-elapsedTime) : g_playStartPos;
        }
        if (playPos < 0) playPos = 0;
        if (playPos > mediaDur) playPos = mediaDur;
        const auto playMts = (int64_t)(playPos*1000);

        if (g_bIsSeeking && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {  // quit seeking
            g_bIsSeeking = false;
            if (bHasVideo)
                g_vidrdr->SeekTo(playMts, false);
            if (bHasAudio)
            {
                g_audPos = g_playStartPos;
                g_audrdr->SeekTo(playMts);
                if (g_isPlay)
                {
                    g_audrnd->Flush();
                    g_audrnd->Resume();
                }
            }
        }

        ImGui::BeginDisabled(!isFileOpened);
        ImGui::SameLine();
        string playBtnLabel = g_isPlay ? "Pause" : "Play ";
        if (ImGui::Button(playBtnLabel.c_str()))
        {
            g_isPlay = !g_isPlay;
            if (g_isPlay)
            {
                if (g_vidrdr && g_vidrdr->IsSuspended())
                    g_vidrdr->Wakeup();
                g_playStartTp = Clock::now();
                if (g_bInStepMode)
                {
                    if (bHasAudio)
                    {
                        if (isForward != g_audrdr->IsDirectionForward())
                            g_audrdr->SetDirection(isForward);
                        g_audrdr->SeekTo(playMts);
                    }
                    g_bInStepMode = false;
                }
                if (bHasAudio)
                    g_audrnd->Resume();
            }
            else
            {
                g_playStartPos = playPos;
                if (bHasAudio)
                    g_audrnd->Pause();
            }
        }

        ImGui::SameLine();
        string dirBtnLabel = isForward ? "Backword" : "Forward";
        if (ImGui::Button(dirBtnLabel.c_str()))
        {
            bool notForward = !isForward;
            if (g_vidrdr && bHasVideo)
            {
                g_vidrdr->SetDirection(notForward);
                g_playStartPos = playPos;
                g_playStartTp = Clock::now();
                g_vidrdr->SeekTo(playMts);
            }
            if (bHasAudio)
            {
                g_audrdr->SetDirection(notForward);
            }
        }

        ImGui::SameLine();
        string suspendBtnLabel = g_vidrdr && g_vidrdr->IsSuspended() ? "WakeUp" : "Suspend";
        if (ImGui::Button(suspendBtnLabel.c_str()))
        {
            if (g_vidrdr->IsSuspended())
                g_vidrdr->Wakeup();
            else
                g_vidrdr->Suspend();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        string cdurBtnLabel = g_isLongCacheDur ? "Short cache duration" : "Long cache duration";
        if (ImGui::Button(cdurBtnLabel.c_str()))
        {
            g_isLongCacheDur = !g_isLongCacheDur;
            if (g_isLongCacheDur)
                g_vidrdr->SetCacheDuration(G_DurTable[1].first, G_DurTable[1].second);
            else
                g_vidrdr->SetCacheDuration(G_DurTable[0].first, G_DurTable[0].second);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Audio Only", &g_audioOnly);
        ImGui::SameLine();
        ImGui::Checkbox("Video Only", &g_videoOnly);
        ImGui::SameLine();
        ImGui::Checkbox("Use HW Accelaration", &g_useHwAccel);

        ImGui::BeginDisabled(!isFileOpened);
        ImGui::Spacing();
        ImGui::TextUnformatted("Position:"); ImGui::SameLine();
        if (ImGui::SliderFloat("##PositionSlider", &playPos, 0, mediaDur, "%.3f"))
        {
            if (!g_bIsSeeking)
            {
                if (g_isPlay && bHasAudio)
                    g_audrnd->Pause();
                g_bIsSeeking = true;
            }
            int64_t seekPos = playPos*1000;
            if (bHasVideo)
                g_vidrdr->SeekTo(seekPos, true);
            g_playStartPos = playPos;
            g_playStartTp = Clock::now();
        } ImGui::SameLine(0, 20);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!bHasVideo);
        ImGui::TextUnformatted("Step:"); ImGui::SameLine();
        bool bDoStepNext = false;
        if (ImGui::Button(" < "))
        {
            if (g_isPlay)
            {
                g_isPlay = false;
                if (bHasAudio)
                    g_audrnd->Pause();
            }
            if (g_vidrdr->IsDirectionForward())
                g_vidrdr->SetDirection(false);
            g_bInStepMode = true;
            bDoStepNext = true;
        } ImGui::SameLine(0, 10);
        if (ImGui::Button(" > "))
        {
            if (g_isPlay)
            {
                g_isPlay = false;
                if (bHasAudio)
                    g_audrnd->Pause();
            }
            if (!g_vidrdr->IsDirectionForward())
                g_vidrdr->SetDirection(true);
            g_bInStepMode = true;
            bDoStepNext = true;
        }
        ImGui::EndDisabled();

        ImGui::Spacing();
        string imgTag;
        if (g_vidrdr && bHasVideo && !g_vidrdr->IsSuspended())
        {
            bool eof;
            ImGui::ImMat vmat;
            MediaCore::VideoFrame::Holder hVf;
            if (g_bInStepMode)
            {
                if (bDoStepNext)
                {
                    hVf = g_vidrdr->ReadNextVideoFrame(eof);
                    if (hVf)
                    {
                        g_playStartPos = (double)hVf->Pos()/1000;
                    }
                }
                imgTag = TimestampToString(g_playStartPos);
            }
            else
            {
                int64_t readPos = playMts;
                hVf = g_vidrdr->ReadVideoFrame(readPos, eof, false);
                if (g_bIsSeeking && !hVf)
                {
                    hVf = g_vidrdr->GetSeekingFlash();
                    if (!hVf)
                        Log(Error) << "No seeking flash available! playPos=" << playPos << endl;
                }
            }
            if (hVf)
            {
                Log(VERBOSE) << "Succeeded to read video frame @pos=" << playPos << "." << endl;
                hVf->GetMat(vmat);
                imgTag = TimestampToString(vmat.time_stamp);
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
                    Log(Error) << "WRONG snapshot format!" << endl;
                    imgValid = false;
                    imgTag += "(bad format)";
                }
                if (imgValid)
                {
                    if (!g_tx)
                    {
                        MatUtils::Size2i txSize(vmat.w, vmat.h);
                        g_tx = g_txmgr->CreateManagedTextureFromMat(vmat, txSize);
                        if (!g_tx)
                            Log(Error) << "FAILED to create ManagedTexture from ImMat! Error is '" << g_txmgr->GetError() << "'." << endl;
                    }
                    else
                    {
                        g_tx->RenderMatToTexture(vmat);
                    }
                }
            }
            else if (!g_bInStepMode)
            {
                // Log(Error) << "FAILED to read video frame @pos=" << playPos << ": " << g_vidrdr->GetError() << endl;
            }
        }
        // AddCheckPoint("ShowImage0");
        ImTextureID tid = g_tx ? g_tx->TextureID() : nullptr;
        if (tid)
        {
            ImVec2 v2DisplaySize = MatUtils::ToImVec2(g_v2DisplayViewSize);
            const auto v2TextureSize = g_tx->GetDisplaySize();
            if (fabs((float)v2TextureSize.x/v2TextureSize.y - v2DisplaySize.x/v2DisplaySize.y) > FLT_EPSILON)
            {
                if (v2TextureSize.x*v2DisplaySize.y > v2TextureSize.y*v2DisplaySize.x)
                    v2DisplaySize.y = v2TextureSize.y*v2DisplaySize.x/v2TextureSize.x;
                else
                    v2DisplaySize.x = v2TextureSize.x*v2DisplaySize.y/v2TextureSize.y;
            }
            ImGui::Image(tid, v2DisplaySize);
        }
        else
            ImGui::Dummy(MatUtils::ToImVec2(g_v2DisplayViewSize));
        // AddCheckPoint("ShowImage1");
        // LogCheckPointsTimeInfo();
        ImGui::TextUnformatted(imgTag.c_str());

        ImGui::Spacing();
        ostringstream oss;
        oss << "Audio pos: " << TimestampToString(g_audPos);
        string audTag = oss.str();
        ImGui::TextUnformatted(audTag.c_str());

        if (g_isOpening)
        {
            ostringstream oss;
            oss << "Opening '" << g_mediaParser->GetUrl() << "' ...";
            string txt = oss.str();
            ImGui::TextUnformatted(txt.c_str());

            if (g_mediaParser->CheckInfoReady(MediaParser::MEDIA_INFO))
            {
                if (g_mediaParser->HasVideo() && !g_audioOnly)
                {
                    if (g_mediaParser->IsImageSequence())
                        g_vidrdr = MediaReader::CreateImageSequenceInstance();
                    else
                        g_vidrdr = MediaReader::CreateVideoInstance();
                    g_vidrdr->SetLogLevel(INFO);
                    g_vidrdr->EnableHwAccel(g_useHwAccel);
                    g_vidrdr->Open(g_mediaParser);
                    g_vidrdr->ConfigVideoReader((uint32_t)g_v2DisplayViewSize.x, (uint32_t)g_v2DisplayViewSize.y,
                            IM_CF_RGBA, IM_DT_INT8, IM_INTERPOLATE_AREA, HwaccelManager::GetDefaultInstance());
                    // g_vidrdr->ConfigVideoReader(1.0f, 1.0f);
                    if (playMts > 0)
                        g_vidrdr->SeekTo(playMts);
                    g_vidrdr->Start();
                }
                if (g_mediaParser->HasAudio() && !g_videoOnly)
                {
                    g_audrdr->Open(g_mediaParser);
                    auto mediaInfo = g_mediaParser->GetMediaInfo();
                    for (auto stream : mediaInfo->streams)
                    {
                        if (stream->type == MediaType::AUDIO)
                            g_audioStreamCount++;
                    }
                    g_chooseAudioIndex = 0;
                    g_audrdr->ConfigAudioReader(c_audioRenderChannels, c_audioRenderSampleRate, "flt", g_chooseAudioIndex);
                    if (playMts > 0)
                        g_audrdr->SeekTo(playMts);
                    g_audrdr->Start();
                }
                if (!bHasVideo && !bHasAudio)
                    Log(Error) << "Neither VIDEO nor AUDIO stream is ready for playback!" << endl;
                g_playStartTp = Clock::now();
                g_isOpening = false;
            }
        }
        ImGui::End(); // 'MainWindow' ends here
    }

    // open file dialog
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            if (g_vidrdr) g_vidrdr->Close();
            g_audrdr->Close();
            g_audrnd->Flush();
            g_audPos = 0;
            g_playStartPos = 0;
            g_audioStreamCount = 0;
            g_chooseAudioIndex = -1;
            if (g_tx) g_tx = nullptr;
            g_isLongCacheDur = false;
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_mediaParser = MediaParser::CreateInstance();
            if (g_isImageSequence)
                g_mediaParser->OpenImageSequence({25, 1}, filePathName, ".+_([[:digit:]]{1,})\\.png", false, true);
            else
                g_mediaParser->Open(filePathName);
            g_isOpening = true;
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

    g_txmgr->UpdateTextureState();
    // Log(DEBUG) << g_txmgr.get() << endl;
    return app_done;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "MediaReaderTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
    property.application.Application_Initialize = MediaReader_Initialize;
    property.application.Application_Finalize = MediaReader_Finalize;
    property.application.Application_Frame = MediaReader_Frame;
}
