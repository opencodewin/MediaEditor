#include "ffmedia.h"
#include "ffmedia_queue.h"
#include "ffmedia_utils.h"
#include "ffmedia_common.h"
#include <assert.h>

#define DEFAULT_VIDEO_BITRATE   10000000
#define DEFAULT_AUDIO_BITRATE   96000

// #define PRINT_DEBUG_INFO

FFMediaSink::FFMediaSink(bool force_cpu)
{
    m_AvFmtCtx = nullptr;
    m_stream_num = 0;
    m_streams = nullptr;

    m_streaming = false;

    m_hw_info.reset(force_cpu, false);
}

const FFMedia_HWInfo* FFMediaSink::get_hw_info()
{
    return &m_hw_info;
}

void FFMediaSink::reset_hw_info(bool force_cpu)
{
    m_hw_info.reset(force_cpu, false);
}

int FFMediaSink::update_encoder_caps(AVCodecID codec_id, bool &use_hw, bool &is_10bit)
{
    m_hw_info.reset(!use_hw, false);
    return m_hw_info.update_encoder_caps(codec_id, use_hw, is_10bit);
}

FFMediaSink::~FFMediaSink()
{
    close();
}

FFMEDIA_RETVALUE FFMediaSink::close()
{
    if (m_streaming)
        stop_streaming();

    for (FFMediaStream* pStream : m_active_streams)
    {
        delete pStream;
    }
    m_active_streams.clear();
    m_stream_num = 0;
    m_streams = nullptr;

    if (m_AvFmtCtx)
    {
        avformat_free_context(m_AvFmtCtx);
        m_AvFmtCtx = nullptr;
    }
    return FFMEDIA_SUCCESS;
}

FFMEDIA_RETVALUE FFMediaSink::open(const char* name)
{
    if (m_AvFmtCtx)
        return FFMEDIA_FAILED;

    if (m_active_streams.empty())
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to open encoder without composed streams \n");
        return FFMEDIA_FAILED;
    }

    int fferr;

    /* create media context */
    m_name = name;
    m_container = get_container_string(m_name);

    fferr = avformat_alloc_output_context2(&m_AvFmtCtx, NULL, NULL, name);
    if (fferr)
    {
        print_av_err_str(fferr);
        return FFMEDIA_FAILED;
    }

    /* add media streams */
    for (auto pMediaStream : m_active_streams)
    {
        if (!pMediaStream->activated)
        {
            assert(pMediaStream->stream);
            AVStream* pSrcStream = (AVStream*)pMediaStream->stream;
            AVStream* pDstStream = avformat_new_stream(m_AvFmtCtx, nullptr);
            if (!pDstStream)
            {
                fprintf(stderr,
                        "[FFMediaSink]Error: fail to add new stream %d to sink format context!\n",
                        pMediaStream->index);
                return FFMEDIA_FAILED;
            }
            fferr = avcodec_parameters_copy(pDstStream->codecpar, pSrcStream->codecpar);
            if (fferr)
            {
                print_av_err_str(fferr);
                return FFMEDIA_FAILED;
            }
            pDstStream->codecpar->codec_tag = 0;
        }
        else
        {
            if (pMediaStream->type == FFMEDIA_STREAMTYPE::VIDEO)
            {
                if (add_video_stream(pMediaStream))
                    return FFMEDIA_FAILED;
            }
            else if (pMediaStream->type == FFMEDIA_STREAMTYPE::AUDIO)
            {
                if (add_audio_stream(pMediaStream))
                    return FFMEDIA_FAILED;
            }
        }
    }

    m_streams = m_AvFmtCtx->streams;
    assert(m_stream_num == m_AvFmtCtx->nb_streams);

#if defined(PRINT_DEBUG_INFO)
    fprintf(stdout, "[FFMediaSink]Info: open %s with %d streams \n", m_name.c_str(), m_stream_num);
#endif
    return FFMEDIA_SUCCESS;
}

int FFMediaSink::compose_stream(FFMediaStream* pSrcStream, AVDictionary* extra_options)
{
    if (m_streams)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to compose stream, try again before open\n");
        return -1;
    }

    if ((pSrcStream->type != FFMEDIA_STREAMTYPE::VIDEO)
        && (pSrcStream->type != FFMEDIA_STREAMTYPE::AUDIO))
    {
        fprintf(stderr, "[FFMediaSink]Error: only support video/audio stream type\n");
        return -1;
    }

    FFMediaStream* pNewStream = new FFMediaStream(m_active_streams.size(), pSrcStream->type);
    pNewStream->activated = pSrcStream->activated;
    if (!pSrcStream->activated)
        pNewStream->stream = pSrcStream->stream;
    else
    {
        if (pNewStream->type == FFMEDIA_STREAMTYPE::VIDEO)
        {
            pNewStream->width = pSrcStream->width;
            pNewStream->height = pSrcStream->height;
            pNewStream->frame_rate = pSrcStream->frame_rate;
            pNewStream->hdr_type = pSrcStream->hdr_type;
            bool use_hw = !(m_hw_info.hw_type == AV_HWDEVICE_TYPE_NONE);
            bool use_10bit = pSrcStream->bit_depth > 8;
            pNewStream->pixel_format = (AVPixelFormat)update_encoder_caps(pSrcStream->codec_id, use_hw, use_10bit);
            pNewStream->bit_depth = use_10bit ? 10 : 8;
        }
        else
        {
            pNewStream->sample_rate = pSrcStream->sample_rate;
            pNewStream->channels = pSrcStream->channels;
            pNewStream->sample_fmt = pSrcStream->sample_fmt;
        }
    }

    pNewStream->tbc = pSrcStream->tbc;
    pNewStream->tbn = pSrcStream->tbn;
    pNewStream->codec_id = pSrcStream->codec_id;
    pNewStream->bit_rate = pSrcStream->bit_rate;
    pNewStream->extra_options = extra_options;

    m_active_streams.emplace_back(pNewStream);
    m_stream_num = m_active_streams.size();

#if defined(PRINT_DEBUG_INFO)
    fprintf(stdout, "[FFMediaSink]Info: compose stream done with %d total streams \n",
            m_active_streams.size());
#endif
    return m_stream_num-1;
}

FFMEDIA_RETVALUE FFMediaSink::add_video_stream(FFMediaStream* pMediaStream)
{
    int fferr;

    AVCodec* codec = nullptr;
    AVStream* pStream = nullptr;

    // TODO: support more hw encoder
    if (m_hw_info.hw_type == AV_HWDEVICE_TYPE_CUDA)
    {
        if (pMediaStream->codec_id == AV_CODEC_ID_HEVC || pMediaStream->codec_id == AV_CODEC_ID_H265)
            codec = (AVCodec*)avcodec_find_encoder_by_name("hevc_nvenc");
        else if (pMediaStream->codec_id == AV_CODEC_ID_H264)
            codec = (AVCodec*)avcodec_find_encoder_by_name("h264_nvenc");
        else
        {
            fprintf(stderr, "[FFMediaSink]Error: unsupport codec for cuda!\n");
            return FFMEDIA_FAILED;
        }
    }
    else if (m_hw_info.hw_type == AV_HWDEVICE_TYPE_VIDEOTOOLBOX)
    {
        if (pMediaStream->codec_id == AV_CODEC_ID_HEVC || pMediaStream->codec_id == AV_CODEC_ID_H265)
            codec = (AVCodec*)avcodec_find_encoder_by_name("hevc_videotoolbox");
        else if (pMediaStream->codec_id == AV_CODEC_ID_H264)
            codec = (AVCodec*)avcodec_find_encoder_by_name("h264_videotoolbox");
        else
        {
            fprintf(stderr, "[FFMediaSink]Error: unsupport codec for videotoolbox!\n");
            return FFMEDIA_FAILED;
        }
    }
    else if (m_hw_info.hw_type == AV_HWDEVICE_TYPE_D3D11VA || m_hw_info.hw_type == AV_HWDEVICE_TYPE_DXVA2)
    {
        if (pMediaStream->codec_id == AV_CODEC_ID_HEVC || pMediaStream->codec_id == AV_CODEC_ID_H265)
            codec = (AVCodec*)avcodec_find_encoder_by_name("hevc_amf");
        else if (pMediaStream->codec_id == AV_CODEC_ID_H264)
            codec = (AVCodec*)avcodec_find_encoder_by_name("h264_amf");
        else
        {
            fprintf(stderr, "[FFMediaSink]Error: unsupport codec for AMF!\n");
            return FFMEDIA_FAILED;
        }
    }
    else
    {
        codec = (AVCodec*)avcodec_find_encoder(pMediaStream->codec_id);
    }

    if (!codec)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to find codec!\n");
        return FFMEDIA_FAILED;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);

    if (!codec_ctx)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to alloc codec context!\n");
        return FFMEDIA_FAILED;
    }
    pMediaStream->codec_ctx = codec_ctx;
    pMediaStream->hw_info = &m_hw_info;

    codec_ctx->width = pMediaStream->width;
    codec_ctx->height = pMediaStream->height;
    codec_ctx->pix_fmt = pMediaStream->pixel_format;
    codec_ctx->time_base = pMediaStream->tbc;
    codec_ctx->framerate = pMediaStream->frame_rate;

    if (m_AvFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    codec_ctx->gop_size = 12;
    codec_ctx->bit_rate = pMediaStream->bit_rate > 0 ? pMediaStream->bit_rate : DEFAULT_VIDEO_BITRATE;

    if (pMediaStream->hdr_type != FFMEDIA_HDRTYPE::NONE)
    {
        if (pMediaStream->hdr_type == FFMEDIA_HDRTYPE::SDR)
        {
            codec_ctx->color_primaries = AVCOL_PRI_BT709;
            codec_ctx->color_trc = AVCOL_TRC_BT709;
            codec_ctx->colorspace = AVCOL_SPC_BT709;
        }
        else if (pMediaStream->hdr_type == FFMEDIA_HDRTYPE::HDR_HLG)
        {
            codec_ctx->color_primaries = AVCOL_PRI_BT2020;
            codec_ctx->color_trc = AVCOL_TRC_ARIB_STD_B67;
            codec_ctx->colorspace = AVCOL_SPC_BT2020_NCL;
        }
        else
        {
            codec_ctx->color_primaries = AVCOL_PRI_BT2020;
            codec_ctx->color_trc = AVCOL_TRC_SMPTE2084;
            codec_ctx->colorspace = AVCOL_SPC_BT2020_NCL;
        }
        codec_ctx->color_range = AVCOL_RANGE_MPEG;
        codec_ctx->chroma_sample_location = AVCHROMA_LOC_LEFT;
    }

    AVDictionary* encOpts = nullptr;
    if (m_hw_info.hw_type != AV_HWDEVICE_TYPE_D3D11VA && m_hw_info.hw_type != AV_HWDEVICE_TYPE_DXVA2)
    {
        av_dict_set(&encOpts, "preset", "slow", 0);
        av_dict_set(&encOpts, "rc", "cbr_hq", 0);
    }
    else
    {
        av_dict_set(&encOpts, "rc", "cbr", 0);
    }
    if (pMediaStream->codec_id == AV_CODEC_ID_HEVC || pMediaStream->codec_id == AV_CODEC_ID_H265)
    {
        if (pMediaStream->bit_depth > 8)
            av_dict_set(&encOpts, "profile", "main10", 0);
        else
            av_dict_set(&encOpts, "profile", "main", 0);
    }
    else
    {
        if (pMediaStream->bit_depth > 8)
        {
            if (m_hw_info.hw_type == AV_HWDEVICE_TYPE_NONE)
                av_dict_set(&encOpts, "profile", "high10", 0);
        }
        else
            av_dict_set(&encOpts, "profile", "high", 0);
    }

    if (pMediaStream->extra_options)
        av_dict_copy(&encOpts, pMediaStream->extra_options, AV_DICT_APPEND);

    fferr = avcodec_open2(codec_ctx, codec, &encOpts);
    av_dict_free(&encOpts);

    if (fferr)
    {
        print_av_err_str(fferr);
        return FFMEDIA_FAILED;
    }

    pStream = avformat_new_stream(m_AvFmtCtx, codec);
    if (!pStream)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to add new stream %d to sink format context!\n",
                pMediaStream->index);
        return FFMEDIA_FAILED;
    }

    pStream->time_base = pMediaStream->tbn;
    pStream->avg_frame_rate = pMediaStream->frame_rate;
    if (avcodec_parameters_from_context(pStream->codecpar, codec_ctx) < 0)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to copy codec parameter from encoder context to "
                        "stream context!\n");
        return FFMEDIA_FAILED;
    }

    // pMediaStream->stream = pStream;
#if defined(PRINT_DEBUG_INFO)
    fprintf(stdout, "[FFMediaSink]Info: video stream %d added, time base {%d, %d} \n", pStream - id,
            pStream->time_base.num, pStrea->time_base.den);
#endif
    return FFMEDIA_SUCCESS;
}

FFMEDIA_RETVALUE FFMediaSink::add_audio_stream(FFMediaStream* pMediaStream)
{
    int fferr;

    AVCodec* codec = nullptr;
    AVStream* pStream = nullptr;

    codec = (AVCodec*)avcodec_find_encoder(pMediaStream->codec_id);
    // if (pMediaStream->codec_id == AV_CODEC_ID_AAC)
    //     codec = avcodec_find_encoder_by_name("libfdk_aac");

    if (!codec)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to find codec!\n");
        return FFMEDIA_FAILED;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);

    if (!codec_ctx)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to alloc codec context!\n");
        return FFMEDIA_FAILED;
    }
    pMediaStream->codec_ctx = codec_ctx;

    codec_ctx->sample_fmt = pMediaStream->sample_fmt;
    if (codec->sample_fmts)
    {
        codec_ctx->sample_fmt = codec->sample_fmts[0];
        for (int i = 0; codec->sample_fmts[i] > 0; i++)
        {
            if (codec->sample_fmts[i] == AV_SAMPLE_FMT_S16)
                codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
        }
    }

    codec_ctx->bit_rate = pMediaStream->bit_rate > 0 ? pMediaStream->bit_rate : DEFAULT_AUDIO_BITRATE;
    codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    codec_ctx->sample_rate = pMediaStream->sample_rate;
    if (codec->supported_samplerates)
    {
        codec_ctx->sample_rate = codec->supported_samplerates[0];
        for (int i = 0; codec->supported_samplerates[i]; i++)
        {
            if (codec->supported_samplerates[i] == pMediaStream->sample_rate)
                codec_ctx->sample_rate = pMediaStream->sample_rate;
        }
    }

#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    codec_ctx->channels = pMediaStream->channels;
    codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
    if (codec->channel_layouts)
    {
        codec_ctx->channel_layout = codec->channel_layouts[0];
        for (int i = 0; codec->channel_layouts[i]; i++)
        {
            if (codec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
        }
    }
    codec_ctx->channels = av_get_channel_layout_nb_channels(codec_ctx->channel_layout);
#else
    av_channel_layout_default(&codec_ctx->ch_layout, pMediaStream->channels);
#endif
    if (m_AvFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* encOpts = nullptr;
    if (pMediaStream->extra_options)
        av_dict_copy(&encOpts, pMediaStream->extra_options, AV_DICT_APPEND);
    fferr = avcodec_open2(codec_ctx, codec, &encOpts);
    av_dict_free(&encOpts);
    if (fferr)
    {
        print_av_err_str(fferr);
        return FFMEDIA_FAILED;
    }
    pMediaStream->sample_rate = codec_ctx->sample_rate;
    pMediaStream->sample_fmt = codec_ctx->sample_fmt;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    pMediaStream->channels = codec_ctx->channels;
#else
    pMediaStream->channels = codec_ctx->ch_layout.nb_channels;
#endif
    pMediaStream->tbc = {1, pMediaStream->sample_rate};
    pMediaStream->tbn = pMediaStream->tbc;

    pStream = avformat_new_stream(m_AvFmtCtx, codec);
    if (!pStream)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to add new stream %d to sink format context!\n",
                pMediaStream->index);
        return FFMEDIA_FAILED;
    }

    pStream->time_base = pMediaStream->tbn;
    if (avcodec_parameters_from_context(pStream->codecpar, codec_ctx) < 0)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to copy codec parameter from encoder context to "
                        "stream context!\n");
        return FFMEDIA_FAILED;
    }

#if defined(PRINT_DEBUG_INFO)
    fprintf(stdout, "[FFMediaSink]Info: audio stream %d added, time base {%d, %d} \n", pStream - id,
            pStream->time_base.num, pStrea->time_base.den);
#endif

    if (codec_ctx->frame_size > 0)
    {
        init_fifo(&pMediaStream->audio_fifo, codec_ctx->sample_fmt, codec_ctx->ch_layout.nb_channels);
        pMediaStream->audio_framesize = codec_ctx->frame_size;
        pMediaStream->audio_sample_cnt = 0;
        pMediaStream->audio_frame = FFMediaStream_alloc_frame(pMediaStream);
    }
    
    // pMediaStream->stream = pStream;
    return FFMEDIA_SUCCESS;
}

FFMEDIA_RETVALUE FFMediaSink::start_streaming()
{
    if (!m_streams)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to start streaming, try open sink first\n");
        return FFMEDIA_FAILED;
    }

    if (m_streaming)
        return FFMEDIA_SUCCESS;

    int fferr;
    if (!(m_AvFmtCtx->oformat->flags & AVFMT_NOFILE))
    {
        fferr = avio_open(&m_AvFmtCtx->pb, m_name.c_str(), AVIO_FLAG_WRITE);
        if (fferr)
        {
            print_av_err_str(fferr);
            return FFMEDIA_FAILED;
        }
    }

    fferr = avformat_write_header(m_AvFmtCtx, NULL);
    if (fferr)
    {
        print_av_err_str(fferr);
        return FFMEDIA_FAILED;
    }

    // update stream time base since they might be changed during write header
    for (int i = 0; i < m_stream_num; i++)
    {
        FFMediaStream* pMediaStream = m_active_streams[i];
        AVStream* pAVStream = m_streams[i];
        pMediaStream->tbn = pAVStream->time_base;
    }
    m_streaming = true;
    return FFMEDIA_SUCCESS;
}

FFMEDIA_RETVALUE FFMediaSink::stop_streaming()
{
    if (!m_streams)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to stop streaming, try open sink first\n");
        return FFMEDIA_FAILED;
    }

    if (!m_streaming)
        return FFMEDIA_SUCCESS;

    av_write_trailer(m_AvFmtCtx);
    m_streaming = false;

    return FFMEDIA_SUCCESS;
}

int FFMediaSink::get_video_stream_idx(const int index)
{
    int target = index < 0 ? 0 : index;
    int result = -1, result_idx = -1;
    AVStream* pStream = nullptr;
    if (m_streams && m_stream_num > 0)
    {
        for (int i = 0; i < m_stream_num; i++)
        {
            pStream = m_streams[i];
            if (pStream && pStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                result++;
                if (result == target)
                {
                    result_idx = i;
                    break;
                }
            }
        }
    }

    return result_idx;
}

int FFMediaSink::get_audio_stream_idx(const int index)
{
    int target = index < 0 ? 0 : index;
    int result = -1, result_idx = -1;
    AVStream* pStream = nullptr;
    if (m_streams && m_stream_num > 0)
    {
        for (int i = 0; i < m_stream_num; i++)
        {
            pStream = m_streams[i];
            if (pStream && pStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                result++;
                if (result == target)
                {
                    result_idx = i;
                    break;
                }
            }
        }
    }

    return result_idx;
}

FFMediaStream* FFMediaSink::get_stream(const int stream_index)
{
    // valid stream id
    if (stream_index < 0 || stream_index >= m_stream_num)
        return nullptr;

    return m_active_streams[stream_index];
}

FFMEDIA_RETVALUE FFMediaSink::write_one_frame(const AVFrame* pFrame, const int stream_index)
{
    int fferr = 0;

    if (stream_index < 0 || stream_index >= m_stream_num || !m_streaming)
        return FFMEDIA_FAILED;

    FFMediaStream* pStream = m_active_streams[stream_index];
    if (!pStream->activated)
        return FFMEDIA_FAILED;

    pStream->read_cnt++;

    if (pStream->type == FFMEDIA_STREAMTYPE::AUDIO && pStream->audio_fifo)
    {
        return write_audio_frame(pFrame, stream_index);
    }

    AVFrame* pEncFrame = av_frame_clone(pFrame);

    if (pStream->read_cnt == 1)
    {
        pStream->pts_offset = 0;
        pStream->start_pts = pEncFrame->pts;
    }
    // pEncFrame->pts -= pStream->start_pts;

    AVPacket* pPkt = encode_one_frame(pEncFrame, stream_index);
    av_frame_free(&pEncFrame);
    if (pPkt)
    {
        av_packet_rescale_ts(pPkt, pStream->tbc, pStream->tbn);
        fferr = av_interleaved_write_frame(m_AvFmtCtx, pPkt);
        pStream->write_cnt++;
        av_packet_free(&pPkt);
        if (fferr)
        {
            print_av_err_str(fferr);
            return FFMEDIA_FAILED;
        }
    }
    return FFMEDIA_SUCCESS;
}

FFMEDIA_RETVALUE FFMediaSink::write_one_packet(const AVPacket* pPkt)
{
    int stream_index = pPkt->stream_index;
    if (stream_index < 0 || stream_index >= m_stream_num || !m_streaming)
        return FFMEDIA_FAILED;

    FFMediaStream* pStream = m_active_streams[stream_index];
    if (pStream->activated)
        return FFMEDIA_FAILED;

    pStream->read_cnt++;

    int fferr = av_interleaved_write_frame(m_AvFmtCtx, av_packet_clone(pPkt));
    pStream->write_cnt++;
    if (fferr)
    {
        print_av_err_str(fferr);
        return FFMEDIA_FAILED;
    }
    return FFMEDIA_SUCCESS;
}

AVPacket* FFMediaSink::encode_one_frame(AVFrame* pFrame, const int stream_index)
{
    if (stream_index < 0 || stream_index >= m_stream_num || !m_streaming)
        return nullptr;

    FFMediaStream* pMediaStream = m_active_streams[stream_index];
    if (!pMediaStream->activated || !pMediaStream->codec_ctx)
        return nullptr;

    int fferr = avcodec_send_frame(pMediaStream->codec_ctx, pFrame);

    while (fferr == AVERROR(EAGAIN))
    {
        fferr = avcodec_send_frame(pMediaStream->codec_ctx, pFrame);
    }
    if (fferr)
    {
        print_av_err_str(fferr);
        return nullptr;
    }

    AVPacket* pPkt = av_packet_alloc();
    if (!pPkt)
    {
        fprintf(stderr, "[FFMediaSink]Error: fail to alloc packet for encoder \n");
        return nullptr;
    }

    fferr = avcodec_receive_packet(pMediaStream->codec_ctx, pPkt);
    if (fferr)
    {
        if (fferr != AVERROR(EAGAIN))
            print_av_err_str(fferr);
        av_packet_free(&pPkt);
        return nullptr;
    }
    pPkt->stream_index = stream_index;
    return pPkt;
}

FFMEDIA_RETVALUE FFMediaSink::write_audio_frame(const AVFrame* pFrame, const int stream_index)
{
    FFMediaStream* pStream = m_active_streams[stream_index];
    AVFrame* pDstFrame = pStream->audio_frame;
    AVCodecContext* codec_ctx = pStream->codec_ctx;
    int fferr = 0;
    if (pStream->audio_sample_cnt == 0)
    {
        pStream->fifo_start_pts = pFrame->pts;
    }

    add_samples_to_fifo(pStream->audio_fifo, (uint8_t**)pFrame->data, (int)pFrame->nb_samples);
    while (true)
    {
        if (av_audio_fifo_size(pStream->audio_fifo) < pStream->audio_framesize)
            break;

        if (av_audio_fifo_read(pStream->audio_fifo, (void**)pDstFrame->data,
                               pStream->audio_framesize)
            < pStream->audio_framesize)
        {
            fprintf(stderr, "[FFMediaSink]Error: Could not read data from FIFO!\n");
            av_frame_free(&pDstFrame);
            break;
        }
        pDstFrame->pts =
            av_rescale_q(pStream->audio_sample_cnt, (AVRational){ 1, codec_ctx->sample_rate },
                         codec_ctx->time_base);
        pDstFrame->pts += pStream->fifo_start_pts;

        pStream->audio_sample_cnt += pDstFrame->nb_samples;

        AVPacket* pPkt = encode_one_frame(pDstFrame, stream_index);
        if (pPkt)
        {
            av_packet_rescale_ts(pPkt, pStream->tbc, pStream->tbn);
            fferr = av_interleaved_write_frame(m_AvFmtCtx, pPkt);
            pStream->write_cnt++;
            av_packet_free(&pPkt);
            if (fferr)
            {
                print_av_err_str(fferr);
                return FFMEDIA_FAILED;
            }
        }
    }

    return FFMEDIA_SUCCESS;
}