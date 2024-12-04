#ifndef __FFMEDIA_UTILS_H__
#define __FFMEDIA_UTILS_H__

#include "ffmedia.h"

enum FFTRANS_TYPE : int
{
    FFTRANS_TYPE_CLONE = 0,
    FFTRANS_TYPE_RESCALE = 1,
    FFTRANS_TYPE_REFORMAT = 2,
    FFTRANS_TYPE_RESAMPLE = 3,
    FFTRANS_TYPE_FAILED,
    FFTRANS_TYPE_RESERVED,
};

class FFVideoScalar
{

public:
    FFVideoScalar(int srcWidth, int srcHeight, AVPixelFormat srcFmt, int dstWidth, int dstHeight,
                  AVPixelFormat dstFmt, int threads = 0);
    ~FFVideoScalar();
    void process(AVFrame* srcFrame, AVFrame* dstFrame);

private:
    bool open_swscalar(int srcW, int srcH, AVPixelFormat srcFmt, int dstW, int dstH,
                       AVPixelFormat dstFmt);
    void freeSwsContextArray();
    void allocSwsContextArray(int srcW, int srcH, AVPixelFormat srcFormat, int dstW, int dstH,
                              AVPixelFormat dstFormat);
    void setSwsColorSpaceCoef(int colorspace);
    SwsContext* m_sws_ctx;

    const int m_threads;
    int m_src_width;
    int m_src_height;
    AVPixelFormat m_src_format;
    int m_dst_width;
    int m_dst_height;
    AVPixelFormat m_dst_format;
};

class FFAudioResampler
{
public:
    FFAudioResampler(int srcChs, int srcRate, AVSampleFormat srcFmt, int dstChs, int dstRate,
                     AVSampleFormat dstFmt);
    ~FFAudioResampler();
    int process(void* srcBuf, void* dstBuf, int srcSamples, int dstSamples);
    int src_channels()
    {
        return m_srcChs;
    }
    int src_frame_rate()
    {
        return m_srcRate;
    }
    AVSampleFormat src_format()
    {
        return m_srcFmt;
    }

private:
    int m_srcChs;
    int m_srcRate;
    AVSampleFormat m_srcFmt;
    int m_dstChs;
    int m_dstRate;
    AVSampleFormat m_dstFmt;

    /*  swscalar impl   */
    SwrContext* m_swrCtx;
};

typedef int (*PFN_FFMEDIA_FRAME_MALLOC)(AVFrame* pFrame, void* context);
typedef struct
{
    PFN_FFMEDIA_FRAME_MALLOC pfn_malloc;
    void* context;
} FFMEDIA_EXTERMALLOC_PARAM;
AVFrame* FFMediaStream_alloc_frame(const FFMediaStream* pStream, void* user_info = nullptr);
int init_fifo(AVAudioFifo** fifo, AVSampleFormat sample_fmt, int channels);
int add_samples_to_fifo(AVAudioFifo* fifo, uint8_t** converted_input_samples, const int frame_size);
AVFrame* new_audio_frame(int channels, AVSampleFormat sample_fmt, int sample_rate, int frame_size);
AVFrame* new_video_frame(int width, int height, int format, int hw_type = AV_HWDEVICE_TYPE_NONE);
#endif  //__FFMEDIA_UTILS_H__
