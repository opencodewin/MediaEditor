#ifndef __FPS_FILTER_H__
#define __FPS_FILTER_H__

#include "ffmedia_define.h"

#include <deque>

enum class RoundPolicy
{
    POLICY_NEAR = 0
};

class FpsFilterItem
{
public:
    int64_t output_pts_codec;
    AVFrame* frame;
};

class FpsFilter
{
public:
    /*
    timebase_in is stream timebase by default
    timebase_out is output 1/fps by default
    policy is timebase
    */
    FpsFilter(AVRational timebase_in, AVRational timebase_out, RoundPolicy policy,
              bool keep_timebase = false);
    virtual ~FpsFilter();
    /*
    reset start timestamp, and filter status
    */
    long reset_filter();
    /*
    send new frame to filter
    */
    long send_new_frame(AVFrame* frame);
    /*
    get frame from filter
    * @return
    *      0 on success, otherwise negative error code:
    *      AVERROR(EAGAIN):   output is not available in the current state -
    user
    *                         must try to send input
    *      AVERROR_EOF:       the encoder has been fully flushed, and there will
    be
    *                         no more output packets
    */
    long receive_revised_frame(AVFrame*& frame);
    long update_input_fps(AVRational fps_in);
    long set_maxium_gap(int64_t timestamp_gap_in_ms);

private:
    AVRational m_tb_in;
    AVRational m_tb_out;
    int64_t m_pts_offset_in;
    int64_t m_pts_offset_out;
    int m_roundpolicy;
    int64_t m_next_pts;
    bool m_keep_timebase;
    std::deque<FpsFilterItem> m_buffer_frames;
    long update_filter_buffer();

    /* statistics */
    int m_cur_frame_out;  ///< number of times current frame has been output
    int m_frames_in;      ///< number of frames on input
    int m_frames_out;     ///< number of frames on output
    int m_frames_dup;     ///< number of frames duplicated
    int m_frames_drop;    ///< number of framed dropped
};

#endif // __FPS_FILTER_H__
