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
#include "MediaReader.h"
#include "AudioRender.hpp"
#include "FFUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using Clock = chrono::steady_clock;

static bool s_audioOnly = false;
// video
static MediaReader* g_vidrdr = nullptr;
static double g_playStartPos = 0.f;
static Clock::time_point g_playStartTp;
static bool g_isPlay = false;
static bool g_isLongCacheDur = false;
static const pair<double, double> G_DurTable[] = {
    {  5, 1 },
    { 10, 2 },
};
static ImTextureID g_imageTid;
static ImVec2 g_imageDisplaySize = { 640, 360 };
// audio
static MediaReader* g_audrdr = nullptr;
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
    SimplePcmStream(MediaReader* audrdr) : m_audrdr(audrdr) {}

    uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override
    {
        if (!m_audrdr)
            return 0;
        uint32_t readSize = buffSize;
        double pos;
        bool eof;
        if (!m_audrdr->ReadAudioSamples(buff, readSize, pos, eof, blocking))
            return 0;
        g_audPos = pos;
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
    MediaReader* m_audrdr;
};
static SimplePcmStream* g_pcmStream = nullptr;


// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "MediaReaderTest";
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
    GetMediaReaderLogger()
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

    g_vidrdr = CreateMediaReader();
    g_audrdr = CreateMediaReader();

    g_pcmStream = new SimplePcmStream(g_audrdr);
    g_audrnd = CreateAudioRender();
    g_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, g_pcmStream);
    if (g_dumpPcm)
        g_fpPcmFile = fopen("MediaReaderTest_PcmDump.pcm", "wb");
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
    ReleaseMediaReader(&g_vidrdr);
    ReleaseMediaReader(&g_audrdr);
    if (g_imageTid)
    {
        ImGui::ImDestroyTexture(g_imageTid);
        g_imageTid = nullptr;
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
    if (g_dumpPcm)
        fclose(g_fpPcmFile);
}

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

        ImGui::SameLine();

        bool isForward;
        float playPos;
        float mediaDur;
        if (g_vidrdr->IsOpened())
        {
            isForward = g_vidrdr->IsDirectionForward();
            const MediaInfo::VideoStream* vstminfo = g_vidrdr->GetVideoStream();
            float vidDur = vstminfo ? (float)vstminfo->duration : 0;
            mediaDur = vidDur;
        }
        if (g_audrdr->IsOpened())
        {
            if (!g_vidrdr->IsOpened())
            {
                isForward = g_audrdr->IsDirectionForward();
                const MediaInfo::AudioStream* astminfo = g_audrdr->GetAudioStream();
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

        string playBtnLabel = g_isPlay ? "Pause" : "Play ";
        if (ImGui::Button(playBtnLabel.c_str()))
        {
            g_isPlay = !g_isPlay;
            if (g_isPlay)
            {
                g_vidrdr->Wakeup();
                g_playStartTp = Clock::now();
                if (g_audrdr->IsOpened())
                    g_audrnd->Resume();
            }
            else
            {
                g_playStartPos = playPos;
                if (g_audrdr->IsOpened())
                    g_audrnd->Pause();
                g_vidrdr->Suspend();
            }
        }

        ImGui::SameLine();

        string dirBtnLabel = isForward ? "Backword" : "Forward";
        if (ImGui::Button(dirBtnLabel.c_str()))
        {
            bool notForward = !isForward;
            if (g_vidrdr->IsOpened())
            {
                g_vidrdr->SetDirection(notForward);
                g_playStartPos = playPos;
                g_playStartTp = Clock::now();
            }
            if (g_audrdr->IsOpened())
            {
                g_audrdr->SetDirection(notForward);
            }
        }

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
        ImGui::Checkbox("Audio Only", &s_audioOnly);

        ImGui::Spacing();

        if (ImGui::SliderFloat("Position", &playPos, 0, mediaDur, "%.3f"))
        {
            if (g_vidrdr->IsOpened())
                g_vidrdr->SeekTo(playPos);
            if (g_audrdr->IsOpened())
                g_audrdr->SeekTo(playPos);
            g_playStartPos = playPos;
            g_playStartTp = Clock::now();
        }

        ImGui::Spacing();

        if (g_vidrdr->IsOpened())
        {
            bool eof;
            ImGui::ImMat vmat;
            if (g_vidrdr->ReadVideoFrame(playPos, vmat, eof))
            {
                string imgTag = TimestampToString(vmat.time_stamp);
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
                    ImGui::ImMatToTexture(vmat, g_imageTid);
                    if (g_imageTid) ImGui::Image(g_imageTid, g_imageDisplaySize);
                }
                else
                {
                    ImGui::Dummy(g_imageDisplaySize);
                }
                ImGui::TextUnformatted(imgTag.c_str());
            }
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
            g_vidrdr->Close();
            g_audrdr->Close();
            if (g_imageTid)
                ImGui::ImDestroyTexture(g_imageTid);
            g_imageTid = nullptr;
            g_isLongCacheDur = false;
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (!s_audioOnly)
            {
                g_vidrdr->Open(filePathName);
                g_vidrdr->ConfigVideoReader((uint32_t)g_imageDisplaySize.x, (uint32_t)g_imageDisplaySize.y);
                g_vidrdr->Start();
                g_audrdr->Open(g_vidrdr->GetMediaParser());
            }
            else
            {
                g_audrdr->Open(filePathName);
            }
            g_audrdr->ConfigAudioReader(c_audioRenderChannels, c_audioRenderSampleRate, "flt");
            g_audrdr->Start();
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
