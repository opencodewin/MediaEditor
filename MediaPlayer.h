#ifndef __GST_MEDIA_PLAYER_H_
#define __GST_MEDIA_PLAYER_H_

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <future>
#include <list>

// GStreamer
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>

#include <imgui_mat.h>

#include "Timeline.h"

//#define LIMIT_DISCOVERER
#if !IMGUI_VULKAN_SHADER
#define VIDEO_FORMAT_RGBA
#else
#define VIDEO_FORMAT_NV12
//#define VIDEO_FORMAT_YV12
#endif
#define MEDIA_PLAYER_DEBUG

#define MAX_PLAY_SPEED 20.0
#define MIN_PLAY_SPEED 0.1
#define N_VFRAME 5
#define N_AFRAME 20

struct MediaInfo
{
    // video info
    guint width;
    guint par_width;  // width to match pixel aspect ratio
    guint height;
    guint depth;
    guint bitrate;
    guint framerate_n;
    guint framerate_d;
    std::string video_codec_name;
    GstVideoInfo frame_video_info;
    bool isimage;
    bool interlaced;
    bool video_valid;
    // audio info
    guint audio_sample_rate;
    guint audio_channels;
    guint audio_depth;
    guint audio_bitrate;
    std::string audio_codec_name;
    GstAudioInfo frame_audio_info;
    bool audio_valid;
    // system info
    bool seekable;
    GstClockTime dt;
    GstClockTime end;

    MediaInfo() {
        width = par_width = 640;
        height = 480;
        depth = 8;
        bitrate = 0;
        framerate_n = 1;
        framerate_d = 25;
        video_codec_name = "";
        isimage = false;
        interlaced = false;
        video_valid = false;
        audio_sample_rate = 44100;
        audio_channels = 2;
        audio_depth = 16;
        audio_bitrate = 0;
        audio_codec_name = "";
        audio_valid = false;
        seekable = false;
        
        dt  = GST_CLOCK_TIME_NONE;
        end = GST_CLOCK_TIME_NONE;
    }

    inline MediaInfo& operator = (const MediaInfo& b)
    {
        if (this != &b) {
            this->dt = b.dt;
            this->end = b.end;
            this->width = b.width;
            this->par_width = b.par_width;
            this->height = b.height;
            this->depth = b.depth;
            this->bitrate = b.bitrate;
            this->framerate_n = b.framerate_n;
            this->framerate_d = b.framerate_d;
            this->video_codec_name = b.video_codec_name;
            this->video_valid = b.video_valid;
            this->frame_video_info = b.frame_video_info;
            this->isimage = b.isimage;
            this->interlaced = b.interlaced;
            this->seekable = b.seekable;
            this->audio_sample_rate = b.audio_sample_rate;
            this->audio_channels = b.audio_channels;
            this->audio_depth = b.audio_depth;
            this->audio_bitrate = b.audio_bitrate;
            this->audio_codec_name = b.audio_codec_name;
            this->audio_valid = b.audio_valid;
            this->frame_audio_info = b.frame_audio_info;
        }
        return *this;
    }
};

class MediaPlayer {

public:

    /**
     * Constructor of a GStreamer Media Player
     */
    MediaPlayer();
    /**
     * Destructor.
     */
    ~MediaPlayer();
    /**
     * Get unique id
     */
    inline uint64_t id() const { return id_; }
    /** 
     * Open a media using gstreamer URI 
     * */
    void open ( const std::string &filename, const std::string &uri = "");
    void reopen ();
    /**
     * Get name of the media
     * */
    std::string uri() const;
    /**
     * Get name of the file
     * */
    std::string filename() const;
    /**
     * Get name of Codec of the media
     * */
    MediaInfo media() const;
    /**
     * True if a media was oppenned
     * */
    bool isOpen() const;
    /**
     * True if problem occured
     * */
    bool failed() const;
    /**
     * Close the Media
     * */
    void close();
    /**
     * Update texture with latest frame
     * Must be called in rendering update loop
     * */
    void update();
    /**
     * Enable / Disable
     * Suspend playing activity
     * (restores playing state when re-enabled)
     * */
    void enable(bool on);
    /**
     * True if enabled
     * */
    bool isEnabled() const;
    /**
     * True if its an image
     * */
    bool isImage() const;
    /**
     * Pause / Play
     * Can play backward if play speed is negative
     * */
    void play(bool on);
    /**
     * Get Pause / Play status
     * Performs a full check of the Gstreamer pipeline if testpipeline is true
     * */
    bool isPlaying(bool testpipeline = false) const;
    /**
     * Speed factor for playing
     * Can be negative.
     * */
    double playSpeed() const;
    /**
     * Set the speed factor for playing
     * Can be negative.
     * */
    void setPlaySpeed(double s);
    /**
     * Loop Mode: Behavior when reaching an extremity
     * */
    typedef enum {
        LOOP_NONE = 0,
        LOOP_REWIND = 1,
        LOOP_BIDIRECTIONAL = 2
    } LoopMode;
    /**
     * Get the current loop mode
     * */
    LoopMode loop() const;
    /**
     * Set the loop mode
     * */
    void setLoop(LoopMode mode);
    /**
     * Seek to next frame when paused
     * (aka next frame)
     * Can go backward if play speed is negative
     * */
    void step();
    /**
     * Jump fast when playing
     * (aka fast-forward)
     * Can go backward if play speed is negative
     * */
    void jump();
    /**
     * Seek to zero
     * */
    void rewind(bool force = false);
    /**
     * Get position time
     * */
    GstClockTime position();
    /**
     * go to a valid position in media timeline
     * pos in nanoseconds.
     * return true if seek is performed
     * */
    bool go_to(GstClockTime pos);
    /**
     * Seek to any position in media
     * pos in nanoseconds.
     * */
    void seek(GstClockTime pos);
    /**
     * Get Audio volume
     * 0.0-1.0
     */
    double volume() const;
    /**
     * Set audio volume
     * 0.0 - 1.0(100%)
     */
    void set_volume(double vol);
    /**
     * @brief timeline contains all info on timing:
     * - start position : timeline.start()
     * - end position   : timeline.end()
     * - duration       : timeline.duration()
     * - frame duration : timeline.step()
     */
    Timeline *timeline();
    void setTimeline(const Timeline &tl);

    float currentTimelineFading();
    /**
     * Get framerate of the media
     * */
    double frameRate() const;
    /**
     * Get rendering update framerate
     * measured during play
     * */
    double updateFrameRate() const;
    /**
     * Get frame width
     * */
    guint width() const;
    /**
     * Get frame height
     * */
    guint height() const;
    /**
     * Get frames display aspect ratio
     * NB: can be different than width() / height()
     * */
    float aspectRatio() const;
    /**
     * Get the Frame
     * */
    ImGui::ImMat videoMat() const;
    /**
     * Get audio sample rate
     */
    guint sample_rate() const;
    /**
     * Get audio channels
     */
    guint channels() const;
    /**
     * Get audio sample depth
     */
    guint audio_depth() const;
    /**
     * Get audio channel levels
     */
    guint audio_level(guint channel) const;
    ImGui::ImMat audioMat() const;
    /**
     * Get the name of the decoder used,
     * return 'software' if no hardware decoder is used
     * NB: perform request on pipeline on first call
     * */
    std::string decoderName();
    /**
     * Forces open using software decoding
     * (i.e. without hadrware decoding)
     * NB: this reopens the video and reset decoder name
     * */
    void setSoftwareDecodingForced(bool on);
    bool softwareDecodingForced();
    /**
     * Option to automatically rewind each time the player is disabled
     * (i.e. when enable(false) is called )
     * */
    inline void setRewindOnDisabled(bool on) { rewind_on_disable_ = on; }
    inline bool rewindOnDisabled() const { return rewind_on_disable_; }
    /**
     * Accept visitors
     * */
    //void accept(Visitor& v);
    /**
     * @brief registered
     * @return list of media players currently registered
     */
    static std::list<MediaPlayer*> registered() { return registered_; }
    static std::list<MediaPlayer*>::const_iterator begin() { return registered_.cbegin(); }
    static std::list<MediaPlayer*>::const_iterator end()   { return registered_.cend(); }

    static MediaInfo UriDiscoverer(const std::string &uri);

private:

    // player description
    uint64_t id_;
    std::string filename_;
    std::string uri_;

    // video output
    ImGui::ImMat VMat;
    // audio output
    std::vector<int> audio_channel_level;
    ImGui::ImMat AMat;

    // general properties of media
    MediaInfo media_;
    Timeline timeline_;
    std::future<MediaInfo> discoverer_;

    // GST & Play status
    GstClockTime position_;
    gdouble rate_;
    LoopMode loop_;
    GstState desired_state_;
    GstElement *pipeline_;
    GstVideoInfo o_frame_video_info_;
    GstAudioInfo o_frame_audio_info_;
    std::atomic<bool> opened_;
    std::atomic<bool> failed_;
    bool seeking_;
    bool enabled_;
    bool rewind_on_disable_;
    bool force_software_decoding_;
    std::string decoder_name_;
    GstElement *volume_;

    // fps counter
    struct TimeCounter {
        GTimer *timer;
        gdouble fps;
    public:
        TimeCounter();
        ~TimeCounter();
        void tic();
        inline gdouble frameRate() const { return fps; }
    };
    TimeCounter timecount_;

    // frame stack
    typedef enum  {
        SAMPLE = 0,
        PREROLL = 1,
        EOS = 2,
        INVALID = 3
    } FrameStatus;

    struct VFrame {
        GstVideoFrame frame;
        FrameStatus status;
        bool full;
        GstClockTime position;
        std::mutex access;

        VFrame() {
            full = false;
            status = INVALID;
            position = GST_CLOCK_TIME_NONE;
        }
        void unmap();
    };

     struct AFrame {
        GstAudioBuffer frame;
        FrameStatus status;
        bool full;
        GstClockTime position;
        std::mutex access;

        AFrame() {
            full = false;
            status = INVALID;
            position = GST_CLOCK_TIME_NONE;
        }
        void unmap();
    };

    // for video frame
    VFrame vframe_[N_VFRAME];
    guint vwrite_index_;
    guint vlast_index_;
    std::mutex vindex_lock_;

    // for audio frame
    AFrame aframe_[N_AFRAME];
    guint awrite_index_;
    guint alast_index_;
    std::mutex aindex_lock_;

    // gst pipeline control
    void execute_open();
    void execute_loop_command();
    void execute_seek_command(GstClockTime target = GST_CLOCK_TIME_NONE);

    // gst frame filling
    void fill_video(guint index);
    void fill_audio(guint index);
    bool fill_video_frame(GstBuffer *buf, FrameStatus status);
    bool fill_audio_frame(GstBuffer *buf, FrameStatus status);

    // gst video callbacks
    static void video_callback_end_of_stream (GstAppSink *, gpointer);
    static GstFlowReturn video_callback_new_preroll (GstAppSink *, gpointer );
    static GstFlowReturn video_callback_new_sample  (GstAppSink *, gpointer);

    // gst audio callbacks
    static void audio_callback_end_of_stream (GstAppSink *, gpointer);
    static GstFlowReturn audio_callback_new_preroll (GstAppSink *, gpointer );
    static GstFlowReturn audio_callback_new_sample  (GstAppSink *, gpointer);

    // global list of registered media player
    static std::list<MediaPlayer*> registered_;
};



#endif // __GST_MEDIA_PLAYER_H_
