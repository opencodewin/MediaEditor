#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <list>
#include "MediaPlayer_FFImpl.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
    #include "libavutil/pixdesc.h"
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
    #include "libavdevice/avdevice.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
}

using namespace std;

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

class MediaPlayer_FFImpl : public MediaPlayer
{
public:
    using Clock = chrono::steady_clock;
    using Duration = chrono::duration<double, milli>;
    using TimePoint = chrono::time_point<Clock>;
    static const TimePoint CLOCK_MIN;

    MediaPlayer_FFImpl() : m_audByteStream(this) {}

    bool SetAudioRender(AudioRender* audrnd) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (IsPlaying())
        {
            m_errMessage = "Can NOT set audio render while the player is playing!";
            return false;
        }
        m_audrnd = audrnd;
        return true;
    }

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!OpenMedia(url))
        {
            Close();
            return false;
        }
        return true;
    }

    bool Close() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_audrnd)
            m_audrnd->CloseDevice();
        m_audByteStream.Reset();
        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
        m_swrOutChannels = 0;
        m_swrOutChnLyt = 0;
        m_swrOutSmpfmt = AV_SAMPLE_FMT_S16;
        m_swrOutSampleRate = 0;
        m_swrPassThrough = false;
        if (m_auddecCtx)
        {
            avcodec_free_context(&m_auddecCtx);
            m_auddecCtx = nullptr;
        }
        if (m_viddecCtx)
        {
            avcodec_free_context(&m_viddecCtx);
            m_viddecCtx = nullptr;
        }
        if (m_viddecHwDevCtx)
        {
            av_buffer_unref(&m_viddecHwDevCtx);
            m_viddecHwDevCtx = nullptr;
        }
        m_vidHwPixFmt = AV_PIX_FMT_NONE;
        m_viddecDevType = AV_HWDEVICE_TYPE_NONE;
        if (m_avfmtCtx)
        {
            avformat_close_input(&m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }
        m_vidStmIdx = -1;
        m_audStmIdx = -1;
        m_vidStream = nullptr;
        m_audStream = nullptr;
        m_viddec = nullptr;
        m_auddec = nullptr;

        m_demuxEof = false;
        m_viddecEof = false;
        m_auddecEof = false;
        m_renderEof = false;

        m_vidpktQMaxSize = 0;
        m_audpktQMaxSize = 0;

        m_errMessage = "";
        return true;
    }

    bool Play() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (m_isPlaying)
            return true;
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }
        if (!HasVideo() && !HasAudio())
        {
            m_errMessage = "No video nor audio is to be played!";
            return false;
        }

        if (m_renderEof)
            if (!Reset())
                return false;

        if (m_runStartTp == CLOCK_MIN)
            m_runStartTp = Clock::now();
        if (m_pauseStartTp != CLOCK_MIN)
        {
            m_pausedDur += Clock::now()-m_pauseStartTp;
            m_pauseStartTp = CLOCK_MIN;
        }

        if (!m_renderThread.joinable())
            StartAllThreads();
        if (m_audrnd)
            m_audrnd->Resume();
        m_isPlaying = true;
        return true;
    }

    bool Pause() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }
        if (m_audrnd)
            m_audrnd->Pause();
        m_pauseStartTp = Clock::now();
        m_isPlaying = false;
        return true;
    }

    bool Reset() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }

        if (m_audrnd)
            m_audrnd->Pause();

        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_audrnd)
            m_audrnd->Flush();
        m_audByteStream.Reset();
        if (m_viddecCtx)
            avcodec_flush_buffers(m_viddecCtx);
        if (m_auddecCtx)
            avcodec_flush_buffers(m_auddecCtx);

        m_demuxEof = false;
        m_viddecEof = false;
        m_auddecEof = false;
        m_renderEof = false;

        int fferr = avformat_seek_file(m_avfmtCtx, -1, INT64_MIN, m_avfmtCtx->start_time, m_avfmtCtx->start_time, 0);
        if (fferr < 0)
        {
            SetFFError("avformat_seek_file(In Reset)", fferr);
            return false;
        }
        return true;
    }

    bool SeekToI(uint64_t pos) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }

        bool isPlaying = m_isPlaying;

        if (m_audrnd)
            m_audrnd->Pause();

        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_audrnd)
            m_audrnd->Flush();
        m_audByteStream.Reset();
        if (m_viddecCtx)
            avcodec_flush_buffers(m_viddecCtx);
        if (m_auddecCtx)
            avcodec_flush_buffers(m_auddecCtx);

        m_demuxEof = false;
        m_viddecEof = false;
        m_auddecEof = false;
        m_renderEof = false;

        int64_t ffpos = av_rescale_q(pos, MILLISEC_TIMEBASE, FFAV_TIMEBASE);
        int fferr = avformat_seek_file(m_avfmtCtx, -1, INT64_MIN, ffpos, ffpos, 0);
        if (fferr < 0)
        {
            SetFFError("avformat_seek_file(In SeekToI)", fferr);
            return false;
        }
        cout << "Seek to " << MillisecToString(pos) << endl;
        m_afterSeeking = true;

        if (isPlaying)
        {
            StartAllThreads();
            if (m_audrnd)
                m_audrnd->Resume();
            m_isPlaying = true;
        }
        return true;
    }

    bool SeekAsync(uint64_t pos) override
    {
        return true;
    }

    bool IsOpened() const override
    {
        return (bool)m_avfmtCtx;
    }

    bool IsPlaying() const override
    {
        return m_isPlaying;
    }

    bool HasVideo() const override
    {
        return m_vidStmIdx >= 0;
    }

    bool HasAudio() const override
    {
        return m_audStmIdx >= 0;
    }

    float GetPlaySpeed() const override
    {
        return 1.0f;
    }

    bool SetPlaySpeed(float speed) override
    {
        return false;
    }

    bool SetPreferHwDecoder(bool prefer) override
    {
        m_vidPreferUseHw = prefer;
        return true;
    }

    uint64_t GetDuration() const override
    {
        if (!m_avfmtCtx)
            return 0;
        int64_t dur = av_rescale(m_avfmtCtx->duration, 1000L, AV_TIME_BASE);
        return dur < 0 ? 0 : (uint64_t)dur;
    }

    uint64_t GetPlayPos() const override
    {
        return static_cast<uint64_t>(m_playPos.count());
    }

    ImGui::ImMat GetVideo() const override
    {
        return m_vidMat;
    }

    bool SetPlayMode(PlayMode mode) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (IsOpened())
        {
            m_errMessage = "Can only change play mode when media is not opened!";
            return false;
        }
        m_playMode = mode;
        return true;
    }

    string GetError() const override
    {
        return m_errMessage;
    }

    bool CheckHwPixFmt(AVPixelFormat pixfmt)
    {
        return pixfmt == m_vidHwPixFmt;
    }

private:
    static const AVRational MILLISEC_TIMEBASE;
    static const AVRational FFAV_TIMEBASE;

    void SetFFError(const string& funcname, int fferr)
    {
        ostringstream oss;
        oss << "'" << funcname << "' returns " << fferr << ".";
        m_errMessage = oss.str();
    }

    bool OpenMedia(const string& url)
    {
        if (IsOpened())
            Close();

        int fferr = 0;
        fferr = avformat_open_input(&m_avfmtCtx, url.c_str(), nullptr, nullptr);
        if (fferr < 0)
        {
            SetFFError("avformat_open_input", fferr);
            return false;
        }
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            SetFFError("avformat_find_stream_info", fferr);
            return false;
        }
        cout << "Open '" << url << "' successfully. " << m_avfmtCtx->nb_streams << " streams are found." << endl;

        if (m_playMode != PlayMode::AUDIO_ONLY)
            m_vidStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &m_viddec, 0);
        if (m_playMode != PlayMode::VIDEO_ONLY)
        m_audStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &m_auddec, 0);
        if (m_vidStmIdx < 0 && m_audStmIdx < 0)
        {
            ostringstream oss;
            oss << "Neither video nor audio stream can be found in '" << url << "'.";
            m_errMessage = oss.str();
            return false;
        }
        m_vidStream = m_vidStmIdx >= 0 ? m_avfmtCtx->streams[m_vidStmIdx] : nullptr;
        m_audStream = m_audStmIdx >= 0 ? m_avfmtCtx->streams[m_audStmIdx] : nullptr;

        if (m_vidStream)
        {
            if (m_vidPreferUseHw)
            {
                if (!OpenHwVideoDecoder())
                    if (!OpenVideoDecoder())
                        return false;
            }
            else
            {
                if (!OpenVideoDecoder())
                    return false;
            }
            int qMaxSize = 0;
            if (m_vidStream->avg_frame_rate.den > 0)
                qMaxSize = (int)(m_vidpktQDuration*m_vidStream->avg_frame_rate.num/m_vidStream->avg_frame_rate.den);
            if (qMaxSize < 20)
                qMaxSize = 20;
            m_vidpktQMaxSize = qMaxSize;
        }
        if (m_audStream)
        {
            if (!OpenAudioDecoder())
                return false;
            m_audpktQMaxSize = 50;

            if (m_audrnd)
            {
                if (!OpenAudioRender())
                    return false;
            }
        }
        return true;
    }

    bool OpenVideoDecoder()
    {
        m_viddecCtx = avcodec_alloc_context3(m_viddec);
        if (!m_viddecCtx)
        {
            m_errMessage = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_viddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidStream->codecpar);
        if (fferr < 0)
        {
            SetFFError("avcodec_parameters_to_context", fferr);
            return false;
        }

        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        cout << "Video decoder '" << m_viddec->name << "' opened." << endl;
        return true;
    }

    bool OpenHwVideoDecoder()
    {
        m_vidHwPixFmt = AV_PIX_FMT_NONE;
        for (int i = 0; ; i++)
        {
            const AVCodecHWConfig* config = avcodec_get_hw_config(m_viddec, i);
            if (!config)
            {
                ostringstream oss;
                oss << "Decoder '" << m_viddec->name << "' does NOT support hardware acceleration.";
                m_errMessage = oss.str();
                return false;
            }
            if ((config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0)
            {
                if (m_vidUseHwType == AV_HWDEVICE_TYPE_NONE || m_vidUseHwType == config->device_type)
                {
                    m_vidHwPixFmt = config->pix_fmt;
                    m_viddecDevType = config->device_type;
                    break;
                }
            }
        }

        m_viddecCtx = avcodec_alloc_context3(m_viddec);
        if (!m_viddecCtx)
        {
            m_errMessage = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_viddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidStream->codecpar);
        if (fferr < 0)
        {
            SetFFError("avcodec_parameters_to_context", fferr);
            return false;
        }
        m_viddecCtx->get_format = get_hw_format;

        fferr = av_hwdevice_ctx_create(&m_viddecHwDevCtx, m_viddecDevType, nullptr, nullptr, 0);
        if (fferr < 0)
        {
            SetFFError("av_hwdevice_ctx_create", fferr);
            return false;
        }
        m_viddecCtx->hw_device_ctx = av_buffer_ref(m_viddecHwDevCtx);

        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        cout << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
        return true;
    }

    bool OpenAudioDecoder()
    {
        m_auddecCtx = avcodec_alloc_context3(m_auddec);
        if (!m_auddecCtx)
        {
            m_errMessage = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_auddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_auddecCtx, m_audStream->codecpar);
        if (fferr < 0)
        {
            SetFFError("avcodec_parameters_to_context", fferr);
            return false;
        }

        fferr = avcodec_open2(m_auddecCtx, m_auddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        cout << "Audio decoder '" << m_auddec->name << "' opened." << endl;

        // setup sw resampler
        int inChannels = m_audStream->codecpar->channels;
        uint64_t inChnLyt = m_audStream->codecpar->channel_layout;
        int inSampleRate = m_audStream->codecpar->sample_rate;
        AVSampleFormat inSmpfmt = (AVSampleFormat)m_audStream->codecpar->format;
        m_swrOutChannels = inChannels > 2 ? 2 : inChannels;
        m_swrOutChnLyt = av_get_default_channel_layout(m_swrOutChannels);
        m_swrOutSmpfmt = AV_SAMPLE_FMT_S16;
        m_swrOutSampleRate = inSampleRate;
        if (inChnLyt <= 0)
            inChnLyt = av_get_default_channel_layout(inChannels);
        if (m_swrOutChnLyt != inChnLyt || m_swrOutSmpfmt != inSmpfmt || m_swrOutSampleRate != inSampleRate)
        {
            m_swrCtx = swr_alloc_set_opts(NULL, m_swrOutChnLyt, m_swrOutSmpfmt, m_swrOutSampleRate, inChnLyt, inSmpfmt, inSampleRate, 0, nullptr);
            if (!m_swrCtx)
            {
                m_errMessage = "FAILED to invoke 'swr_alloc_set_opts()' to create 'SwrContext'!";
                return false;
            }
            int fferr = swr_init(m_swrCtx);
            if (fferr < 0)
            {
                SetFFError("swr_init", fferr);
                return false;
            }
            m_swrPassThrough = false;
        }
        else
        {
            m_swrPassThrough = true;
        }
        return true;
    }

    bool OpenAudioRender()
    {
        if (!m_audrnd->OpenDevice(
                m_swrOutSampleRate, m_swrOutChannels,
                AudioRender::PcmFormat::SINT16, &m_audByteStream))
        {
            m_errMessage = m_audrnd->GetError();
            return false;
        }
        return true;
    }

    void DemuxThreadProc()
    {
        cout << "Enter DemuxThreadProc()..." << endl;
        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        while (!m_quitPlay)
        {
            bool idleLoop = true;

            if (!avpktLoaded)
            {
                int fferr = av_read_frame(m_avfmtCtx, &avpkt);
                if (fferr == 0)
                {
                    avpktLoaded = true;
                    idleLoop = false;
                }
                else
                {
                    if (fferr == AVERROR_EOF)
                        cout << "Demuxer EOF." << endl;
                    else
                        cerr << "Demuxer ERROR! 'av_read_frame' returns " << fferr << "." << endl;
                    break;
                }
            }

            if (avpkt.stream_index == m_vidStmIdx)
            {
                if (m_vidpktQ.size() < m_vidpktQMaxSize)
                {
                    AVPacket* enqpkt = av_packet_clone(&avpkt);
                    if (!enqpkt)
                    {
                        cerr << "FAILED to invoke 'av_packet_clone(DemuxThreadProc)'!" << endl;
                        break;
                    }
                    lock_guard<mutex> lk(m_vidpktQLock);
                    m_vidpktQ.push_back(enqpkt);
                    av_packet_unref(&avpkt);
                    avpktLoaded = false;
                    idleLoop = false;
                }
            }
            else if (avpkt.stream_index == m_audStmIdx)
            {
                if (m_vidpktQMaxSize > 0 || m_audpktQ.size() < m_audpktQMaxSize)
                {
                    AVPacket* enqpkt = av_packet_clone(&avpkt);
                    if (!enqpkt)
                    {
                        cerr << "FAILED to invoke 'av_packet_clone(DemuxThreadProc)'!" << endl;
                        break;
                    }
                    lock_guard<mutex> lk(m_audpktQLock);
                    m_audpktQ.push_back(enqpkt);
                    av_packet_unref(&avpkt);
                    avpktLoaded = false;
                    idleLoop = false;
                }
            }
            else
            {
                av_packet_unref(&avpkt);
                avpktLoaded = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_demuxEof = true;
        cout << "Leave DemuxThreadProc()." << endl;
    }

    void VideoDecodeThreadProc()
    {
        cout << "Enter VideoDecodeThreadProc()..." << endl;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        while (!m_quitPlay)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            // retrieve output frame
            bool hasOutput;
            do{
                if (!avfrmLoaded)
                {
                    int fferr = avcodec_receive_frame(m_viddecCtx, &avfrm);
                    if (fferr == 0)
                    {
                        if (m_afterSeeking)
                        {
                            cout << "retrieved VIDEO frame after seek " << MillisecToString(av_rescale_q(avfrm.pts, m_vidStream->time_base, MILLISEC_TIMEBASE)) << endl;
                            m_afterSeeking = false;
                        }
                        avfrmLoaded = true;
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                            cerr << "FAILED to invoke 'avcodec_receive_frame'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                        quitLoop = true;
                        break;
                    }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded)
                {
                    if (m_vidfrmQ.size() < m_vidfrmQMaxSize)
                    {
                        lock_guard<mutex> lk(m_vidfrmQLock);
                        AVFrame* enqfrm = av_frame_clone(&avfrm);
                        m_vidfrmQ.push_back(enqfrm);
                        av_frame_unref(&avfrm);
                        avfrmLoaded = false;
                        idleLoop = false;
                    }
                    else
                        break;
                }
            } while (hasOutput);
            if (quitLoop)
                break;

            // input packet to decoder
            if (!inputEof)
            {
                while (m_vidpktQ.size() > 0)
                {
                    AVPacket* avpkt = m_vidpktQ.front();
                    int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        if (m_afterSeeking)
                            cout << "decode VIDEO packet after seek " << MillisecToString(av_rescale_q(avpkt->pts, m_vidStream->time_base, MILLISEC_TIMEBASE)) << endl;
                        lock_guard<mutex> lk(m_vidpktQLock);
                        m_vidpktQ.pop_front();
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else
                    {
                        if (fferr != AVERROR(EAGAIN))
                        {
                            cerr << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
                        }
                        break;
                    }
                }
                if (quitLoop)
                    break;

                if (m_vidpktQ.size() == 0 && m_demuxEof)
                {
                    avcodec_send_packet(m_viddecCtx, nullptr);
                    idleLoop = false;
                    inputEof = true;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_viddecEof = true;
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        cout << "Leave VideoDecodeThreadProc()." << endl;
    }

    void AudioDecodeThreadProc()
    {
        cout << "Enter AudioDecodeThreadProc()..." << endl;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        while (!m_quitPlay)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            // retrieve output frame
            bool hasOutput;
            do{
                if (!avfrmLoaded)
                {
                    int fferr = avcodec_receive_frame(m_auddecCtx, &avfrm);
                    if (fferr == 0)
                    {
                        avfrmLoaded = true;
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                            cerr << "FAILED to invoke 'avcodec_receive_frame'(AudioDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                        quitLoop = true;
                        break;
                    }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded)
                {
                    if (m_audfrmQ.size() < m_audfrmQMaxSize)
                    {
                        lock_guard<mutex> lk(m_audfrmQLock);
                        AVFrame* enqfrm = av_frame_clone(&avfrm);
                        m_audfrmQ.push_back(enqfrm);
                        av_frame_unref(&avfrm);
                        avfrmLoaded = false;
                        idleLoop = false;
                    }
                    else
                        break;
                }
            } while (hasOutput);
            if (quitLoop)
                break;

            // input packet to decoder
            if (!inputEof)
            {
                while (m_audpktQ.size() > 0)
                {
                    AVPacket* avpkt = m_audpktQ.front();
                    int fferr = avcodec_send_packet(m_auddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        if (m_afterSeeking)
                            cout << "decode AUDIO packet after seek " << MillisecToString(av_rescale_q(avpkt->pts, m_audStream->time_base, MILLISEC_TIMEBASE)) << endl;
                        lock_guard<mutex> lk(m_audpktQLock);
                        m_audpktQ.pop_front();
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else
                    {
                        if (fferr != AVERROR(EAGAIN))
                        {
                            cerr << "FAILED to invoke 'avcodec_send_packet'(AudioDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
                        }
                        break;
                    }
                }
                if (quitLoop)
                    break;

                if (m_audpktQ.size() == 0 && m_demuxEof)
                {
                    avcodec_send_packet(m_auddecCtx, nullptr);
                    idleLoop = false;
                    inputEof = true;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_auddecEof = true;
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        cout << "Leave AudioDecodeThreadProc()." << endl;
    }

    void SwrThreadProc()
    {
        while (!m_quitPlay)
        {
            bool idleLoop = true;
            if (!m_audfrmQ.empty())
            {
                if (m_swrfrmQ.size() < m_swrfrmQMaxSize)
                {
                    AVFrame* srcfrm = m_audfrmQ.front();;
                    AVFrame* dstfrm = nullptr;
                    if (m_swrPassThrough)
                    {
                        dstfrm = srcfrm;
                    }
                    else
                    {
                        dstfrm = av_frame_alloc();
                        if (!dstfrm)
                        {
                            m_errMessage = "FAILED to allocate new AVFrame for 'swr_convert()'!";
                            break;
                        }
                        dstfrm->format = (int)m_swrOutSmpfmt;
                        dstfrm->sample_rate = m_swrOutSampleRate;
                        dstfrm->channels = m_swrOutChannels;
                        dstfrm->channel_layout = m_swrOutChnLyt;
                        dstfrm->nb_samples = swr_get_out_samples(m_swrCtx, srcfrm->nb_samples);
                        int fferr = av_frame_get_buffer(dstfrm, 0);
                        if (fferr < 0)
                        {
                            SetFFError("av_frame_get_buffer(SwrThreadProc)", fferr);
                            break;
                        }
                        av_frame_copy_props(dstfrm, srcfrm);
                        dstfrm->pts = swr_next_pts(m_swrCtx, srcfrm->pts);
                        fferr = swr_convert(m_swrCtx, dstfrm->data, dstfrm->nb_samples, (const uint8_t **)srcfrm->data, srcfrm->nb_samples);
                        if (fferr < 0)
                        {
                            SetFFError("swr_convert(SwrThreadProc)", fferr);
                            break;
                        }
                    }
                    {
                        lock_guard<mutex> lk(m_audfrmQLock);
                        m_audfrmQ.pop_front();
                    }
                    {
                        lock_guard<mutex> lk(m_swrfrmQLock);
                        m_swrfrmQ.push_back(dstfrm);
                    }
                    if (srcfrm != dstfrm)
                        av_frame_free(&srcfrm);
                    idleLoop = false;
                }
            }
            else if (m_auddecEof)
                break;

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_swrEof = true;
    }

    void RenderThreadProc()
    {
        while (!m_quitPlay)
        {
            if (!m_isPlaying)
            {
                this_thread::sleep_for(chrono::milliseconds(5));
                continue;
            }

            bool vidIdleRun = true;
            // Audio
            if (HasAudio())
            {
                m_playPos = chrono::duration_cast<Duration>(m_audioTs-Duration(m_audioOffset));
            }
            else
            {
                m_playPos = Clock::now()-m_runStartTp-m_pausedDur;
            }

            // Video
            if (HasVideo() && !m_vidfrmQ.empty())
            {
                AVFrame* vidfrm = m_vidfrmQ.front();
                int64_t mts = av_rescale_q(vidfrm->pts, m_vidStream->time_base, MILLISEC_TIMEBASE);
                if (m_playPos.count() >= mts)
                {
                    lock_guard<mutex> lk(m_vidfrmQLock);
                    m_vidfrmQ.pop_front();
                    ConvertAVFrameToImMat(vidfrm, m_vidMat);
                    m_vidMat.time_stamp = (double)mts/1000;
                    av_frame_free(&vidfrm);
                    vidIdleRun = false;
                }
            }

            if (vidIdleRun)
                this_thread::sleep_for(chrono::milliseconds(1));
        }
        m_renderEof = true;
    }

    bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat)
    {
        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
        AVFrame* swfrm = nullptr;
        if ((desc->flags&AV_PIX_FMT_FLAG_HWACCEL) > 0)
        {
            swfrm = av_frame_alloc();
            if (!swfrm)
            {
                m_errMessage = "FAILED to allocate new AVFrame for ImMat conversion!";
                return false;
            }
            int fferr = av_hwframe_transfer_data(swfrm, avfrm, 0);
            if (fferr < 0)
            {
                SetFFError("av_hwframe_transfer_data", fferr);
                av_frame_free(&swfrm);
                return false;
            }
            desc = av_pix_fmt_desc_get((AVPixelFormat)swfrm->format);
            avfrm = swfrm;
        }

        if (desc->nb_components <= 0 || desc->nb_components > 4)
        {
            cerr << "INVALID 'nb_component' value " << desc->nb_components << " of pixel format '"
                << desc->name << "', can only support value from 1 ~ 4." << endl;
            return false;
        }

#define ISYUV420P(format)   \
        (format == AV_PIX_FMT_YUV420P || \
        format == AV_PIX_FMT_YUVJ420P || \
        format == AV_PIX_FMT_YUV420P9 || \
        format == AV_PIX_FMT_YUV420P10 || \
        format == AV_PIX_FMT_YUV420P12 || \
        format == AV_PIX_FMT_YUV420P14 || \
        format == AV_PIX_FMT_YUV420P16)

#define ISYUV422P(format) \
        (format == AV_PIX_FMT_YUV422P || \
        format == AV_PIX_FMT_YUVJ422P || \
        format == AV_PIX_FMT_YUV422P9 || \
        format == AV_PIX_FMT_YUV422P10 || \
        format == AV_PIX_FMT_YUV422P12 || \
        format == AV_PIX_FMT_YUV422P14 || \
        format == AV_PIX_FMT_YUV422P16)

#define ISYUV444P(format) \
        (format == AV_PIX_FMT_YUV444P || \
        format == AV_PIX_FMT_YUVJ420P || \
        format == AV_PIX_FMT_YUV444P9 || \
        format == AV_PIX_FMT_YUV444P10 || \
        format == AV_PIX_FMT_YUV444P12 || \
        format == AV_PIX_FMT_YUV444P14 || \
        format == AV_PIX_FMT_YUV444P16)

#define ISNV12(format) \
        (format == AV_PIX_FMT_NV12 || \
        format == AV_PIX_FMT_NV21 || \
        format == AV_PIX_FMT_NV16 || \
        format == AV_PIX_FMT_NV20LE || \
        format == AV_PIX_FMT_NV20BE || \
        format == AV_PIX_FMT_P010LE || \
        format == AV_PIX_FMT_P010BE || \
        format == AV_PIX_FMT_P016LE || \
        format == AV_PIX_FMT_P016BE || \
        format == AV_PIX_FMT_NV24 || \
        format == AV_PIX_FMT_NV42 || \
        format == AV_PIX_FMT_NV20)

        int bitDepth = desc->comp[0].depth;
        ImColorSpace color_space =  avfrm->colorspace == AVCOL_SPC_BT470BG ||
                                    avfrm->colorspace == AVCOL_SPC_SMPTE170M ||
                                    avfrm->colorspace == AVCOL_SPC_BT470BG ? IM_CS_BT601 :
                                    avfrm->colorspace == AVCOL_SPC_BT709 ? IM_CS_BT709 :
                                    avfrm->colorspace == AVCOL_SPC_BT2020_NCL ||
                                    avfrm->colorspace == AVCOL_SPC_BT2020_CL ? IM_CS_BT2020 : IM_CS_BT709;
        ImColorRange color_range =  avfrm->color_range == AVCOL_RANGE_MPEG ? IM_CR_NARROW_RANGE :
                                    avfrm->color_range == AVCOL_RANGE_JPEG ? IM_CR_FULL_RANGE : IM_CR_NARROW_RANGE;
        ImColorFormat color_format = ISYUV420P(avfrm->format) ? IM_CF_YUV420 :
                                    ISYUV422P(avfrm->format) ? IM_CF_YUV422 :
                                    ISYUV444P(avfrm->format) ? IM_CF_YUV444 :
                                    ISNV12(avfrm->format) ? bitDepth == 10 ? IM_CF_P010LE : IM_CF_NV12 : IM_CF_YUV420;
        const int width = avfrm->width;
        const int height = avfrm->height;
        // int bytesPerPix = bitDepth > 8 ? 2 : 1;

        ImGui::ImMat mat_V;
        mat_V.create_type(width, height, 4, bitDepth > 8 ? IM_DT_INT16 : IM_DT_INT8);
        for (int i = 0; i < desc->nb_components; i++)
        {
            ImGui::ImMat ch = mat_V.channel(i);
            int chWidth = width;
            int chHeight = height;
            if ((desc->flags&AV_PIX_FMT_FLAG_RGB) == 0 && i > 0)
            {
                chWidth >>= desc->log2_chroma_w;
                chHeight >>= desc->log2_chroma_h;
            }
            if (desc->nb_components > i && desc->comp[i].plane == i)
            {
                uint8_t* src_data = avfrm->data[i]+desc->comp[i].offset;
                uint8_t* dst_data = (uint8_t*)ch.data;
                int bytesPerLine = chWidth*desc->comp[i].step;
                for (int j = 0; j < chHeight; j++)
                {
                    memcpy(dst_data, src_data, bytesPerLine);
                    src_data += avfrm->linesize[i];
                    dst_data += bytesPerLine;
                }
            }
        }

        mat_V.color_space = color_space;
        mat_V.color_range = color_range;
        mat_V.color_format = color_format;
        mat_V.depth = bitDepth;
        mat_V.flags = IM_MAT_FLAGS_VIDEO_FRAME;
        if (avfrm->pict_type == AV_PICTURE_TYPE_I) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_I;
        if (avfrm->pict_type == AV_PICTURE_TYPE_P) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_P;
        if (avfrm->pict_type == AV_PICTURE_TYPE_B) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_B;
        if (avfrm->interlaced_frame) mat_V.flags |= IM_MAT_FLAGS_VIDEO_INTERLACED;

        vmat = mat_V;
        if (swfrm)
            av_frame_free(&swfrm);
        return true;
    }

    void StartAllThreads()
    {
        m_quitPlay = false;
        m_demuxThread = thread(&MediaPlayer_FFImpl::DemuxThreadProc, this);
        if (HasVideo())
            m_viddecThread = thread(&MediaPlayer_FFImpl::VideoDecodeThreadProc, this);
        if (HasAudio())
        {
            m_auddecThread = thread(&MediaPlayer_FFImpl::AudioDecodeThreadProc, this);
            m_audswrThread = thread(&MediaPlayer_FFImpl::SwrThreadProc, this);
        }
        m_renderThread = thread(&MediaPlayer_FFImpl::RenderThreadProc, this);
    }

    void WaitAllThreadsQuit()
    {
        m_quitPlay = true;
        if (m_demuxThread.joinable())
        {
            m_demuxThread.join();
            m_demuxThread = thread();
        }
        if (m_viddecThread.joinable())
        {
            m_viddecThread.join();
            m_viddecThread = thread();
        }
        if (m_auddecThread.joinable())
        {
            m_auddecThread.join();
            m_auddecThread = thread();
        }
        if (m_audswrThread.joinable())
        {
            m_audswrThread.join();
            m_audswrThread = thread();
        }
        if (m_renderThread.joinable())
        {
            m_renderThread.join();
            m_renderThread = thread();
        }
        m_isPlaying = false;
    }

    void FlushAllQueues()
    {
        for (AVPacket* avpkt : m_vidpktQ)
            av_packet_free(&avpkt);
        m_vidpktQ.clear();
        for (AVPacket* avpkt : m_audpktQ)
            av_packet_free(&avpkt);
        m_audpktQ.clear();
        for (AVFrame* avfrm : m_vidfrmQ)
            av_frame_free(&avfrm);
        m_vidfrmQ.clear();
        for (AVFrame* avfrm : m_audfrmQ)
            av_frame_free(&avfrm);
        m_audfrmQ.clear();
        for (AVFrame* avfrm : m_swrfrmQ)
            av_frame_free(&avfrm);
        m_swrfrmQ.clear();
    }

    string MillisecToString(int64_t millisec)
    {
        ostringstream oss;
        if (millisec < 0)
        {
            oss << "-";
            millisec = -millisec;
        }
        uint64_t t = (uint64_t) millisec;
        uint32_t milli = (uint32_t)(t%1000); t /= 1000;
        uint32_t sec = (uint32_t)(t%60); t /= 60;
        uint32_t min = (uint32_t)(t%60); t /= 60;
        uint32_t hour = (uint32_t)t;
        oss << setfill('0') << setw(2) << hour << ':'
            << setw(2) << min << ':'
            << setw(2) << sec << '.'
            << setw(3) << milli;
        return oss.str();
    }

    class AudioByteStream : public AudioRender::ByteStream
    {
    public:
        AudioByteStream(MediaPlayer_FFImpl* outterThis) : m_outterThis(outterThis) {}

        uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override
        {
            uint32_t loadSize = 0;
            if (m_unconsumedAudfrm)
            {
                uint32_t copySize = m_frmPcmDataSize-m_consumedPcmDataSize;
                if (copySize > buffSize)
                    copySize = buffSize;
                memcpy(buff, m_unconsumedAudfrm->data[0]+m_consumedPcmDataSize, copySize);
                loadSize += copySize;
                m_consumedPcmDataSize += copySize;
                if (m_consumedPcmDataSize >= m_frmPcmDataSize)
                {
                    av_frame_free(&m_unconsumedAudfrm);
                    m_unconsumedAudfrm = nullptr;
                    m_frmPcmDataSize = 0;
                    m_consumedPcmDataSize = 0;
                }
            }
            bool tsUpdate = false;
            Duration audTs;
            while (loadSize < buffSize)
            {
                bool idleLoop = true;
                AVFrame* audfrm = nullptr;
                if (m_outterThis->m_swrfrmQ.empty())
                {
                    if (m_outterThis->m_auddecEof)
                        break;
                }
                else
                {
                    audfrm = m_outterThis->m_swrfrmQ.front();
                    lock_guard<mutex> lk(m_outterThis->m_swrfrmQLock);
                    m_outterThis->m_swrfrmQ.pop_front();
                }

                if (audfrm != nullptr)
                {
                    if (m_frameSize == 0)
                    {
                        uint32_t bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)audfrm->format);
                        m_frameSize = bytesPerSample*m_outterThis->m_swrOutChannels;
                    }
                    uint32_t frmPcmDataSize = m_frameSize*audfrm->nb_samples;
                    tsUpdate = true;
                    audTs = Duration(av_rescale_q(audfrm->pts, m_outterThis->m_audStream->time_base, MILLISEC_TIMEBASE));

                    uint32_t copySize = buffSize-loadSize;
                    if (copySize > frmPcmDataSize)
                        copySize = frmPcmDataSize;
                    memcpy(buff+loadSize, audfrm->data[0], copySize);
                    loadSize += copySize;
                    if (copySize < frmPcmDataSize)
                    {
                        m_unconsumedAudfrm = audfrm;
                        m_frmPcmDataSize = frmPcmDataSize;
                        m_consumedPcmDataSize = copySize;
                    }
                    else
                    {
                        av_frame_free(&audfrm);
                    }
                    idleLoop = false;
                }
                else if (!blocking)
                    break;
                if (idleLoop)
                    this_thread::sleep_for(chrono::milliseconds(5));
            }
            if (tsUpdate)
                m_outterThis->m_audioTs = audTs;
            return loadSize;
        }

        void Reset()
        {
            if (m_unconsumedAudfrm)
            {
                av_frame_free(&m_unconsumedAudfrm);
                m_unconsumedAudfrm = nullptr;
            }
            m_frmPcmDataSize = 0;
            m_consumedPcmDataSize = 0;
            m_frameSize = 0;
        }

    private:
        MediaPlayer_FFImpl* m_outterThis;
        AVFrame* m_unconsumedAudfrm{nullptr};
        uint32_t m_frmPcmDataSize{0};
        uint32_t m_consumedPcmDataSize{0};
        uint32_t m_frameSize{0};
    };

private:
    string m_errMessage;
    bool m_vidPreferUseHw{true};
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};
    PlayMode m_playMode{PlayMode::NORMAL};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVStream* m_vidStream{nullptr};
    AVStream* m_audStream{nullptr};
    const AVCodec* m_viddec{nullptr};
    const AVCodec* m_auddec{nullptr};
    AVCodecContext* m_viddecCtx{nullptr};
    AVCodecContext* m_auddecCtx{nullptr};
    AVPixelFormat m_vidHwPixFmt{AV_PIX_FMT_NONE};
    AVHWDeviceType m_viddecDevType{AV_HWDEVICE_TYPE_NONE};
    AVBufferRef* m_viddecHwDevCtx{nullptr};
    SwrContext* m_swrCtx{nullptr};
    AVSampleFormat m_swrOutSmpfmt{AV_SAMPLE_FMT_S16};
    int m_swrOutSampleRate;
    int m_swrOutChannels;
    int64_t m_swrOutChnLyt;

    // demuxing thread
    thread m_demuxThread;
    float m_vidpktQDuration{2.0f};
    int m_vidpktQMaxSize{0};
    list<AVPacket*> m_vidpktQ;
    mutex m_vidpktQLock;
    int m_audpktQMaxSize{0};
    list<AVPacket*> m_audpktQ;
    mutex m_audpktQLock;
    bool m_demuxEof{false};
    // video decoding thread
    thread m_viddecThread;
    int m_vidfrmQMaxSize{4};
    list<AVFrame*> m_vidfrmQ;
    mutex m_vidfrmQLock;
    bool m_viddecEof{false};
    // audio decoding thread
    thread m_auddecThread;
    int m_audfrmQMaxSize{4};
    list<AVFrame*> m_audfrmQ;
    mutex m_audfrmQLock;
    bool m_auddecEof{false};
    // pcm format conversion thread
    thread m_audswrThread;
    int m_swrfrmQMaxSize{4};
    list<AVFrame*> m_swrfrmQ;
    mutex m_swrfrmQLock;
    bool m_swrPassThrough{false};
    bool m_swrEof{false};
    // rendering thread
    thread m_renderThread;
    bool m_renderEof{false};

    recursive_mutex m_ctlLock;
    bool m_quitPlay{false};
    bool m_isPlaying{false};
    bool m_afterSeeking{false};

    bool m_isSeeking{false};
    list<uint64_t> m_seekPosList;
    mutex m_seekPosListLock;

    Duration m_audioTs;
    int64_t m_audioOffset{0};
    Duration m_playPos, m_pausedDur;
    TimePoint m_runStartTp{CLOCK_MIN}, m_pauseStartTp{CLOCK_MIN};

    ImGui::ImMat m_vidMat;
    AudioRender* m_audrnd{nullptr};
    AudioByteStream m_audByteStream;
};

constexpr MediaPlayer_FFImpl::TimePoint MediaPlayer_FFImpl::CLOCK_MIN = MediaPlayer_FFImpl::Clock::time_point::min();
const AVRational MediaPlayer_FFImpl::MILLISEC_TIMEBASE = { 1, 1000 };
const AVRational MediaPlayer_FFImpl::FFAV_TIMEBASE = { 1, AV_TIME_BASE };

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaPlayer_FFImpl* mp = reinterpret_cast<MediaPlayer_FFImpl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (mp->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

MediaPlayer* CreateMediaPlayer()
{
    return static_cast<MediaPlayer*>(new MediaPlayer_FFImpl());
}

void ReleaseMediaPlayer(MediaPlayer** player)
{
    if (!player)
        return;
    MediaPlayer_FFImpl* ffPlayer = dynamic_cast<MediaPlayer_FFImpl*>(*player);
    ffPlayer->Close();
    delete ffPlayer;
    *player = nullptr;
}