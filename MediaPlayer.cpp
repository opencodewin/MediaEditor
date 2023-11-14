#include "MediaPlayer.h"

namespace MEC
{
/***********************************************************************************************************
 * Media Player Functions
 ***********************************************************************************************************/
MediaPlayer::MediaPlayer()
{
    g_txmgr = RenderUtils::TextureManager::CreateInstance();
    g_txmgr->SetLogLevel(Logger::INFO);
    g_audrdr = MediaCore::MediaReader::CreateInstance();
    g_audrdr->SetLogLevel(Logger::INFO);

    g_pcmStream = new SimplePcmStream(g_audrdr);
    g_audrnd = MediaCore::AudioRender::CreateInstance();
    g_audrnd->OpenDevice(c_audioRenderSampleRate, c_audioRenderChannels, c_audioRenderFormat, g_pcmStream);
    MediaCore::HwaccelManager::GetDefaultInstance()->Init();
}

MediaPlayer::~MediaPlayer()
{
    if (g_vidrdr)
    {
        g_vidrdr->Close();
    }
    if (g_audrdr)
    {
        g_audrdr->Close();
    }
    if (g_audrnd)
    {
        g_audrnd->CloseDevice();
        MediaCore::AudioRender::ReleaseInstance(&g_audrnd);
    }
    if (g_pcmStream)
    {
        delete g_pcmStream;
        g_pcmStream = nullptr;
    }
    g_vidrdr = nullptr;
    g_audrdr = nullptr;

    g_tx = nullptr;
    g_txmgr = nullptr;
}
}