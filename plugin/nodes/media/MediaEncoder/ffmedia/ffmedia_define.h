#ifndef __FFMEDIA_DEFINE_H__
#define __FFMEDIA_DEFINE_H__

#ifdef __cplusplus
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#endif

// ffmpeg includes
#define __STDC_CONSTANT_MACROS
extern "C"
{
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavutil/avstring.h"
#include "libavutil/pixdesc.h"
#include "libavutil/mathematics.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
};

enum FFMEDIA_RETVALUE : int
{
    FFMEDIA_SUCCESS = 0,
    FFMEDIA_FAILED = 1,
};

enum class FFMEDIA_HDRTYPE
{
    NONE,
    SDR,
    HDR_PQ,
    HDR_HLG,
};

enum class FFMEDIA_STREAMTYPE
{
    VIDEO,
    AUDIO,
    SUBTITLE,
    RESERVED,
};

enum class FFMEDIA_STREAMINGFLAG
{
    NONE,
    PACKET,
    FRAME,
};

#endif  //__FFMEDIA_DEFINE_H__
