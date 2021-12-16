#include <iostream>
#include <sstream>
#include <iomanip>
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/pixdesc.h"
    #include "libavutil/hwcontext.h"
}

using namespace std;

string MillisecToString(int64_t millisec)
{
    ostringstream oss;
    if (millisec < 0)
    {
        oss << "-";
        millisec = -millisec;
    }
    uint64_t t = (uint64_t) millisec;
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    oss << setfill('0') << setw(2) << hour << ':'
        << setw(2) << min << ':'
        << setw(2) << sec << '.'
        << setw(3) << milli;
    return oss.str();
}

string TimestampToString(double timestamp)
{
    return MillisecToString((int64_t)(timestamp*1000));
}

bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
    AVFrame* swfrm = nullptr;
    if ((desc->flags&AV_PIX_FMT_FLAG_HWACCEL) > 0)
    {
        swfrm = av_frame_alloc();
        if (!swfrm)
        {
            cerr << "FAILED to allocate new AVFrame for ImMat conversion!" << endl;
            return false;
        }
        int fferr = av_hwframe_transfer_data(swfrm, avfrm, 0);
        if (fferr < 0)
        {
            cerr << "av_hwframe_transfer_data() FAILED! fferr = " << fferr << "." << endl;
            av_frame_free(&swfrm);
            return false;
        }
        desc = av_pix_fmt_desc_get((AVPixelFormat)swfrm->format);
        avfrm = swfrm;
    }

    if (desc->nb_components <= 0 || desc->nb_components > 4)
    {
        cerr << "INVALID 'nb_component' value " << desc->nb_components << " of pixel format '"
            << desc->name << "', can only support value from 1 ~ 4." << endl;
        return false;
    }

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

#define ISYUV444P(format) \
    (format == AV_PIX_FMT_YUV444P || \
    format == AV_PIX_FMT_YUVJ420P || \
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

    int bitDepth = desc->comp[0].depth;
    ImColorSpace color_space =  avfrm->colorspace == AVCOL_SPC_BT470BG ||
                                avfrm->colorspace == AVCOL_SPC_SMPTE170M ||
                                avfrm->colorspace == AVCOL_SPC_BT470BG ? IM_CS_BT601 :
                                avfrm->colorspace == AVCOL_SPC_BT709 ? IM_CS_BT709 :
                                avfrm->colorspace == AVCOL_SPC_BT2020_NCL ||
                                avfrm->colorspace == AVCOL_SPC_BT2020_CL ? IM_CS_BT2020 : IM_CS_BT709;
    ImColorRange color_range =  avfrm->color_range == AVCOL_RANGE_MPEG ? IM_CR_NARROW_RANGE :
                                avfrm->color_range == AVCOL_RANGE_JPEG ? IM_CR_FULL_RANGE : IM_CR_NARROW_RANGE;
    ImColorFormat color_format = ISYUV420P(avfrm->format) ? IM_CF_YUV420 :
                                ISYUV422P(avfrm->format) ? IM_CF_YUV422 :
                                ISYUV444P(avfrm->format) ? IM_CF_YUV444 :
                                ISNV12(avfrm->format) ? bitDepth == 10 ? IM_CF_P010LE : IM_CF_NV12 : IM_CF_YUV420;
    const int width = avfrm->width;
    const int height = avfrm->height;

    ImGui::ImMat mat_V;
    int channel = 2;
    ImDataType dataType = bitDepth > 8 ? IM_DT_INT16 : IM_DT_INT8;
    if (color_format == IM_CF_YUV444)
        mat_V.create_type(width, height, 3, dataType);
    else
        mat_V.create_type(width, height, 2, dataType);
    uint8_t* prevDataPtr = nullptr;
    for (int i = 0; i < desc->nb_components; i++)
    {
        int chWidth = width;
        int chHeight = height;
        if ((desc->flags&AV_PIX_FMT_FLAG_RGB) == 0 && i > 0)
        {
            chWidth >>= desc->log2_chroma_w;
            chHeight >>= desc->log2_chroma_h;
        }
        if (desc->nb_components > i && desc->comp[i].plane == i)
        {
            uint8_t* src_data = avfrm->data[i]+desc->comp[i].offset;
            uint8_t* dst_data;
            if (i < channel)
                dst_data = (uint8_t*)mat_V.channel(i).data;
            else
                dst_data = prevDataPtr;
            int bytesPerLine = chWidth*desc->comp[i].step;
            for (int j = 0; j < chHeight; j++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += avfrm->linesize[i];
                dst_data += bytesPerLine;
            }
            prevDataPtr = dst_data;
        }
    }

    mat_V.color_space = color_space;
    mat_V.color_range = color_range;
    mat_V.color_format = color_format;
    mat_V.depth = bitDepth;
    mat_V.flags = IM_MAT_FLAGS_VIDEO_FRAME;
    if (avfrm->pict_type == AV_PICTURE_TYPE_I) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_I;
    if (avfrm->pict_type == AV_PICTURE_TYPE_P) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_P;
    if (avfrm->pict_type == AV_PICTURE_TYPE_B) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_B;
    if (avfrm->interlaced_frame) mat_V.flags |= IM_MAT_FLAGS_VIDEO_INTERLACED;
    mat_V.time_stamp = timestamp;

    vmat = mat_V;
    if (swfrm)
        av_frame_free(&swfrm);
    return true;
}
