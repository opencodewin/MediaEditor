#include "ffmedia_utils.h"
#include "ffmedia_common.h"

/* audio resampler */
FFAudioResampler::FFAudioResampler(int srcChs, int srcRate, AVSampleFormat srcFmt, int dstChs,
                                   int dstRate, AVSampleFormat dstFmt)
{
    m_srcChs = srcChs;
    m_srcRate = srcRate;
    m_srcFmt = srcFmt;

    m_dstChs = dstChs;
    m_dstRate = dstRate;
    m_dstFmt = dstFmt;

#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        int64_t dst_chlyt = av_get_default_channel_layout(dstChs);
        int64_t src_chlyt = av_get_default_channel_layout(srcChs);
        //av_get_channel_layout_string(chlytDescBuff, sizeof(chlytDescBuff), channels, (uint64_t)chlyt);
#else
        AVChannelLayout dst_chlyt{AV_CHANNEL_ORDER_UNSPEC, 0};
        AVChannelLayout src_chlyt{AV_CHANNEL_ORDER_UNSPEC, 0};
        av_channel_layout_default(&dst_chlyt, dstChs);
        av_channel_layout_default(&src_chlyt, srcChs);
        //av_channel_layout_describe(&chlyt, chlytDescBuff, sizeof(chlytDescBuff));
#endif

    int fferr = swr_alloc_set_opts2(&m_swrCtx, &dst_chlyt, dstFmt, dstRate, &src_chlyt, srcFmt, srcRate, 0, NULL);

    if (fferr < 0 || swr_init(m_swrCtx) < 0)
    {
        fprintf(stderr, "[ffmpeg_utils]Error:Failed to initialize the resampling contex! \n");
        exit(-1);
    }
}

FFAudioResampler::~FFAudioResampler()
{
    if (m_swrCtx)
        swr_free(&m_swrCtx);
    m_swrCtx = nullptr;
}

int FFAudioResampler::process(void* srcBuf, void* dstBuf, int srcSamples, int dstSamples)
{
    return swr_convert(m_swrCtx, (uint8_t**)dstBuf, dstSamples, (const uint8_t**)srcBuf,
                       srcSamples);
}

/* video scalar */
FFVideoScalar::FFVideoScalar(int srcWidth, int srcHeight, AVPixelFormat srcFmt, int dstWidth,
                             int dstHeight, AVPixelFormat dstFmt, int threads)
    : m_threads(threads)
{
    m_src_width = srcWidth;
    m_src_height = srcHeight;
    m_src_format = srcFmt;

    m_dst_width = dstWidth;
    m_dst_height = dstHeight;
    m_dst_format = dstFmt;

    m_sws_ctx = nullptr;
    allocSwsContextArray(srcWidth, srcHeight, srcFmt, dstWidth, dstHeight, dstFmt);
}

FFVideoScalar::~FFVideoScalar()
{
    freeSwsContextArray();
}

void FFVideoScalar::process(AVFrame* srcFrame, AVFrame* dstFrame)
{
    if (dstFrame->colorspace != srcFrame->colorspace)
    {
        setSwsColorSpaceCoef(srcFrame->colorspace);
    }

    sws_scale(m_sws_ctx, srcFrame->data, srcFrame->linesize, 0, srcFrame->height, dstFrame->data,
              dstFrame->linesize);

    return;
}

void FFVideoScalar::freeSwsContextArray()
{
    if (m_sws_ctx != nullptr)
        sws_freeContext(m_sws_ctx);
    m_sws_ctx = nullptr;
}

void FFVideoScalar::allocSwsContextArray(int srcW, int srcH, AVPixelFormat srcFormat, int dstW,
                                         int dstH, AVPixelFormat dstFormat)
{
    freeSwsContextArray();
    m_sws_ctx = sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, SWS_BILINEAR, NULL,
                               NULL, NULL);
}

void FFVideoScalar::setSwsColorSpaceCoef(int colorspace)
{
    const int* inv_table;
    const int* table;
    int srcRange, dstRange, brightness, contrast, saturation;
    int fferr;
    fferr = sws_getColorspaceDetails(m_sws_ctx, (int**)&inv_table, &srcRange, (int**)&table,
                                     &dstRange, &brightness, &contrast, &saturation);
    table = inv_table = sws_getCoefficients(colorspace);
    fferr = sws_setColorspaceDetails(m_sws_ctx, inv_table, srcRange, table, dstRange, brightness,
                                     contrast, saturation);
}

int init_fifo(AVAudioFifo** fifo, AVSampleFormat sample_fmt, int channels)
{
    /* Create the FIFO buffer based on the specified output sample format. */
    if (!(*fifo = av_audio_fifo_alloc(sample_fmt, channels, 1)))
    {
        fprintf(stderr, "[FFMediaUtils]Error: fail to allocate FIFO \n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

int add_samples_to_fifo(AVAudioFifo* fifo, uint8_t** converted_input_samples, const int frame_size)
{
    int fferr;

    /* Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples. */
    fferr = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size);
    if (fferr)
    {
        print_av_err_str(fferr);
        return fferr;
    }

    /* Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void**)converted_input_samples, frame_size) < frame_size)
    {
        fprintf(stderr, "[FFMediaUtils]Error: fail write data to FIFO \n");
        return AVERROR_EXIT;
    }
    return 0;
}

/* custom avframe malloc*/
#if 0
static void free_cuda_pinmem(void* apaque, uint8_t* data)
{
    cudaFreeHost(data);
}

int alloc_cuda_pinmem(void* frame, void* context)
{
    AVFrame* pFrame = (AVFrame*)frame;
    int buf_size;
    int linesize;
    void * cuMem;
    cudaError_t cuda_error;
    if(pFrame->format == AV_PIX_FMT_UYVY422)
    {
        linesize = pFrame->width * 2;
        buf_size = linesize * pFrame->height;
    }
    else // if(pFrame->format == AV_PIX_FMT_YUV422P10LE)
        return -1;

    cuda_error = cudaMallocHost(&cuMem, buf_size);
    if (cuda_error != cudaSuccess)
        return -1;

    pFrame->data[0] = (uint8_t *)cuMem;
    pFrame->linesize[0] = linesize;
    pFrame->buf[0] = av_buffer_create((uint8_t *)cuMem,
                                     buf_size,
                                     &free_cuda_pinmem,
                                     nullptr,
                                     AV_BUFFER_FLAG_READONLY);
    
    return 0;
}
#endif

AVFrame* FFMediaStream_alloc_frame(const FFMediaStream* pStream, void* user_info)
{
    AVFrame* pDstFrame = nullptr;
    if (pStream->type == FFMEDIA_STREAMTYPE::VIDEO)
    {
        pDstFrame = av_frame_alloc();
        if (!pDstFrame)
            return nullptr;
        pDstFrame->width = pStream->width;
        pDstFrame->height = pStream->height;
        pDstFrame->format = pStream->pixel_format;

        if (pStream->hdr_type != FFMEDIA_HDRTYPE::NONE)
        {
            if (pStream->hdr_type == FFMEDIA_HDRTYPE::SDR)
            {
                pDstFrame->color_primaries = AVCOL_PRI_BT709;
                pDstFrame->color_trc = AVCOL_TRC_BT709;
                pDstFrame->colorspace = AVCOL_SPC_BT709;
            }
            else if (pStream->hdr_type == FFMEDIA_HDRTYPE::HDR_HLG)
            {
                pDstFrame->color_primaries = AVCOL_PRI_BT2020;
                pDstFrame->color_trc = AVCOL_TRC_ARIB_STD_B67;
                pDstFrame->colorspace = AVCOL_SPC_BT2020_NCL;
            }
            else
            {
                pDstFrame->color_primaries = AVCOL_PRI_BT2020;
                pDstFrame->color_trc = AVCOL_TRC_SMPTE2084;
                pDstFrame->colorspace = AVCOL_SPC_BT2020_NCL;
            }
            pDstFrame->color_range = AVCOL_RANGE_MPEG;
            pDstFrame->chroma_location = AVCHROMA_LOC_LEFT;
        }
        if (user_info && pStream->hw_info && pStream->hw_info->hw_type == AV_HWDEVICE_TYPE_CUDA)
        {
            FFMEDIA_EXTERMALLOC_PARAM* param = (FFMEDIA_EXTERMALLOC_PARAM*)user_info;
            PFN_FFMEDIA_FRAME_MALLOC pfn_malloc = param->pfn_malloc;
            if (pfn_malloc(pDstFrame, param->context))
            {
                fprintf(stderr, "[FFMediaUtils]Error: fail to allocate cuda frame \n");
                av_frame_free(&pDstFrame);
                return nullptr;
            }
        }
        else
        {
            av_frame_get_buffer(pDstFrame, 0);
            av_frame_make_writable(pDstFrame);
        }
    }
    else if (pStream->type == FFMEDIA_STREAMTYPE::AUDIO)
    {
        pDstFrame = av_frame_alloc();
        if (!pDstFrame)
            return nullptr;
        pDstFrame->format = pStream->sample_fmt;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        pDstFrame->channels = pStream->channels;
        pDstFrame->channel_layout = av_get_default_channel_layout(pStream->channels);
#else
        av_channel_layout_default(&pDstFrame->ch_layout, pStream->channels);
#endif
        pDstFrame->sample_rate = pStream->sample_rate;
        if (user_info)
            pDstFrame->nb_samples = *(int*)user_info;
        else
            pDstFrame->nb_samples = pStream->codec_ctx->frame_size;
        av_frame_get_buffer(pDstFrame, 0);
        av_frame_make_writable(pDstFrame);
    }
    return pDstFrame;
}

AVFrame* new_audio_frame(int channels, AVSampleFormat sample_fmt, int sample_rate, int frame_size)
{
    AVFrame* pFrame = av_frame_alloc();
    if (!pFrame)
        return nullptr;
    pFrame->format = sample_fmt;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    pFrame->channels = channels;
    pFrame->channel_layout = av_get_default_channel_layout(channels);
#else
    av_channel_layout_default(&pFrame->ch_layout, channels);
#endif
    pFrame->sample_rate = sample_rate;
    pFrame->nb_samples = frame_size;
    av_frame_get_buffer(pFrame, 0);
    av_frame_make_writable(pFrame);
    return pFrame;
}

AVFrame* new_video_frame(int width, int height, int format, int hw_type)
{
    AVFrame* pFrame = av_frame_alloc();
    if (!pFrame)
        return nullptr;
    pFrame->width = width;
    pFrame->height = height;
    pFrame->format = format;

    if (hw_type == AV_HWDEVICE_TYPE_VIDEOTOOLBOX)
    {
        // TBD
        {
            printf("[FFMedia] Error@%d: VIDEOTOOLBOX support not implemented yet \n", __LINE__);
            av_frame_free(&pFrame);
            return nullptr;
        }
    }
    if (hw_type == AV_HWDEVICE_TYPE_CUDA)
    {
        // TBD
        {
            printf("[FFMedia] Error@%d: CUDA support not implemented yet \n", __LINE__);
            av_frame_free(&pFrame);
            return nullptr;
        }
    }
    else
    {
        av_frame_get_buffer(pFrame, 0);
        av_frame_make_writable(pFrame);
    }
    return pFrame;
}
