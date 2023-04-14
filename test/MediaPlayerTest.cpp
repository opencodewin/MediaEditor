#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <imgui_extra_widget.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
// #include <Lut3D.h>
#endif
#include "MediaReader.h"
#include "AudioRender.h"
#include "FFUtils.h"
#include "Log.h"

static std::string ini_file = "Media_Player.ini";
static std::string bookmark_path = "bookmark.ini";
static bool g_isOpening = false;
static bool g_isOpened = false;
static bool g_useHwAccel = true;
static int32_t g_audioStreamCount = 0;
static int32_t g_chooseAudioIndex = -1;
static MediaCore::MediaParser::Holder g_mediaParser;

// video
static MediaCore::MediaReader::Holder g_vidrdr;
static double g_playStartPos = 0.f;
static std::chrono::steady_clock::time_point g_playStartTp;
static bool g_isPlay = false;
static ImTextureID g_texture = 0;
// audio
static MediaCore::MediaReader::Holder g_audrdr;
static MediaCore::AudioRender* g_audrnd = nullptr;
const int c_audioRenderChannels = 2;
const int c_audioRenderSampleRate = 44100;
const MediaCore::AudioRender::PcmFormat c_audioRenderFormat = MediaCore::AudioRender::PcmFormat::FLOAT32;
static double g_audPos = 0;

static int lstack = 0, rstack = 0;
static int lcount = 0, rcount = 0;
static float audio_decibel_left = 0, audio_decibel_right = 0;

class SimplePcmStream : public MediaCore::AudioRender::ByteStream
{
public:
    SimplePcmStream(MediaCore::MediaReader::Holder audrdr) : m_audrdr(audrdr) {}

    uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override
    {
        if (!m_audrdr)
            return 0;
        uint32_t readSize = buffSize;
        double pos;
        bool eof;
        if (!m_audrdr->ReadAudioSamples(buff, readSize, pos, eof, blocking))
            return 0;
        //
        int channdels = m_audrdr->GetAudioOutChannels();
        size_t sample_size = readSize / channdels / sizeof(float);
        ImGui::ImMat mat;
        int fft_size = sample_size  > 256 ? 256 : sample_size > 128 ? 128 : 64;
        mat.create_type(fft_size, 1, channdels, IM_DT_FLOAT32);
        float * data = (float *)buff;
        for (int x = 0; x < mat.w; x++)
        {
            for (int i = 0; i < mat.c; i++)
            {
                mat.at<float>(x, 0, i) = data[x * mat.c + i];
            }
        }
        for (int c = 0; c < mat.c; c++)
        {
            auto channel_data = mat.channel(c);
            ImGui::ImRFFT((float *)channel_data.data, channel_data.w, true);
            if (c == 0)
            {
                audio_decibel_left = ImGui::ImDoDecibel((float*)channel_data.data, channel_data.w);
            }
            else if (c == 1)
            {
                audio_decibel_right = ImGui::ImDoDecibel((float*)channel_data.data, channel_data.w);
            }
        }

        g_audPos = pos;
        return readSize;
    }

    void Flush() override {}

    bool GetTimestampMs(int64_t& ts) override
    {
        return false;
    }

private:
    MediaCore::MediaReader::Holder m_audrdr;
};
static SimplePcmStream* g_pcmStream = nullptr;


// Application Framework Functions
static void MediaPlayer_Initialize(void** handle)
{
#ifdef USE_BOOKMARK
	// load bookmarks
	std::ifstream docFile(bookmark_path, std::ios::in);
	if (docFile.is_open())
	{
		std::stringstream strStream;
		strStream << docFile.rdbuf();//read the file
		ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
		docFile.close();
	}
#endif

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
    g_vidrdr = MediaCore::MediaReader::CreateVideoInstance();
    g_audrdr = MediaCore::MediaReader::CreateInstance();

    g_pcmStream = new SimplePcmStream(g_audrdr);
    g_audrnd = MediaCore::AudioRender::CreateInstance();
    g_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, g_pcmStream);
}

static void MediaPlayer_Finalize(void** handle)
{
    if (g_audrnd) { g_audrnd->CloseDevice(); MediaCore::AudioRender::ReleaseInstance(&g_audrnd); }
    if (g_pcmStream) { delete g_pcmStream; g_pcmStream = nullptr; }
    if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
    g_vidrdr = nullptr;
    g_audrdr = nullptr;

#ifdef USE_BOOKMARK
	// save bookmarks
	std::ofstream configFileWriter(bookmark_path, std::ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif
}

static bool MediaPlayer_Frame(void * handle, bool app_will_quit)
{
    static bool show_ctrlbar = true;
    static bool show_log_window = false; 
    static bool convert_hdr = false;
    static bool has_hdr = false;
    static bool muted = false;
    static bool full_screen = false;
    static int ctrlbar_hide_count = 0;
    bool app_done = false;
    auto& io = ImGui::GetIO();
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    float playPos = 0;
    bool isForward = false;
    float mediaDur = 0;
    Log::Render();

    if (g_vidrdr->IsOpened())
    {
        isForward = g_vidrdr->IsDirectionForward();
        const MediaCore::VideoStream* vstminfo = g_vidrdr->GetVideoStream();
        float vidDur = vstminfo ? (float)vstminfo->duration : 0;
        mediaDur = vidDur;
    }
    if (g_audrdr->IsOpened())
    {
        if (!g_vidrdr->IsOpened())
        {
            isForward = g_audrdr->IsDirectionForward();
            const MediaCore::AudioStream* astminfo = g_audrdr->GetAudioStream();
            float audDur = astminfo ? (float)astminfo->duration : 0;
            mediaDur = audDur;
        }
        playPos = g_isPlay ? g_audPos : g_playStartPos;
    }
    else
    {
        double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>((std::chrono::steady_clock::now()-g_playStartTp)).count();
        playPos = g_isPlay ? (isForward ? g_playStartPos+elapsedTime : g_playStartPos-elapsedTime) : g_playStartPos;
    }
    if (playPos < 0) playPos = 0;
    if (playPos > mediaDur) playPos = mediaDur;

    // Show PlayControl panel
    if (g_isOpened && (show_ctrlbar && io.FrameCountSinceLastInput))
    {
        ctrlbar_hide_count++;
        if (ctrlbar_hide_count >= 100)
        {
            ctrlbar_hide_count = 0;
            show_ctrlbar = false;
        }
    }
    if (io.FrameCountSinceLastInput == 0)
    {
        ctrlbar_hide_count = 0;
        show_ctrlbar = true;
    }

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.85f);
    ImVec2 panel_size(io.DisplaySize.x - 40.0, 120);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5);
    if (show_ctrlbar && ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        int i = ImGui::FindWindowByName("Control")->Size.x;
        ImGui::Indent((i - 32.0f) * 0.4f);
        ImVec2 size = ImVec2(32.0f, 32.0f); // Size of the image we want to make visible
        ImFont* font = ImGui::GetFont();
        float org_scale = font->Scale;
        // add open button
        ImGui::SetWindowFontScale(org_scale * 1.2);
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN, size))
        {
            //ImGui::ShowTooltipOnHover("Open Media File.");
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi){.mp4,.mov,.mkv,.webm,.avi,.MP4,.MOV,.MKV,WEBM,.AVI},.*";
			ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", 
                                                    filters, 
                                                    "/mnt/data2/video/hd/", 
                                                    1, 
                                                    nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark |
                                                    ImGuiFileDialogFlags_Modal);
        }
        ImGui::ShowTooltipOnHover("Open Media File.");
        // add play button
        ImGui::SameLine();
        ImGui::SetWindowFontScale(org_scale * 1.5);
        if (ImGui::Button(g_isOpened ? (g_isPlay ? ICON_FAD_PAUSE : ICON_FAD_PLAY) : ICON_FAD_PLAY, size))
        {
            g_isPlay = !g_isPlay;
            if (g_isPlay)
            {
                if (g_vidrdr->IsSuspended())
                    g_vidrdr->Wakeup();
                g_playStartTp = std::chrono::steady_clock::now();
                if (g_audrdr->IsOpened())
                    g_audrnd->Resume();
            }
            else
            {
                g_playStartPos = playPos;
                if (g_audrdr->IsOpened())
                    g_audrnd->Pause();
                audio_decibel_left = audio_decibel_right = 0;
            }
        }
        ImGui::ShowTooltipOnHover("Toggle Play/Pause.");
        ImGui::SameLine();
        if (ImGui::Button(g_isOpening ? muted ? ICON_FA_VOLUME_OFF : ICON_FA_VOLUME_HIGH : ICON_FA_VOLUME_HIGH, size))
        {
            muted = !muted;
        }
        ImGui::ShowTooltipOnHover("Toggle Audio Mute.");
        // add about button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_TAG, size))
        {
            ImGui::OpenPopup("##about", ImGuiPopupFlags_AnyPopup);
        }
        ImGui::ShowTooltipOnHover("Show About.");

        ImGui::SetWindowFontScale(org_scale);
        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("##about", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("ImGUI Media Player");
            ImGui::Separator();
            ImGui::Text("OpenCodeWin 2023");
            ImGui::Separator();
            int i = ImGui::GetCurrentWindow()->ContentSize.x;
            ImGui::Indent((i - 40.0f) * 0.5f);
            if (ImGui::Button("OK", ImVec2(40, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine(); ImGui::Dummy(size);
        // add software decode button
        ImGui::SameLine();
        if (ImGui::ToggleButton("SW", &g_useHwAccel, size * 0.75))
        {
            g_vidrdr->EnableHwAccel(g_useHwAccel);
        }
        ImGui::ShowTooltipOnHover("Software decoder");
        // add show log button
        ImGui::SameLine();
        ImGui::ToggleButton(ICON_FA_LIST_UL, &show_log_window, size * 0.75);
        ImGui::ShowTooltipOnHover("Show Log");
        // add button end

        // show time info
        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine();
        ImGui::Text("%s/%s", MillisecToString(playPos * 1000).c_str(), MillisecToString(mediaDur * 1000).c_str());

        ImGui::Unindent((i - 32.0f) * 0.4f);
        ImGui::Separator();

        float padding = style.FramePadding.x * 8;
        auto timescale_width = io.DisplaySize.x - padding;
        if (g_isOpened)
        {
            int decibel_left = audio_decibel_left;
            int decibel_right = audio_decibel_right;
            ImGui::UvMeter("##lhuvr", ImVec2(timescale_width, 10), &decibel_left, 0, 96, 200, &lstack, &lcount);
            ImGui::UvMeter("##rhuvr", ImVec2(timescale_width, 10), &decibel_right, 0, 96, 200, &rstack, &rcount);
        }
        else
        {
            int zero_channel_level = 0;
            ImGui::UvMeter("##lhuvr", ImVec2(timescale_width, 10), &zero_channel_level, 0, 96, 200);
            ImGui::UvMeter("##rhuvr", ImVec2(timescale_width, 10), &zero_channel_level, 0, 96, 200);
        }
        ImGui::Separator();

        ImGui::PushItemWidth(timescale_width);
        if (g_isOpened)
        {
            if (ImGui::SliderFloat("##timeline", &playPos, 0.f, mediaDur, "%.1f"))
            {
                if (g_vidrdr->IsOpened())
                    g_vidrdr->SeekTo(playPos);
                if (g_audrdr->IsOpened())
                    g_audrdr->SeekTo(playPos);
                g_playStartPos = playPos;
                g_playStartTp = std::chrono::steady_clock::now();
            }
        }
        else
        {
            float seek_t = 0;
            ImGui::SliderFloat("##timeline", &seek_t, 0.f, 0.f, "%.1f");
        }
        ImGui::PopItemWidth();
        ImGui::Separator();

        ImGui::End();
    }

    // handle key event
    if (g_isOpened && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space), false))
    {
        if (g_isPlay)
        {
            if (g_vidrdr->IsSuspended())
                    g_vidrdr->Wakeup();
            g_playStartTp = std::chrono::steady_clock::now();
            if (g_audrdr->IsOpened())
                g_audrnd->Resume();
        }
        else 
        {
            g_playStartPos = playPos;
            if (g_audrdr->IsOpened())
                g_audrnd->Pause();
        }
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        if (g_isOpened)
        {
            g_vidrdr->Close();
            g_audrdr->Close();
            g_isOpened = false;
        }
        app_done = true;
    }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos -= 5e+9;
    //     if (g_pos < 0) g_pos = 0;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos += 5e+9;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos -= 1e+10;
    //     if (g_pos < 0) g_pos = 0;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow), true))
    // {
    //     auto g_pos = g_player->position();
    //     if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
    //     g_pos += 1e+10;
    //     g_player->seek(g_pos);
    // }
    // if (g_player->IsOpened() && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z), true))
    // {
    //     g_player->step();
    // }

    // handle screen double click event
    if (ImGui::IsMouseDoubleClicked(0) && !ImGui::IsAnyItemHovered())
    {
        full_screen = !full_screen;
        Application_FullScreen(full_screen);
    }

    if (g_vidrdr->IsOpened() && !g_vidrdr->IsSuspended())
    {
        bool eof;
        ImGui::ImMat vmat;
        if (g_vidrdr->ReadVideoFrame(playPos, vmat, eof))
        {
            bool imgValid = true;
            if (vmat.empty())
            {
                imgValid = false;
            }
            if (imgValid &&
                ((vmat.color_format != IM_CF_RGBA && vmat.color_format != IM_CF_ABGR) ||
                vmat.type != IM_DT_INT8 ||
                (vmat.device != IM_DD_CPU && vmat.device != IM_DD_VULKAN)))
            {
                imgValid = false;
            }
            if (imgValid) ImGui::ImMatToTexture(vmat, g_texture);
        }
        else
        {
        }
    }
    if (g_isOpening)
    {
        if (g_mediaParser->CheckInfoReady(MediaCore::MediaParser::MEDIA_INFO))
        {
            if (g_mediaParser->HasVideo())
            {
                g_vidrdr->EnableHwAccel(g_useHwAccel);
                g_vidrdr->Open(g_mediaParser);
                g_vidrdr->ConfigVideoReader(1.0f, 1.0f);
                g_vidrdr->Start();
            }
            if (g_mediaParser->HasAudio())
            {
                g_audrdr->Open(g_mediaParser);
                auto mediaInfo = g_mediaParser->GetMediaInfo();
                for (auto stream : mediaInfo->streams)
                {
                    if (stream->type == MediaCore::MediaType::AUDIO)
                        g_audioStreamCount++;
                }
                g_chooseAudioIndex = 0;
                g_audrdr->ConfigAudioReader(c_audioRenderChannels, c_audioRenderSampleRate, "flt", g_chooseAudioIndex);
                g_audrdr->Start();
            }
            if (!g_vidrdr->IsOpened() && !g_audrdr->IsOpened())
            {
                g_isOpened = false;
                //Log(Error) << "Neither VIDEO nor AUDIO stream is ready for playback!" << endl;
            }
            else
                g_isOpened = true;
            g_isOpening = false;
        }
    }

    // file dialog
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.75;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            g_vidrdr->Close();
            g_audrdr->Close();
            g_audioStreamCount = 0;
            g_chooseAudioIndex = -1;
            if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_mediaParser = MediaCore::MediaParser::CreateInstance();
            g_mediaParser->Open(filePathName);
            g_isOpened = false;
            g_isOpening = true;
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Show Log Window
    if (show_log_window)
    {
        Log::ShowLogWindow(&show_log_window);
    }

    if (g_texture)
    {
        ImVec2 window_size = io.DisplaySize;
        bool bViewisLandscape = window_size.x >= window_size.y ? true : false;
        // bool bRenderisLandscape = g_player->width() >= g_player->height() ? true : false;
        bool bRenderisLandscape = true;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? window_size.y : window_size.x;
        float adj_h = bNeedChangeScreenInfo ? window_size.x : window_size.y;
        float adj_x = adj_w;
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        float offset_x = (window_size.x - adj_x) / 2.0;
        float offset_y = (window_size.y - adj_y) / 2.0;
        ImGui::GetBackgroundDrawList ()->AddImage (
            (void *)g_texture,
            ImVec2 (offset_x, offset_y),
            ImVec2 (offset_x + adj_x, offset_y + adj_y),
            ImVec2 (0, 0),
            ImVec2 (1, 1)
        );
    }
    if (app_will_quit)
    {
        app_done = true;
    }
    return app_done;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "Media Player";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
    property.font_scale = 1.5f;
    property.application.Application_Initialize = MediaPlayer_Initialize;
    property.application.Application_Finalize = MediaPlayer_Finalize;
    property.application.Application_Frame = MediaPlayer_Frame;
}
