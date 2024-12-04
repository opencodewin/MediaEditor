
#include "ffmedia.h"
#include "ffmedia_utils.h"
#include "ffmedia_queue.h"
#include "ffmedia_common.h"

FFMediaStream::FFMediaStream(int idx, FFMEDIA_STREAMTYPE _type)
{
    index = idx;
    type = _type;

    width = 0;
    height = 0;
    frame_rate = {1, 1};
    pixel_format = AV_PIX_FMT_NONE;
    hdr_type = FFMEDIA_HDRTYPE::NONE;
    bit_depth = 8;
    hw_output = false;
    hw_info = nullptr;

    sample_rate = 0;
    channels = 0;
    sample_fmt = AV_SAMPLE_FMT_NONE;
    
    tbc = {1, 1};
    tbn = {1, 1};
    codec_ctx = nullptr;
    codec_id = AV_CODEC_ID_NONE;
    bit_rate = 0;

    activated = false;
    eof = false;
    
    streaming_queue = nullptr;
    read_cnt = 0;
    drop_cnt = 0;
    start_pts = -1;
    next_pts = -1;
    pts_offset = 0;

    extra_options = nullptr;
    write_cnt = 0;

    audio_frame = nullptr;
    audio_fifo = nullptr;
    audio_framesize = 0;
    audio_sample_cnt = 0;
    fifo_start_pts = -1;
}

FFMediaStream::~FFMediaStream()
{
    if(streaming_queue)
    {
        FFMedia_Queue *queue = (FFMedia_Queue*)streaming_queue;
        queue->stop();
        queue->flush();
        queue->quit();
        delete queue;
        streaming_queue = nullptr;
    }
    if (codec_ctx)
    {
        avcodec_free_context(&codec_ctx);
        codec_ctx = nullptr;
    }
    if(audio_frame)
    {
        av_frame_free(&audio_frame);
        audio_frame = nullptr;
    }
    if (audio_fifo)
    {
        av_audio_fifo_free(audio_fifo);
        audio_fifo = nullptr;
    }
}
