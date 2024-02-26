#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <immat.h>
#include <ImGuiFileDialog.h>
#if IMGUI_VULKAN_SHADER
#include <ColorConvert_vulkan.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/avstring.h>
#include <libavutil/bprint.h>
#include <libavutil/fifo.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#ifdef __cplusplus
}
#endif

#define ISRGB(format)   \
    (format == AV_PIX_FMT_RGB24 || \
    format == AV_PIX_FMT_BGR24 || \
    format == AV_PIX_FMT_BGR8 || \
    format == AV_PIX_FMT_BGR4 || \
    format == AV_PIX_FMT_BGR4_BYTE || \
    format == AV_PIX_FMT_RGB8 || \
    format == AV_PIX_FMT_RGB4 || \
    format == AV_PIX_FMT_RGB4_BYTE || \
    format == AV_PIX_FMT_ARGB || \
    format == AV_PIX_FMT_RGBA || \
    format == AV_PIX_FMT_ABGR || \
    format == AV_PIX_FMT_BGRA || \
    format == AV_PIX_FMT_RGB48BE || \
    format == AV_PIX_FMT_RGB48LE || \
    format == AV_PIX_FMT_RGB565BE || \
    format == AV_PIX_FMT_RGB565LE || \
    format == AV_PIX_FMT_RGB555BE || \
    format == AV_PIX_FMT_RGB555LE || \
    format == AV_PIX_FMT_BGR565BE || \
    format == AV_PIX_FMT_BGR565LE || \
    format == AV_PIX_FMT_BGR555BE || \
    format == AV_PIX_FMT_BGR555LE || \
    format == AV_PIX_FMT_RGB444LE || \
    format == AV_PIX_FMT_RGB444BE || \
    format == AV_PIX_FMT_BGR444LE || \
    format == AV_PIX_FMT_BGR444BE || \
    format == AV_PIX_FMT_BGR48BE || \
    format == AV_PIX_FMT_BGR48LE || \
    format == AV_PIX_FMT_RGBA64BE || \
    format == AV_PIX_FMT_RGBA64LE || \
    format == AV_PIX_FMT_BGRA64BE || \
    format == AV_PIX_FMT_BGRA64LE || \
    format == AV_PIX_FMT_0RGB || \
    format == AV_PIX_FMT_RGB0 || \
    format == AV_PIX_FMT_0BGR || \
    format == AV_PIX_FMT_BGR0)

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

#define ISYUV440P(format) \
    (format == AV_PIX_FMT_YUV440P || \
    format == AV_PIX_FMT_YUVJ440P || \
    format == AV_PIX_FMT_YUV440P10LE || \
    format == AV_PIX_FMT_YUV440P10BE || \
    format == AV_PIX_FMT_YUV440P12LE || \
    format == AV_PIX_FMT_YUV440P12BE)

#define ISYUV444P(format) \
    (format == AV_PIX_FMT_YUV444P || \
    format == AV_PIX_FMT_YUVJ444P || \
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

#define ISBE(format) \
    (format == AV_PIX_FMT_RGB48BE || \
    format == AV_PIX_FMT_RGB565BE || \
    format == AV_PIX_FMT_RGB555BE || \
    format == AV_PIX_FMT_BGR565BE || \
    format == AV_PIX_FMT_BGR555BE || \
    format == AV_PIX_FMT_RGB444BE || \
    format == AV_PIX_FMT_BGR444BE || \
    format == AV_PIX_FMT_BGR48BE || \
    format == AV_PIX_FMT_RGBA64BE || \
    format == AV_PIX_FMT_BGRA64BE || \
    format == AV_PIX_FMT_NV20BE || \
    format == AV_PIX_FMT_P010BE || \
    format == AV_PIX_FMT_P016BE || \
    format == AV_PIX_FMT_GRAY9BE || \
    format == AV_PIX_FMT_GRAY10BE || \
    format == AV_PIX_FMT_GRAY12BE || \
    format == AV_PIX_FMT_GRAY14BE || \
    format == AV_PIX_FMT_GRAY16BE)

#define ISMONO(format) \
    (format == AV_PIX_FMT_GRAY8 || \
    format == AV_PIX_FMT_GRAY9BE || \
    format == AV_PIX_FMT_GRAY9LE || \
    format == AV_PIX_FMT_GRAY10BE || \
    format == AV_PIX_FMT_GRAY10LE || \
    format == AV_PIX_FMT_GRAY12BE || \
    format == AV_PIX_FMT_GRAY12LE || \
    format == AV_PIX_FMT_GRAY14BE || \
    format == AV_PIX_FMT_GRAY14LE || \
    format == AV_PIX_FMT_GRAY16LE || \
    format == AV_PIX_FMT_PAL8 || \
    format == AV_PIX_FMT_GRAY16BE)

static inline std::string PrintTimeStamp(double time_stamp)
{
    char buffer[1024] = {0};
    int hours = 0, mins = 0, secs = 0, ms = 0;
    if (!isnan(time_stamp))
    {
        time_stamp *= 1000.f;
        hours = time_stamp / 1000 / 60 / 60; time_stamp -= hours * 60 * 60 * 1000;
        mins = time_stamp / 1000 / 60; time_stamp -= mins * 60 * 1000;
        secs = time_stamp / 1000; time_stamp -= secs * 1000;
        ms = time_stamp;
    }
    snprintf(buffer, 1024, "%02d:%02d:%02d.%03d", hours, mins, secs, ms);
    return std::string(buffer);
}

static enum AVPixelFormat  m_hw_pix_fmt {AV_PIX_FMT_NONE};
static enum AVHWDeviceType m_hw_type {AV_HWDEVICE_TYPE_NONE};
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == m_hw_pix_fmt)
            return *p;
    }
    av_log(NULL, AV_LOG_WARNING, "Failed to get HW surface format.");
    return ctx->pix_fmt;//AV_PIX_FMT_NONE;
}

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&ctx->hw_device_ctx, type,
                                    NULL, NULL, 0)) < 0) 
    {
        av_log(NULL, AV_LOG_WARNING, "Failed to create specified HW device.");
        return err;
    }

    return err;
}

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type, int device_type = 0)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    if (*stream_idx == -1)
    {
        ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
        if (ret < 0) 
        {
            fprintf(stderr, "Could not find %s stream in input file\n", av_get_media_type_string(type));
            return ret;
        } 
        *stream_idx = ret;
    }
    else
    {
        ret = *stream_idx;
    }

    stream_index = ret;
    st = fmt_ctx->streams[stream_index];
    /* find decoder for the stream */
    dec = (AVCodec*)avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) 
    {
        fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
        return AVERROR(EINVAL);
    }
    /* Allocate a codec context for the decoder */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (!*dec_ctx) 
    {
        fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
        return AVERROR(ENOMEM);
    }
    /* Copy codec parameters from input stream to output codec context */
    if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) 
    {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
        return ret;
    }
    if ((*dec_ctx)->codec_type == AVMEDIA_TYPE_VIDEO && device_type == 0)
    {
        // try hw codec
        for (int i = 0;; i++) 
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(dec, i);
            if (!config)
            {
                av_log(NULL, AV_LOG_WARNING,"Decoder %s does not support HW.", dec->name);
                m_hw_pix_fmt = AV_PIX_FMT_NONE;
                m_hw_type = AV_HWDEVICE_TYPE_NONE;
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) 
            {
                m_hw_pix_fmt = config->pix_fmt;
                m_hw_type = config->device_type;
                break;
            }
        }
    }
    (*dec_ctx)->codec_id = dec->id;
    // init hw codec
    if ((*dec_ctx)->codec_type == AVMEDIA_TYPE_VIDEO && device_type == 0 && m_hw_pix_fmt != AV_PIX_FMT_NONE)
    {
        (*dec_ctx)->get_format = get_hw_format;
        if ((ret = hw_decoder_init(*dec_ctx, m_hw_type)) < 0)
            return ret;
    }
    /* Init the decoders */
    if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) 
    {
        fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
        return ret;
    }
    
    (*dec_ctx)->pkt_timebase = st->time_base;

    return 0;
}

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct MediaSourceNode final : Node
{
    struct media_stream
    {
        int                 m_index {-1};
        enum AVMediaType    m_type {AVMEDIA_TYPE_UNKNOWN};
        AVCodecContext *    m_dec_ctx {nullptr};
        AVStream *          m_stream {nullptr};
        AVFrame *           m_frame {nullptr};
        Pin *               m_flow {nullptr};
        Pin *               m_mat {nullptr};
#if IMGUI_VULKAN_SHADER
        ImGui::ColorConvert_vulkan * m_yuv2rgb {nullptr};
#else
        struct SwsContext*  m_img_convert_ctx {nullptr};
#endif
    };

    BP_NODE_WITH_NAME(MediaSourceNode, "Media Source", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    MediaSourceNode(BP* blueprint): Node(blueprint)
    {
        m_Name = "Media Source";
        m_HasCustomLayout = true;
        m_OutputPins.push_back(&m_Exit);
        m_OutputPins.push_back(&m_OReset);
    }

    ~MediaSourceNode()
    {
        CloseMedia();
    }

    Pin* InsertOutputPin(PinType type, const std::string name) override
    {
        auto pin = FindPin(name);
        if (pin) return pin;
        pin = NewPin(type, name);
        if (pin)
        {
            pin->m_Flags |= PIN_FLAG_FORCESHOW;
            m_OutputPins.push_back(pin);
        }
        return pin;
    }

    void CloseMedia()
    {
        // rebuild output pins
        for (auto iter = m_OutputPins.begin(); iter != m_OutputPins.end();)
        {
            if ((*iter)->m_Name != m_Exit.m_Name && (*iter)->m_Name != m_OReset.m_Name)
            {
                (*iter)->Unlink();
                if ((*iter)->m_LinkFrom.size() > 0)
                {
                    for (auto from_pin : (*iter)->m_LinkFrom)
                    {
                        auto link = m_Blueprint->GetPinFromID(from_pin);
                        if (link)
                        {
                            link->Unlink();
                        }
                    }
                }
                iter = m_OutputPins.erase(iter);
            }
            else
                ++iter;
        }
        for (auto stream : m_streams)
        {
            if (stream->m_dec_ctx) avcodec_free_context(&stream->m_dec_ctx);
            if (stream->m_frame) av_frame_free(&stream->m_frame);
#if IMGUI_VULKAN_SHADER
            if (stream->m_yuv2rgb) delete stream->m_yuv2rgb;
#else
            if (stream->m_img_convert_ctx) sws_freeContext(stream->m_img_convert_ctx);
#endif
            if (stream->m_flow) delete stream->m_flow;
            if (stream->m_mat) delete stream->m_mat;
            delete stream;
        }
        m_streams.clear();

        if (m_fmt_ctx) { avformat_close_input(&m_fmt_ctx); m_fmt_ctx = nullptr; }
        if (m_pkt) { av_packet_free(&m_pkt); m_pkt = nullptr; }
        //m_next_pts = AV_NOPTS_VALUE;
        m_last_video_pts = AV_NOPTS_VALUE;
        m_hw_pix_fmt = AV_PIX_FMT_NONE;
        m_hw_type = AV_HWDEVICE_TYPE_NONE;
        m_total_time = 0;
        m_current_pts = 0;
        m_need_update = false;
    }

    void OpenMedia()
    {
        // Open New file
        if (avformat_open_input(&m_fmt_ctx, m_path.c_str(), NULL, NULL) < 0) 
            return; // Could not open source file
        if (avformat_find_stream_info(m_fmt_ctx, NULL) < 0) 
        {
            avformat_close_input(&m_fmt_ctx); m_fmt_ctx = nullptr;
            return; // Could not find stream information
        }
        // check all a/v streams
        for (int i = 0; i < m_fmt_ctx->nb_streams; i++)
        {
            auto stream = m_fmt_ctx->streams[i];
            int stream_index = stream->index;
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                // create video codec context
                AVCodecContext* video_dec_ctx = nullptr;
                if (open_codec_context(&stream_index, &video_dec_ctx, m_fmt_ctx, AVMEDIA_TYPE_VIDEO, m_device) >= 0)
                {
                    struct media_stream * new_stream = new media_stream;
                    if (new_stream)
                    {
                        new_stream->m_index = stream_index;
                        new_stream->m_type = AVMEDIA_TYPE_VIDEO;
                        new_stream->m_dec_ctx = video_dec_ctx;
                        new_stream->m_stream = m_fmt_ctx->streams[stream_index];
                        new_stream->m_frame = av_frame_alloc();
                        std::string flow_pin_name = "VOut:" + std::to_string(stream_index);
                        new_stream->m_flow = InsertOutputPin(BluePrint::PinType::Flow, flow_pin_name);
                        std::string mat_pin_name = "V:" + std::to_string(stream_index);
                        new_stream->m_mat = InsertOutputPin(BluePrint::PinType::Mat, mat_pin_name);
                        m_streams.push_back(new_stream);
                    }
                }
            }
            else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                AVCodecContext* audio_dec_ctx = nullptr;
                if (open_codec_context(&stream_index, &audio_dec_ctx, m_fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0)
                {
                    struct media_stream * new_stream = new media_stream;
                    if (new_stream)
                    {
                        new_stream->m_index = stream_index;
                        new_stream->m_type = AVMEDIA_TYPE_AUDIO;
                        new_stream->m_dec_ctx = audio_dec_ctx;
                        new_stream->m_stream = m_fmt_ctx->streams[stream_index];
                        new_stream->m_frame = av_frame_alloc();
                        std::string flow_pin_name = "AOut:" + std::to_string(stream_index);
                        new_stream->m_flow = InsertOutputPin(BluePrint::PinType::Flow, flow_pin_name);
                        std::string mat_pin_name = "A:" + std::to_string(stream_index);
                        new_stream->m_mat = InsertOutputPin(BluePrint::PinType::Mat, mat_pin_name);
                        m_streams.push_back(new_stream);
                    }
                }
            }
        }
        av_dump_format(m_fmt_ctx, 0, m_path.c_str(), 0);
        m_total_time = (double)m_fmt_ctx->duration / (double)AV_TIME_BASE;
        if (m_total_time < 1e-6)
        {
            for (auto stream : m_streams)
            {
                m_total_time = (double)stream->m_stream->duration * av_q2d(stream->m_stream->time_base);
                if (m_total_time > 1e-6)
                    break;
            }
        }
        m_pkt = av_packet_alloc();
        m_paused = false;
        m_need_update = false;
    }

    int OutVideoFrame(media_stream* stream)
    {
        int ret;
        AVFrame *tmp_frame = nullptr;
        AVFrame *sw_frame = av_frame_alloc();
        if (!sw_frame)
        {
            fprintf(stderr, "Can not alloc frame\n");
            return -1;
        }
        if (m_last_video_pts != AV_NOPTS_VALUE)
        {
            if (stream->m_frame->pict_type == AV_PICTURE_TYPE_B && stream->m_frame->pts < m_last_video_pts)
            {
                // do we only need skip first gop B frame after first I frame?
                fprintf(stderr, "Output frame isn't in current GOP and decoder maybe not completed\n");
                av_frame_free(&sw_frame);
                return -1;
            }
        }
        m_last_video_pts = stream->m_frame->pts;
        if (stream->m_frame->format == m_hw_pix_fmt)
        {
            /* retrieve data from GPU to CPU */
            if ((ret = av_hwframe_map(sw_frame, stream->m_frame, AV_HWFRAME_MAP_READ)) < 0)
            {
                fprintf(stderr, "Error mapping the data from HW memory\n");
                av_frame_unref(sw_frame);
                sw_frame->format = (int)AV_PIX_FMT_NONE;
                if ((ret = av_hwframe_transfer_data(sw_frame, stream->m_frame, 0)) < 0) 
                {
                    fprintf(stderr, "Error transferring the data to system memory\n");
                    av_frame_free(&sw_frame);
                    return -1;
                }
                else
                {
                    tmp_frame = sw_frame;
                }
            }
            else
            {
                tmp_frame = sw_frame;
            }
        }
        else
        {
            tmp_frame = stream->m_frame;
        }
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((AVPixelFormat)tmp_frame->format);
        int video_depth = stream->m_dec_ctx->bits_per_raw_sample;
        if (video_depth == 0 && desc)
            video_depth = desc->comp[0].depth != 0 ? desc->comp[0].depth : 8;
        int data_shift = video_depth > 8 ? 1 : 0;

        ImColorSpace color_space =  stream->m_dec_ctx->colorspace == AVCOL_SPC_BT470BG ||
                                    stream->m_dec_ctx->colorspace == AVCOL_SPC_SMPTE170M ||
                                    stream->m_dec_ctx->colorspace == AVCOL_SPC_BT470BG ? IM_CS_BT601 :
                                    stream->m_dec_ctx->colorspace == AVCOL_SPC_BT709 ? IM_CS_BT709 :
                                    stream->m_dec_ctx->colorspace == AVCOL_SPC_BT2020_NCL ||
                                    stream->m_dec_ctx->colorspace == AVCOL_SPC_BT2020_CL ? IM_CS_BT2020 : IM_CS_BT709;
        ImColorRange color_range =  stream->m_dec_ctx->color_range == AVCOL_RANGE_MPEG ? IM_CR_NARROW_RANGE :
                                    stream->m_dec_ctx->color_range == AVCOL_RANGE_JPEG ? IM_CR_FULL_RANGE : IM_CR_NARROW_RANGE;
        ImColorFormat color_format = ISYUV420P(tmp_frame->format) ? IM_CF_YUV420 :
                                    ISYUV422P(tmp_frame->format) ? IM_CF_YUV422 :
                                    ISYUV440P(tmp_frame->format) ? IM_CF_YUV440 :
                                    ISYUV444P(tmp_frame->format) ? IM_CF_YUV444 :
                                    ISNV12(tmp_frame->format) ? video_depth == 10 ? IM_CF_P010LE : IM_CF_NV12 : 
                                    ISRGB(tmp_frame->format) ? IM_CF_RGBA : 
                                    ISMONO(tmp_frame->format) ? IM_CF_GRAY : IM_CF_YUV420;

        bool is_yuv = IM_ISYUV(color_format);
        bool is_rgb = IM_ISRGB(color_format);
        bool is_be = ISBE(tmp_frame->format);
        bool is_mono = IM_ISMONO(color_format);
        AVRational tb = stream->m_stream->time_base;
        int64_t time_stamp = stream->m_frame->best_effort_timestamp;
        double current_video_pts = (time_stamp == AV_NOPTS_VALUE) ? NAN : time_stamp * av_q2d(tb);
        
        if (is_yuv)
        {
            int out_w = tmp_frame->linesize[0] / (desc->comp[0].step > 0 ? desc->comp[0].step : 1);
            int out_h = tmp_frame->height;
            int UV_shift_w = ISYUV420P(tmp_frame->format) || ISYUV422P(tmp_frame->format) ? 1 : 0;
            int UV_shift_h = ISYUV420P(tmp_frame->format) || ISNV12(tmp_frame->format) ? 1 : 0;
            m_mutex.lock();
        
            // using separated mat to convert color from AVFrame, prevent copy frame one time
            ImGui::ImMat mat_Y, mat_U, mat_V;
            mat_Y.create_type(out_w, out_h, 1, tmp_frame->data[0], data_shift ? IM_DT_INT16 : IM_DT_INT8);
            mat_U.create_type(out_w >> UV_shift_w, out_h >> UV_shift_h, 1, tmp_frame->data[1], data_shift ? IM_DT_INT16 : IM_DT_INT8);
            if (!ISNV12(tmp_frame->format))
                mat_V.create_type(out_w >> UV_shift_w, out_h >> UV_shift_h, 1, tmp_frame->data[2], data_shift ? IM_DT_INT16 : IM_DT_INT8);
            mat_Y.time_stamp = current_video_pts;
            mat_Y.color_space = color_space;
            mat_Y.color_range = color_range;
            mat_Y.color_format = color_format;
            mat_Y.depth = video_depth;
            mat_Y.dw = tmp_frame->width;
            mat_Y.dh = tmp_frame->height;
            mat_Y.flags = IM_MAT_FLAGS_VIDEO_FRAME;
            if (stream->m_frame->pict_type == AV_PICTURE_TYPE_I) mat_Y.flags |= IM_MAT_FLAGS_VIDEO_FRAME_I;
            if (stream->m_frame->pict_type == AV_PICTURE_TYPE_P) mat_Y.flags |= IM_MAT_FLAGS_VIDEO_FRAME_P;
            if (stream->m_frame->pict_type == AV_PICTURE_TYPE_B) mat_Y.flags |= IM_MAT_FLAGS_VIDEO_FRAME_B;
            #if LIBAVUTIL_VERSION_MAJOR > 59 || defined(FF_API_INTERLACED_FRAME)
                if (stream->m_frame->flags & AV_FRAME_FLAG_INTERLACED) mat_Y.flags |= IM_MAT_FLAGS_VIDEO_INTERLACED;
            #endif

#if IMGUI_VULKAN_SHADER
            if (!stream->m_yuv2rgb)
            {
                int gpu = m_device == IM_DD_CPU ? -1 : ImGui::get_default_gpu_index();
                stream->m_yuv2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!stream->m_yuv2rgb)
                {
                    m_mutex.unlock();
                    return -1;
                }
            }
            if (stream->m_yuv2rgb)
            {
                ImGui::ImMat im_RGB;
                im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_Y.type : m_mat_data_type;
                im_RGB.color_format = IM_CF_ABGR;
                if (m_device == 0)
                {
                    im_RGB.device = IM_DD_VULKAN;
                }
                stream->m_yuv2rgb->YUV2RGBA(mat_Y, mat_U, mat_V, im_RGB, IM_INTERPOLATE_NONE);
                im_RGB.flags = mat_Y.flags;
                im_RGB.time_stamp = current_video_pts;
                im_RGB.color_space = color_space;
                im_RGB.color_range = color_range;
                im_RGB.depth = video_depth;
                im_RGB.rate = {stream->m_stream->avg_frame_rate.num, stream->m_stream->avg_frame_rate.den};
                auto pin = (MatPin*)stream->m_mat;
                if (pin) pin->SetValue(im_RGB);
            }
#else
            // ffmpeg swscale
            if (!stream->m_img_convert_ctx)
            {
                AVPixelFormat format =  mat_Y.color_format == IM_CF_YUV420 ? (video_depth > 8 ? AV_PIX_FMT_YUV420P10 : AV_PIX_FMT_YUV420P) :
                                        mat_Y.color_format == IM_CF_YUV422 ? (video_depth > 8 ? AV_PIX_FMT_YUV422P10 : AV_PIX_FMT_YUV422P) :
                                        mat_Y.color_format == IM_CF_YUV444 ? (video_depth > 8 ? AV_PIX_FMT_YUV444P10 : AV_PIX_FMT_YUV444P) :
                                        mat_Y.color_format == IM_CF_NV12 ? (video_depth > 8 ? AV_PIX_FMT_P010LE : AV_PIX_FMT_NV12) :
                                        AV_PIX_FMT_YUV420P;
                stream->m_img_convert_ctx = sws_getCachedContext(
                                    stream->m_img_convert_ctx,
                                    mat_Y.w,
                                    mat_Y.h,
                                    (AVPixelFormat)tmp_frame->format,
                                    mat_Y.w,
                                    mat_Y.h,
                                    AV_PIX_FMT_RGBA,
                                    SWS_BICUBIC,
                                    NULL, NULL, NULL);
            }
            if (stream->m_img_convert_ctx)
            {
                ImGui::ImMat im_RGB(mat_Y.w, mat_Y.h, 4, 1u);
                uint8_t *dst_data[] = { (uint8_t *)im_RGB.data };
                int dst_linesize[] = { mat_Y.w * 4 }; // how many for 16 bits?
                sws_scale(
                    stream->m_img_convert_ctx,
                    tmp_frame->data,
                    tmp_frame->linesize,
                    0, mat_Y.h,
                    dst_data,
                    dst_linesize
                );
                im_RGB.flags = mat_Y.flags;
                im_RGB.type = IM_DT_INT8;
                im_RGB.time_stamp = current_video_pts;
                im_RGB.color_space = color_space;
                im_RGB.color_range = color_range;
                im_RGB.depth = video_depth;
                im_RGB.rate = {stream->m_stream->avg_frame_rate.num, stream->m_stream->avg_frame_rate.den};
                auto pin = (MatPin*)stream->m_mat;
                if (pin) pin->SetValue(im_RGB);
            }
#endif
            m_mutex.unlock();
        }
        else if (is_rgb)
        {
            ImGui::ImMat mat_in;
            int image_width = tmp_frame->linesize[0] / desc->nb_components / (data_shift ? 2 : 1);
            mat_in.create_type(image_width, tmp_frame->height, desc->nb_components, tmp_frame->data[0], data_shift ? (is_be ? IM_DT_INT16_BE : IM_DT_INT16) /*IM_DT_INT16*/ : IM_DT_INT8);
            mat_in.dw = tmp_frame->width;
            mat_in.dh = tmp_frame->height;
            m_mutex.lock();
#if IMGUI_VULKAN_SHADER
            if (!stream->m_yuv2rgb)
            {
                int gpu = m_device == IM_DD_CPU ? -1 : ImGui::get_default_gpu_index();
                stream->m_yuv2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!stream->m_yuv2rgb)
                {
                    m_mutex.unlock();
                    return -1;
                }
            }
            if (stream->m_yuv2rgb)
            {
                ImGui::ImMat im_RGB;
                im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? IM_DT_INT8 : m_mat_data_type;
                im_RGB.color_format = IM_CF_ABGR;
                if (m_device == 0)
                {
                    im_RGB.device = IM_DD_VULKAN;
                }
                stream->m_yuv2rgb->Conv(mat_in, im_RGB);
                im_RGB.flags = IM_MAT_FLAGS_VIDEO_FRAME;
                im_RGB.time_stamp = current_video_pts;
                im_RGB.color_space = color_space;
                im_RGB.color_range = color_range;
                im_RGB.depth = video_depth;
                im_RGB.rate = {stream->m_stream->avg_frame_rate.num, stream->m_stream->avg_frame_rate.den};
                auto pin = (MatPin*)stream->m_mat;
                if (pin) pin->SetValue(im_RGB);
            }
#endif
            m_mutex.unlock();
        }
        else if(is_mono)
        {
            ImGui::ImMat mat_in;
            int image_width = tmp_frame->linesize[0] / desc->nb_components / (data_shift ? 2 : 1);
            mat_in.create_type(image_width, tmp_frame->height, desc->nb_components, tmp_frame->data[0], data_shift ? (is_be ? IM_DT_INT16_BE : IM_DT_INT16) /*IM_DT_INT16*/ : IM_DT_INT8);
            mat_in.dw = tmp_frame->width;
            mat_in.dh = tmp_frame->height;
            m_mutex.lock();
#if IMGUI_VULKAN_SHADER
            if (!stream->m_yuv2rgb)
            {
                int gpu = m_device == IM_DD_CPU ? -1 : ImGui::get_default_gpu_index();
                stream->m_yuv2rgb = new ImGui::ColorConvert_vulkan(gpu);
                if (!stream->m_yuv2rgb)
                {
                    m_mutex.unlock();
                    return -1;
                }
            }
            if (stream->m_yuv2rgb)
            {
                ImGui::ImMat im_RGB;
                im_RGB.type = IM_DT_INT8;
                im_RGB.color_format = IM_CF_ABGR;
                if (m_device == 0)
                {
                    im_RGB.device = IM_DD_VULKAN;
                }
                im_RGB.device = IM_DD_CPU;
                stream->m_yuv2rgb->ConvertColorFormat(mat_in, im_RGB);
                im_RGB.flags = IM_MAT_FLAGS_VIDEO_FRAME;
                im_RGB.elempack = 4;
                im_RGB.time_stamp = current_video_pts;
                im_RGB.color_space = color_space;
                im_RGB.color_range = color_range;
                im_RGB.depth = video_depth;
                im_RGB.rate = {stream->m_stream->avg_frame_rate.num, stream->m_stream->avg_frame_rate.den};
                auto pin = (MatPin*)stream->m_mat;
                if (pin) pin->SetValue(im_RGB);
            }
#endif
        m_mutex.unlock();
    }
        av_frame_free(&sw_frame);
        m_current_pts = current_video_pts;
        return 0;
    }

    int OutAudioFrame(media_stream* stream)
    {
        // Generate Audio Mat
        ImGui::ImMat mat_A;
        int data_size = av_get_bytes_per_sample((enum AVSampleFormat)stream->m_frame->format);
        AVRational tb = (AVRational){1, stream->m_frame->sample_rate};
        ImDataType type  =  (stream->m_frame->format == AV_SAMPLE_FMT_FLT) || (stream->m_frame->format == AV_SAMPLE_FMT_FLTP) ? IM_DT_FLOAT32 :
                            (stream->m_frame->format == AV_SAMPLE_FMT_S32) || (stream->m_frame->format == AV_SAMPLE_FMT_S32P) ? IM_DT_INT32:
                            (stream->m_frame->format == AV_SAMPLE_FMT_S16) || (stream->m_frame->format == AV_SAMPLE_FMT_S16P) ? IM_DT_INT16:
                            IM_DT_INT8;
        double current_audio_pts = (stream->m_frame->pts == AV_NOPTS_VALUE) ? NAN : stream->m_frame->pts * av_q2d(tb);
        m_mutex.lock();
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        int channels = stream->m_frame->channels;
#else
        int channels = stream->m_frame->ch_layout.nb_channels;
#endif
        mat_A.create_type(stream->m_frame->nb_samples, 1, channels, type);
        for (int i = 0; i < channels; i++)
        {
            for (int x = 0; x < stream->m_frame->nb_samples; x++)
            {
                if (type == IM_DT_FLOAT32)
                    mat_A.at<float>(x, 0, i) = ((float *)stream->m_frame->data[i])[x];
                else if (type == IM_DT_INT32)
                    mat_A.at<int32_t>(x, 0, i) = ((int32_t *)stream->m_frame->data[i])[x];
                else if (type == IM_DT_INT16)
                    mat_A.at<int16_t>(x, 0, i) = ((int16_t *)stream->m_frame->data[i])[x];
                else
                    mat_A.at<int8_t>(x, 0, i) = ((int8_t *)stream->m_frame->data[i])[x];
            }
        }
        mat_A.time_stamp = current_audio_pts;
        mat_A.rate = {stream->m_frame->sample_rate, 1};
        mat_A.flags = IM_MAT_FLAGS_AUDIO_FRAME;
        auto pin = (MatPin*)stream->m_mat;
        if (pin) pin->SetValue(mat_A);
        m_mutex.unlock();
        m_current_pts = current_audio_pts;
        return 0;
    }

    FlowPin DecodeMedia()
    {
        while (true)
        {
            int ret = 0;
            ret = av_read_frame(m_fmt_ctx, m_pkt);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF) 
                {
                    for (auto stream : m_streams) avcodec_flush_buffers(stream->m_dec_ctx); 
                    return m_Exit;
                }
                break;
            }
            // find stream index
            auto iter = std::find_if(m_streams.begin(), m_streams.end(), [&](const media_stream* ss) {
                return ss->m_index == m_pkt->stream_index;
            });
            if (iter == m_streams.end())
                break;
            media_stream * stream = *iter;
            ret = avcodec_send_packet(stream->m_dec_ctx, m_pkt);
            av_packet_unref(m_pkt);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    for (auto stream : m_streams) avcodec_flush_buffers(stream->m_dec_ctx); 
                    return m_Exit;
                }
                break;
            }
            ret = avcodec_receive_frame(stream->m_dec_ctx, stream->m_frame);
            if (ret < 0) 
            {
                if (ret == AVERROR_EOF)
                {
                    for (auto stream : m_streams) avcodec_flush_buffers(stream->m_dec_ctx); 
                    return m_Exit;
                }
                else if (ret == AVERROR(EAGAIN)) continue;
                else break;
            }
            else
            {
                if (stream->m_type == AVMEDIA_TYPE_VIDEO)
                {
                    ret = OutVideoFrame(stream);
                    av_frame_unref(stream->m_frame);
                    if (ret != 0)
                        break;
                    return *(FlowPin*)stream->m_flow;
                }
                else if (stream->m_type == AVMEDIA_TYPE_AUDIO)
                {
                    ret = OutAudioFrame(stream);
                    av_frame_unref(stream->m_frame);
                    if (ret != 0)
                        break;
                    return *(FlowPin*)stream->m_flow;
                }
            }
        }
        return {};
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_fmt_ctx) av_seek_frame(m_fmt_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            Reset(context);
            return {};
        }
        if (m_fmt_ctx)
        {
            if (m_need_update || !m_paused)
            {
                m_need_update = false;
                auto ret = DecodeMedia();
                if (ret.m_Name != "Exit")
                    context.PushReturnPoint(entryPoint);
                return ret;
            }
            else
            {
                if (threading)
                    ImGui::sleep((int)(40));
                else
                    ImGui::sleep(0);
                context.PushReturnPoint(entryPoint);
            }
        }
        return {};
    }
    
    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        const char *filters = "Media Files(*.mp4 *.mov *.mkv *.webm *.mxf *.avi){.mp4,.mov,.mkv,.webm,.mxf,.avi,.MP4,.MOV,.MKV,.MXF,.WEBM,.AVI},.*";
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImVec2 minSize = ImVec2(400, 300);
		ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        auto& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = true;
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_DontShowHiddenFiles | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_Modal;
        vflags |= ImGuiFileDialogFlags_ShowBookmark;
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose File"))
        {
            IGFD::FileDialogConfig config;
            config.path = m_path.empty() ? "." : m_path;
            config.countSelectionMax = 1;
            config.userDatas = this;
            config.flags = vflags;
            ImGuiFileDialog::Instance()->OpenDialog("##NodeMediaSourceDlgKey", "Choose File", 
                                                    filters, 
                                                    config);
        }
        ImGui::SameLine(0);
        ImGui::TextUnformatted(m_file_name.c_str());

        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        ImGui::Separator();
        changed |= ImGui::RadioButton("GPU",  (int *)&m_device, 0); ImGui::SameLine();
        changed |= ImGui::RadioButton("CPU",   (int *)&m_device, -1);

        if (ImGuiFileDialog::Instance()->Display("##NodeMediaSourceDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_path = ImGuiFileDialog::Instance()->GetFilePathName();
                m_file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                CloseMedia();
                OpenMedia();
                changed = true;
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = false;
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        ImGui::Text("%s", m_file_name.c_str());
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::Dummy(ImVec2(320, 8));
        ImGui::PushItemWidth(300);
        if (ImGui::Button(m_paused ? ICON_FAD_PLAY : ICON_FAD_PAUSE, ImVec2(32.0f, 32.0f)))
        {
            if (m_fmt_ctx) m_paused = !m_paused;
        }
        ImGui::SameLine();
        float time = m_current_pts;
        float total_time = m_total_time;
        if (ImGui::SliderFloat("##time", &time, 0, total_time, "%.2f", flags))
        {
            if (m_fmt_ctx)
            {
                // Seek
                int64_t seek_time = time * AV_TIME_BASE;
                av_seek_frame(m_fmt_ctx, -1, seek_time, AVSEEK_FLAG_BACKWARD);
                m_current_pts = time;
                m_need_update = true;
            }
        }
        string str_current_time = PrintTimeStamp(m_current_pts);
        string str_total_time = PrintTimeStamp(m_total_time);
        ImGui::Text("%s / %s", str_current_time.c_str(), str_total_time.c_str());
        for (auto stream : m_streams)
        {
            if (stream->m_type == AVMEDIA_TYPE_VIDEO)
            {
                ImGui::Text("     Video: %d x %d @ %.2f", stream->m_stream->codecpar->width, 
                                                    stream->m_stream->codecpar->height, 
                                                    (float)stream->m_stream->r_frame_rate.num / (float)stream->m_stream->r_frame_rate.den);
                ImGui::Text("            %d bit depth", stream->m_stream->codecpar->bits_per_raw_sample > 0 ? stream->m_stream->codecpar->bits_per_raw_sample : 0);

            }
            if (stream->m_type == AVMEDIA_TYPE_AUDIO)
            {
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                ImGui::Text("     Audio: %d @ %d", stream->m_stream->codecpar->channels, stream->m_stream->codecpar->sample_rate);
#else
                ImGui::Text("     Audio: %d @ %d", stream->m_stream->codecpar->ch_layout.nb_channels, stream->m_stream->codecpar->sample_rate);
#endif
                ImGui::Text("            %d bit depth", stream->m_stream->codecpar->bits_per_coded_sample > 0 ? stream->m_stream->codecpar->bits_per_coded_sample : 0);
            }
        }

        ImGui::PopItemWidth();
        return false;
    }

    int LoadPins(const imgui_json::array* PinValueArray, std::vector<Pin *>& pinArray)
    {
        for (auto& pinValue : *PinValueArray)
        {
            string pinType;
            PinType type = PinType::Any;
            if (!imgui_json::GetTo<imgui_json::string>(pinValue, "type", pinType)) // check pin type
                continue;
            PinTypeFromString(pinType, type);

            std::string name;
            if (pinValue.contains("name"))
                imgui_json::GetTo<imgui_json::string>(pinValue, "name", name);

            auto iter = std::find_if(m_OutputPins.begin(), m_OutputPins.end(), [name](const Pin * pin)
            {
                return pin->m_Name == name;
            });
            if (iter != m_OutputPins.end())
            {
                if (!(*iter)->Load(pinValue))
                {
                    return BP_ERR_GENERAL;
                }
            }
            else
            {
                Pin *new_pin = NewPin(type, name);
                if (new_pin)
                {
                    if (!new_pin->Load(pinValue))
                    {
                        delete new_pin;
                        return BP_ERR_GENERAL;
                    }
                    pinArray.push_back(new_pin);
                }
            }
        }
        return BP_ERR_NONE;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if (!value.is_object())
            return BP_ERR_NODE_LOAD;

        if (!imgui_json::GetTo<imgui_json::number>(value, "id", m_ID)) // required
            return BP_ERR_NODE_LOAD;

        if (!imgui_json::GetTo<imgui_json::string>(value, "name", m_Name)) // required
            return BP_ERR_NODE_LOAD;

        imgui_json::GetTo<imgui_json::boolean>(value, "break_point", m_BreakPoint); // optional

        imgui_json::GetTo<imgui_json::number>(value, "group_id", m_GroupID); // optional

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("device_type"))
        {
            auto& val = value["device_type"];
            if (val.is_number()) 
                m_device = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("media_path"))
        {
            auto& val = value["media_path"];
            if (val.is_string()) 
                m_path = val.get<imgui_json::string>();
        }
        if (value.contains("file_name"))
        {
            auto& val = value["file_name"];
            if (val.is_string())
            {
                m_file_name = val.get<imgui_json::string>();
            }
        }

        const imgui_json::array* inputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "input_pins", inputPinsArray)) // optional
        {
            auto pins = GetInputPins();

            if (pins.size() != inputPinsArray->size())
                return BP_ERR_PIN_NUMPER;

            auto pin = pins.data();
            for (auto& pinValue : *inputPinsArray)
            {
                if (!(*pin)->Load(pinValue))
                {
                    return BP_ERR_INPIN_LOAD;
                }
                ++pin;
            }
        }

        const imgui_json::array* outputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "output_pins", outputPinsArray)) // optional
        {
            if (LoadPins(outputPinsArray, m_OutputPins) != BP_ERR_NONE)
                return BP_ERR_INPIN_LOAD;
        }

        if (!m_path.empty())
            OpenMedia();
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["device_type"] = imgui_json::number(m_device);
        value["media_path"] = m_path;
        value["file_name"] = m_file_name;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }

    FlowPin   m_Enter           = { this, "Enter" };
    FlowPin   m_Reset           = { this, "Reset" };
    FlowPin   m_Exit            = { this, "Exit" };
    FlowPin   m_OReset          = { this, "Reset Out"};

    Pin*      m_InputPins[2] = { &m_Enter, &m_Reset };
    std::vector<Pin *>          m_OutputPins;
private:
    int                 m_device  {0};          // 0 = GPU -1 = CPU
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    std::string         m_path;
    std::string         m_file_name;

    AVFormatContext*    m_fmt_ctx {nullptr};
    AVPacket*           m_pkt {nullptr};
    std::vector<media_stream*> m_streams;

    double              m_total_time    {0};
    double              m_current_pts   {0};
    int64_t             m_last_video_pts {AV_NOPTS_VALUE};
    bool                m_paused        {false};
    bool                m_need_update   {false};
};
}

BP_NODE_DYNAMIC_WITH_NAME(MediaSourceNode, "Media Source", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
