#include "MediaPlayer.h"

namespace MEC
{
/***********************************************************************************************************
 * Media Player Functions
 ***********************************************************************************************************/
MediaPlayer::MediaPlayer()
{
    m_txmgr = RenderUtils::TextureManager::CreateInstance();
    m_txmgr->SetLogLevel(Logger::INFO);
    m_audrdr = MediaCore::MediaReader::CreateInstance();
    m_audrdr->SetLogLevel(Logger::INFO);

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
    }
    if (m_audrdr)
    {
        m_audrdr->Close();
    }
    if (m_audrnd)
    {
        m_audrnd->CloseDevice();
        MediaCore::AudioRender::ReleaseInstance(&m_audrnd);
    }
    if (m_pcmStream)
    {
        delete m_pcmStream;
        m_pcmStream = nullptr;
    }
    m_vidrdr = nullptr;
    m_audrdr = nullptr;

    m_tx = nullptr;
    m_txmgr = nullptr;
}

void MediaPlayer::Open(std::string url)
{
    m_mediaParser = MediaCore::MediaParser::CreateInstance();
    m_mediaParser->Open(url);
    if (m_mediaParser->HasVideo())
    {
        m_vidrdr = MediaCore::MediaReader::CreateVideoInstance();
        m_vidrdr->SetLogLevel(Logger::DEBUG);
        m_vidrdr->EnableHwAccel(m_useHwAccel);
        m_vidrdr->Open(m_mediaParser);
        m_vidrdr->ConfigVideoReader(1.f, 1.f, IM_CF_RGBA, IM_DT_INT8, IM_INTERPOLATE_AREA, MediaCore::HwaccelManager::GetDefaultInstance());
        m_vidrdr->Start();
    }
    if (m_mediaParser->HasAudio())
    {
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
    }
    m_playURL = url;
    m_playStartTp = Clock::now();
}

void MediaPlayer::Close()
{
    if (m_vidrdr) m_vidrdr->Close();
    if (m_audrdr) m_audrdr->Close();
    m_audrnd->Flush();
    m_pcmStream->m_audPos = 0;
    m_playStartPos = 0;
    m_audioStreamCount = 0;
    if (m_tx) m_tx = nullptr;
    m_isPlay = false;
    m_playURL.clear();
}

bool MediaPlayer::IsOpened()
{
    return (m_vidrdr && m_vidrdr->IsOpened()) || 
        (m_audrdr && m_audrdr->IsOpened());
}

bool MediaPlayer::HasVideo()
{
    return m_vidrdr && m_vidrdr->IsOpened();
}

bool MediaPlayer::HasAudio()
{
    return m_audrdr && m_audrdr->IsOpened();
}

float MediaPlayer::GetVideoDuration()
{
    if (!m_vidrdr) return 0.f;
    const MediaCore::VideoStream* vstminfo = m_vidrdr->GetVideoStream();
    float vidDur = vstminfo ? (float)vstminfo->duration : 0;
    return vidDur;
}

float MediaPlayer::GetAudioDuration()
{
    if (!m_audrdr) return 0.f;
    const MediaCore::AudioStream* astminfo = m_audrdr->GetAudioStream();
    float audDur = astminfo ? (float)astminfo->duration : 0.f;
    return audDur;
}

float MediaPlayer::GetCurrentPos()
{
    float pos = 0.f;
    if (HasAudio())
    {
        pos = m_isPlay ? m_pcmStream->m_audPos : m_playStartPos;
    }
    else
    {
        double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>((Clock::now()-m_playStartTp)).count();
        pos = m_isPlay ? m_playStartPos + elapsedTime : m_playStartPos;
    }
    return pos;
}

bool MediaPlayer::Play()
{
    if (!m_vidrdr && !m_audrdr)
        return false;
    if (m_vidrdr && m_vidrdr->IsSuspended())
        m_vidrdr->Wakeup();
    m_playStartTp = Clock::now();
    m_playStartPos = GetCurrentPos();
    if (m_audrdr->IsOpened())
        m_audrnd->Resume();
    m_isPlay = true;
    return true;
}

bool MediaPlayer::Pause()
{
    if (!m_vidrdr && !m_audrdr)
        return false;
    m_playStartPos = GetCurrentPos();
    if (m_audrdr->IsOpened())
        m_audrnd->Pause();
    m_isPlay = false;
    return true;
}

bool MediaPlayer::Seek(float pos)
{
    if (!m_vidrdr && !m_audrdr)
        return false;
    int64_t seekPos = pos * 1000;
    if (m_vidrdr && m_vidrdr->IsOpened())
        m_vidrdr->SeekTo(seekPos);
    if (m_audrdr && m_audrdr->IsOpened())
        m_audrdr->SeekTo(seekPos);
    m_playStartPos = pos;
    m_playStartTp = Clock::now();
    return true;
}

bool MediaPlayer::IsPlaying()
{
    return m_isPlay;
}

ImTextureID MediaPlayer::GetFrame(float pos)
{
    if (m_vidrdr && m_vidrdr->IsOpened() && !m_vidrdr->IsSuspended())
    {
        bool eof;
        ImGui::ImMat vmat;
        int64_t readPos = (int64_t)(pos*1000);
        auto hVf = m_vidrdr->ReadVideoFrame(readPos, eof, false);
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
    }

    return m_tx ? m_tx->TextureID() : nullptr;
}
}