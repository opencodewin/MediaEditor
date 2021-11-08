#include <thread>

using namespace std;

#define ABS_DIFF(a, b) ( (a) < (b) ? (b - a) : (a - b) )
#define SIGN(a)	   (((a) < 0) ? -1.0 : 1.0)
#include "Log.h"
#include "SystemToolkit.h"
#include "BaseToolkit.h"
#include "GstToolkit.h"
#include "MediaPlayer.h"

std::list<MediaPlayer*> MediaPlayer::m_registered;

template<typename T>
static int calculate_audio_db(const T* data, int channels, int channel_index, size_t length, const float max_level) 
{
    static const float kMaxSquaredLevel = max_level * max_level;
    constexpr float kMinLevel = -96.f;
    float sum_square_ = 0;
    size_t sample_count_ = 0;
    for (size_t i = 0; i < length; i += channels) 
    {
        T audio_data = data[i + channel_index];
        sum_square_ += audio_data * audio_data;
    }
    sample_count_ += length / channels;
    float rms = sum_square_ / (sample_count_ * kMaxSquaredLevel);
    rms = 10 * log10(rms);
    if (rms < kMinLevel)
        rms = kMinLevel;
    rms = -kMinLevel + rms;
    return static_cast<int>(rms + 0.5);
}

MediaPlayer::MediaPlayer()
{
    // create unique id
    m_id = BaseToolkit::uniqueId();

    m_uri = "undefined";
    m_pipeline = nullptr;
    m_video_appsink = nullptr;
    m_audio_appsink = nullptr;
    m_opened = false;
    m_is_camera = false;
    m_enabled = true;
    m_desired_state = GST_STATE_PAUSED;

    m_failed = false;
    m_seeking = false;
    m_rewind_on_disable = false;
    m_force_software_decoding = false;
    m_decoder_name = "";
    m_rate = 1.0;
    m_position = GST_CLOCK_TIME_NONE;
    m_loop_mode = LoopMode::LOOP_REWIND;

    // audio level
    m_audio_channel_level.clear();
}

MediaPlayer::~MediaPlayer()
{
    close();
}

ImGui::ImMat MediaPlayer::videoMat()
{
    ImGui::ImMat mat;
    if (!m_enabled  || !m_opened || m_failed)
        return mat;
    m_video_lock.lock();
    // Do we need jump directly (after seek) to a pre-roll ?
    // if (m_seeking)
    //{
    //}
    if (!m_video_frame.empty())
    {
        mat = m_video_frame.at(0);
        m_video_frame.erase(m_video_frame.begin());
        // we just displayed a vframe : set position time to frame PTS
        if (m_position == GST_CLOCK_TIME_NONE || !m_media_info.audio_valid)
            m_position = isnan(mat.time_stamp) ?  GST_CLOCK_TIME_NONE : mat.time_stamp * 1e+9;
    }
    else
    {
#ifdef MEDIA_PLAYER_DEBUG
        //Log::Warning("Video Frame queue empty");
#endif
    }
    if (mat.flags & IM_MAT_FLAGS_CUSTOM_EOS)
        m_need_loop = true;
    m_video_lock.unlock();
    return mat;
}

ImGui::ImMat MediaPlayer::audioMat()
{
    ImGui::ImMat mat;
    if (!m_enabled  || !m_opened || m_failed)
        return mat;
    m_audio_lock.lock();
    // Do we need jump directly (after seek) to a pre-roll ?
    // if (m_seeking)
    //{
    //}
    if (!m_audio_frame.empty())
    {
        mat = m_audio_frame.at(0);
        m_audio_frame.erase(m_audio_frame.begin());
        // do we set position time to frame PTS ? Sync time to audio
        //if (m_position == GST_CLOCK_TIME_NONE || !m_media_info.video_valid)
            m_position = isnan(mat.time_stamp) ? GST_CLOCK_TIME_NONE : mat.time_stamp * 1e+9;
    }
    else
    {
#ifdef MEDIA_PLAYER_DEBUG
        //Log::Warning("Audio Frame queue empty");
#endif
    }
    m_audio_lock.unlock();
    if (mat.flags & IM_MAT_FLAGS_CUSTOM_EOS)
        m_need_loop = true;

    return mat;
}

guint MediaPlayer::audio_level(guint channel) const
{
    if (channel < m_audio_channel_level.size())
    {
        return m_audio_channel_level[channel];
    }
    return 0;
}

MediaInfo MediaPlayer::UriDiscoverer(const std::string &uri)
{
#ifdef MEDIA_PLAYER_DEBUG
    Log::Debug("Checking file '%s'", uri.c_str());
#endif

#ifdef LIMIT_DISCOVERER
    // Limiting the number of discoverer thread to TWO in parallel
    // Otherwise, a large number of discoverers are executed (when loading a file)
    // leading to a peak of memory and CPU usage : this causes slow down of FPS
    // and a hungry consumption of RAM.
    static std::mutex mtx_primary;
    static std::mutex mtx_secondary;
    bool use_primary = true;
    if (!mtx_primary.try_lock())
    { 
        // non-blocking
        use_primary = false;
        mtx_secondary.lock(); // blocking
    }
#endif
    MediaInfo stream_info;
    GError *err = NULL;
    GstDiscoverer *discoverer = gst_discoverer_new(5 * GST_SECOND, &err);

    /* Instantiate the Discoverer */
    if (!discoverer)
    {
        Log::Warning("MediaPlayer Error creating discoverer instance: %s\n", err->message);
    }
    else 
    {
        GstDiscovererInfo *info = NULL;
        info = gst_discoverer_discover_uri(discoverer, uri.c_str(), &err);
        GstDiscovererResult result = gst_discoverer_info_get_result(info);
        switch (result)
        {
        case GST_DISCOVERER_URI_INVALID:
            Log::Error("'%s': Invalid URI", uri.c_str());
            break;
        case GST_DISCOVERER_ERROR:
            Log::Error("'%s': %s", uri.c_str(), err->message);
            break;
        case GST_DISCOVERER_TIMEOUT:
            Log::Error("'%s': Timeout loading", uri.c_str());
            break;
        case GST_DISCOVERER_BUSY:
            Log::Error("'%s': Busy", uri.c_str());
            break;
        case GST_DISCOVERER_MISSING_PLUGINS:
        {
            const GstStructure *s = gst_discoverer_info_get_misc(info);
            gchar *str = gst_structure_to_string(s);
            Log::Error("'%s': Unknown file format (%s)", uri.c_str(), str);
            g_free(str);
        }
            break;
        default:
        case GST_DISCOVERER_OK:
            break;
        }

        // no error, handle information found
        if (result == GST_DISCOVERER_OK)
        {
            GList *video_streams = gst_discoverer_info_get_video_streams(info);
            GList *tmp;
            for (tmp = video_streams; tmp && !stream_info.video_valid; tmp = tmp->next)
            {
                GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *)tmp->data;
                if (!stream_info.video_valid && GST_IS_DISCOVERER_VIDEO_INFO(tmpinf))
                {
                    // found a video / image stream : fill-in information
                    GstDiscovererVideoInfo* vinfo = GST_DISCOVERER_VIDEO_INFO(tmpinf);
                    stream_info.width = gst_discoverer_video_info_get_width(vinfo);
                    stream_info.height = gst_discoverer_video_info_get_height(vinfo);
                    guint parn = gst_discoverer_video_info_get_par_num(vinfo);
                    guint pard = gst_discoverer_video_info_get_par_denom(vinfo);
                    stream_info.par_width = (stream_info.width * parn) / pard;
                    stream_info.depth = gst_discoverer_video_info_get_depth(vinfo);
                    stream_info.interlaced = gst_discoverer_video_info_is_interlaced(vinfo);
                    stream_info.bitrate = gst_discoverer_video_info_get_bitrate(vinfo);
                    stream_info.isimage = gst_discoverer_video_info_is_image(vinfo);
                    // if its a video, set duration, framerate, etc.
                    if (!stream_info.isimage)
                    {
                        stream_info.end = gst_discoverer_info_get_duration (info) ;
                        stream_info.seekable = gst_discoverer_info_get_seekable (info);
                        stream_info.framerate_n = gst_discoverer_video_info_get_framerate_num(vinfo);
                        stream_info.framerate_d = gst_discoverer_video_info_get_framerate_denom(vinfo);
                        if (stream_info.framerate_n == 0 || stream_info.framerate_d == 0)
                        {
                            Log::Info("'%s': No framerate indicated in the file; using default 30fps", uri.c_str());
                            stream_info.framerate_n = 30;
                            stream_info.framerate_d = 1;
                        }
                        stream_info.dt = ( (GST_SECOND * static_cast<guint64>(stream_info.framerate_d)) / (static_cast<guint64>(stream_info.framerate_n)) );
                        // confirm (or infirm) that its not a single frame
                        if ( stream_info.end < stream_info.dt * 2)
                            stream_info.isimage = true;
                    }
                    // try to fill-in the codec information
                    GstCaps *caps = gst_discoverer_stream_info_get_caps(tmpinf);
                    if (caps)
                    {
                        gst_video_info_from_caps(&stream_info.frame_video_info, caps);
                        gchar *codecstring = gst_pb_utils_get_codec_description(caps);
                        stream_info.video_codec_name = std::string(codecstring);
                        g_free(codecstring);
                        gst_caps_unref(caps);
                    }
                    const GstTagList *tags = gst_discoverer_stream_info_get_tags(tmpinf);
                    if (tags)
                    {
                        gchar *container = NULL;
                        if ( gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &container))
                             stream_info.video_codec_name += ", " + std::string(container);
                        if (container)
                            g_free(container);
                    }
                    // exit loop
                    // inform that it succeeded
                    // TODO::Dicky if we need save all streams info?
                    stream_info.video_valid = true;
                }
            }
            gst_discoverer_stream_info_list_free(video_streams);

            GList *audio_streams = gst_discoverer_info_get_audio_streams(info);
            for (tmp = audio_streams; tmp && !stream_info.audio_valid; tmp = tmp->next)
            {
                GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
                if (!stream_info.audio_valid && GST_IS_DISCOVERER_AUDIO_INFO(tmpinf))
                {
                    // found a audio stream : fill-in information
                    GstDiscovererAudioInfo* ainfo = GST_DISCOVERER_AUDIO_INFO(tmpinf);
                    stream_info.audio_sample_rate = gst_discoverer_audio_info_get_sample_rate(ainfo);
                    stream_info.audio_channels = gst_discoverer_audio_info_get_channels(ainfo);
                    stream_info.audio_depth = gst_discoverer_audio_info_get_depth(ainfo);
                    stream_info.audio_bitrate = gst_discoverer_audio_info_get_bitrate(ainfo);
                    // try to fill-in the codec information
                    GstCaps *caps = gst_discoverer_stream_info_get_caps(tmpinf);
                    if (caps)
                    {
                        gst_audio_info_from_caps(&stream_info.frame_audio_info, caps);
                        gchar *codecstring = gst_pb_utils_get_codec_description(caps);
                        stream_info.video_codec_name = std::string(codecstring);
                        g_free(codecstring);
                        gst_caps_unref(caps);
                    }
                    const GstTagList *tags = gst_discoverer_stream_info_get_tags(tmpinf);
                    if (tags)
                    {
                        gchar *container = NULL;
                        if ( gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &container))
                             stream_info.audio_codec_name += ", " + std::string(container);
                        if (container)
                            g_free(container);
                    }
                    // exit loop
                    // inform that it succeeded
                    // TODO::Dicky if we need save all streams info?
                    stream_info.audio_valid = true;
                }
            }
            gst_discoverer_stream_info_list_free(audio_streams);

            if (!stream_info.video_valid)
            {
                Log::Warning("'%s': No video stream", uri.c_str());
            }
            if (!stream_info.audio_valid)
            {
                Log::Warning("'%s': No audio stream", uri.c_str());
            }
        }

        if (info)
            gst_discoverer_info_unref (info);

        gst_discoverer_stop (discoverer);
        g_object_unref(discoverer);
    }

    g_clear_error(&err);

#ifdef LIMIT_DISCOVERER
    if (use_primary)
        mtx_primary.unlock();
    else
        mtx_secondary.unlock();
#endif
    // return the info
    return stream_info;
}

void MediaPlayer::open(const std::string & filename, const string &uri)
{
    // set path
    m_filename = filename;

    // set uri to open
    if (uri.empty())
        m_uri = GstToolkit::filename_to_uri(filename);
    else
        m_uri = uri;

    if (m_uri.empty())
        m_failed = true;

    // close before re-openning
    if (isOpen())
        close();

    if (m_filename.compare("camera") == 0)
    {
        m_is_camera = true;
        execute_open_camera();
    }
    else
    {
        // start URI discovering thread:
        m_discoverer = std::async(MediaPlayer::UriDiscoverer, m_uri);
        // wait for discoverer to finish in the future (test in update)

        // debug without thread
        //m_media_info = MediaPlayer::UriDiscoverer(m_uri);
        //if (m_media_info.audio_valid)
        //{
        //    m_timeline.setEnd( m_media_info.end );
        //    m_timeline.setStep( m_media_info.dt );
        //    execute_open();
        //}
    }
}

void MediaPlayer::reopen()
{
    // re-openning is meaningfull only if it was already open
    if (m_pipeline != nullptr)
    {
        // reload : terminate pipeline and re-create it
        close();
        execute_open();
    }
}

void MediaPlayer::execute_open() 
{
    // Create gstreamer pipeline :
    //         "uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! appsink "
    // equivalent to command line
    //         "gst-launch-1.0 uridecodebin uri=file:///path_to_file/filename.mp4 ! videoconvert ! ximagesink"
    string description = "uridecodebin3 name=decoder uri=\"" + m_uri + "\" use-buffering=true ! queue max-size-time=0 ! ";
    // NB: queue adds some control over the buffer, thereby limiting the frame delay. zero size means no buffering

#ifdef VIDEO_FORMAT_RGBA
    // video deinterlacing method (if media is interlaced)
    //      tomsmocomp (0) – Motion Adaptive: Motion Search
    //      greedyh (1) – Motion Adaptive: Advanced Detection
    //      greedyl (2) – Motion Adaptive: Simple Detection
    //      vfir (3) – Blur Vertical
    //      linear (4) – Linear
    //      scalerbob (6) – Double lines
    if (m_media_info.interlaced)
        description += "deinterlace method=2 ! ";

    // video convertion algorithm (should only do colorspace conversion, no scaling)
    // chroma-resampler:
    //      Duplicates the samples when upsampling and drops when downsampling 0
    //      Uses linear interpolation 1 (default)
    //      Uses cubic interpolation 2
    //      Uses sinc interpolation 3
    //  dither:
    //      no dithering 0
    //      propagate rounding errors downwards 1
    //      Dither with floyd-steinberg error diffusion 2
    //      Dither with Sierra Lite error diffusion 3
    //      ordered dither using a bayer pattern 4 (default)
    description += "videoconvert chroma-resampler=1 dither=0 ! "; // fast
#else
    description += "videoconvert chroma-mode=3 primaries-mode=0 matrix-mode=3 ! ";
#endif

    // hack to compensate for lack of PTS in gif animations
    if (m_media_info.video_codec_name.compare("image/gst-libav-gif") == 0)
    {
        description += "videorate ! video/x-raw,framerate=";
        description += std::to_string(m_media_info.framerate_n) + "/";
        description += std::to_string(m_media_info.framerate_d) + " ! ";
    }

    // set app sink
    description += "appsink name=video_appsink";

    // set audio convert and sink
    if (m_media_info.audio_valid)
    {
        description += " decoder. ! queue max-size-time=0 ! audioconvert !";
        description += " audio/x-raw,channels=" + std::to_string(m_media_info.audio_channels);
        // format: { (string)F64LE, (string)F64BE, (string)F32LE, (string)F32BE, (string)S32LE, (string)S32BE, 
        //           (string)U32LE, (string)U32BE, (string)S24_32LE, (string)S24_32BE, (string)U24_32LE, (string)U24_32BE,
        //           (string)S24LE, (string)S24BE, (string)U24LE, (string)U24BE, (string)S20LE, (string)S20BE,
        //           (string)U20LE, (string)U20BE, (string)S18LE, (string)S18BE, (string)U18LE, (string)U18BE,
        //           (string)S16LE, (string)S16BE, (string)U16LE, (string)U16BE, (string)S8, (string)U8 }
#ifdef AUDIO_FORMAT_FLOAT
        description += ",format=F32LE";
#else
        description += ",format=S16LE";
#endif
        description += ",rate=" + std::to_string(m_media_info.audio_sample_rate) + " ! ";
        description += " appsink name=audio_appsink";
    }

    // parse pipeline descriptor
    GError *error = NULL;
    m_pipeline = gst_parse_launch(description.c_str(), &error);
    if (error != NULL)
    {
        Log::Warning("MediaPlayer %s Could not construct pipeline %s:\n%s", std::to_string(m_id).c_str(), description.c_str(), error->message);
        g_clear_error(&error);
        m_failed = true;
        return;
    }

    // setup pipeline
    g_object_set(G_OBJECT(m_pipeline), "name", std::to_string(m_id).c_str(), NULL);
    gst_pipeline_set_auto_flush_bus( GST_PIPELINE(m_pipeline), true);

    // format: { AYUV64, ARGB64, GBRA_12LE, GBRA_12BE, Y412_LE, Y412_BE, A444_10LE, GBRA_10LE, 
    //           A444_10BE, GBRA_10BE, A422_10LE, A422_10BE, A420_10LE, A420_10BE, RGB10A2_LE, BGR10A2_LE,
    //           Y410, GBRA, ABGR, VUYA, BGRA, AYUV, ARGB, 
    //           RGBA, A420, AV12, Y444_16LE, Y444_16BE, v216, P016_LE, P016_BE,
    //           Y444_12LE, GBR_12LE, Y444_12BE, GBR_12BE, I422_12LE, I422_12BE, Y212_LE, Y212_BE,
    //           I420_12LE, I420_12BE, P012_LE, P012_BE, Y444_10LE, GBR_10LE, Y444_10BE, GBR_10BE,
    //           r210, I422_10LE, I422_10BE, NV16_10LE32, Y210, v210, UYVP, I420_10LE,
    //           I420_10BE, P010_10LE, NV12_10LE32, NV12_10LE40, P010_10BE, Y444, RGBP, GBR,
    //           BGRP, NV24, xBGR, BGRx, xRGB, RGBx, BGR, IYU2,
    //           v308, RGB, Y42B, NV61, NV16, VYUY, UYVY, YVYU,
    //           YUY2, I420, YV12, NV21, NV12, NV12_64Z32, NV12_4L4, NV12_32L32,
    //           Y41B, IYU1, YVU9, YUV9, RGB16, BGR16, RGB15, BGR15,
    //           RGB8P, GRAY16_LE, GRAY16_BE, GRAY10_LE32, GRAY8 }
    int pixel_element_depth = m_media_info.depth / 3;
#ifdef VIDEO_FORMAT_RGBA
    string capstring = "video/x-raw,format=RGBA,width="+ std::to_string(m_media_info.width) +
            ",height=" + std::to_string(m_media_info.height);
#elif defined(VIDEO_FORMAT_NV12)
    string capstring = "video/x-raw,format=" + (pixel_element_depth == 8 ? std::string("NV12") : std::string("P010_10LE")) + ",width=" + std::to_string(m_media_info.width) +
            ",height=" + std::to_string(m_media_info.height);
#elif defined(VIDEO_FORMAT_YV12)
    string capstring = "video/x-raw,format=" + (pixel_element_depth == 8 ? std::string("I420") : std::string("I420_10LE")) + ",width=" + std::to_string(m_media_info.width) +
            ",height=" + std::to_string(m_media_info.height);
#else
    #error "please define VIDEO_FORMAT_ in header file"
#endif
    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps(&m_frame_video_info, caps))
    {
        Log::Warning("MediaPlayer %s Could not configure video frame info", std::to_string(m_id).c_str());
        m_failed = true;
        return;
    }

    // setup uridecodebin
    if (m_force_software_decoding)
    {
        g_object_set(G_OBJECT(gst_bin_get_by_name(GST_BIN(m_pipeline), "decoder")), "force-sw-decoders", true,  NULL);
    }

    // setup appsink
    m_video_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "video_appsink");
    if (!m_video_appsink)
    {
        Log::Warning("MediaPlayer %s Could not configure video_appsink", std::to_string(m_id).c_str());
        m_failed = true;
        return;
    }

    // instruct the sink to send samples synched in time
    gst_base_sink_set_sync(GST_BASE_SINK(m_video_appsink), true);

    // instruct sink to use the required caps
    gst_app_sink_set_caps(GST_APP_SINK(m_video_appsink), caps);

    // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
    gst_app_sink_set_max_buffers(GST_APP_SINK(m_video_appsink), 2);
    gst_app_sink_set_buffer_list_support(GST_APP_SINK(m_video_appsink), true);
    gst_app_sink_set_drop(GST_APP_SINK(m_video_appsink), false);

#ifdef USE_GST_APPSINK_CALLBACKS
    // set the callbacks
    GstAppSinkCallbacks callbacks;
    callbacks.new_preroll = video_callback_new_preroll;
    if (m_media_info.isimage)
    {
        callbacks.eos = NULL;
        callbacks.new_sample = NULL;
    }
    else 
    {
        callbacks.eos = video_callback_end_of_stream;
        callbacks.new_sample = video_callback_new_sample;
    }
    gst_app_sink_set_callbacks(GST_APP_SINK(m_video_appsink), &callbacks, this, NULL);
    gst_app_sink_set_emit_signals(GST_APP_SINK(m_video_appsink), false);
#else
    // connect video signals callbacks
    g_signal_connect(G_OBJECT(m_video_appsink), "new-preroll", G_CALLBACK(video_callback_new_preroll), this);
    if (!m_media_info.isimage)
    {
        g_signal_connect(G_OBJECT(m_video_appsink), "new-sample", G_CALLBACK(video_callback_new_sample), this);
        g_signal_connect(G_OBJECT(m_video_appsink), "eos", G_CALLBACK(video_callback_end_of_stream), this);
    }
    gst_app_sink_set_emit_signals(GST_APP_SINK(m_video_appsink), true);
#endif

    // done with ref to sink
    gst_caps_unref (caps);

    if (m_media_info.audio_valid)
    {
#ifdef AUDIO_FORMAT_FLOAT
        gst_audio_info_set_format(&m_frame_audio_info, GST_AUDIO_FORMAT_F32LE, m_media_info.audio_sample_rate, m_media_info.audio_channels, nullptr);
#else
        gst_audio_info_set_format(&m_frame_audio_info, GST_AUDIO_FORMAT_S16LE, m_media_info.audio_sample_rate, m_media_info.audio_channels, nullptr);
#endif
        GstCaps *caps_audio = gst_audio_info_to_caps(&m_frame_audio_info);
        if (!caps_audio)
        {
            Log::Warning("MediaPlayer %s Could not configure audio frame info", std::to_string(m_id).c_str());
            m_failed = true;
            return;
        }

        // setup audio app sink
        m_audio_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "audio_appsink");
        if (!m_audio_appsink)
        {
            Log::Warning("MediaPlayer %s Could not get audio_appsink", std::to_string(m_id).c_str());
        }
        else
        {
            // instruct the sink to send samples synched in time
            gst_base_sink_set_sync(GST_BASE_SINK(m_audio_appsink), true);

            // instruct sink to use the required caps
            gst_app_sink_set_caps(GST_APP_SINK(m_audio_appsink), caps_audio);

            // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
            gst_app_sink_set_max_buffers(GST_APP_SINK(m_audio_appsink), 20);
            gst_app_sink_set_buffer_list_support(GST_APP_SINK(m_audio_appsink), true);
            gst_app_sink_set_drop (GST_APP_SINK(m_audio_appsink), false);

#ifdef USE_GST_APPSINK_CALLBACKS
            // set the callbacks
            GstAppSinkCallbacks callbacks;
            callbacks.new_preroll = audio_callback_new_preroll;
            callbacks.eos = audio_callback_end_of_stream;
            callbacks.new_sample = audio_callback_new_sample;
            gst_app_sink_set_callbacks(GST_APP_SINK(m_audio_appsink), &callbacks, this, NULL);
            gst_app_sink_set_emit_signals(GST_APP_SINK(m_audio_appsink), false);
#else
            // connect video signals callbacks
            g_signal_connect(G_OBJECT(m_audio_appsink), "new-preroll", G_CALLBACK(audio_callback_new_preroll), this);
            g_signal_connect(G_OBJECT(m_audio_appsink), "new-sample", G_CALLBACK(audio_callback_new_sample), this);
            g_signal_connect(G_OBJECT(m_audio_appsink), "eos", G_CALLBACK(audio_callback_end_of_stream), this);
            gst_app_sink_set_emit_signals(GST_APP_SINK(m_audio_appsink), true);
#endif
        }
        // done with ref to audio caps
        gst_caps_unref(caps_audio);

        // init audio channel level
        m_audio_channel_level.resize(m_media_info.audio_channels);
    }

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, m_desired_state);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        Log::Warning("MediaPlayer %s Could not open '%s'", std::to_string(m_id).c_str(), m_uri.c_str());
        m_failed = true;
        return;
    }

    // in case discoverer failed to get duration
    if (m_timeline.end() == GST_CLOCK_TIME_NONE)
    {
        gint64 d = GST_CLOCK_TIME_NONE;
        if (gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &d))
            m_timeline.setEnd(d);
    }

    // all good
    Log::Info("MediaPlayer %s Opened '%s' (%s %d x %d)", std::to_string(m_id).c_str(),
              m_uri.c_str(), m_media_info.video_codec_name.c_str(), m_media_info.width, m_media_info.height);

    Log::Info("MediaPlayer %s Timeline [%ld %ld] %ld frames, %d gaps", std::to_string(m_id).c_str(),
              m_timeline.begin(), m_timeline.end(), m_timeline.numFrames(), m_timeline.numGaps());

    m_opened = true;

    // register media player
    MediaPlayer::m_registered.push_back(this);
}

void MediaPlayer::execute_open_camera() 
{
    // Create gstreamer pipeline
    // setup m_media_info
    m_media_info.audio_valid = false;
    m_media_info.width = m_media_info.par_width = 1280;
    m_media_info.height = 720;
    m_media_info.framerate_n = 1;
    m_media_info.framerate_d = 25;
    m_media_info.isimage = false;
    m_media_info.interlaced = false;
    m_media_info.depth = 24;

    string description = "";

#ifdef __APPLE__
    description += "avfvideosrc name=camera do-timestamp=true !";
#elif defined(__linux__)
    description += "v4l2src name=camera !"; // not test yet
#elif defined(_WIN32)
    description += "ksvideosrc name=camera !"; // not test yet
#else
    #error "Not supported platform"
#endif

    description += " videoscale ! videoconvert ! video/x-raw,format=" + std::string("NV12") + ",width=" + std::to_string(m_media_info.width) + ",height=" + std::to_string(m_media_info.height) + " !";
    
    description += "appsink name=video_appsink";

    // parse pipeline descriptor
    GError *error = NULL;
    m_pipeline = gst_parse_launch(description.c_str(), &error);
    if (error != NULL)
    {
        Log::Warning("MediaPlayer %s Could not construct pipeline %s:\n%s", std::to_string(m_id).c_str(), description.c_str(), error->message);
        g_clear_error(&error);
        m_failed = true;
        return;
    }

    m_media_info.video_valid = true;
    int pixel_element_depth = m_media_info.depth / 3;
    // setup pipeline
    g_object_set(G_OBJECT(m_pipeline), "name", std::to_string(m_id).c_str(), NULL);
    gst_pipeline_set_auto_flush_bus( GST_PIPELINE(m_pipeline), true);
    
#ifdef VIDEO_FORMAT_RGBA
    string capstring = "video/x-raw,format=BGRA,width="+ std::to_string(m_media_info.width) +
            ",height=" + std::to_string(m_media_info.height);
#elif defined(VIDEO_FORMAT_NV12)
    string capstring = "video/x-raw,format=" + std::string("NV12") + ",width=" + std::to_string(m_media_info.width) +
            ",height=" + std::to_string(m_media_info.height);
#elif defined(VIDEO_FORMAT_YV12)
    string capstring = "video/x-raw,format=" + (pixel_element_depth == 8 ? std::string("I420") : std::string("I420_10LE")) + ",width=" + std::to_string(m_media_info.width) +
            ",height=" + std::to_string(m_media_info.height);
#else
    #error "please define VIDEO_FORMAT_ in header file"
#endif

    GstCaps *caps = gst_caps_from_string(capstring.c_str());
    if (!gst_video_info_from_caps(&m_frame_video_info, caps))
    {
        Log::Warning("MediaPlayer %s Could not configure video frame info", std::to_string(m_id).c_str());
        m_failed = true;
        return;
    }

    // done with ref to sink
    gst_caps_unref (caps);

    // setup appsink
    m_video_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "video_appsink");
    if (!m_video_appsink)
    {
        Log::Warning("MediaPlayer %s Could not configure video_appsink", std::to_string(m_id).c_str());
        m_failed = true;
        return;
    }

    // Instruct appsink to drop old buffers when the maximum amount of queued buffers is reached.
    gst_app_sink_set_max_buffers(GST_APP_SINK(m_video_appsink), 2);
    gst_app_sink_set_drop(GST_APP_SINK(m_video_appsink), false);

#ifdef USE_GST_APPSINK_CALLBACKS
    // set the callbacks
    GstAppSinkCallbacks callbacks;
    callbacks.new_preroll = video_callback_new_preroll;
    if (m_media_info.isimage)
    {
        callbacks.eos = NULL;
        callbacks.new_sample = NULL;
    }
    else 
    {
        callbacks.eos = video_callback_end_of_stream;
        callbacks.new_sample = video_callback_new_sample;
    }
    gst_app_sink_set_callbacks(GST_APP_SINK(m_video_appsink), &callbacks, this, NULL);
    gst_app_sink_set_emit_signals(GST_APP_SINK(m_video_appsink), false);
#else
    // connect video signals callbacks
    g_signal_connect(G_OBJECT(m_video_appsink), "new-preroll", G_CALLBACK(video_callback_new_preroll), this);
    if (!m_media_info.isimage)
    {
        g_signal_connect(G_OBJECT(m_video_appsink), "new-sample", G_CALLBACK(video_callback_new_sample), this);
        g_signal_connect(G_OBJECT(m_video_appsink), "eos", G_CALLBACK(video_callback_end_of_stream), this);
    }
    gst_app_sink_set_emit_signals(GST_APP_SINK(m_video_appsink), true);
#endif

    // set to desired state (PLAY or PAUSE)
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, m_desired_state);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        Log::Warning("MediaPlayer %s Could not open '%s'", std::to_string(m_id).c_str(), m_uri.c_str());
        m_failed = true;
        return;
    }

    // all good
    Log::Info("MediaPlayer %s Opened '%s' (%s %d x %d)", std::to_string(m_id).c_str(),
              m_uri.c_str(), m_media_info.video_codec_name.c_str(), m_media_info.width, m_media_info.height);


    m_opened = true;
    // register media player
    MediaPlayer::m_registered.push_back(this);
}

bool MediaPlayer::isOpen() const
{
    return m_opened;
}

bool MediaPlayer::isCamera() const
{
    return m_is_camera;
}

bool MediaPlayer::failed() const
{
    return m_failed;
}

void MediaPlayer::clean_video_buffer(bool full)
{
    m_video_lock.lock();
    for (auto it = m_video_frame.begin(); it != m_video_frame.end();)
    {
        if (full)
        {
            it->release();
            it = m_video_frame.erase(it);
        }
        else 
        {
            if (!(it->flags & IM_MAT_FLAGS_CUSTOM_PREROLL))
            {
                it->release();
                it = m_video_frame.erase(it);
            }
            else
                ++it;
        }
    }
    m_video_lock.unlock();
}

void MediaPlayer::clean_audio_buffer(bool full)
{
    m_audio_lock.lock();
    for (auto it = m_audio_frame.begin(); it != m_audio_frame.end();)
    {
        if (full)
        {
            it->release();
            it = m_audio_frame.erase(it);
        }
        else 
        {
            if (!(it->flags & IM_MAT_FLAGS_CUSTOM_PREROLL))
            {
                it->release();
                it = m_audio_frame.erase(it);
            }
            else
                ++it;
        }
    }
    m_audio_lock.unlock();
}

void MediaPlayer::clean_buffer(bool full)
{
    clean_video_buffer(full);
    clean_audio_buffer(full);
}

void MediaPlayer::close()
{
    // not openned?
    if (!m_opened)
    {
        // wait for loading to finish
        if (m_discoverer.valid())
            m_discoverer.wait();
        // nothing else to change
        return;
    }

    // un-ready the media player
    m_opened = false;
    m_is_camera = false;
    m_failed = false;
    m_seeking = false;
    m_decoder_name = "";
    m_rate = 1.0;
    m_position = GST_CLOCK_TIME_NONE;
    // clean up GST
    if (m_pipeline != nullptr)
    {
        // force flush
        GstState state;
        gst_element_send_event(m_pipeline, gst_event_new_seek (1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                    GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_NONE, 0) );
        gst_element_get_state(m_pipeline, &state, NULL, GST_CLOCK_TIME_NONE);

        // end pipeline
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_element_get_state(m_pipeline, &state, NULL, GST_CLOCK_TIME_NONE);

        gst_object_unref (m_pipeline);
        m_pipeline = nullptr;
    }

    if (m_video_appsink != nullptr)
    {
        gst_object_unref(m_video_appsink);
        m_video_appsink = nullptr;
    }
    if (m_audio_appsink != nullptr)
    {
        gst_object_unref(m_audio_appsink);
        m_audio_appsink = nullptr;
    }

    clean_buffer(true);

    m_audio_channel_level.clear();

#ifdef MEDIA_PLAYER_DEBUG
    Log::Debug("MediaPlayer %s closed", std::to_string(m_id).c_str());
#endif

    // unregister media player
    MediaPlayer::m_registered.remove(this);
}


guint MediaPlayer::width() const
{
    return m_media_info.width;
}

guint MediaPlayer::height() const
{
    return m_media_info.height;
}

float MediaPlayer::aspectRatio() const
{
    return static_cast<float>(m_media_info.par_width) / static_cast<float>(m_media_info.height);
}

guint MediaPlayer::sample_rate() const
{
    return m_media_info.audio_sample_rate;
}

guint MediaPlayer::channels() const
{
    return m_media_info.audio_channels;
}

guint MediaPlayer::audio_depth() const
{
    return m_media_info.audio_depth;
}

GstClockTime MediaPlayer::position()
{
    if (m_position == GST_CLOCK_TIME_NONE && m_pipeline != nullptr)
    {
        gint64 p = GST_CLOCK_TIME_NONE;
        if ( gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &p) )
            m_position = p;
    }
    return m_position;
}

GstClockTime MediaPlayer::duration()
{
    return m_timeline.end();
}

void MediaPlayer::enable(bool on)
{
    if (!m_opened || m_pipeline == nullptr)
        return;

    if (m_enabled != on)
    {

        // option to automatically rewind each time the player is disabled
        if (!on && m_rewind_on_disable && m_desired_state == GST_STATE_PLAYING)
            rewind(true);

        // apply change
        m_enabled = on;

        // default to pause
        GstState requested_state = GST_STATE_PAUSED;

        // unpause only if enabled
        if (m_enabled)
            requested_state = m_desired_state;

        //  apply state change
        GstStateChangeReturn ret = gst_element_set_state(m_pipeline, requested_state);
        if (ret == GST_STATE_CHANGE_FAILURE)
        {
            Log::Warning("MediaPlayer %s Failed to enable", std::to_string(m_id).c_str());
            m_failed = true;
        }

    }
}

bool MediaPlayer::isEnabled() const
{
    return m_enabled;
}

bool MediaPlayer::isImage() const
{
    return m_media_info.isimage;
}

std::string MediaPlayer::decoderName()
{
    // m_decoder_name not initialized
    if (m_decoder_name.empty())
    {
        // try to know if it is a hardware decoder
        m_decoder_name = GstToolkit::used_gpu_decoding_plugins(m_pipeline);
        // nope, then it is a sofware decoder
        if (m_decoder_name.empty())
            m_decoder_name = "software";
    }

    return m_decoder_name;
}

bool MediaPlayer::softwareDecodingForced()
{
    return m_force_software_decoding;
}

void MediaPlayer::setSoftwareDecodingForced(bool on)
{
    bool need_reload = m_force_software_decoding != on;

    // set parameter
    m_force_software_decoding = on;
    m_decoder_name = "";

    // changing state requires reload
    if (need_reload)
        reopen();
}

void MediaPlayer::play(bool on)
{
    // ignore if disabled, and cannot play an image
    if (!m_enabled || m_media_info.isimage)
        return;

    // request state 
    GstState requested_state = on ? GST_STATE_PLAYING : GST_STATE_PAUSED;

    // ignore if requesting twice same state
    if (m_desired_state == requested_state)
        return;

    // accept request to the desired state
    m_desired_state = requested_state;

    // if not ready yet, the requested state will be handled later
    if (m_pipeline == nullptr)
        return;

    if (m_is_camera && !on)
    {
        close();
        return;
    }
    // requesting to play, but stopped at end of stream : rewind first !
    if (m_desired_state == GST_STATE_PLAYING)
    {
        if ((m_rate < 0.0 && m_position <= m_timeline.next(0))
             || (m_rate > 0.0 && m_position >= m_timeline.previous(m_timeline.last())))
            rewind();
    }

    // all ready, apply state change immediately
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, m_desired_state);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        Log::Warning("MediaPlayer %s Failed to set play state", std::to_string(m_id).c_str());
        m_failed = true;
    }

#ifdef MEDIA_PLAYER_DEBUG
    else if (on)
        Log::Debug("MediaPlayer %s Start", std::to_string(m_id).c_str());
    else
        Log::Debug("MediaPlayer %s Stop [%ld]", std::to_string(m_id).c_str(), position());
#endif
}

bool MediaPlayer::isPlaying(bool testpipeline) const
{
    // image cannot play
    if (m_media_info.isimage)
        return false;

    // if not ready yet, answer with requested state
    if ( !testpipeline || m_pipeline == nullptr || !m_enabled)
        return m_desired_state == GST_STATE_PLAYING;

    // if ready, answer with actual state
    GstState state;
    gst_element_get_state(m_pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    return state == GST_STATE_PLAYING;
}

bool MediaPlayer::isSeeking() const
{
    return m_seeking;
}

MediaPlayer::LoopMode MediaPlayer::loopMode() const
{
    return m_loop_mode;
}
    
void MediaPlayer::setLoopMode(MediaPlayer::LoopMode mode)
{
    m_loop_mode = mode;
}

void MediaPlayer::rewind(bool force)
{
    if (!m_enabled || !m_media_info.seekable)
        return;

    // playing forward, loop to begin
    if (m_rate > 0.0)
    {
        // begin is the end of a gab which includes the first PTS (if exists)
        // normal case, begin is zero
        execute_seek_command(m_timeline.next(0));
    }
    // playing backward, loop to endTimeInterval gap;
    else
    {
        // end is the start of a gab which includes the last PTS (if exists)
        // normal case, end is last frame
        execute_seek_command(m_timeline.previous(m_timeline.last()));
    }

    if (force)
    {
        GstState state;
        gst_element_get_state(m_pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
        update();
    }
}

void MediaPlayer::step()
{
    // useful only when Paused
    if (!m_enabled || isPlaying())
        return;

    if ((m_rate < 0.0 && m_position <= m_timeline.next(0))
         || (m_rate > 0.0 && m_position >= m_timeline.previous(m_timeline.last())))
        rewind();

    // step 
    gst_element_send_event(m_pipeline, gst_event_new_step(GST_FORMAT_BUFFERS, 1, ABS(m_rate), TRUE,  FALSE));
}

bool MediaPlayer::go_to(GstClockTime pos)
{
    bool ret = false;
    TimeInterval gap;
    if (pos != GST_CLOCK_TIME_NONE )
    {
        GstClockTime jumpPts = pos;
        if (m_timeline.getGapAt(pos, gap))
        {
            // if in a gap, find closest seek target
            if (gap.is_valid())
            {
                // jump in one or the other direction
                jumpPts = (m_rate > 0.f) ? gap.end : gap.begin;
            }
        }

        if (ABS_DIFF(m_position, jumpPts) > 2 * m_timeline.step())
        {
            ret = true;
            seek( jumpPts );
        }
    }
    return ret;
}

void MediaPlayer::seek(GstClockTime pos)
{
    if (!m_enabled || !m_media_info.seekable || m_seeking)
        return;

    // apply seek
    GstClockTime target = CLAMP(pos, m_timeline.begin(), m_timeline.end());
    execute_seek_command(target);
}

void MediaPlayer::jump()
{
    if (!m_enabled || !isPlaying())
        return;

    gst_element_send_event(m_pipeline, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 30.f * ABS(m_rate), TRUE,  FALSE));
}

void MediaPlayer::update()
{
    // not ready yet
    if (!m_opened)
    {
        if (m_discoverer.valid())
        {
            // try to get info from discoverer
            if (m_discoverer.wait_for( std::chrono::milliseconds(500) ) == std::future_status::ready )
            {
                m_media_info = m_discoverer.get();
                // if its ok, open the media
                if (m_media_info.video_valid || m_media_info.audio_valid)
                {
                    m_timeline.setEnd(m_media_info.end);
                    m_timeline.setStep(m_media_info.dt);
                    m_failed = false;
                    execute_open();
                }
                else 
                {
                    Log::Error("MediaPlayer %s Loading cancelled", std::to_string(m_id).c_str());
                    m_failed = true;
                }
            }
            else
            {
                Log::Warning("MediaPlayer %s Discoverer not ready", std::to_string(m_id).c_str());
            }
        }
        // wait next frame to display
        return;
    }

    // discard
    if (m_failed)
        return;

    // prevent unnecessary updates: disabled or already filled image
    if (!m_enabled)
        return;

    // if already seeking (asynch)
    if (m_seeking)
    {
        // request status update to pipeline (re-sync gst thread)
        GstState state;
        gst_element_get_state(m_pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
        // seek should be resolved next frame
        // m_seeking = false; // ?? do in preroll
        // do NOT do another seek yet
    }
    // otherwise check for need to seek (pipeline management)
    else 
    {
        // manage timeline: test if position falls into a gap
        TimeInterval gap;
        if (m_position != GST_CLOCK_TIME_NONE && m_timeline.getGapAt(m_position, gap))
        {
            // if in a gap, seek to next section
            if (gap.is_valid())
            {
                // jump in one or the other direction
                GstClockTime jumpPts = m_timeline.step(); // round jump time to frame pts
                if (m_rate > 0.f)
                    jumpPts *= (gap.end / m_timeline.step()) + 1; // FWD: go to end of gap
                else
                    jumpPts *= (gap.begin / m_timeline.step());   // BWD: go to begin of gap
                // (if not beginnig or end of timeline)
                if (jumpPts > m_timeline.first() && jumpPts < m_timeline.last())
                    // seek to jump PTS time
                    seek(jumpPts);
                // otherwise, we should loop
                else
                    m_need_loop = true;
            }
        }
    }

    // manage loop mode
    if (m_need_loop)
    {
        execute_loop_command();
    }
}

void MediaPlayer::execute_loop_command()
{
    if (m_loop_mode==LOOP_REWIND)
    {
        rewind();
    } 
    else if (m_loop_mode==LOOP_BIDIRECTIONAL)
    {
        m_rate *= - 1.f;
        execute_seek_command();
    }
    else 
    {
        //LOOP_NONE
        play(false);
    }
    m_need_loop = false;
}

void MediaPlayer::execute_seek_command(GstClockTime target)
{
    if (m_pipeline == nullptr || !m_media_info.seekable)
        return;

    // seek position : default to target
    GstClockTime seek_pos = target;

    // no target given
    if (target == GST_CLOCK_TIME_NONE) 
        // create seek event with current position (rate changed ?)
        seek_pos = m_position;
    // target is given but useless
    else if (ABS_DIFF(target, m_position) < m_timeline.step())
    {
        // ignore request
        return;
    }

    // seek with flush (always)
    int seek_flags = GST_SEEK_FLAG_FLUSH;

    // seek with trick mode if fast speed
    if (ABS(m_rate) > 1.0)
        seek_flags |= GST_SEEK_FLAG_TRICKMODE;
    else
        seek_flags |= GST_SEEK_FLAG_ACCURATE;

    // create seek event depending on direction
    GstEvent *seek_event = nullptr;
    if (m_rate > 0)
    {
        seek_event = gst_event_new_seek(m_rate, GST_FORMAT_TIME, (GstSeekFlags) seek_flags,
            GST_SEEK_TYPE_SET, seek_pos, GST_SEEK_TYPE_END, 0);
    }
    else 
    {
        seek_event = gst_event_new_seek(m_rate, GST_FORMAT_TIME, (GstSeekFlags) seek_flags,
            GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, seek_pos);
    }

    // Send the event (ASYNC)
    if (seek_event && !gst_element_send_event(m_pipeline, seek_event))
        Log::Warning("MediaPlayer %s Seek failed", std::to_string(m_id).c_str());
    else 
    {
        m_seeking = true;
#ifdef MEDIA_PLAYER_DEBUG
        Log::Debug("MediaPlayer %s Seek %ld %.1f", std::to_string(m_id).c_str(), seek_pos, m_rate);
#endif
    }
}

void MediaPlayer::setPlaySpeed(double s)
{
    if (m_media_info.isimage)
        return;

    // bound to interval [-MAX_PLAY_SPEED MAX_PLAY_SPEED] 
    m_rate = CLAMP(s, -MAX_PLAY_SPEED, MAX_PLAY_SPEED);
    // skip interval [-MIN_PLAY_SPEED MIN_PLAY_SPEED]
    if (ABS(m_rate) < MIN_PLAY_SPEED)
        m_rate = SIGN(m_rate) * MIN_PLAY_SPEED;
        
    // apply with seek
    execute_seek_command();
}

double MediaPlayer::playSpeed() const
{
    return m_rate;
}

Timeline *MediaPlayer::timeline()
{
    return &m_timeline;
}

float MediaPlayer::currentTimelineFading()
{
    return m_timeline.fadingAt(m_position);
}

void MediaPlayer::setTimeline(const Timeline &tl)
{
    m_timeline = tl;
}

MediaInfo MediaPlayer::media() const
{
    return m_media_info;
}

std::string MediaPlayer::uri() const
{
    return m_uri;
}

std::string MediaPlayer::filename() const
{
    return m_filename;
}

double MediaPlayer::frameRate() const
{
    return static_cast<double>(m_media_info.framerate_n) / static_cast<double>(m_media_info.framerate_d);;
}

double MediaPlayer::updateFrameRate() const
{
    return m_timecount.frameRate();
}


// CALLBACKS
void MediaPlayer::fill_video(GstVideoFrame* frame, FrameStatus status, GstClockTime position, GstClockTime duration)
{
    ImGui::ImMat mat;
    if (!frame)
    {
        if (status == INVALID && position == GST_CLOCK_TIME_NONE && duration == GST_CLOCK_TIME_NONE)
        {
            // Can't get frame from buffer, ignore
            return;
        }
        else if (status == UNSUPPORTED)
        {
            mat.time_stamp = position == GST_CLOCK_TIME_NONE ? NAN : position / (1e+9);
            mat.duration = duration == GST_CLOCK_TIME_NONE ? NAN : duration / (1e+9);
            mat.flags |= IM_MAT_FLAGS_VIDEO_FRAME | IM_MAT_FLAGS_CUSTOM_UNSUPPORTED;
        }
        else if (status == EOS)
        {
            mat.time_stamp = position == GST_CLOCK_TIME_NONE ? NAN : position / (1e+9);
            mat.flags |= IM_MAT_FLAGS_VIDEO_FRAME | IM_MAT_FLAGS_CUSTOM_EOS;
        }
        else
        {
            // (should never happen)
            return;
        }
    }
    else
    {
#ifdef VIDEO_FORMAT_RGBA
        int data_shift = m_media_info.depth > 32 ? 1 : 0;
#ifdef WIN32
        data_shift = 0; // WIN32 only support 8bit RGBA?
#endif
        mat.create_type(m_media_info.width, m_media_info.height, 4, data_shift ? IM_DT_INT16 : IM_DT_INT8);
        uint8_t* src_data = (uint8_t*)frame->data[0];
        uint8_t* dst_data = (uint8_t*)mat.data;
        memcpy(dst_data, src_data, m_media_info.width * m_media_info.height * (data_shift ? 2 : 1) * 4);
#else
        int data_shift = frame->info.finfo->bits > 8;
#ifdef VIDEO_FORMAT_NV12
        int UV_shift_w = 0;
#elif defined(VIDEO_FORMAT_YV12)
        int UV_shift_w = 1;
#else
        #error "please define VIDEO_FORMAT_ in header file"
#endif
        int UV_shift_h = 1;
        mat.create_type(m_media_info.width, m_media_info.height, 4, data_shift ? IM_DT_INT16 : IM_DT_INT8);
        ImGui::ImMat mat_Y = mat.channel(0);
        {
            uint8_t* src_data = (uint8_t*)frame->data[0];
            uint8_t* dst_data = (uint8_t*)mat_Y.data;
            for (int i = 0; i < m_media_info.height; i++)
            {
                memcpy(dst_data, src_data, m_media_info.width * (data_shift ? 2 : 1));
                src_data += GST_VIDEO_FRAME_PLANE_STRIDE(frame, 0);
                dst_data += m_media_info.width << data_shift;
            }
        }
        ImGui::ImMat mat_Cb = mat.channel(1);
        {
            uint8_t* src_data = (uint8_t*)frame->data[1];
            uint8_t* dst_data = (uint8_t*)mat_Cb.data;
            for (int i = 0; i < m_media_info.height >> UV_shift_h; i++)
            {
                memcpy(dst_data, src_data, (m_media_info.width >> UV_shift_w) * (data_shift ? 2 : 1));
                src_data += GST_VIDEO_FRAME_PLANE_STRIDE(frame, 1);
                dst_data += (m_media_info.width >> UV_shift_w) << data_shift;
            }
        }
#ifdef VIDEO_FORMAT_YV12
        ImGui::ImMat mat_Cr = mat.channel(2);
        {
            uint8_t* src_data = (uint8_t*)frame->data[2];
            uint8_t* dst_data = (uint8_t*)mat_Cr.data;
            for (int i = 0; i < m_media_info.height >> UV_shift_h; i++)
            {
                memcpy(dst_data, src_data, (m_media_info.width >> UV_shift_w) * (data_shift ? 2 : 1));
                src_data += GST_VIDEO_FRAME_PLANE_STRIDE(frame, 2);
                dst_data += (m_media_info.width >> UV_shift_w) << data_shift;
            }
        }
#endif
#endif
        auto color_space = GST_VIDEO_INFO_COLORIMETRY(&m_media_info.frame_video_info);
        auto color_range = GST_VIDEO_INFO_CHROMA_SITE(&m_media_info.frame_video_info);
        mat.time_stamp = position == GST_CLOCK_TIME_NONE ? NAN : position / (1e+9);
        mat.duration = duration == GST_CLOCK_TIME_NONE ? NAN : duration / (1e+9);
        mat.depth = m_media_info.depth / 3;
        mat.rate = {static_cast<int>(m_media_info.framerate_n), static_cast<int>(m_media_info.framerate_d)};
        mat.flags = IM_MAT_FLAGS_VIDEO_FRAME;
#ifdef VIDEO_FORMAT_RGBA
        mat.color_space = IM_CS_SRGB;
        mat.color_format = IM_CF_ABGR;
        mat.color_range = IM_CR_FULL_RANGE;
#else
        mat.color_space = color_space.primaries == GST_VIDEO_COLOR_PRIMARIES_BT709 ? IM_CS_BT709 :
                        color_space.primaries == GST_VIDEO_COLOR_PRIMARIES_BT2020 ? IM_CS_BT2020 : IM_CS_BT601;
        mat.color_range = color_range == GST_VIDEO_CHROMA_SITE_JPEG ? IM_CR_FULL_RANGE : 
                        color_range == GST_VIDEO_CHROMA_SITE_MPEG2 ? IM_CR_NARROW_RANGE : IM_CR_NARROW_RANGE;
#ifdef VIDEO_FORMAT_NV12
        mat.color_format = data_shift ? mat.depth == 10 ? IM_CF_P010LE : IM_CF_NV12 :IM_CF_NV12;
        mat.flags |= IM_MAT_FLAGS_VIDEO_FRAME_UV;
#elif defined(VIDEO_FORMAT_YV12)
        mat.color_format = IM_CF_YUV420;
#else
        #error "please define VIDEO_FORMAT_ in header file"
#endif
        if (GST_VIDEO_INFO_IS_INTERLACED(&frame->info))  mat.flags |= IM_MAT_FLAGS_VIDEO_INTERLACED;
#endif
        if (gst_video_colorimetry_matches(&color_space, GST_VIDEO_COLORIMETRY_BT2100_PQ)) mat.flags |= IM_MAT_FLAGS_VIDEO_HDR_PQ;
        if (gst_video_colorimetry_matches(&color_space, GST_VIDEO_COLORIMETRY_BT2100_HLG)) mat.flags |= IM_MAT_FLAGS_VIDEO_HDR_HLG;
        if (status == PREROLL) mat.flags |= IM_MAT_FLAGS_CUSTOM_PREROLL;
        if (status == SAMPLE) mat.flags |= IM_MAT_FLAGS_CUSTOM_NORMAL;
    }

    // put mat into queue
    if (m_video_frame.size() >= N_VFRAME)
    {
        auto bmat = m_video_frame.at(0);
        bmat.release();
        m_video_frame.erase(m_video_frame.begin());
#ifdef MEDIA_PLAYER_DEBUG
        Log::Debug("MediaPlayer %s Video buffer full", std::to_string(m_id).c_str());
#endif
    }
    m_video_frame.push_back(mat);
}

bool MediaPlayer::fill_video_frame(GstBuffer *buf, FrameStatus status)
{
    if (status == PREROLL)
    {
        clean_video_buffer(true);
        m_seeking = false;
    }

    // lock access to frame
    m_video_lock.lock();

    // a buffer is given (not EOS)
    if (buf != NULL)
    {
        GstVideoFrame frame;
        // get the frame from buffer
        if ( !gst_video_frame_map(&frame, &m_frame_video_info, buf, GST_MAP_READ))
        {
#ifdef MEDIA_PLAYER_DEBUG
            Log::Debug("MediaPlayer %s Failed to map the video buffer", std::to_string(m_id).c_str());
#endif
            // free access to frame & exit
            fill_video(nullptr, INVALID, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);
            m_video_lock.unlock();
            return false;
        }

        // successfully filled the frame
        //m_position = buf->pts; // ?
        // validate frame format
#ifdef VIDEO_FORMAT_RGBA
        if (GST_VIDEO_INFO_IS_RGB(&frame.info) && GST_VIDEO_INFO_N_PLANES(&frame.info) == 1)
#elif defined(VIDEO_FORMAT_NV12)
        if (GST_VIDEO_INFO_IS_YUV(&frame.info) && GST_VIDEO_INFO_N_PLANES(&frame.info) == 2) // NV12/NV16
#elif defined(VIDEO_FORMAT_YV12)
        if (GST_VIDEO_INFO_IS_YUV(&frame.info) && GST_VIDEO_INFO_N_PLANES(&frame.info) == 3) // I420/I420_10LE
#else
        #error "please define VIDEO_FORMAT_ in header file"
#endif
        {
            // put video data into video queue and set presentation time stamp
            fill_video(&frame, status, buf->pts, buf->duration);
            // set the start position (i.e. pts of first frame we got)
            if (m_timeline.first() == GST_CLOCK_TIME_NONE)
            {
                m_timeline.setFirst(buf->pts);
            }
        }
        else 
        {
            // full but invalid frame : will be deleted next iteration
            // (should never happen)
#ifdef MEDIA_PLAYER_DEBUG
            Log::Debug("MediaPlayer %s Received an Invalid video frame", std::to_string(m_id).c_str());
#endif
            gst_video_frame_unmap(&frame);
            // free access to frame & exit
            fill_video(nullptr, UNSUPPORTED, buf->pts, buf->duration);
            m_video_lock.unlock();
            return false;
        }
        gst_video_frame_unmap(&frame);
    }
    else 
    {
        // else; null buffer for EOS: give a position
        fill_video(nullptr, EOS, m_rate > 0.0 ? m_timeline.end() : m_timeline.begin(), GST_CLOCK_TIME_NONE);
    }

    // unlock access to frame
    m_video_lock.unlock();

    // calculate actual FPS of update
    m_timecount.tic();

    return true;
}

void MediaPlayer::video_callback_end_of_stream(GstAppSink *, gpointer p)
{
    MediaPlayer *m = static_cast<MediaPlayer *>(p);
    if (m && m->m_opened)
    {
        m->fill_video_frame(NULL, MediaPlayer::EOS);
    }
}

GstFlowReturn MediaPlayer::video_callback_new_preroll(GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // blocking read pre-roll samples
    GstSample *sample = gst_app_sink_pull_preroll(sink);

    // if got a valid sample
    if (sample != NULL)
    {
        // send frames to media player only if ready
        MediaPlayer *m = static_cast<MediaPlayer *>(p);
        if (m && m->m_opened)
        {
            // get buffer from sample
            GstBuffer *buf = gst_sample_get_buffer (sample);

            // fill frame from buffer
            if (!m->fill_video_frame(buf, MediaPlayer::PREROLL))
                ret = GST_FLOW_ERROR;
            else if (m->playSpeed() < 0.f && !(buf->pts > 0) ) 
            {
                // loop negative rate: emulate an EOS
                m->fill_video_frame(NULL, MediaPlayer::EOS);
            }
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref(sample);

    return ret;
}

GstFlowReturn MediaPlayer::video_callback_new_sample(GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    // non-blocking read new sample
    GstSample *sample = gst_app_sink_pull_sample(sink);

    // if got a valid sample
    if (sample != NULL && !gst_app_sink_is_eos (sink))
    {
        // send frames to media player only if ready
        MediaPlayer *m = static_cast<MediaPlayer *>(p);
        if (m && m->m_opened)
        {
            // get buffer from sample (valid until sample is released)
            GstBuffer *buf = gst_sample_get_buffer (sample) ;

            // fill frame with buffer
            if (!m->fill_video_frame(buf, MediaPlayer::SAMPLE))
                ret = GST_FLOW_ERROR;
            else if (m->playSpeed() < 0.f && !(buf->pts > 0) )
            {
                // loop negative rate: emulate an EOS
                m->fill_video_frame(NULL, MediaPlayer::EOS);
            }
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}

void MediaPlayer::fill_audio(GstAudioBuffer* frame, FrameStatus status, GstClockTime position, GstClockTime duration)
{
    ImGui::ImMat mat;
    if (!frame)
    {
        if (status == INVALID && position == GST_CLOCK_TIME_NONE && duration == GST_CLOCK_TIME_NONE)
        {
            // Can't get frame from buffer, ignore
            return;
        }
        else if (status == UNSUPPORTED)
        {
            mat.time_stamp = position == GST_CLOCK_TIME_NONE ? NAN : position / (1e+9);
            mat.duration = duration == GST_CLOCK_TIME_NONE ? NAN : duration / (1e+9);
            mat.flags |= IM_MAT_FLAGS_AUDIO_FRAME | IM_MAT_FLAGS_CUSTOM_UNSUPPORTED;
        }
        else if (status == EOS)
        {
            mat.time_stamp = position == GST_CLOCK_TIME_NONE ? NAN : position / (1e+9);
            mat.flags |= IM_MAT_FLAGS_AUDIO_FRAME | IM_MAT_FLAGS_CUSTOM_EOS;
        }
        else
        {
            // (should never happen)
            return;
        }
    }
    else
    {
        // deal with the frame at reading index
        auto data = frame->planes[0];
        // calculate audio channels' level
        for (int i = 0; i < m_frame_audio_info.channels; i++)
        {
#ifdef AUDIO_FORMAT_FLOAT
            m_audio_channel_level[i] = calculate_audio_db<float>((const float *)data, m_frame_audio_info.channels, i, frame->n_samples, 1.0f);
#else
            m_audio_channel_level[i] = calculate_audio_db<int16_t>((const int16_t *)data, m_frame_audio_info.channels, i, frame->n_samples, (float)(1 << 15));
#endif
        }
        auto total_sample_length = frame->n_samples;
        // change packet mode data to planner data
#ifdef AUDIO_FORMAT_FLOAT
        mat.create_type(total_sample_length, 1, m_frame_audio_info.channels, IM_DT_FLOAT32);
        float * buffer = (float *)data;
#else
        mat.create_type(total_sample_length, 1, m_frame_audio_info.channels, IM_DT_INT16);
        int16_t * buffer = (int16_t *)data;
#endif
        for (int i = 0; i < mat.w; i++)
        {
            for (int c = 0; c < mat.c; c++)
            {
#ifdef AUDIO_FORMAT_FLOAT
                mat.at<float>(i, 0, c) = (*buffer); buffer ++;
#else
                mat.at<int16_t>(i, 0, c) = (*buffer); buffer ++;
#endif
            }
        }
        mat.time_stamp = position == GST_CLOCK_TIME_NONE ? NAN : position / 1e+9;
        mat.duration = duration == GST_CLOCK_TIME_NONE ? NAN : duration / 1e+9;
        mat.rate = {m_frame_audio_info.rate, 1};
        mat.flags = IM_MAT_FLAGS_AUDIO_FRAME;
        if (status == PREROLL) mat.flags |= IM_MAT_FLAGS_CUSTOM_PREROLL;
        if (status == SAMPLE) mat.flags |= IM_MAT_FLAGS_CUSTOM_NORMAL;
    }

    // put mat into queue
    if (m_audio_frame.size() >= N_AFRAME)
    {
        auto bmat = m_audio_frame.at(0);
        bmat.release();
        m_audio_frame.erase(m_audio_frame.begin());
#ifdef MEDIA_PLAYER_DEBUG
        Log::Debug("MediaPlayer %s Audio buffer full", std::to_string(m_id).c_str());
#endif
    }
    m_audio_frame.push_back(mat);
}

bool MediaPlayer::fill_audio_frame(GstBuffer *buf, FrameStatus status)
{
    if (status == PREROLL)
    {
        clean_audio_buffer(true);
        m_seeking = false;
    }

    // lock access to frame
    m_audio_lock.lock();

    // a buffer is given (not EOS)
    if (buf != NULL)
    {
        GstAudioBuffer frame;
        // get the frame from buffer
        if (!gst_audio_buffer_map (&frame, &m_frame_audio_info, buf, GST_MAP_READ))
        {
#ifdef MEDIA_PLAYER_DEBUG
            Log::Debug("MediaPlayer %s Failed to map the audio buffer", std::to_string(m_id).c_str());
#endif
            // free access to frame & exit
            fill_audio(&frame, INVALID, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);
            m_audio_lock.unlock();
            return false;
        }

        // successfully filled the frame
        //m_position = buf->pts; // ?
        // validate frame format
#ifdef AUDIO_FORMAT_FLOAT
        if (GST_AUDIO_INFO_IS_FLOAT(&frame.info) && GST_AUDIO_BUFFER_N_PLANES(&frame) == 1)
#else
        if (GST_AUDIO_INFO_IS_INTEGER(&frame.info) && GST_AUDIO_BUFFER_N_PLANES(&frame) == 1)
#endif
        {
            // put data into audio queue and set presentation time stamp
            fill_audio(&frame, status, buf->pts, buf->duration);
            // set the start position (i.e. pts of first frame we got)
            if (m_timeline.first() == GST_CLOCK_TIME_NONE) 
            {
                m_timeline.setFirst(buf->pts);
            }
        }
        else 
        {
            // full but invalid frame : will be deleted next iteration
            // (should never happen)
#ifdef MEDIA_PLAYER_DEBUG
            Log::Debug("MediaPlayer %s Received an Invalid audio frame", std::to_string(m_id).c_str());
#endif
            // free access to frame & exit
            gst_audio_buffer_unmap(&frame);
            fill_audio(nullptr, UNSUPPORTED, buf->pts, buf->duration);
            m_audio_lock.unlock();
            return false;
        }
        gst_audio_buffer_unmap(&frame);
    }
    else 
    {
        // else; null buffer for EOS: give a position
        fill_audio(nullptr, EOS, m_rate > 0.0 ? m_timeline.end() : m_timeline.begin(), GST_CLOCK_TIME_NONE);
    }

    // unlock access to frame
    m_audio_lock.unlock();

    return true;
}

void MediaPlayer::audio_callback_end_of_stream(GstAppSink *, gpointer p)
{
    MediaPlayer *m = static_cast<MediaPlayer *>(p);
    if (m && m->m_opened)
    {
        m->fill_audio_frame(NULL, MediaPlayer::EOS);
    }
}

GstFlowReturn MediaPlayer::audio_callback_new_preroll(GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;
    // blocking read pre-roll samples
    GstSample *sample = gst_app_sink_pull_preroll(sink);

    // if got a valid sample
    if (sample != NULL)
    {
        // send frames to media player only if ready
        MediaPlayer *m = static_cast<MediaPlayer *>(p);
        if (m && m->m_opened)
        {
            // get buffer from sample
            GstBuffer *buf = gst_sample_get_buffer (sample);
            // fill audio from buffer
            if (!m->fill_audio_frame(buf, MediaPlayer::PREROLL))
                ret = GST_FLOW_ERROR;
            else if (m->playSpeed() < 0.f && !(buf->pts > 0) )
            {
                // loop negative rate: emulate an EOS
                m->fill_audio_frame(NULL, MediaPlayer::EOS);
            }
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}

GstFlowReturn MediaPlayer::audio_callback_new_sample (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;
    // non-blocking read new sample
    GstSample *sample = gst_app_sink_pull_sample(sink);

    // if got a valid sample
    if (sample != NULL && !gst_app_sink_is_eos(sink))
    {
        // send frames to media player only if ready
        MediaPlayer *m = static_cast<MediaPlayer *>(p);
        if (m && m->m_opened)
        {
            // get buffer from sample (valid until sample is released)
            GstBuffer *buf = gst_sample_get_buffer(sample) ;
            // fill audio with buffer
            if ( !m->fill_audio_frame(buf, MediaPlayer::SAMPLE))
                ret = GST_FLOW_ERROR;
            else if (m->playSpeed() < 0.f && !(buf->pts > 0))
            {
                // loop negative rate: emulate an EOS
                m->fill_audio_frame(NULL, MediaPlayer::EOS);
            }
        }
    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}

float MediaPlayer::video_buffer_extent()
{
    float ret = 0.0;
    ret = (float)m_video_frame.size() / (float)N_VFRAME;
    return ret;
}

float MediaPlayer::audio_buffer_extent()
{
    float ret = 0.0;
    ret = (float)m_audio_frame.size() / (float)N_AFRAME;
    return ret;
}

MediaPlayer::TimeCounter::TimeCounter()
{
    timer = g_timer_new ();
}

MediaPlayer::TimeCounter::~TimeCounter()
{
    g_free(timer);
}

void MediaPlayer::TimeCounter::tic ()
{
    const double dt = g_timer_elapsed (timer, NULL) * 1000.0;

    // ignore refresh after too little time
    if (dt > 3.0)
    {
        // restart timer
        g_timer_start(timer);
        // calculate instantaneous framerate
        // Exponential moving averate with previous framerate to filter jitter
        fps = CLAMP( 0.5 * fps + 500.0 / dt, 0.0, 1000.0);
    }
}

