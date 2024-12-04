#include "imgui.h"
#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <imgui_helper.h>
#if IMGUI_RENDERING_VULKAN
#include "imgui_impl_vulkan.h"
#endif
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>
#endif
#if IMGUI_ICONS
#include <icons.h>
#define ICON_SAVE_TEXTURE     "\uf03e"
#else
#define ICON_SAVE_TEXTURE      "[S]"
#endif
#define USE_BOOKMARK
#include <ImGuiFileDialog.h>

#include "ffmedia/ffmedia.h"
#include "ffmedia/ffmedia_utils.h"
#include "ffmedia/ffmedia_queue.h"

static const char* hdr_system_items[] = { "None", "SDR", "HDR PQ", "HDR HLG"};

#define NODE_VERSION    0x01020000

//#define PRINT_INPUT_PTS
//#define PRINT_OUTPUT_PTS

namespace BluePrint
{
struct FFMpegEncoderNode final : Node
{
    BP_NODE_WITH_NAME(FFMpegEncoderNode, "Media Encoder", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    FFMpegEncoderNode(BP* blueprint): Node(blueprint) { 
        m_Name = "Media Encoder"; 
        m_video_empty_queue = new FFMedia_Queue(m_video_frame_queue_size, (pfn_release)av_frame_unref);
        m_video_sink_queue = new FFMedia_Queue(m_video_frame_queue_size, (pfn_release)av_frame_unref);
    }
    ~FFMpegEncoderNode()
    {
        m_mutex.lock();
        m_EncodeMat = ImGui::ImMat();
#if IMGUI_VULKAN_SHADER
        if (m_convert) { delete m_convert; m_convert = nullptr; }
#else
        if (m_img_convert_ctx) { sws_freeContext(m_img_convert_ctx); m_img_convert_ctx = nullptr; }
#endif
        m_stream_index = 0;
        m_sink.close();
        m_video_stream = nullptr;
        m_video_stream_idx = -1;
        if (m_video_empty_queue)  { m_video_empty_queue->flush(); delete m_video_empty_queue; }
        if (m_video_sink_queue)  { m_video_sink_queue->flush(); delete m_video_sink_queue; }
        m_video_frame_num = 0;
        m_video_drop_frames =0;
        m_video_pts_ms = 0;

        m_audio_stream = nullptr;
        m_audio_stream_idx = -1;
        if (m_audio_frame) av_frame_free(&m_audio_frame);
        if (m_audio_fifo) { av_audio_fifo_free(m_audio_fifo); m_audio_fifo = nullptr; }
        m_audio_pts_offset = AV_NOPTS_VALUE;
        m_audio_frame_size = 0;
        m_audio_sample_num = 0;
        m_audio_frame_num = 0;
        m_audio_pts_ms = 0;
        if (m_audio_iframe) av_frame_free(&m_audio_iframe);
        
        m_streaming = false;
        m_mutex.unlock();
    }
    
    void OnStop(Context& context) override
    {
        m_mutex.lock();
        if (m_streaming)
        {
            m_sink.stop_streaming();
            printf("OnStop with %d video and %d audio frames finished \n", m_video_frame_num, m_audio_frame_num);
            if (m_video_drop_frames)
                printf("OnStop with %d video frames dropped \n", m_video_drop_frames);
            m_sink.close();
        }
        m_mutex.unlock();
        Reset(context);
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        
        m_mutex.lock();

        m_EncodeMat = ImGui::ImMat();
#if IMGUI_VULKAN_SHADER
        if (m_convert) { delete m_convert; m_convert = nullptr; }
#else
        if (m_img_convert_ctx) { sws_freeContext(m_img_convert_ctx); m_img_convert_ctx = nullptr; }
#endif
        m_stream_index = 0;
        m_sink.close();
        m_video_stream = nullptr;
        m_video_stream_idx = -1;
        if (m_video_empty_queue)  m_video_empty_queue->flush();
        if (m_video_sink_queue)  m_video_sink_queue->flush();
        m_video_frame_num = 0;
        m_video_drop_frames =0;
        m_video_pts_ms = 0;

        m_audio_stream = nullptr;
        m_audio_stream_idx = -1;
        if (m_audio_frame) av_frame_free(&m_audio_frame);
        if (m_audio_fifo) { av_audio_fifo_free(m_audio_fifo); m_audio_fifo = nullptr; }
        m_audio_pts_offset = AV_NOPTS_VALUE;
        m_audio_frame_size = 0;
        m_audio_sample_num = 0;
        m_audio_frame_num = 0;
        m_audio_pts_ms = 0;
        if (m_audio_iframe) av_frame_free(&m_audio_iframe);
        
        m_streaming = false;

        m_mutex.unlock();
    }

    void conv_mat_int8(ImGui::ImMat& in_mat, ImGui::ImMat& out_mat)
    {
        size_t total_size = in_mat.total();
        unsigned char* dst = (unsigned char*)out_mat.data;
        int size; 
        if (in_mat.type == IM_DT_INT16)
        {
            short *src = (short*)in_mat.data;
            size = total_size / 2;
            for (int i=0;i<size;i++)
                *dst++ = *src++ >> 8;
        }
        else if (in_mat.type == IM_DT_INT32)
        {
            int *src = (int*)in_mat.data;
            size = total_size / 4;
            for (int i=0;i<size;i++)
                *dst++ = *src++ >> 24;
        }
        else if (in_mat.type == IM_DT_INT64)
        {
            int64_t *src = (int64_t*)in_mat.data;
            size = total_size / 8;
            for (int i=0;i<size;i++)
                *dst++ = *src++ >> 56;
        }
        else if (in_mat.type == IM_DT_FLOAT32)
        {
            float *src = (float*)in_mat.data;
            size = total_size / 4;
            for (int i=0;i<size;i++)
            {
                float data = *src++;
                if (data < 0) data = 0;
                if (data > 1) data = 1;
                *dst++ = data * 256;
            }
        }
        else
        {
            printf("copy mat format unknown! \n");
        }
    }

    void check_hw_caps()
    {
        bool use_hw = !(m_video_device == 0);
        bool use_10bit = m_video_bit_depth > 8;
        m_output_pix_fmt = (AVPixelFormat)m_sink.update_encoder_caps(m_video_codec==0 ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC, use_hw, use_10bit);
        m_video_bit_depth = use_10bit ? 10 : 8;
        m_video_device = use_hw ? 1 : 0;
    }

    void add_video_stream()
    {
        check_hw_caps();

        FFMediaStream stream(m_stream_index++, FFMEDIA_STREAMTYPE::VIDEO);
        stream.activated = true;
        stream.width = m_input_width;
        stream.height = m_input_height;
        stream.frame_rate = m_frame_rate;
        stream.tbc = av_inv_q(m_frame_rate);
        stream.tbn = stream.tbc;
        stream.pixel_format = m_output_pix_fmt;
        stream.bit_rate = m_video_bitrate;
        stream.bit_depth = m_video_bit_depth;
        stream.codec_id = !m_video_codec ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
        stream.hdr_type = m_hdr_type;
        m_video_stream_idx = m_sink.compose_stream(&stream);
        m_video_stream = m_sink.get_stream(m_video_stream_idx);
        m_output_pix_fmt = m_video_stream->pixel_format;
    }

    void add_audio_stream(ImGui::ImMat& mat)
    {
        m_output_samplerate = m_input_samplerate;
        m_output_channels = m_input_channels;

        FFMediaStream stream(m_stream_index++, FFMEDIA_STREAMTYPE::AUDIO);
        stream.sample_rate = m_output_samplerate;
        stream.tbc = {1, m_output_samplerate};
        stream.tbn = stream.tbc;
        stream.channels = m_output_channels;
        stream.sample_fmt = m_output_sample_fmt;
        stream.bit_rate = m_audio_bitrate;
        stream.codec_id = AV_CODEC_ID_AAC;
        stream.activated = true;
        m_audio_stream_idx = m_sink.compose_stream(&stream);
        m_audio_stream = m_sink.get_stream(m_audio_stream_idx);

        if (!m_audio_fifo)
            init_fifo(&m_audio_fifo, m_output_sample_fmt, m_output_channels);
    }

    void start_streaming()
    {
        m_sink.open(m_save_file_path.c_str());
        m_sink.start_streaming();
        m_streaming = true;

        if (m_withAudio)
        {
            assert(
                m_output_sample_fmt == m_audio_stream->sample_fmt &&
                m_output_channels == m_audio_stream->channels &&
                m_output_samplerate == m_audio_stream->sample_rate &&
                0 < m_audio_stream->audio_framesize);
        }
    }

    void encode_video()
    {
        void* pFrame = nullptr;
        if (m_EncodeMat.empty())
            return;
        #if defined(PRINT_INPUT_PTS)
        printf("input video pts %.3fs \n", m_EncodeMat.time_stamp);
        #endif

        if (!m_streaming && !m_withAudio)
            start_streaming();

        m_video_empty_queue->pop(pFrame, false);
        if (!pFrame)
        {
            pFrame = FFMediaStream_alloc_frame(m_video_stream);
        }
        
        if (!isnan(m_EncodeMat.time_stamp))
        {
            ((AVFrame*)pFrame)->pts = m_EncodeMat.time_stamp * 1000;
        }
        else
        {
            printf("invalid source pts found for video\n");
            ((AVFrame*)pFrame)->pts = av_rescale_q(m_video_frame_num, m_video_stream->tbc, (AVRational){1, 1000});
        }

        ImGui::ImMat mat_Y = m_EncodeMat.channel(0);
        {
            uint8_t* dst_data = ((AVFrame*)pFrame)->data[0];
            uint8_t* src_data = (uint8_t*)mat_Y.data;
            for (int i = 0; i < mat_Y.h; i++)
            {
                memcpy(dst_data, src_data, mat_Y.w * mat_Y.elembits() / 8);
                dst_data += ((AVFrame*)pFrame)->linesize[0];
                src_data +=  mat_Y.w * mat_Y.elembits() / 8;
            }
        }
        if (m_EncodeMat.color_format == IM_CF_YUV444)
        {
            ImGui::ImMat mat_U = m_EncodeMat.channel(1);
            {
                uint8_t* dst_data = ((AVFrame*)pFrame)->data[1];
                uint8_t* src_data = (uint8_t*)mat_U.data;
                for (int i = 0; i < mat_U.h; i++)
                {
                    memcpy(dst_data, src_data, mat_U.w * mat_U.elembits() / 8);
                    dst_data += ((AVFrame*)pFrame)->linesize[1];
                    src_data += mat_U.w * mat_U.elembits() / 8;
                }
            }
            ImGui::ImMat mat_V = m_EncodeMat.channel(2);
            {
                uint8_t* dst_data = ((AVFrame*)pFrame)->data[2];
                uint8_t* src_data = (uint8_t*)mat_V.data;
                for (int i = 0; i < mat_V.h; i++)
                {
                    memcpy(dst_data, src_data, mat_V.w * mat_V.elembits() / 8);
                    dst_data += ((AVFrame*)pFrame)->linesize[2];
                    src_data += mat_V.w * mat_V.elembits() / 8;
                }
            }
        }
        else
        {
            ImGui::ImMat mat_UV = m_EncodeMat.channel(1);
            if (m_EncodeMat.color_format == IM_CF_NV12 || m_EncodeMat.color_format == IM_CF_P010LE)
            {
                uint8_t* dst_data = ((AVFrame*)pFrame)->data[1];
                uint8_t* src_data = (uint8_t*)mat_UV.data;
                for (int i = 0; i < mat_UV.h / 2; i++)
                {
                    memcpy(dst_data, src_data, (mat_UV.w * mat_UV.elembits() / 8));
                    dst_data += ((AVFrame*)pFrame)->linesize[1];
                    src_data += mat_UV.w * mat_UV.elembits() / 8;
                }
            }
            else
            {
                int UV_H = m_EncodeMat.color_format == IM_CF_YUV420 ? mat_UV.h / 2 : mat_UV.h;
                uint8_t* dst_u_data = ((AVFrame*)pFrame)->data[1];
                uint8_t* dst_v_data = ((AVFrame*)pFrame)->data[2];
                uint8_t* src_u_data = (uint8_t*)mat_UV.data;
                uint8_t* src_v_data = (uint8_t*)mat_UV.data + mat_UV.w * UV_H / 2 * mat_UV.elembits() / 8;
                for (int i = 0; i < UV_H; i++)
                {
                    memcpy(dst_u_data, src_u_data, (mat_UV.w * mat_UV.elembits() / 8) / 2);
                    dst_u_data += ((AVFrame*)pFrame)->linesize[1];
                    src_u_data += (mat_UV.w * mat_UV.elembits() / 8) / 2;
                    memcpy(dst_v_data, src_v_data, (mat_UV.w * mat_UV.elembits() / 8) / 2);
                    dst_v_data += ((AVFrame*)pFrame)->linesize[2];
                    src_v_data += (mat_UV.w * mat_UV.elembits() / 8) / 2;
                }
            }
        }
        
        m_video_frame_num++;

        if (!m_withAudio)
        {
            m_video_pts_ms = ((AVFrame*)pFrame)->pts;
            ((AVFrame*)pFrame)->pts = av_rescale_q(((AVFrame*)pFrame)->pts,
                                    (AVRational){ 1, 1000 }, m_video_stream->tbc);
            m_sink.write_one_frame((const AVFrame*)pFrame, m_video_stream_idx);
            m_video_empty_queue->push(pFrame);
            #if defined(PRINT_OUTPUT_PTS)
            printf("output video (no audio) frame %d pts %.3fs \n", m_video_frame_num, m_video_pts_ms/1000.f);
            #endif
        }
        else
        {
            if(!m_video_sink_queue->push(pFrame))
            {
                m_video_drop_frames++;
                printf("drop %d frame caused by unsync \n", m_video_drop_frames);
            }
        }
    }

    void encode_audio(ImGui::ImMat& mat)
    {
        #if defined(PRINT_INPUT_PTS)
        printf("input audio pts %.3fs \n", mat.time_stamp);
        #endif
        if (!m_audio_iframe || mat.w > m_audio_iframe->nb_samples)
        {
            if (m_audio_iframe)
                av_frame_free(&m_audio_iframe);
            m_audio_iframe = new_audio_frame(m_output_channels, m_output_sample_fmt, m_output_samplerate, mat.w);
        }

        if (m_audio_pts_offset == AV_NOPTS_VALUE)
        {
            if (!isnan(mat.time_stamp))
            {
                m_audio_pts_offset = mat.time_stamp * 1000;
            }
            else
            {
                printf("invalid source pts found for audio\n");
                m_audio_pts_offset = 0;
            }
            m_audio_pts_offset = av_rescale_q(m_audio_pts_offset, (AVRational){1, 1000}, m_audio_stream->tbc);
        }
        
        if (mat.type == IM_DT_FLOAT32)
        {
            for (int c = 0; c < m_output_channels; c++)
            {
                ImGui::ImMat channel_mat = mat.channel(c);
                memcpy(m_audio_iframe->data[c], channel_mat.data, channel_mat.w * sizeof(float));
            }
        }
        else if (mat.type == IM_DT_INT16)
        {
            for (int c = 0; c < m_output_channels; c++)
            {
                float* dst = (float*)m_audio_iframe->data[c];
                for (int i = 0; i < mat.w; i++)
                {
                    *dst = mat.at<int16_t>(i, 0, c) / 32768.f;
                    dst ++;
                }
            }
        }

        add_samples_to_fifo(m_audio_fifo, m_audio_iframe->data, mat.w);

        if (!m_video_stream)
            return;

        if (!m_streaming)
            start_streaming();

        if (!m_audio_frame)
        {
            m_audio_frame_size = m_audio_stream->codec_ctx->frame_size;
            m_audio_frame = new_audio_frame(m_output_channels, m_output_sample_fmt, m_output_samplerate, m_audio_frame_size);
        }

        while(av_audio_fifo_size(m_audio_fifo) >= m_audio_frame_size)
        {
            if (av_audio_fifo_read(m_audio_fifo, (void**)m_audio_frame->data, m_audio_frame_size) < m_audio_frame_size)
                return;
            
            m_audio_frame->pts = m_audio_sample_num;
            m_audio_frame->pts += m_audio_pts_offset;
            m_audio_pts_ms = av_rescale_q(m_audio_frame->pts, m_audio_stream->tbc,
                                    (AVRational){ 1, 1000 });
            m_audio_sample_num += m_audio_frame->nb_samples;
            m_audio_frame_num++;

            m_sink.write_one_frame((const AVFrame*)m_audio_frame, m_audio_stream_idx);
            #if defined(PRINT_OUTPUT_PTS)
            printf("output audio frame %d pts %.3fs \n", m_audio_frame_num, m_audio_pts_ms/1000.f);
            #endif

            while(m_audio_pts_ms > m_video_pts_ms)
            {
                void* pFrame = nullptr;
                if (m_video_sink_queue->pop(pFrame, false))
                {
                    m_video_pts_ms = ((AVFrame*)pFrame)->pts;
                    ((AVFrame*)pFrame)->pts = av_rescale_q(((AVFrame*)pFrame)->pts,
                                            (AVRational){ 1, 1000 }, m_video_stream->tbc);
                    m_sink.write_one_frame((const AVFrame*)pFrame, m_video_stream_idx);
                    m_video_empty_queue->push(pFrame, false);
                    #if defined(PRINT_OUTPUT_PTS)
                    printf("output video frame %d pts %.3fs \n", m_video_frame_num, m_video_pts_ms/1000.f);
                    #endif
                }
                else
                    break;
            }
        }
        
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        else if (entryPoint.m_ID == m_VidIn.m_ID)
        {
            auto mat = context.GetPinValue<ImGui::ImMat>(m_VidMat);
            if (!mat.empty())
            {
                m_mutex.lock();
                m_input_width = mat.w;
                m_input_height = mat.h;
                m_frame_rate = {mat.rate.den != 0 ? mat.rate.num : 25, mat.rate.den != 0 ? mat.rate.den : 1};
                if(!m_video_stream)
                    add_video_stream();
#if IMGUI_VULKAN_SHADER
                if (!m_convert)
                {
                    int gpu = mat.device == IM_DD_VULKAN ? mat.device_number : ImGui::get_default_gpu_index();
                    m_convert = new ImGui::ColorConvert_vulkan(gpu);
                    if (!m_convert)
                    {
                        m_mutex.unlock();
                        return {};
                    }
                }
#endif
                if (mat.device == IM_DD_VULKAN)
                {
#if IMGUI_VULKAN_SHADER
                    ImGui::VkMat Im_RGB;
                    if (mat.c == 1)
                    {
                        int video_depth = mat.type == IM_DT_INT8 ? 8 : mat.type == IM_DT_INT16 ? 16 : 8;
                        int video_shift = mat.depth != 0 ? mat.depth : mat.type == IM_DT_INT8 ? 8 : mat.type == IM_DT_INT16 ? 16 : 8;
                        if (mat.flags & IM_MAT_FLAGS_VIDEO_FRAME_UV)
                            mat.color_format = IM_CF_NV12;
                        ImGui::VkMat in_mat = mat;
                        Im_RGB.type = in_mat.type;
                        m_convert->GRAY2RGBA(in_mat, Im_RGB, mat.color_space, mat.color_range, video_depth, video_shift);
                    }
                    else
                    {
                        Im_RGB = mat;
                    }
                    m_color_format = m_output_pix_fmt == AV_PIX_FMT_NV12 ? IM_CF_NV12 : m_output_pix_fmt == AV_PIX_FMT_P010LE ? IM_CF_P010LE : IM_CF_YUV420;
                    m_EncodeMat.type = m_video_bit_depth > 8 ? IM_DT_INT16 : IM_DT_INT8;
                    m_EncodeMat.color_format = m_color_format;
                    m_EncodeMat.color_space = m_color_space;
                    m_EncodeMat.color_range = m_color_range;
                    m_convert->ConvertColorFormat(Im_RGB, m_EncodeMat);
                    m_EncodeMat.depth = m_video_bit_depth;
                    m_EncodeMat.time_stamp = mat.time_stamp;
#endif
                }
                else if (mat.device == IM_DD_CPU)
                {
                    ImGui::ImMat mat_rgb;
                    if (mat.type != IM_DT_INT8)
                    {
                        mat_rgb.create(mat.w, mat.h, 4, 1u);
                        conv_mat_int8(mat, mat_rgb);
                    }
                    else
                    {
                        mat_rgb = mat;
                    }
#if !IMGUI_VULKAN_SHADER
                    int video_depth = mat.type == IM_DT_INT8 ? 8 : mat.type == IM_DT_INT16 ? 16 : 8;
                    int video_shift = mat.depth != 0 ? mat.depth : mat.type == IM_DT_INT8 ? 8 : mat.type == IM_DT_INT16 ? 16 : 8;
                    m_EncodeMat.create_type(mat.w, mat.h, 4, m_video_bit_depth > 8 ? IM_DT_INT16 : IM_DT_INT8);
                    if (!m_img_convert_ctx)
                    {
                        m_img_convert_ctx = sws_getCachedContext(
                                    m_img_convert_ctx,
                                    mat_rgb.w,
                                    mat_rgb.h,
                                    AV_PIX_FMT_RGBA,
                                    mat_rgb.w,
                                    mat_rgb.h,
                                    m_output_pix_fmt,
                                    SWS_BICUBIC,
                                    NULL, NULL, NULL);
                    }
                    if (m_img_convert_ctx)
                    {
                        ImGui::ImMat MatY = m_EncodeMat.channel(0);
                        ImGui::ImMat MatU = m_EncodeMat.channel(1);
                        ImGui::ImMat MatV = m_EncodeMat.channel(2);
                        int mat_u_width = MatU.w >> (m_color_format == IM_CF_YUV420 ? 1 : 0);
                        int mat_v_width = MatV.w >> (m_color_format == IM_CF_YUV420 ? 1 : 0);
                        int data_shift = m_video_bit_depth > 8 ? 2 : 1;
                        uint8_t *src_data[] = { (uint8_t *)mat_rgb.data };
                        uint8_t *dst_data[] = { (uint8_t *)MatY.data, (uint8_t *)MatU.data, (uint8_t *)MatV.data };
                        int src_linesize[] = { mat_rgb.w * 4 };
                        int dst_linesize[] = { MatY.w * data_shift, mat_u_width * data_shift, mat_v_width * data_shift};
                        sws_scale(
                            m_img_convert_ctx,
                            src_data,
                            src_linesize,
                            0, mat_rgb.h,
                            dst_data,
                            dst_linesize
                        );
                        m_EncodeMat.color_format = m_color_format;
                        m_EncodeMat.color_space = m_color_space;
                        m_EncodeMat.color_range = m_color_range;
                        m_EncodeMat.depth = m_video_bit_depth;
                        m_EncodeMat.time_stamp = mat_rgb.time_stamp;
                    }
#else
                    m_EncodeMat = mat;
#endif
                }
                else
                {
                    // TODO:JJ
                }

                encode_video();
                m_mutex.unlock();
            }
        }
        else if (entryPoint.m_ID == m_AudIn.m_ID && m_withAudio)
        {
            auto mat = context.GetPinValue<ImGui::ImMat>(m_AudMat);
            if (!mat.empty())
            {
                m_mutex.lock();
                m_input_samplerate = mat.rate.num;
                m_input_channels = mat.c;
                if(!m_audio_stream)
                    add_audio_stream(mat);
                encode_audio(mat);
                m_mutex.unlock();
            }
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        // Draw custom layout
        bool is_10bit = m_video_bit_depth > 8 ? true : false;
        changed |= ImGui::Checkbox("10bit video", &is_10bit);
        m_video_bit_depth = is_10bit ? 10 : 8;
        ImGui::Separator();
        changed |= ImGui::Checkbox("With Audio", &m_withAudio);
        ImGui::Separator();
        ImGui::Text("Encode Device: "); ImGui::SameLine();
        changed |= ImGui::RadioButton("CPU", &m_video_device, 0); ImGui::SameLine();
        changed |= ImGui::RadioButton("GPU", &m_video_device, 1);
        ImGui::Separator();
        ImGui::Text("Encode codec: "); ImGui::SameLine();
        changed |= ImGui::RadioButton("H264", &m_video_codec, 0); ImGui::SameLine();
        changed |= ImGui::RadioButton("H265", &m_video_codec, 1);
        ImGui::Separator();
        int video_bitrate = m_video_bitrate / (1000 * 1000);
        changed |= ImGui::SliderInt("Video Bitrate", &video_bitrate, 1, 200, "%dMbps", ImGuiSliderFlags_None);
        m_video_bitrate = video_bitrate * 1000 * 1000;
        int audio_bitrate = m_audio_bitrate / 1000;
        changed |= ImGui::SliderInt("Audio Bitrate", &audio_bitrate, 1, 500, "%dKbps", ImGuiSliderFlags_None);
        m_audio_bitrate = audio_bitrate * 1000;
        ImGui::Separator();
        changed |= ImGui::Combo("HDR System", (int *)&m_hdr_type, hdr_system_items, IM_ARRAYSIZE(hdr_system_items));
        if (m_hdr_type == FFMEDIA_HDRTYPE::HDR_HLG || m_hdr_type == FFMEDIA_HDRTYPE::HDR_PQ)
            m_color_space = IM_CS_BT2020;
        else
            m_color_space = IM_CS_BT709;
        ImGui::Separator();
        // open file dialog
        auto& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = true;
        string file_name;
        auto separator = m_save_file_path.find_last_of('/');
        if (separator != std::string::npos)
            file_name = m_save_file_path.substr(separator + 1);
        else
            file_name = m_save_file_path;
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_SaveFile_Default;
        if (!m_isShowBookmark)      vflags &= ~ImGuiFileDialogFlags_ShowBookmark;
        if (m_isShowHiddenFiles)    vflags &= ~ImGuiFileDialogFlags_DontShowHiddenFiles;
        if (m_Blueprint->GetStyleLight())
            ImGuiFileDialog::Instance()->SetLightStyle();
        else
            ImGuiFileDialog::Instance()->SetDarkStyle();
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Output File Path..."))
        {
            IGFD::FileDialogConfig config;
            config.path = file_name.empty() ? "." : file_name;
            config.countSelectionMax = 1;
            config.userDatas = this;
            config.flags = vflags;
            ImGuiFileDialog::Instance()->OpenDialog("##EncodeChooseFileDlgKey", "Choose File", 
                                                    m_filters.c_str(), 
                                                    config);
        }
        if (ImGuiFileDialog::Instance()->Display("##EncodeChooseFileDlgKey"))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_save_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        ImGui::SameLine(0);
        ImGui::TextUnformatted(file_name.c_str());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            io.ConfigViewportsNoDecoration = false;
        check_hw_caps();
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("save_path_name"))
        {
            auto& val = value["save_path_name"];
            if (val.is_string()) 
                m_save_file_path = val.get<imgui_json::string>();
        }
        if (value.contains("filter"))
        {
            auto& val = value["filter"];
            if (val.is_string())
            {
                m_filters = val.get<imgui_json::string>();
            }
        }
        if (value.contains("show_bookmark"))
        {
            auto& val = value["show_bookmark"];
            if (val.is_boolean()) m_isShowBookmark = val.get<imgui_json::boolean>();
        }
        if (value.contains("show_hidden"))
        {
            auto& val = value["show_hidden"];
            if (val.is_boolean()) m_isShowHiddenFiles = val.get<imgui_json::boolean>();
        }
        if (value.contains("with_audio"))
        {
            auto& val = value["with_audio"];
            if (val.is_boolean()) m_withAudio = val.get<imgui_json::boolean>();
        }
        if (value.contains("encode_device"))
        {
            auto& val = value["encode_device"];
            if (val.is_string()) 
                m_video_device = val.get<imgui_json::string>() == "CPU" ? 0 : 1;
        }
        if (value.contains("encode_codec"))
        {
            auto& val = value["encode_codec"];
            if (val.is_string()) 
                m_video_codec = val.get<imgui_json::string>() == "H264" ? 0 : 1;
        }
        if (value.contains("video_bitrate"))
        {
            auto& val = value["video_bitrate"];
            if (val.is_number()) 
                m_video_bitrate = val.get<imgui_json::number>();
        }
        if (value.contains("audio_bitrate"))
        {
            auto& val = value["audio_bitrate"];
            if (val.is_number()) 
                m_audio_bitrate = val.get<imgui_json::number>();
        }
        if (value.contains("video_bit_depth"))
        {
            auto& val = value["video_bit_depth"];
            if (val.is_number()) 
                m_video_bit_depth = val.get<imgui_json::number>();
        }
        if (value.contains("hdr_type"))
        {
            auto& val = value["hdr_type"];
            if (val.is_number()) 
                m_hdr_type = (FFMEDIA_HDRTYPE)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["save_path_name"] = m_save_file_path;
        value["show_bookmark"] = m_isShowBookmark;
        value["show_hidden"] = m_isShowHiddenFiles;
        value["filter"] = m_filters;
        value["with_audio"] = m_withAudio;
        value["encode_device"] = !!m_video_device ? "GPU" : "CPU";
        value["encode_codec"] = !!m_video_codec ? "HEVC" : "H264";
        value["video_bitrate"] = imgui_json::number(m_video_bitrate);
        value["audio_bitrate"] = imgui_json::number(m_audio_bitrate);
        value["video_bit_depth"] = imgui_json::number(m_video_bit_depth);
        value["hdr_type"] = imgui_json::number(m_hdr_type);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin   m_VidIn   = { this, "V.Enter" };
    FlowPin   m_AudIn   = { this, "A.Enter" };
    FlowPin   m_Reset   = { this, "Reset" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_VidMat  = { this, "Video Mat" };
    MatPin    m_AudMat  = { this, "Audio Mat" };
    Pin* m_InputPins[5] = { &m_VidIn, &m_AudIn, &m_Reset, &m_VidMat, &m_AudMat };
    Pin* m_OutputPins[2] = { &m_Exit, &m_OReset };

#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan *m_convert   {nullptr};
#else
    struct SwsContext *m_img_convert_ctx {nullptr};
#endif
    std::string m_save_file_path    {"output.mp4"};
    std::string m_filters {".mp4"};
    bool m_isShowBookmark {false};
    bool m_isShowHiddenFiles {true};
    std::mutex m_mutex;
    ImGui::ImMat m_EncodeMat;

    // configurations
    int m_input_width           {0};
    int m_input_height          {0};
    AVRational m_frame_rate     {25, 1};
    AVPixelFormat m_output_pix_fmt {AV_PIX_FMT_NV12};
    int m_output_samplerate     {44100};
    int m_output_channels       {2};
    int m_input_samplerate      {0};
    int m_input_channels        {0};
    AVSampleFormat m_input_sample_fmt {AV_SAMPLE_FMT_FLT};
    AVSampleFormat m_output_sample_fmt {AV_SAMPLE_FMT_FLTP};
    bool m_withAudio            {true};
    int m_video_codec           {0};  // 0 for H264, 1 for HEVC
    int m_video_device          {0}; // 0 for CPU, 1 for GPU
    int m_video_bitrate         {10*1000*1000};
    int m_audio_bitrate         {128*1000};

    // encoder
    int m_stream_index              {0};
    FFMediaSink m_sink;
    FFMediaStream* m_video_stream   {nullptr};
    int m_video_stream_idx          {-1};
    int m_video_frame_queue_size    {16};
    FFMedia_Queue* m_video_empty_queue {nullptr};
    FFMedia_Queue* m_video_sink_queue {nullptr};
    int m_video_frame_num           {0};
    int m_video_drop_frames         {0};
    int64_t m_video_pts_ms          {0};
    int m_video_bit_depth           {8};
    ImColorFormat m_color_format    {IM_CF_NV12};
    ImColorSpace m_color_space      {IM_CS_BT709};           // TODO::need check input Mat flags
    ImColorRange m_color_range      {IM_CR_NARROW_RANGE};    // TODO::need check input Mat flags
    FFMEDIA_HDRTYPE m_hdr_type      {FFMEDIA_HDRTYPE::SDR};

    FFMediaStream* m_audio_stream   {nullptr};
    int m_audio_stream_idx          {-1};
    AVFrame* m_audio_frame          {nullptr};
    AVAudioFifo* m_audio_fifo       {nullptr};
    int m_audio_frame_size          {0};
    int m_audio_sample_num          {0};
    int m_audio_frame_num           {0};
    int64_t m_audio_pts_ms          {0};
    AVFrame* m_audio_iframe         {nullptr};
    int64_t m_audio_pts_offset      {AV_NOPTS_VALUE};

    // control
    bool m_streaming {false};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(FFMpegEncoderNode, "Media Encoder", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
