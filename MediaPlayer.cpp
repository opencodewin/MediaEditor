#include "MediaPlayer.h"

namespace MEC
{
/***********************************************************************************************************
 * Media Player Functions
 ***********************************************************************************************************/
MediaPlayer::MediaPlayer(RenderUtils::TextureManager::Holder hTxmgr)
{
    m_txmgr = hTxmgr ? hTxmgr : RenderUtils::TextureManager::CreateInstance();
    //m_txmgr->SetLogLevel(Logger::INFO);

    m_pcmStream = new SimplePcmStream(m_audrdr);
    m_audrnd = MediaCore::AudioRender::CreateInstance();
    m_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, m_pcmStream);
    MediaCore::HwaccelManager::GetDefaultInstance()->Init();
}

MediaPlayer::~MediaPlayer()
{
    if (m_vidrdr)
    {
        m_vidrdr->Close();
        m_vidrdr = nullptr;
    }
    if (m_audrnd)
    {
        m_audrnd->CloseDevice();
        MediaCore::AudioRender::ReleaseInstance(&m_audrnd);
    }
    m_pcmStream->SetAudioReader(nullptr);
    if (m_audrdr)
    {
        m_audrdr->Close();
        m_audrdr = nullptr;
    }
    if (m_pcmStream)
    {
        delete m_pcmStream;
        m_pcmStream = nullptr;
    }
    m_mediaParser = nullptr;
    m_tx = nullptr;
    m_txmgr = nullptr;
}

void MediaPlayer::Open(const std::string& url)
{
    m_mediaParser = MediaCore::MediaParser::CreateInstance();
    m_mediaParser->Open(url);
    if (m_mediaParser->HasVideo())
    {
        m_vidrdr = MediaCore::MediaReader::CreateVideoInstance();
        // m_vidrdr->SetLogLevel(Logger::DEBUG);
        m_vidrdr->EnableHwAccel(m_useHwAccel);
        m_vidrdr->Open(m_mediaParser);
        m_vidrdr->ConfigVideoReader(1.f, 1.f, IM_CF_RGBA, IM_DT_INT8, IM_INTERPOLATE_AREA, MediaCore::HwaccelManager::GetDefaultInstance());
        m_vidrdr->Start();
        m_bIsVideoReady = true;
    }
    if (m_mediaParser->HasAudio())
    {
        m_audrdr = MediaCore::MediaReader::CreateInstance();
        // m_audrdr->SetLogLevel(Logger::DEBUG);
        m_audrdr->Open(m_mediaParser);
        auto mediaInfo = m_mediaParser->GetMediaInfo();
        for (auto stream : mediaInfo->streams)
        {
            if (stream->type == MediaCore::MediaType::AUDIO)
                m_audioStreamCount++;
        }
        m_chooseAudioIndex = 0;
        m_audrdr->ConfigAudioReader(
                                    c_audioRenderChannels,
                                    c_audioRenderSampleRate,
                                    "flt",
                                    m_chooseAudioIndex);
        m_audrdr->Start();
        m_bIsAudioReady = true;
        m_pcmStream->SetAudioReader(m_audrdr);
    }
    m_playURL = url;
    m_playStartTp = Clock::now();
}

void MediaPlayer::Open(MediaCore::MediaParser::Holder hParser)
{
    if (!hParser->IsOpened())
        throw std::runtime_error("INVALID argument! MediaParser must be opened.");
    m_mediaParser = hParser;
    if (hParser->HasVideo())
    {
        if (hParser->IsImageSequence())
            m_vidrdr = MediaCore::MediaReader::CreateImageSequenceInstance();
        else
            m_vidrdr = MediaCore::MediaReader::CreateVideoInstance();
        // m_vidrdr->SetLogLevel(Logger::DEBUG);
        m_vidrdr->EnableHwAccel(m_useHwAccel);
        m_vidrdr->Open(hParser);
        m_vidrdr->ConfigVideoReader(1.f, 1.f, IM_CF_RGBA, IM_DT_INT8, IM_INTERPOLATE_AREA, MediaCore::HwaccelManager::GetDefaultInstance());
        m_vidrdr->Start();
        m_bIsVideoReady = true;
    }
    if (hParser->HasAudio())
    {
        m_audrdr = MediaCore::MediaReader::CreateInstance();
        // m_audrdr->SetLogLevel(Logger::DEBUG);
        m_audrdr->Open(hParser);
        auto mediaInfo = hParser->GetMediaInfo();
        for (auto stream : mediaInfo->streams)
        {
            if (stream->type == MediaCore::MediaType::AUDIO)
                m_audioStreamCount++;
        }
        m_chooseAudioIndex = 0;
        m_audrdr->ConfigAudioReader(
                                    c_audioRenderChannels,
                                    c_audioRenderSampleRate,
                                    "flt",
                                    m_chooseAudioIndex);
        m_audrdr->Start();
        m_bIsAudioReady = true;
        m_pcmStream->SetAudioReader(m_audrdr);
    }
    m_playURL = hParser->GetUrl();
    m_playStartTp = Clock::now();
}

void MediaPlayer::Close()
{
    if (m_vidrdr)
    {
        m_vidrdr->Close();
        m_vidrdr = nullptr;
    }
    m_bIsVideoReady = false;
    if (m_audrnd)
    {
        m_audrnd->Pause();
        m_audrnd->Flush();
    }
    m_pcmStream->SetAudioReader(nullptr);
    if (m_audrdr)
    {
        m_audrdr->Close();
        m_audrdr = nullptr;
    }
    m_bIsAudioReady = false;
    m_mediaParser = nullptr;
    m_pcmStream->m_audPos = 0;
    m_playStartPos = 0;
    m_audioStreamCount = 0;
    if (m_tx) m_tx = nullptr;
    m_bIsPlay = false;
    m_playURL.clear();
}

float MediaPlayer::GetVideoDuration()
{
    if (!m_bIsVideoReady) return 0.f;
    const MediaCore::VideoStream* vstminfo = m_vidrdr->GetVideoStream();
    float vidDur = vstminfo ? (float)vstminfo->duration : 0;
    return vidDur;
}

float MediaPlayer::GetAudioDuration()
{
    if (!m_bIsAudioReady) return 0.f;
    const MediaCore::AudioStream* astminfo = m_audrdr->GetAudioStream();
    float audDur = astminfo ? (float)astminfo->duration : 0.f;
    return audDur;
}

float MediaPlayer::GetCurrentPos()
{
    float pos = 0.f;
    if (m_bIsSeeking)
        pos = m_playStartPos;
    else if (m_bIsAudioReady)
        pos = m_bIsPlay ? m_pcmStream->m_audPos : m_playStartPos;
    else
    {
        double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>((Clock::now()-m_playStartTp)).count();
        pos = m_bIsPlay ? m_playStartPos + elapsedTime : m_playStartPos;
    }
    return pos;
}

bool MediaPlayer::Play()
{
    if (!m_bIsVideoReady && !m_bIsAudioReady)
        return false;
    if (m_bIsPlay)
        return true;
    m_playStartTp = Clock::now();
    m_playStartPos = GetCurrentPos();
    if (m_bIsAudioReady)
    {
        if (!m_vidrdr->IsDirectionForward())
            m_vidrdr->SetDirection(true);
    }
    if (m_bIsAudioReady)
    {
        if (!m_audrdr->IsDirectionForward())
            m_audrdr->SetDirection(true);
        if (m_audioNeedSeek)
        {
            int64_t seekPos = m_playStartPos * 1000;
            m_audrdr->SeekTo(seekPos);
            m_audrnd->Flush();
            m_audioNeedSeek = false;
        }
        m_audrnd->Resume();
    }
    m_bIsPlay = true;
    return true;
}

bool MediaPlayer::Pause()
{
    if (!m_bIsVideoReady && !m_bIsAudioReady)
        return false;
    if (!m_bIsPlay)
        return true;
    m_playStartPos = GetCurrentPos();
    if (m_bIsAudioReady)
        m_audrnd->Pause();
    m_bIsPlay = false;
    return true;
}

bool MediaPlayer::Seek(float pos, bool bSeekingMode)
{
    if (!m_bIsVideoReady && !m_bIsAudioReady)
        return false;
    m_bIsSeeking = bSeekingMode;
    int64_t seekMts = pos * 1000;
    if (m_bIsVideoReady)
        m_vidrdr->SeekTo(seekMts, bSeekingMode);
    if (bSeekingMode)
    {
        if (m_bIsAudioReady)
            m_audrnd->Pause();
    }
    else if (m_bIsAudioReady)
    {
        m_audrdr->SeekTo(seekMts);
        m_audioNeedSeek = false;
        m_audrnd->Flush();
        m_pcmStream->m_audPos = pos;
        if (m_bIsPlay)
            m_audrnd->Resume();
    }
    m_playStartPos = pos;
    m_playStartTp = Clock::now();
    return true;
}

bool MediaPlayer::Step(bool forward)
{
    if (!m_bIsVideoReady && !m_bIsAudioReady)
        return false;
    if (m_bIsPlay)
        return false;
    bool eof;
    if (m_bIsAudioReady && forward != m_audrdr->IsDirectionForward())
        m_audrdr->SetDirection(forward);
    if (m_bIsVideoReady)
    {
        if (forward != m_vidrdr->IsDirectionForward())
            m_vidrdr->SetDirection(forward);
        auto frame = m_vidrdr->ReadNextVideoFrame(eof);
        if (frame)
        {
            ImGui::ImMat vmat;
            frame->GetMat(vmat);
            if (!m_tx)
            {
                RenderUtils::Vec2<int32_t> txSize(vmat.w, vmat.h);
                m_tx = m_txmgr->CreateManagedTextureFromMat(vmat, txSize);
            }
            else
                m_tx->RenderMatToTexture(vmat);
            m_playStartPos = (double)frame->Pos() / 1000.0;
            m_audioNeedSeek = true;
        }
    }
    return true;
}

ImTextureID MediaPlayer::GetFrame(float pos, bool blocking)
{
    if (!m_bIsVideoReady)
        return nullptr;

    bool eof;
    ImGui::ImMat vmat;
    int64_t readPos = (int64_t)(pos*1000);
    auto hVf = m_vidrdr->ReadVideoFrame(readPos, eof, blocking);
    if (!hVf && m_bIsSeeking)
        hVf = m_vidrdr->GetSeekingFlash();
    if (hVf)
    {
        Logger::Log(Logger::VERBOSE) << "Succeeded to read video frame @pos=" << pos << "." << std::endl;
        hVf->GetMat(vmat);
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
            Logger::Log(Logger::Error) << "WRONG snapshot format!" << std::endl;
            imgValid = false;
        }
        if (imgValid)
        {
            if (!m_tx)
            {
                RenderUtils::Vec2<int32_t> txSize(vmat.w, vmat.h);
                m_tx = m_txmgr->CreateManagedTextureFromMat(vmat, txSize);
                if (!m_tx)
                    Logger::Log(Logger::Error) << "FAILED to create ManagedTexture from ImMat! Error is '" << \
                                                                        m_txmgr->GetError() << "'." << std::endl;
            }
            else
            {
                m_tx->RenderMatToTexture(vmat);
            }
        }
    }
    return m_tx ? m_tx->TextureID() : nullptr;
}
}