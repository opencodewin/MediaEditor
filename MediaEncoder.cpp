#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <list>
#include "MediaEncoder.h"
#include "FFUtils.h"
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
using namespace Logger;

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

class MediaEncoder_Impl : public MediaEncoder
{
public:
    static ALogger* s_logger;

    MediaEncoder_Impl()
    {
        m_logger = GetMediaEncoderLogger();
    }

    MediaEncoder_Impl(const MediaEncoder_Impl&) = delete;
    MediaEncoder_Impl(MediaEncoder_Impl&&) = delete;
    MediaEncoder_Impl& operator=(const MediaEncoder_Impl&) = delete;

    virtual ~MediaEncoder_Impl() {}

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_opened)
            Close();

        if (!Open_Internal(url))
        {
            Close();
            return false;
        }

        m_opened = true;
        return true;
    }

    bool Close() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        int fferr;
        bool success = true;

        TerminateAllThreads();
        FlushAllQueues();

        if (m_avfmtCtx)
        {
            fferr = av_write_trailer(m_avfmtCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("av_write_trailer", fferr);
                success = false;
            }
            if ((m_avfmtCtx->oformat->flags&AVFMT_NOFILE) == 0)
                avio_closep(&m_avfmtCtx->pb);
            avformat_free_context(m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }

        CloseVideoComponents();
        CloseAudioComponents();

        m_muxEof = false;
        m_started = false;
        m_opened = false;

        return success;
    }

    int ConfigureVideoStream(const std::string& codecName,
            string& imageFormat, uint32_t width, uint32_t height,
            const MediaInfo::Ratio& frameRate, uint64_t bitRate,
            unordered_map<string, string>* extraOpts = nullptr) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return -1;
        }
        if (m_started)
        {
            m_errMsg = "This MediaEncoder already started!";
            return -1;
        }

        if (ConfigureVideoStream_Internal(codecName, imageFormat, width, height, frameRate, bitRate, extraOpts))
        {
            m_vidStmIdx = m_vidAvStm->index;
            imageFormat = av_get_pix_fmt_name(m_videncPixfmt);
        }
        else
        {
            CloseVideoComponents();
        }

        return m_vidStmIdx;
    }

    int ConfigureAudioStream(const std::string& codecName,
            string& sampleFormat, uint32_t channels, uint32_t sampleRate, uint64_t bitRate) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return -1;
        }
        if (m_started)
        {
            m_errMsg = "This MediaEncoder already started!";
            return -1;
        }

        if (ConfigureAudioStream_Internal(codecName, sampleFormat, channels, sampleRate, bitRate))
        {
            m_audStmIdx = m_audAvStm->index;
            sampleFormat = av_get_sample_fmt_name(m_audencSmpfmt);
        }
        else
        {
            CloseAudioComponents();
        }

        return m_audStmIdx;
    }

    bool Start() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        if (m_started)
        {
            m_errMsg = "This MediaEncoder already started!";
            return false;
        }
        if (!HasVideo() && !HasAudio())
        {
            m_errMsg = "No video nor audio stream has been added!";
            return false;
        }

        StartAllThreads();
        m_started = true;
        return true;
    }

    bool WaitAndFinishEncoding() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        if (!m_started)
        {
            m_errMsg = "This MediaEncoder has NOT started yet!";
            return false;
        }

        while (!m_muxEof)
            this_thread::sleep_for(chrono::milliseconds(5));

        bool success = true;
        int fferr;
        if (m_avfmtCtx)
        {
            fferr = av_write_trailer(m_avfmtCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("av_write_trailer", fferr);
                success = false;
            }
            if ((m_avfmtCtx->oformat->flags&AVFMT_NOFILE) == 0)
                avio_closep(&m_avfmtCtx->pb);
            avformat_free_context(m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }

        return success;
    }

    bool EncodeVideoFrame(int streamIndex, ImGui::ImMat vmat) override
    {
        return true;
    }

    bool EncodeAudioSamples(int streamIndex, uint8_t* buf, uint32_t size) override
    {
        return true;
    }

    bool IsOpened() const override
    {
        return m_opened;
    }

    bool HasVideo() const override
    {
        return m_vidStmIdx >= 0;
    }

    bool HasAudio() const override
    {
        return m_audStmIdx >= 0;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

    bool CheckHwPixFmt(AVPixelFormat pixfmt)
    {
        return pixfmt == m_videncPixfmt;
    }

private:
    string FFapiFailureMessage(const string& apiName, int fferr)
    {
        ostringstream oss;
        oss << "FF api '" << apiName << "' returns error! fferr=" << fferr << ".";
        return oss.str();
    }

    bool Open_Internal(const string& url)
    {
        int fferr;

        fferr = avformat_alloc_output_context2(&m_avfmtCtx, nullptr, nullptr, url.c_str());
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avformat_alloc_output_context2", fferr);
            return false;
        }

        if ((m_avfmtCtx->oformat->flags&AVFMT_NOFILE) == 0)
        {
            fferr = avio_open(&m_avfmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("avio_open", fferr);
                return false;
            }
        }
        return true;
    }

    bool ConfigureVideoStream_Internal(const std::string& codecName,
            string& imageFormat, uint32_t width, uint32_t height,
            const MediaInfo::Ratio& frameRate, uint64_t bitRate,
            unordered_map<string, string>* extraOpts)
    {
        m_videnc = avcodec_find_encoder_by_name(codecName.c_str());
        if (!m_videnc)
        {
            ostringstream oss;
            oss << "Can NOT find encoder by name '" << codecName << "'!";
            m_errMsg = oss.str();
            return false;
        }
        else if (m_videnc->type != AVMEDIA_TYPE_VIDEO)
        {
            ostringstream oss;
            oss << "Codec name '" << codecName << "' is NOT for an VIDEO encoder!";
            m_errMsg = oss.str();
            return false;
        }

        if (!imageFormat.empty())
        {
            m_videncPixfmt = av_get_pix_fmt(imageFormat.c_str());
            if (m_videncPixfmt < 0)
            {
                ostringstream oss;
                oss << "INVALID image format '" << imageFormat << "'!";
                m_errMsg = oss.str();
                return false;
            }
        }
        else
        {
            if (m_videnc->pix_fmts)
                m_videncPixfmt = m_videnc->pix_fmts[0];
            else
                m_videncPixfmt = AV_PIX_FMT_YUV420P;
        }

        m_videncCtx = avcodec_alloc_context3(m_videnc);
        if (!m_videncCtx)
        {
            m_errMsg = "FAILED to allocate AVCodecContext by 'avcodec_alloc_context3'!";
            return false;
        }

        int fferr;
        if (m_vidPreferUseHw)
        {
            for (int i = 0; ; i++)
            {
                const AVCodecHWConfig* config = avcodec_get_hw_config(m_videnc, i);
                if (!config)
                {
                    ostringstream oss;
                    oss << "Encoder '" << m_videnc->name << "' does NOT support hardware acceleration.";
                    m_errMsg = oss.str();
                    return false;
                }
                if ((config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0)
                {
                    if (m_vidUseHwType == AV_HWDEVICE_TYPE_NONE || m_vidUseHwType == config->device_type)
                    {
                        m_videncPixfmt = config->pix_fmt;
                        m_viddecDevType = config->device_type;
                        break;
                    }
                }
            }
            m_logger->Log(DEBUG) << "Use hardware device type '" << av_hwdevice_get_type_name(m_viddecDevType) << "'." << endl;

            m_videncCtx->opaque = this;
            m_videncCtx->get_format = get_hw_format;

            fferr = av_hwdevice_ctx_create(&m_videncHwDevCtx, m_viddecDevType, nullptr, nullptr, 0);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("av_hwdevice_ctx_create", fferr);
                return false;
            }
            m_videncCtx->hw_device_ctx = av_buffer_ref(m_videncHwDevCtx);
        }

        if (!m_imgCvter.SetOutSize(width, height))
        {
            ostringstream oss;
            oss << "FAILED to set 'ImMatToAVFrameConverter' with out-size " << width << "x" << height << "!";
            m_errMsg = oss.str();
            return false;
        }
        if (!m_imgCvter.SetOutPixelFormat(m_videncPixfmt))
        {
            ostringstream oss;
            oss << "FAILED to set 'ImMatToAVFrameConverter' with pixel-format '" << av_get_pix_fmt_name(m_videncPixfmt) << "'!";
            m_errMsg = oss.str();
            return false;
        }

        m_videncCtx->pix_fmt = m_videncPixfmt;
        m_videncCtx->width = width;
        m_videncCtx->height = height;
        m_videncCtx->time_base = { frameRate.den, frameRate.num };
        m_videncCtx->framerate = { frameRate.num, frameRate.den };
        m_videncCtx->bit_rate = bitRate;
        m_videncCtx->sample_aspect_ratio = { 1, 1 };

        AVDictionary *encOpts = nullptr;
        if (extraOpts)
        {
            auto iter = extraOpts->begin();
            while (iter != extraOpts->end())
            {
                auto& elem = *iter++;
                if (elem.first == ENCOPT__PROFILE)
                    av_dict_set(&encOpts, "profile", elem.second.c_str(), 0);
                else if (elem.first == ENCOPT__PRESET)
                    av_dict_set(&encOpts, "preset", elem.second.c_str(), 0);
                else if (elem.first == ENCOPT__MAX_B_FRAMES)
                    m_videncCtx->max_b_frames = stoi(elem.second);
                else if (elem.first == ENCOPT__GOP_SIZE)
                    m_videncCtx->gop_size = stoi(elem.second);
                else if (elem.first == ENCOPT__ASPECT_RATIO)
                {
                    int num = 1, den = 1;
                    size_t pos = elem.second.find('/');
                    if (pos == string::npos)
                    {
                        ostringstream oss;
                        oss << "INVALID encoder extra-option '" << ENCOPT__ASPECT_RATIO "' value '" << elem.second << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                    else
                    {
                        num = stoi(elem.second.substr(0, pos));
                        den = stoi(elem.second.substr(pos+1));
                    }
                    m_videncCtx->sample_aspect_ratio = { num, den };
                }
                else if (elem.first == ENCOPT__COLOR_RANGE)
                {
                    m_videncCtx->color_range = (AVColorRange)av_color_range_from_name(elem.second.c_str());
                    if (m_videncCtx->color_range < 0)
                    {
                        ostringstream oss;
                        oss << "INVALID encoder extra-option '" << ENCOPT__COLOR_RANGE "' value '" << elem.second << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                    if (!m_imgCvter.SetOutColorRange(m_videncCtx->color_range))
                    {
                        ostringstream oss;
                        oss << "FAILED to set 'ImMatToAVFrameConverter' with color-range '" << av_color_range_name(m_videncCtx->color_range) << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                }
                else if (elem.first == ENCOPT__COLOR_SPACE)
                {
                    m_videncCtx->colorspace = (AVColorSpace)av_color_space_from_name(elem.second.c_str());
                    if (m_videncCtx->colorspace < 0)
                    {
                        ostringstream oss;
                        oss << "INVALID encoder extra-option '" << ENCOPT__COLOR_SPACE "' value '" << elem.second << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                    if (!m_imgCvter.SetOutColorSpace(m_videncCtx->colorspace))
                    {
                        ostringstream oss;
                        oss << "FAILED to set 'ImMatToAVFrameConverter' with color-space '" << av_color_space_name(m_videncCtx->colorspace) << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                }
                else if (elem.first == ENCOPT__COLOR_PRIMARIES)
                {
                    m_videncCtx->color_primaries = (AVColorPrimaries)av_color_primaries_from_name(elem.second.c_str());
                    if (m_videncCtx->color_primaries < 0)
                    {
                        ostringstream oss;
                        oss << "INVALID encoder extra-option '" << ENCOPT__COLOR_PRIMARIES "' value '" << elem.second << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                }
                else if (elem.first == ENCOPT__COLOR_TRC)
                {
                    m_videncCtx->color_trc = (AVColorTransferCharacteristic)av_color_transfer_from_name(elem.second.c_str());
                    if (m_videncCtx->color_trc < 0)
                    {
                        ostringstream oss;
                        oss << "INVALID encoder extra-option '" << ENCOPT__COLOR_TRC "' value '" << elem.second << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                }
                else if (elem.first == ENCOPT__CHROMA_LOCATION)
                {
                    m_videncCtx->chroma_sample_location = (AVChromaLocation)av_chroma_location_from_name(elem.second.c_str());
                    if (m_videncCtx->chroma_sample_location < 0)
                    {
                        ostringstream oss;
                        oss << "INVALID encoder extra-option '" << ENCOPT__CHROMA_LOCATION "' value '" << elem.second << "'!";
                        m_errMsg = oss.str();
                        return false;
                    }
                }
                else
                {
                    ostringstream oss;
                    oss << "UNKNOWN encoder extra-option name '" << elem.first << "'!";
                    m_errMsg = oss.str();
                    return false;
                }
            }
        }

        fferr = avcodec_open2(m_videncCtx, m_videnc, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            return false;
        }

        m_vidAvStm = avformat_new_stream(m_avfmtCtx, m_videnc);
        if (!m_vidAvStm)
        {
            m_errMsg = "FAILED to create new stream by 'avformat_new_stream'!";
            return false;
        }
        m_vidAvStm->id = m_avfmtCtx->nb_streams-1;
        if ((m_avfmtCtx->oformat->flags&AVFMT_GLOBALHEADER) != 0)
            m_videncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; 
        m_vidAvStm->time_base = m_videncCtx->time_base;
        m_vidAvStm->avg_frame_rate = m_videncCtx->framerate;
        avcodec_parameters_from_context(m_vidAvStm->codecpar, m_videncCtx);

        return true;
    }

    void CloseVideoComponents()
    {
        if (m_videncCtx)
        {
            avcodec_free_context(&m_videncCtx);
            m_videncCtx = nullptr;
        }
        m_videnc = nullptr;
        if (m_videncHwDevCtx)
        {
            av_buffer_unref(&m_videncHwDevCtx);
            m_videncHwDevCtx = nullptr;
        }
        m_viddecDevType = AV_HWDEVICE_TYPE_NONE;
        m_videncPixfmt = AV_PIX_FMT_NONE;
        m_vidAvStm = nullptr;
        m_vidStmIdx = -1;
        m_vidinpEof = false;
        m_videncEof = false;
    }

    bool ConfigureAudioStream_Internal(const std::string& codecName,
            string& sampleFormat, uint32_t channels, uint32_t sampleRate, uint64_t bitRate)
    {
        m_audenc = avcodec_find_encoder_by_name(codecName.c_str());
        if (!m_audenc)
        {
            ostringstream oss;
            oss << "Can NOT find encoder by name '" << codecName << "'!";
            m_errMsg = oss.str();
            return false;
        }
        else if (m_audenc->type != AVMEDIA_TYPE_AUDIO)
        {
            ostringstream oss;
            oss << "Codec name '" << codecName << "' is NOT for an AUDIO encoder!";
            m_errMsg = oss.str();
            return false;
        }

        if (!sampleFormat.empty())
        {
            m_audencSmpfmt = av_get_sample_fmt(sampleFormat.c_str());
            if (m_audencSmpfmt < 0)
            {
                ostringstream oss;
                oss << "INVALID sample format '" << sampleFormat << "'!";
                m_errMsg = oss.str();
                return false;
            }
        }
        else
        {
            if (m_audenc->sample_fmts)
                m_audencSmpfmt = m_audenc->sample_fmts[0];
            else
                m_audencSmpfmt = AV_SAMPLE_FMT_FLT;
        }

        m_audencCtx = avcodec_alloc_context3(m_audenc);
        if (!m_audencCtx)
        {
            m_errMsg = "FAILED to allocate AVCodecContext by 'avcodec_alloc_context3'!";
            return false;
        }

        m_audencCtx->sample_fmt = m_audencSmpfmt;
        m_audencCtx->channels = channels;
        m_audencCtx->sample_rate = sampleRate;
        m_audencCtx->bit_rate = bitRate;
        m_audencCtx->time_base = { 1, (int)sampleRate };

        int fferr;
        fferr = avcodec_open2(m_audencCtx, m_audenc, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            return false;
        }

        m_audAvStm = avformat_new_stream(m_avfmtCtx, m_audenc);
        if (!m_audAvStm)
        {
            m_errMsg = "FAILED to create new stream by 'avformat_new_stream'!";
            return false;
        }
        m_audAvStm->id = m_avfmtCtx->nb_streams-1;
        if ((m_avfmtCtx->oformat->flags&AVFMT_GLOBALHEADER) != 0)
            m_audencCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; 
        m_audAvStm->time_base = m_audencCtx->time_base;
        avcodec_parameters_from_context(m_audAvStm->codecpar, m_audencCtx);

        return true;
    }

    void CloseAudioComponents()
    {
        if (m_audencCtx)
        {
            avcodec_free_context(&m_audencCtx);
            m_audencCtx = nullptr;
        }
        m_audenc = nullptr;
        m_audencSmpfmt = AV_SAMPLE_FMT_NONE;
        m_audAvStm = nullptr;
        m_audStmIdx = -1;
        m_audinpEof = false;
        m_audencEof = false;
    }

    void StartAllThreads()
    {
        m_quit = false;
        if (HasVideo())
            m_videncThread = thread(&MediaEncoder_Impl::VideoEncodingThreadProc, this);
        if (HasAudio())
            m_audencThread = thread(&MediaEncoder_Impl::AudioEncodingThreadProc, this);
        m_muxThread = thread(&MediaEncoder_Impl::MuxingThreadProc, this);
    }

    void TerminateAllThreads()
    {
        m_quit = true;
        if (m_videncThread.joinable())
            m_videncThread.join();
        if (m_audencThread.joinable())
            m_audencThread.join();
        if (m_muxThread.joinable())
            m_muxThread.join();
    }

    void FlushAllQueues()
    {
        m_vmatQ.clear();
        m_audfrmQ.clear();
    }

    SelfFreeAVFramePtr ConvertImMatToAVFrame(ImGui::ImMat& vmat)
    {
        if (vmat.empty())
            return nullptr;
        SelfFreeAVFramePtr vfrm = AllocSelfFreeAVFramePtr();
        if (!vfrm)
        {
            m_logger->Log(Error) << "FAILED to allocate new 'SelfFreeAVFramePtr'!";
            return nullptr;
        }
        int64_t pts = av_rescale_q((int64_t)(vmat.time_stamp*1000), MILLISEC_TIMEBASE, m_videncCtx->time_base);
        m_imgCvter.ConvertImage(vmat, vfrm.get(), pts);
        return vfrm;
    }

    void VideoEncodingThreadProc()
    {
        m_logger->Log(VERBOSE) << "Enter VideoEncodingThreadProc()..." << endl;

        SelfFreeAVFramePtr encfrm;
        while (m_quit)
        {
            bool idleLoop = true;

            if (!encfrm)
            {
                if (!m_vmatQ.empty())
                {
                    ImGui::ImMat vmat;
                    {
                        lock_guard<mutex> lk(m_vmatQLock);
                        vmat = m_vmatQ.front();
                        m_vmatQ.pop_front();
                    }
                    encfrm = ConvertImMatToAVFrame(vmat);
                }
                else if (m_vidinpEof)
                {
                    avcodec_send_frame(m_videncCtx, nullptr);
                    break;
                }
            }

            if (encfrm)
            {
                int fferr = avcodec_send_frame(m_videncCtx, encfrm.get());
                if (fferr == 0)
                {
                    encfrm = nullptr;
                }
                else
                {
                    if (fferr != AVERROR(EAGAIN))
                    {
                        m_logger->Log(Error) << "Video encoder ERROR! avcodec_send_frame() returns " << fferr << "." << endl;
                        break;
                    }
                }
                idleLoop = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        m_logger->Log(VERBOSE) << "Leave VideoEncodingThreadProc()." << endl;
    }

    void AudioEncodingThreadProc()
    {
        m_logger->Log(VERBOSE) << "Enter AudioEncodingThreadProc()..." << endl;

        SelfFreeAVFramePtr encfrm;
        while (m_quit)
        {
            bool idleLoop = true;

            if (!encfrm)
            {
                if (!m_audfrmQ.empty())
                {
                    lock_guard<mutex> lk(m_audfrmQLock);
                    encfrm = m_audfrmQ.front();
                    m_audfrmQ.pop_front();
                }
                else if (m_audinpEof)
                {
                    avcodec_send_frame(m_audencCtx, nullptr);
                    break;
                }
            }

            if (encfrm)
            {
                int fferr = avcodec_send_frame(m_audencCtx, encfrm.get());
                if (fferr == 0)
                {
                    encfrm = nullptr;
                }
                else
                {
                    if (fferr != AVERROR(EAGAIN))
                    {
                        m_logger->Log(Error) << "Audio encoder ERROR! avcodec_send_frame() returns " << fferr << "." << endl;
                        break;
                    }
                }
                idleLoop = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        m_logger->Log(VERBOSE) << "Leave AudioEncodingThreadProc()." << endl;
    }

    void MuxingThreadProc()
    {
        m_logger->Log(VERBOSE) << "Enter MuxingThreadProc()..." << endl;

        AVPacket avpkt{0};
        bool avpktLoaded = false;
        while (m_quit)
        {
            bool idleLoop = true;
            int fferr;

            if (m_videncCtx && !m_videncEof && !avpktLoaded)
            {
                fferr = avcodec_receive_packet(m_videncCtx, &avpkt);
                if (fferr == 0)
                {
                    avpkt.stream_index = m_vidStmIdx;
                    av_packet_rescale_ts(&avpkt, m_videncCtx->time_base, m_vidAvStm->time_base);
                    idleLoop = false;
                }
                else if (fferr == AVERROR_EOF)
                {
                    m_videncEof = true;
                    idleLoop = false;
                }
                else if (fferr != AVERROR(EAGAIN))
                {
                    m_logger->Log(Error) << "In muxing thread, video 'avcodec_receive_packet' FAILED with return code " << fferr << "!" << endl;
                    break;
                }
            }

            if (m_audencCtx && !m_audencEof && !avpktLoaded)
            {
                fferr = avcodec_receive_packet(m_audencCtx, &avpkt);
                if (fferr == 0)
                {
                    avpkt.stream_index = m_audStmIdx;
                    av_packet_rescale_ts(&avpkt, m_audencCtx->time_base, m_audAvStm->time_base);
                    idleLoop = false;
                }
                else if (fferr == AVERROR_EOF)
                {
                    m_audencEof = true;
                    idleLoop = false;
                }
                else if (fferr != AVERROR(EAGAIN))
                {
                    m_logger->Log(Error) << "In muxing thread, audio 'avcodec_receive_packet' FAILED with return code " << fferr << "!" << endl;
                    break;
                }
            }

            if (avpktLoaded)
            {
                fferr = av_interleaved_write_frame(m_avfmtCtx, &avpkt);
                if (fferr == 0)
                {
                    av_packet_unref(&avpkt);
                    avpktLoaded = false;
                    idleLoop = false;
                }
                else
                {
                    m_logger->Log(Error) << "'av_interleaved_write_frame' FAILED with return code " << fferr << "!" << endl;
                    break;
                }
            }
            else if ((!HasVideo() || m_videncEof) && (!HasAudio() || m_audencEof))
            {
                break;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        m_muxEof = true;
        m_logger->Log(VERBOSE) << "Leave MuxingThreadProc()." << endl;
    }

private:
    string m_errMsg;
    ALogger* m_logger;
    recursive_mutex m_apiLock;
    bool m_vidPreferUseHw{true};
    bool m_quit{false};
    bool m_opened{false};
    bool m_started{false};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
#if LIBAVFORMAT_VERSION_MAJOR >= 59
    const AVCodec* m_videnc{nullptr};
    const AVCodec* m_audenc{nullptr};
#else
    AVCodec* m_videnc{nullptr};
    AVCodec* m_audenc{nullptr};
#endif
    AVStream* m_vidAvStm{nullptr};
    AVStream* m_audAvStm{nullptr};
    AVCodecContext* m_videncCtx{nullptr};
    AVBufferRef* m_videncHwDevCtx{nullptr};
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};
    AVHWDeviceType m_viddecDevType{AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat m_videncPixfmt{AV_PIX_FMT_NONE};
    AVCodecContext* m_audencCtx{nullptr};
    AVSampleFormat m_audencSmpfmt{AV_SAMPLE_FMT_NONE};

    ImMatToAVFrameConverter m_imgCvter;

    // video encoding thread
    thread m_videncThread;
    list<ImGui::ImMat> m_vmatQ;
    uint32_t m_vmatQMaxSize;
    mutex m_vmatQLock;
    bool m_vidinpEof{false};
    bool m_videncEof{false};
    // audio encoding thread
    thread m_audencThread;
    list<SelfFreeAVFramePtr> m_audfrmQ;
    uint32_t m_audfrmQMaxSize;
    mutex m_audfrmQLock;
    bool m_audinpEof{false};
    bool m_audencEof{false};
    // muxing thread
    thread m_muxThread;
    list<AVPacket*> m_vidpktQ;
    mutex m_vidpktQLock;
    list<AVPacket*> m_audpktQ;
    mutex m_audpktQLock;
    bool m_muxEof{false};
};

ALogger* MediaEncoder_Impl::s_logger;

ALogger* GetMediaEncoderLogger()
{
    if (!MediaEncoder_Impl::s_logger)
        MediaEncoder_Impl::s_logger = GetLogger("MEncoder");
    return MediaEncoder_Impl::s_logger;
}

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaEncoder_Impl* ms = reinterpret_cast<MediaEncoder_Impl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (ms->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

MediaEncoder* CreateMediaEncoder()
{
    return new MediaEncoder_Impl();
}

void ReleaseMediaEncoder(MediaEncoder** menc)
{
    if (menc == nullptr || *menc == nullptr)
        return;
    MediaEncoder_Impl* mencoder = dynamic_cast<MediaEncoder_Impl*>(*menc);
    mencoder->Close();
    delete mencoder;
    *menc = nullptr;
}
