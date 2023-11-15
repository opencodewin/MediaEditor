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
}