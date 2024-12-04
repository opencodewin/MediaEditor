#include "fps_filter.h"

//#define PRINT_DEBUG_INFO

FpsFilter::FpsFilter(AVRational timebase_in, AVRational timebase_out, RoundPolicy policy,
                     bool keep_timebase)
{
    m_tb_in = timebase_in;
    m_tb_out = timebase_out;
    switch (policy)
    {
    case RoundPolicy::POLICY_NEAR:
    default:
        m_roundpolicy = AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX;
        break;
    }
    m_keep_timebase = keep_timebase;
    m_buffer_frames.clear();
    reset_filter();
}

FpsFilter::~FpsFilter()
{
    for (auto item : m_buffer_frames)
    {
        av_frame_free(&item.frame);
    }
    m_buffer_frames.clear();
}

long FpsFilter::reset_filter()
{
    for (auto item : m_buffer_frames)
    {
        av_frame_free(&item.frame);
    }
    m_buffer_frames.clear();
    m_pts_offset_in = AV_NOPTS_VALUE;
    m_pts_offset_out = AV_NOPTS_VALUE;
    m_next_pts = AV_NOPTS_VALUE;
    m_cur_frame_out = 0;
    m_frames_in = 0;
    m_frames_out = 0;
    m_frames_drop = 0;
    m_frames_dup = 0;
    return 0;
}

long FpsFilter::send_new_frame(AVFrame* frame)
{
    if (frame == nullptr)
    {
        fprintf(stdout, "[FPS_FILTER]Warning: empty input frame, maybe EOF \n");
        return AVERROR_EOF;
    }
    if (frame->pts == AV_NOPTS_VALUE)
    {
        fprintf(stderr, "[FPS_FILTER]Error: no input pts \n");
        return AVERROR(EINVAL);
    }
    AVFrame* new_frame = av_frame_clone(frame);
    av_frame_free(&frame);

    if (m_pts_offset_in == AV_NOPTS_VALUE || m_pts_offset_out == AV_NOPTS_VALUE)
    {
        m_pts_offset_in = new_frame->pts;
        m_pts_offset_out =
            av_rescale_q_rnd(new_frame->pts, m_tb_in, m_tb_out, (enum AVRounding)m_roundpolicy);
        m_next_pts = m_pts_offset_out;
    }
    FpsFilterItem item;
    item.frame = new_frame;
    item.output_pts_codec = m_pts_offset_out
                            + av_rescale_q_rnd(new_frame->pts - m_pts_offset_in, m_tb_in, m_tb_out,
                                               (enum AVRounding)m_roundpolicy);
    m_buffer_frames.emplace_back(item);
    return 0;
}

long FpsFilter::receive_revised_frame(AVFrame*& frame)
{
    if (m_buffer_frames.size() <= 1)
    {
#if defined(PRINT_DEBUG_INFO)
        fprintf(stdout, "[FPS_FILTER]Info: not enough bufferd frames \n");
#endif
        return AVERROR(EAGAIN);
    }
    if (m_next_pts == AV_NOPTS_VALUE)
    {
        if (m_buffer_frames[0].output_pts_codec != AV_NOPTS_VALUE)
        {
            m_next_pts = m_buffer_frames[0].output_pts_codec;
            // fall down
        }
        else
        {
            fprintf(stderr, "[FPS_FILTER]Error: buffer with no timestamp \n");
            update_filter_buffer();
            return AVERROR(EAGAIN);
        }
    }
    if (m_buffer_frames.size() >= 2 && m_buffer_frames[1].output_pts_codec <= m_next_pts)
    {
        // need drop frame
        update_filter_buffer();
#if defined(PRINT_DEBUG_INFO)
        fprintf(stdout, "[FPS_FILTER]Info: first buffer not useful, release it \n");
#endif
        return AVERROR(EAGAIN);
    }
    else
    {
        // real output
        frame = av_frame_clone(m_buffer_frames[0].frame);
        if (frame == nullptr)
        {
            return AVERROR(ENOMEM);
        }
        int64_t out_pts = m_next_pts++;
        if (m_keep_timebase)
        {
            frame->pts =
                av_rescale_q_rnd(out_pts, m_tb_out, m_tb_in, (enum AVRounding)m_roundpolicy);
        }
        else
        {
            frame->pts = out_pts;
        }
#if defined(PRINT_DEBUG_INFO)
        fprintf(stdout, "[FPS_FILTER]Info: rewriting frame pts from %ld to %ld \n",
                m_buffer_frames[0].output_pts_codec, frame->pts);
#endif
        m_cur_frame_out++;
    }

    return 0;
}

long FpsFilter::update_input_fps(AVRational fps_in) { return 0; }

long FpsFilter::set_maxium_gap(int64_t timestamp_gap_in_ms) { return 0; }

long FpsFilter::update_filter_buffer()
{
    if (m_buffer_frames.size() < 1)
    {
        fprintf(stderr, "[FPS_FILTER]Error: filter buffer is empty, no need to update filter \n");
        return -1;
    }
    FpsFilterItem outdated_item = m_buffer_frames.front();
    m_buffer_frames.pop_front();
    /* Update statistics counters */
    m_frames_out += m_cur_frame_out;
    if (m_cur_frame_out > 1)
    {
#if defined(PRINT_DEBUG_INFO)
        fprintf(stdout, "[FPS_FILTER]Info: duplicated frame with pts %ld for %d times \n",
                outdated_item.frame->pts, m_cur_frame_out - 1);
#endif
        m_frames_dup += m_cur_frame_out - 1;
    }
    else if (m_cur_frame_out == 0)
    {
#if defined(PRINT_DEBUG_INFO)
        fprintf(stdout, "[FPS_FILTER]Info: dropping frame with pts %ld \n",
                outdated_item.frame->pts);
#endif
        m_frames_drop++;
    }
    av_frame_free(&outdated_item.frame);
    m_cur_frame_out = 0;
    return 0;
}
