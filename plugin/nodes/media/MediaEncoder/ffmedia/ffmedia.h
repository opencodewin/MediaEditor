#ifndef __FFMEDIA_H__
#define __FFMEDIA_H__

#include "ffmedia_define.h"

#include <mutex>
#include <thread>
#include <vector>
#include <string>

class FFMedia_HWInfo
{
public:
    void reset(bool force_cpu = false, bool keep_gpu_output = false){
        hw_type = AV_HWDEVICE_TYPE_NONE;
        hw_device = -1;
        hw_keep = false;
        if (!force_cpu)
            hw_type = find_hwdevice_type();
        if (hw_type != AV_HWDEVICE_TYPE_NONE)
        {
            hw_device = 0;
            hw_keep = keep_gpu_output;
        }
    }

    static bool check_hwdevice_support(enum AVHWDeviceType type)
    {
        int ret;
        // TODO::need add more hw support
        if (type != AV_HWDEVICE_TYPE_CUDA &&
            type != AV_HWDEVICE_TYPE_VIDEOTOOLBOX &&
            type != AV_HWDEVICE_TYPE_DXVA2 &&
            type != AV_HWDEVICE_TYPE_D3D11VA)
            return false;

        AVBufferRef * buffer = nullptr;
        buffer = av_hwdevice_ctx_alloc(type);
        if (!buffer)
            return false;
        ret = av_hwdevice_ctx_init(buffer);
        if (ret == 0)
        {
            av_buffer_unref(&buffer);
            return true;
        }
        else
        {
            av_buffer_unref(&buffer);
            return false;
        }
    }

    static enum AVHWDeviceType find_hwdevice_type()
    {
        enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;

        while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        {
            if (check_hwdevice_support(type))
                break;
        }
        return type;
    }

    int update_encoder_caps(AVCodecID codec_id, bool &use_hw, bool &is_10bit)
    {
        if (hw_type == AV_HWDEVICE_TYPE_NONE || !use_hw)
        {
            use_hw = false;
            if (is_10bit)
                return AV_PIX_FMT_YUV420P10LE;
            else
                return AV_PIX_FMT_YUV420P;
        }

        if (hw_type == AV_HWDEVICE_TYPE_CUDA)
        {
            if (codec_id == AV_CODEC_ID_HEVC || codec_id == AV_CODEC_ID_H265)
            {
                if (is_10bit)
                    return AV_PIX_FMT_P010LE;
                else
                    return AV_PIX_FMT_NV12;
            }
            else if (codec_id == AV_CODEC_ID_H264)
            {
                is_10bit = false;
                return AV_PIX_FMT_NV12;
            }
        }

        if (hw_type == AV_HWDEVICE_TYPE_VIDEOTOOLBOX)
        {
            if (codec_id == AV_CODEC_ID_HEVC || codec_id == AV_CODEC_ID_H265)
            {
                if (is_10bit)
                    return AV_PIX_FMT_P010LE;
                else
                    return AV_PIX_FMT_NV12;
            }
            else if (codec_id == AV_CODEC_ID_H264)
            {
                is_10bit = false;
                return AV_PIX_FMT_NV12;
            }
        }

        if (hw_type == AV_HWDEVICE_TYPE_DXVA2 || hw_type == AV_HWDEVICE_TYPE_D3D11VA)
        {
            is_10bit = false;
            return AV_PIX_FMT_NV12;
        }

        is_10bit = false;
        return AV_PIX_FMT_NONE;
    }

    enum AVHWDeviceType hw_type {AV_HWDEVICE_TYPE_NONE};
    int hw_device {-1};
    bool hw_keep {false};
};

class FFMediaStream
{
public:
    FFMediaStream(int idx, FFMEDIA_STREAMTYPE type);
    ~FFMediaStream();
    AVStream* stream = nullptr;
    int index;
    FFMEDIA_STREAMTYPE type;

    // video
    int width;
    int height;
    AVRational frame_rate;
    AVPixelFormat pixel_format;
    FFMEDIA_HDRTYPE hdr_type;
    int bit_depth;
    bool hw_output;    
    FFMedia_HWInfo* hw_info;

    // audio
    int sample_rate;
    int channels;
    AVSampleFormat sample_fmt;

    // general
    AVRational tbc;     // codec timebase
    AVRational tbn;     // stream timebase
    AVCodecContext* codec_ctx = nullptr;
    enum AVCodecID codec_id;
    int bit_rate;

    // control
    bool activated;  // true for frame stream, which is decoded for source or needs encode for sink,
                     // false for packet stream
    bool eof;

    // streaming
    void* streaming_queue = nullptr;
    int read_cnt;
    int drop_cnt;
    int64_t start_pts;
    int64_t next_pts;
    int64_t pts_offset;

    // extra encoder params
    AVDictionary* extra_options = nullptr;
    int write_cnt;

    /* audio fifo */
    AVFrame* audio_frame;
    AVAudioFifo* audio_fifo;
    size_t audio_framesize;
    int audio_sample_cnt;
    int64_t fifo_start_pts;
};

class FFMediaSink
{

public:
    FFMediaSink(bool force_cpu = false);
    ~FFMediaSink();
    FFMEDIA_RETVALUE open(const char* name);
    FFMEDIA_RETVALUE close();
    FFMEDIA_RETVALUE start_streaming();
    FFMEDIA_RETVALUE stop_streaming();
    int compose_stream(FFMediaStream* pStream, AVDictionary* extra_options = nullptr);
    int get_video_stream_idx(const int index = -1);
    int get_audio_stream_idx(const int index = -1);
    FFMediaStream* get_stream(const int stream_idx);
    FFMEDIA_RETVALUE write_one_frame(const AVFrame* pFrame, const int stream_idx);
    FFMEDIA_RETVALUE write_one_packet(const AVPacket* pPkt);
    const FFMedia_HWInfo* get_hw_info();
    void reset_hw_info(bool force_cpu = false);
    int update_encoder_caps(AVCodecID codec_id, bool &use_hw, bool &is_10bit);

private:
    FFMEDIA_RETVALUE add_audio_stream(FFMediaStream* pStream);
    FFMEDIA_RETVALUE add_video_stream(FFMediaStream* pStream);
    AVPacket* encode_one_frame(AVFrame* pFrame, const int stream_idx);
    FFMEDIA_RETVALUE write_audio_frame(const AVFrame* pFrame, const int stream_idx);
    std::string m_name;
    std::string m_container;
    std::string m_extra_params;
    const static int m_max_stream_num = 16;
    const bool m_restrict_check = false;

    AVFormatContext* m_AvFmtCtx;
    int m_stream_num;
    AVStream** m_streams;
    bool m_streaming;  // in streaming flag

    /* cuda video codec*/
    FFMedia_HWInfo m_hw_info;

    /* active streams*/
    std::vector<FFMediaStream*> m_active_streams;
};

#endif  //__FFMEDIA_H__
