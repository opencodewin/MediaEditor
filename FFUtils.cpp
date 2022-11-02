#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <functional>
#include <algorithm>
#include "Logger.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/pixdesc.h"
    #include "libavutil/hwcontext.h"
    #include "libavutil/avutil.h"
    #include "libavutil/opt.h"
    #include "libavutil/channel_layout.h"
    #include "libavcodec/codec_desc.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
}

using namespace std;
using namespace Logger;

const AVRational MILLISEC_TIMEBASE = { 1, 1000 };
const AVRational MICROSEC_TIMEBASE = { 1, 1000000 };
const AVRational FF_AV_TIMEBASE = { 1, AV_TIME_BASE };

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

AVPixelFormat GetAVPixelFormatByName(const std::string& name)
{
    string fmtLowerCase(name);
    transform(fmtLowerCase.begin(), fmtLowerCase.end(), fmtLowerCase.begin(), [] (char c) {
        if (c <= 'Z' && c >= 'A')
            return (char)(c-('Z'-'z'));
        return c;
    });
    return av_get_pix_fmt(fmtLowerCase.c_str());
}

ImColorFormat ConvertPixelFormatToColorFormat(AVPixelFormat pixfmt)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixfmt);
    ImColorFormat clrfmt = (ImColorFormat)-1;
    if (pixfmt == AV_PIX_FMT_GRAY8 || pixfmt == AV_PIX_FMT_GRAY10 || pixfmt == AV_PIX_FMT_GRAY12 ||
        pixfmt == AV_PIX_FMT_GRAY14 || pixfmt == AV_PIX_FMT_GRAY16 || pixfmt == AV_PIX_FMT_GRAYF32)
        clrfmt = IM_CF_GRAY;
    else if (pixfmt == AV_PIX_FMT_BGR24 || pixfmt == AV_PIX_FMT_BGR48)
        clrfmt = IM_CF_RGB;
    else if (pixfmt == AV_PIX_FMT_ABGR || pixfmt == AV_PIX_FMT_0BGR)
        clrfmt = IM_CF_RGBA;
    else if (pixfmt == AV_PIX_FMT_BGRA || pixfmt == AV_PIX_FMT_BGR0 || pixfmt == AV_PIX_FMT_BGRA64)
        clrfmt = IM_CF_ARGB;
    else if (pixfmt == AV_PIX_FMT_RGB24 || pixfmt == AV_PIX_FMT_RGB48)
        clrfmt = IM_CF_BGR;
    else if (pixfmt == AV_PIX_FMT_ARGB || pixfmt == AV_PIX_FMT_0RGB)
        clrfmt = IM_CF_BGRA;
    else if (pixfmt == AV_PIX_FMT_RGBA || pixfmt == AV_PIX_FMT_RGB0 || pixfmt == AV_PIX_FMT_RGBA64)
        clrfmt = IM_CF_ABGR;
    else if (pixfmt == AV_PIX_FMT_YUV420P || pixfmt == AV_PIX_FMT_YUV420P10 || pixfmt == AV_PIX_FMT_YUV420P12 ||
        pixfmt == AV_PIX_FMT_YUV420P14 || pixfmt == AV_PIX_FMT_YUV420P16)
        clrfmt = IM_CF_YUV420;
    else if (pixfmt == AV_PIX_FMT_YUV422P || pixfmt == AV_PIX_FMT_YUV422P10 || pixfmt == AV_PIX_FMT_YUV422P12 ||
        pixfmt == AV_PIX_FMT_YUV422P14 || pixfmt == AV_PIX_FMT_YUV422P16)
        clrfmt = IM_CF_YUV422;
    else if (pixfmt == AV_PIX_FMT_YUV444P || pixfmt == AV_PIX_FMT_YUV444P10 || pixfmt == AV_PIX_FMT_YUV444P12 ||
        pixfmt == AV_PIX_FMT_YUV444P14 || pixfmt == AV_PIX_FMT_YUV444P16)
        clrfmt = IM_CF_YUV444;
    else if (pixfmt == AV_PIX_FMT_YUVA444P || pixfmt == AV_PIX_FMT_YUVA444P10 || pixfmt == AV_PIX_FMT_YUV444P12 ||
        pixfmt == AV_PIX_FMT_YUVA444P16)
        clrfmt = IM_CF_YUVA;
    else if (pixfmt == AV_PIX_FMT_NV12 || pixfmt == AV_PIX_FMT_NV21)
        clrfmt = IM_CF_NV12;
    else if (pixfmt == AV_PIX_FMT_P010)
        clrfmt = IM_CF_P010LE;
    return clrfmt;
}

static ImColorFormat FindPresentColorFormatForPixelFormat(AVPixelFormat pixfmt)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixfmt);
    ImColorFormat clrfmt;
    if ((desc->flags&AV_PIX_FMT_FLAG_RGB) != 0)
    {
        if (desc->nb_components > 3)
            clrfmt = IM_CF_RGBA;
        else
            clrfmt = IM_CF_RGB;
    }
    else
    {
        if (desc->nb_components > 3)
            clrfmt = IM_CF_YUVA;
        else
            clrfmt = IM_CF_YUV444;
    }
    return clrfmt;
}

static AVPixelFormat ConvertColorFormatToPixelFormat(ImColorFormat clrfmt, ImDataType dtype)
{
    AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    if (clrfmt == IM_CF_GRAY)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_GRAY8;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_GRAY16;
        else if (dtype == IM_DT_FLOAT32)
            pixfmt = AV_PIX_FMT_GRAYF32;
    }
    else if (clrfmt == IM_CF_BGR)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_RGB24;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_RGB48;
    }
    else if (clrfmt == IM_CF_ABGR)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_RGBA;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_RGBA64;
    }
    else if (clrfmt == IM_CF_BGRA)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_ARGB;
    }
    else if (clrfmt == IM_CF_RGB)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_BGR24;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_BGR48;
    }
    else if (clrfmt == IM_CF_ARGB)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_BGRA;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_BGRA64;
    }
    else if (clrfmt == IM_CF_RGBA)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_ABGR;
    }
    else if (clrfmt == IM_CF_YUV420)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUV420P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUV420P16;
    }
    else if (clrfmt == IM_CF_YUV422)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUV422P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUV422P16;
    }
    else if (clrfmt == IM_CF_YUV444)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUV444P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUV444P16;
    }
    else if (clrfmt == IM_CF_YUVA)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUVA444P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUVA444P16;
    }
    else if (clrfmt == IM_CF_NV12)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_NV12;
    }
    else if (clrfmt == IM_CF_P010LE)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_P010LE;
    }
    return pixfmt;
}

static ImColorSpace ConvertAVColorSpaceToImColorSpace(AVColorSpace clrspc)
{
    ImColorSpace imclrspc;
    if (clrspc == AVCOL_SPC_RGB)
        imclrspc = IM_CS_SRGB;
    else if (clrspc == AVCOL_SPC_UNSPECIFIED)
        imclrspc = IM_CS_BT601;
    else if (clrspc == AVCOL_SPC_BT709)
        imclrspc = IM_CS_BT709;
    else if (clrspc == AVCOL_SPC_BT2020_CL || clrspc == AVCOL_SPC_BT2020_NCL)
        imclrspc = IM_CS_BT2020;
    else
        imclrspc = IM_CS_SRGB;

    return imclrspc;
}

static AVColorSpace ConvertImColorSpaceToAVColorSpace(ImColorSpace imclrspc)
{
    AVColorSpace clrspc = AVCOL_SPC_UNSPECIFIED;
    if (imclrspc == IM_CS_SRGB)
        clrspc = AVCOL_SPC_RGB;
    else if (imclrspc == IM_CS_BT709)
        clrspc = AVCOL_SPC_BT709;
    else if (imclrspc == IM_CS_BT2020)
        clrspc = AVCOL_SPC_BT2020_CL;
    return clrspc;
}

static ImColorRange ConvertAVColorRangeToImColorRange(AVColorRange clrrng)
{
    ImColorRange imclrrng;
    if (clrrng == AVCOL_RANGE_JPEG)
        imclrrng = IM_CR_FULL_RANGE;
    else
        imclrrng = IM_CR_NARROW_RANGE;
    return imclrrng;
}

static AVColorRange ConvertImColorRangeToAVColorRange(ImColorRange imclrrng)
{
    AVColorRange clrrng = AVCOL_RANGE_UNSPECIFIED;
    if (imclrrng == IM_CR_FULL_RANGE)
        clrrng = AVCOL_RANGE_JPEG;
    else if (imclrrng == IM_CR_NARROW_RANGE)
        clrrng = AVCOL_RANGE_MPEG;
    return clrrng;
}

SelfFreeAVFramePtr AllocSelfFreeAVFramePtr()
{
    SelfFreeAVFramePtr frm = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* avfrm) {
        if (avfrm)
            av_frame_free(&avfrm);
    });
    if (!frm.get())
        return nullptr;
    return frm;
}

SelfFreeAVFramePtr CloneSelfFreeAVFramePtr(const AVFrame* avfrm)
{
    SelfFreeAVFramePtr frm = shared_ptr<AVFrame>(av_frame_clone(avfrm), [](AVFrame* avfrm) {
        if (avfrm)
            av_frame_free(&avfrm);
    });
    if (!frm.get())
        return nullptr;
    return frm;
}

SelfFreeAVFramePtr WrapSelfFreeAVFramePtr(AVFrame* avfrm)
{
    SelfFreeAVFramePtr frm = shared_ptr<AVFrame>(avfrm, [](AVFrame* avfrm) {
        if (avfrm)
            av_frame_free(&avfrm);
    });
    if (!frm.get())
        return nullptr;
    return frm;
}

bool IsHwFrame(const AVFrame* avfrm)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
    return (desc->flags&AV_PIX_FMT_FLAG_HWACCEL) > 0;
}

bool HwFrameToSwFrame(AVFrame* swfrm, const AVFrame* hwfrm)
{
    av_frame_unref(swfrm);
    int fferr = av_hwframe_transfer_data(swfrm, hwfrm, 0);
    if (fferr < 0)
    {
        Log(Error) << "av_hwframe_transfer_data() FAILED! fferr = " << fferr << "." << endl;
        return false;
    }
    av_frame_copy_props(swfrm, hwfrm);
    return true;
}

bool MakeAVFrameCopy(AVFrame* dst, const AVFrame* src)
{
    av_frame_unref(dst);
    dst->format = src->format;
    dst->width = src->width;
    dst->height = src->height;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    dst->channels = src->channels;
    dst->channel_layout = src->channel_layout;
#else
    dst->ch_layout = src->ch_layout;
#endif
    dst->sample_rate = dst->sample_rate;
    int fferr;
    if ((fferr = av_frame_get_buffer(dst, 0)) < 0)
    {
        Log(Error) << "av_frame_get_buffer() FAILED! fferr = " << fferr << "." << endl;
        return false;
    }
    if ((fferr = av_frame_copy(dst, src)) < 0)
    {
        Log(Error) << "av_frame_copy() FAILED! fferr = " << fferr << "." << endl;
        return false;
    }
    av_frame_copy_props(dst, src);
    return true;
}

bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp)
{
    SelfFreeAVFramePtr swfrm;
    if (IsHwFrame(avfrm))
    {
        swfrm = AllocSelfFreeAVFramePtr();
        if (!swfrm)
        {
            Log(Error) << "FAILED to allocate new AVFrame for ImMat conversion!" << endl;
            return false;
        }
        if (!HwFrameToSwFrame(swfrm.get(), avfrm))
            return false;
        avfrm = swfrm.get();
    }

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
    if (desc->nb_components <= 0 || desc->nb_components > 4)
    {
        Log(Error) << "INVALID 'nb_component' value " << desc->nb_components << " of pixel format '"
            << desc->name << "', can only support value from 1 ~ 4." << endl;
        return false;
    }
    bool isRgb = (desc->flags&AV_PIX_FMT_FLAG_RGB) > 0;

    int bitDepth = desc->comp[0].depth;
    ImColorSpace color_space =  avfrm->colorspace == AVCOL_SPC_BT470BG ||
                                avfrm->colorspace == AVCOL_SPC_SMPTE170M ||
                                avfrm->colorspace == AVCOL_SPC_BT470BG ? IM_CS_BT601 :
                                avfrm->colorspace == AVCOL_SPC_BT709 ? IM_CS_BT709 :
                                avfrm->colorspace == AVCOL_SPC_BT2020_NCL ||
                                avfrm->colorspace == AVCOL_SPC_BT2020_CL ? IM_CS_BT2020 : IM_CS_BT709;
    ImColorRange color_range =  avfrm->color_range == AVCOL_RANGE_MPEG ? IM_CR_NARROW_RANGE :
                                avfrm->color_range == AVCOL_RANGE_JPEG ? IM_CR_FULL_RANGE : IM_CR_NARROW_RANGE;
    ImColorFormat clrfmt = ConvertPixelFormatToColorFormat((AVPixelFormat)avfrm->format);
    if ((int)clrfmt < 0)
        return false;
    ImColorFormat color_format = clrfmt;
    const int width = avfrm->width;
    const int height = avfrm->height;

    ImGui::ImMat mat_V;
    int channel;
    ImDataType dataType = bitDepth > 8 ? IM_DT_INT16 : IM_DT_INT8;
    if (isRgb)
        channel = desc->nb_components;
    else
    {
        if (color_format == IM_CF_YUV444)
            channel = 3;
        else
            channel = 2;
    }
    mat_V.create_type(width, height, channel, dataType);
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
    return true;
}

bool ConvertImMatToAVFrame(const ImGui::ImMat& vmat, AVFrame* avfrm, int64_t pts)
{
    if (vmat.device != IM_DD_CPU)
    {
        Log(Error) << "Input ImMat is NOT a CPU mat!" << endl;
        return false;
    }
    AVPixelFormat cvtPixfmt = ConvertColorFormatToPixelFormat(vmat.color_format, vmat.type);
    if (cvtPixfmt < 0)
    {
        Log(Error) << "FAILED to convert ImColorFormat " << vmat.color_format << " and ImDataType " << vmat.type << " to AVPixelFormat!" << endl;
        return false;
    }
    av_frame_unref(avfrm);
    avfrm->width = vmat.w;
    avfrm->height = vmat.h;
    avfrm->format = (int)cvtPixfmt;
    int fferr;
    fferr = av_frame_get_buffer(avfrm, 0);
    if (fferr < 0)
    {
        Log(Error) << "FF api 'av_frame_get_buffer' failed! return code is " << fferr << ".";
        return false;
    }

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(cvtPixfmt);
    bool isRgb = (desc->flags&AV_PIX_FMT_FLAG_RGB) > 0;
    uint8_t* prevDataPtr = nullptr;
    int channel;
    if (isRgb)
        channel = desc->nb_components;
    else
    {
        if (vmat.color_format == IM_CF_YUV444)
            channel = 3;
        else
            channel = 2;
    }
    for (int i = 0; i < desc->nb_components; i++)
    {
        int chWidth = vmat.w;
        int chHeight = vmat.h;
        if ((desc->flags&AV_PIX_FMT_FLAG_RGB) == 0 && i > 0)
        {
            chWidth >>= desc->log2_chroma_w;
            chHeight >>= desc->log2_chroma_h;
        }
        if (desc->nb_components > i && desc->comp[i].plane == i)
        {
            uint8_t* dst_data = avfrm->data[i]+desc->comp[i].offset;
            uint8_t* src_data;
            if (i < channel)
                src_data = (uint8_t*)vmat.channel(i).data;
            else
                src_data = prevDataPtr;
            int bytesPerLine = chWidth*desc->comp[i].step;
            for (int j = 0; j < chHeight; j++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += bytesPerLine;
                dst_data += avfrm->linesize[i];
            }
            prevDataPtr = src_data;
        }
    }

    avfrm->colorspace = ConvertImColorSpaceToAVColorSpace(vmat.color_space);
    avfrm->color_range = ConvertImColorRangeToAVColorRange(vmat.color_range);
    avfrm->pts = pts;
    return true;
}

AVFrameToImMatConverter::AVFrameToImMatConverter()
{
#if IMGUI_VULKAN_SHADER
    m_useVulkanComponents = true;
    if (m_useVulkanComponents)
    {
        m_imgClrCvt = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
        m_imgRsz = new ImGui::Resize_vulkan(ImGui::get_default_gpu_index());
    }
#else
    m_useVulkanComponents = false;
#endif
}

AVFrameToImMatConverter::~AVFrameToImMatConverter()
{
#if IMGUI_VULKAN_SHADER
    if (m_imgClrCvt)
    {
        delete m_imgClrCvt;
        m_imgClrCvt = nullptr;
    }
    if (m_imgRsz)
    {
        delete m_imgRsz;
        m_imgRsz = nullptr;
    }
#endif
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}

bool AVFrameToImMatConverter::SetOutSize(uint32_t width, uint32_t height)
{
    if (m_outWidth == width && m_outHeight == height)
        return true;

    m_outWidth = width;
    m_outHeight = height;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool AVFrameToImMatConverter::SetOutColorFormat(ImColorFormat clrfmt)
{
    if (clrfmt != IM_CF_RGBA)
    {
        m_errMsg = string("Do NOT SUPPORT output color format ")+to_string((int)clrfmt)+"!";
        return false;
    }
    if (m_outClrFmt == clrfmt)
        return true;

    m_outClrFmt = clrfmt;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool AVFrameToImMatConverter::SetResizeInterpolateMode(ImInterpolateMode interp)
{
    if (m_resizeInterp == interp)
        return true;

    switch (interp)
    {
        case IM_INTERPOLATE_NEAREST:
            m_swsFlags = 0;
            break;
        case IM_INTERPOLATE_BILINEAR:
            m_swsFlags = SWS_BILINEAR;
            break;
        case IM_INTERPOLATE_BICUBIC:
            m_swsFlags = SWS_BICUBIC;
            break;
        case IM_INTERPOLATE_AREA:
            m_swsFlags = SWS_AREA;
            break;
        default:
            return false;
    }
    m_resizeInterp = interp;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool AVFrameToImMatConverter::ConvertImage(const AVFrame* avfrm, ImGui::ImMat& outMat, double timestamp)
{
    if (m_useVulkanComponents)
    {
#if IMGUI_VULKAN_SHADER
        // AVFrame -> ImMat
        ImGui::ImMat inMat;
        if (!ConvertAVFrameToImMat(avfrm, inMat, timestamp))
        {
            m_errMsg = "Failed to invoke 'ConvertAVFrameToImMat()'!";
            return false;
        }
        if (inMat.color_format == IM_CF_ABGR || inMat.color_format == IM_CF_ARGB ||
            inMat.color_format == IM_CF_RGBA || inMat.color_format == IM_CF_BGRA)
        {
            outMat = inMat;
            outMat.time_stamp = timestamp;
            return true;
        }

        // YUV -> RGB
        ImGui::VkMat rgbMat;
        rgbMat.type = IM_DT_INT8;
        rgbMat.color_format = IM_CF_ABGR;
        rgbMat.w = m_outWidth;
        rgbMat.h = m_outHeight;
        if (!m_imgClrCvt->ConvertColorFormat(inMat, rgbMat, m_resizeInterp))
        {
            m_errMsg = m_imgClrCvt->GetError();
            return false;
        }
        if (m_outputCpuMat && rgbMat.device == IM_DD_VULKAN)
        {
            outMat.type = IM_DT_INT8;
            m_imgClrCvt->Conv(rgbMat, outMat);
        }
        else
            outMat = rgbMat;
        outMat.time_stamp = timestamp;
        return true;
#else
        m_errMsg = "Vulkan shader components is NOT ENABLED!";
        return false;
#endif
    }
    else
    {
        SelfFreeAVFramePtr swfrm;
        if (IsHwFrame(avfrm))
        {
            swfrm = AllocSelfFreeAVFramePtr();
            if (!swfrm)
            {
                Log(Error) << "FAILED to allocate new AVFrame for ImMat conversion!" << endl;
                return false;
            }
            if (!HwFrameToSwFrame(swfrm.get(), avfrm))
                return false;
            avfrm = swfrm.get();
        }

        int outWidth = m_outWidth == 0 ? avfrm->width : m_outWidth;
        int outHeight = m_outHeight == 0 ? avfrm->height : m_outHeight;
        if (!(m_swsCtx || m_passThrough) ||
            m_swsInWidth != avfrm->width || m_swsInHeight != avfrm->height ||
            (int)m_swsInFormat != avfrm->format || m_swsClrspc != avfrm->colorspace)
        {
            if (m_swsCtx)
            {
                sws_freeContext(m_swsCtx);
                m_swsCtx = nullptr;
            }
            if (avfrm->width != outWidth || avfrm->height != outHeight || avfrm->format != (int)m_swsOutFormat)
            {
                m_swsCtx = sws_getContext(avfrm->width, avfrm->height, (AVPixelFormat)avfrm->format, outWidth, outHeight, m_swsOutFormat, m_swsFlags, nullptr, nullptr, nullptr);
                if (!m_swsCtx)
                {
                    ostringstream oss;
                    oss << "FAILED to create SwsContext from WxH(" << avfrm->width << "x" << avfrm->height << "):Fmt(" << avfrm->format << ") -> WxH(" << outWidth << "x" << outHeight << "):Fmt(" << (int)m_swsOutFormat << ") with flags(" << m_swsFlags << ")!";
                    m_errMsg = oss.str();
                    return false;
                }
                int srcRange, dstRange, brightness, contrast, saturation;
                int *invTable0, *table0;
                sws_getColorspaceDetails(m_swsCtx, &invTable0, &srcRange, &table0, &dstRange, &brightness, &contrast, &saturation);
                const int *invTable1, *table1;
                table1 = invTable1 = sws_getCoefficients(avfrm->colorspace);
                sws_setColorspaceDetails(m_swsCtx, invTable1, srcRange, table1, dstRange, brightness, contrast, saturation);
                m_swsInWidth = avfrm->width;
                m_swsInHeight = avfrm->height;
                m_swsInFormat = (AVPixelFormat)avfrm->format;
                m_swsClrspc = avfrm->colorspace;
            }
            else
            {
                m_swsClrspc = avfrm->colorspace;
                m_passThrough = true;
            }
        }

        SelfFreeAVFramePtr swsfrm;
        if (!m_passThrough && m_swsCtx)
        {
            swsfrm = AllocSelfFreeAVFramePtr();
            if (!swsfrm)
            {
                m_errMsg = "FAILED to allocate AVFrame to perform 'swscale'!";
                return false;
            }

            AVFrame* pfrm = swsfrm.get();
            pfrm->width = outWidth;
            pfrm->height = outHeight;
            pfrm->format = (int)m_swsOutFormat;
            int fferr = av_frame_get_buffer(pfrm, 0);
            if (fferr < 0)
            {
                m_errMsg = string("FAILED to invoke 'av_frame_get_buffer()' for 'swsfrm'! fferr = ")+to_string(fferr)+".";
                return false;
            }
            fferr = sws_scale(m_swsCtx, avfrm->data, avfrm->linesize, 0, avfrm->height, swsfrm->data, swsfrm->linesize);
            av_frame_copy_props(swsfrm.get(), avfrm);
            avfrm = swsfrm.get();
        }

        // AVFrame -> ImMat
        if (!ConvertAVFrameToImMat(avfrm, outMat, timestamp))
        {
            m_errMsg = "Failed to invoke 'ConvertAVFrameToImMat()'!";
            return false;
        }

        outMat.time_stamp = timestamp;
        return true;
    }
}

ImMatToAVFrameConverter::ImMatToAVFrameConverter()
{
#if IMGUI_VULKAN_SHADER
    m_useVulkanComponents = true;
    m_imgClrCvt = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
    if (m_useVulkanComponents)
    {
        m_imgRsz = new ImGui::Resize_vulkan(ImGui::get_default_gpu_index());
    }
    m_outMatClrfmt = ConvertPixelFormatToColorFormat(m_outPixfmt);
    if (m_outMatClrfmt < 0)
    {
        m_outMatClrfmt = FindPresentColorFormatForPixelFormat(m_outPixfmt);
        m_isClrfmtMatched = false;
    }
    else
        m_isClrfmtMatched = true;
    m_outMatClrspc = ConvertAVColorSpaceToImColorSpace(m_outClrspc);
    m_outMatClrrng = ConvertAVColorRangeToImColorRange(m_outClrrng);
#else
    m_useVulkanComponents = false;
#endif
    m_pixDesc = av_pix_fmt_desc_get(m_outPixfmt);
    m_outBitsPerPix = m_pixDesc->comp[0].depth;
}

ImMatToAVFrameConverter::~ImMatToAVFrameConverter()
{
#if IMGUI_VULKAN_SHADER
    if (m_imgClrCvt)
    {
        delete m_imgClrCvt;
        m_imgClrCvt = nullptr;
    }
    if (m_imgRsz)
    {
        delete m_imgRsz;
        m_imgRsz = nullptr;
    }
#endif
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}

bool ImMatToAVFrameConverter::SetOutSize(uint32_t width, uint32_t height)
{
    if (m_outWidth == width && m_outHeight == height)
        return true;

    m_outWidth = width;
    m_outHeight = height;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetOutPixelFormat(AVPixelFormat pixfmt)
{
    if (m_outPixfmt == pixfmt)
        return true;

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixfmt);
    if (!desc)
    {
        ostringstream oss;
        oss << "FAILED to get 'AVPixFmtDescriptor' for AVPixelFormat " << pixfmt << "!";
        m_errMsg = oss.str();
        return false;
    }

    m_outPixfmt = pixfmt;
    m_pixDesc = desc;
    m_outBitsPerPix = desc->comp[0].depth;
#if IMGUI_VULKAN_SHADER
    m_outMatClrfmt = ConvertPixelFormatToColorFormat(m_outPixfmt);
    if (m_outMatClrfmt < 0)
    {
        m_outMatClrfmt = FindPresentColorFormatForPixelFormat(m_outPixfmt);
        m_isClrfmtMatched = false;
    }
    else
        m_isClrfmtMatched = true;
#endif

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetOutColorSpace(AVColorSpace clrspc)
{
    if (m_outClrspc == clrspc)
        return true;

    m_outClrspc = clrspc;
#if IMGUI_VULKAN_SHADER
    m_outMatClrspc = ConvertAVColorSpaceToImColorSpace(clrspc);
#endif

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetOutColorRange(AVColorRange clrrng)
{
    if (m_outClrrng == clrrng)
        return true;

    m_outClrrng = clrrng;
#if IMGUI_VULKAN_SHADER
    m_outMatClrrng = ConvertAVColorRangeToImColorRange(clrrng);
#endif

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetResizeInterpolateMode(ImInterpolateMode interp)
{
    if (m_resizeInterp == interp)
        return true;

    switch (interp)
    {
        case IM_INTERPOLATE_NEAREST:
            m_swsFlags = 0;
            break;
        case IM_INTERPOLATE_BILINEAR:
            m_swsFlags = SWS_BILINEAR;
            break;
        case IM_INTERPOLATE_BICUBIC:
            m_swsFlags = SWS_BICUBIC;
            break;
        case IM_INTERPOLATE_AREA:
            m_swsFlags = SWS_AREA;
            break;
        default:
            return false;
    }
    m_resizeInterp = interp;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::ConvertImage(const ImGui::ImMat& vmat, AVFrame* avfrm, int64_t pts)
{
    ImGui::ImMat inMat = vmat;
    if (m_useVulkanComponents)
    {
#if IMGUI_VULKAN_SHADER
        // Resize
        uint32_t outWidth = m_outWidth == 0 ? inMat.w : m_outWidth;
        uint32_t outHeight = m_outHeight == 0 ? inMat.h : m_outHeight;
        if (outWidth != inMat.w || outHeight != inMat.h)
        {
            ImGui::VkMat rszMat;
            rszMat.type = m_outBitsPerPix > 8 ? IM_DT_INT16 : IM_DT_INT8;
            m_imgRsz->Resize(inMat, rszMat, (float)outWidth/inMat.w, (float)outHeight/inMat.h, m_resizeInterp);
            inMat = rszMat;
        }

        // RGB -> YUV
        bool isSrcRgb = inMat.color_format >= IM_CF_BGR && inMat.color_format <= IM_CF_RGBA;
        bool isDstYuv = (m_pixDesc->flags&AV_PIX_FMT_FLAG_RGB) == 0;
        if (isSrcRgb && isDstYuv)
        {
            ImGui::ImMat yuvMat;
            yuvMat.type = inMat.type;
            yuvMat.color_format = m_outMatClrfmt;
            yuvMat.color_space = m_outMatClrspc;
            yuvMat.color_range = m_outMatClrrng;
            if (!m_imgClrCvt->ConvertColorFormat(inMat, yuvMat))
            {
                m_errMsg = m_imgClrCvt->GetError();
                return false;
            }

            inMat = yuvMat;
        }
#else
        m_errMsg = "Vulkan shader components is NOT ENABLED!";
        return false;
#endif
    }

    // ImMat -> AVFrame
    AVPixelFormat cvtPixfmt = ConvertColorFormatToPixelFormat(inMat.color_format, inMat.type);
    if (cvtPixfmt < 0)
    {
        ostringstream oss;
        oss << "FAILED to convert ImColorFormat " << inMat.color_format << " and ImDataType " << inMat.type << " to AVPixelFormat!";
        m_errMsg = oss.str();
        return false;
    }

#if IMGUI_VULKAN_SHADER
    if (inMat.device == IM_DD_VULKAN)
    {
        ImGui::VkMat vkMat = inMat;
        ImGui::ImMat cpuMat;
        cpuMat.type = IM_DT_INT8;
        m_imgClrCvt->Conv(vkMat, cpuMat);
        inMat = cpuMat;
    }
#endif

    int outWidth = m_outWidth>0 ? m_outWidth : inMat.w;
    int outHeight = m_outHeight>0 ? m_outHeight : inMat.h;
    if (m_outPixfmt == cvtPixfmt && outWidth == inMat.w && outHeight == inMat.h)
    {
        if (!ConvertImMatToAVFrame(inMat, avfrm, pts))
        {
            m_errMsg = "FAILED to invoke 'ConvertImMatToAVFrame'!";
            return false;
        }
        return true;
    }

    SelfFreeAVFramePtr cvtfrm = AllocSelfFreeAVFramePtr();
    if (!cvtfrm)
    {
        m_errMsg = "FAILED allocate AVFrame for conversion from ImMat!";
        return false;
    }
    if (!ConvertImMatToAVFrame(inMat, cvtfrm.get(), pts))
    {
        m_errMsg = "FAILED to invoke 'ConvertImMatToAVFrame'!";
        return false;
    }

    if (!m_swsCtx || m_swsInWidth != inMat.w || m_swsInHeight != inMat.h || m_swsInFormat != cvtPixfmt)
    {
        if (m_swsCtx)
        {
            sws_freeContext(m_swsCtx);
            m_swsCtx = nullptr;
        }
        m_swsCtx = sws_getContext(inMat.w, inMat.h, cvtPixfmt, outWidth, outHeight, m_outPixfmt, m_swsFlags, nullptr, nullptr, nullptr);
        if (!m_swsCtx)
        {
            ostringstream oss;
            oss << "FAILED to create SwsContext from WxH(" << inMat.w << "x" << inMat.h << "):Fmt(" << (int)cvtPixfmt
                << ") -> WxH(" << outWidth << "x" << outHeight << "):Fmt(" << (int)m_outPixfmt << ") with flags(" << m_swsFlags << ")!";
            m_errMsg = oss.str();
            return false;
        }
        int srcRange, dstRange, brightness, contrast, saturation;
        int *invTable0, *table0;
        sws_getColorspaceDetails(m_swsCtx, &invTable0, &srcRange, &table0, &dstRange, &brightness, &contrast, &saturation);
        const int *invTable1, *table1;
        table1 = invTable1 = sws_getCoefficients(m_outClrspc);
        sws_setColorspaceDetails(m_swsCtx, invTable1, srcRange, table1, dstRange, brightness, contrast, saturation);
        m_swsInWidth = inMat.w;
        m_swsInHeight = inMat.h;
        m_swsInFormat = cvtPixfmt;
    }

    av_frame_unref(avfrm);
    avfrm->format = m_outPixfmt;
    avfrm->width = outWidth;
    avfrm->height = outHeight;
    int fferr;
    fferr = av_frame_get_buffer(avfrm, 0);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "FAILED to invoke 'av_frame_get_buffer()' for 'swsfrm'! fferr = " << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    fferr = sws_scale(m_swsCtx, cvtfrm->data, cvtfrm->linesize, 0, outHeight, avfrm->data, avfrm->linesize);
    av_frame_copy_props(avfrm, cvtfrm.get());

    return true;
}

static void ImMatWrapper_AVFrame_buffer_free(void *opaque, uint8_t *data)
{
    // do nothing
    return;
}

static AVSampleFormat GetAVSampleFormatByDataType(ImDataType dataType, bool isPlanar)
{
    AVSampleFormat smpfmt;
    switch (dataType)
    {
        case IM_DT_INT8:
            smpfmt = isPlanar ? AV_SAMPLE_FMT_U8P : AV_SAMPLE_FMT_U8;
            break;
        case IM_DT_INT16:
            smpfmt = isPlanar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
            break;
        case IM_DT_INT32:
            smpfmt = isPlanar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
            break;
        case IM_DT_INT64:
            smpfmt = isPlanar ? AV_SAMPLE_FMT_S64P : AV_SAMPLE_FMT_S64;
            break;
        case IM_DT_FLOAT32:
            smpfmt = isPlanar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
            break;
        case IM_DT_FLOAT64:
            smpfmt = isPlanar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
            break;
        default:
            smpfmt = AV_SAMPLE_FMT_NONE;
    }
    return smpfmt;
}

SelfFreeAVFramePtr ImMatWrapper_AVFrame::GetWrapper(int64_t pts)
{
    SelfFreeAVFramePtr avfrm = AllocSelfFreeAVFramePtr();
    if (!avfrm)
    {
        Log(Error) << "FAILED to allocate new AVFrame instance by 'av_frame_alloc()'!" << endl;
        return nullptr;
    }
    if (m_isVideo)
    {
        avfrm->width = m_mat.w;
        avfrm->height = m_mat.h;
        AVPixelFormat pixfmt = ConvertColorFormatToPixelFormat(m_mat.color_format, m_mat.type);
        if (pixfmt == AV_PIX_FMT_NONE)
        {
            Log(Error) << "CANNOT find suitable AVPixelFormat for ImColorFormat " << m_mat.color_format << "!" << endl;
            return nullptr;
        }
        avfrm->format = (int)pixfmt;
        AVBufferRef* extBufRef = av_buffer_create((uint8_t*)m_mat.data, m_mat.total()*m_mat.elemsize, ImMatWrapper_AVFrame_buffer_free, this, AV_BUFFER_FLAG_READONLY);
        memset(avfrm->data, 0, sizeof(avfrm->data));
        memset(avfrm->linesize, 0, sizeof(avfrm->linesize));
        memset(avfrm->buf, 0, sizeof(avfrm->buf));
        avfrm->data[0] = (uint8_t*)m_mat.data;
        avfrm->linesize[0] = m_mat.w*m_mat.c*m_mat.elemsize;
        avfrm->buf[0] = extBufRef;
    }
    else
    {
        memset(avfrm->data, 0, sizeof(avfrm->data));
        memset(avfrm->linesize, 0, sizeof(avfrm->linesize));
        memset(avfrm->buf, 0, sizeof(avfrm->buf));
        avfrm->nb_samples = m_mat.w;
        const int channels = m_mat.c;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        avfrm->channels = channels;
        avfrm->channel_layout = av_get_default_channel_layout(channels);
#else
        av_channel_layout_default(&avfrm->ch_layout, channels);
#endif
        bool isPlanar = channels==1 ? false : m_mat.elempack==1;
        AVSampleFormat smpfmt = GetAVSampleFormatByDataType(m_mat.type, isPlanar);
        avfrm->format = (int)smpfmt;
        avfrm->sample_rate = m_mat.rate.num;
        const int bytesPerSample = av_get_bytes_per_sample(smpfmt);
        if (isPlanar)
        {
            int bytesPerPlan = avfrm->nb_samples*bytesPerSample;
            avfrm->linesize[0] = bytesPerPlan;
            uint8_t* bufptr = (uint8_t*)m_mat.data;
            for (int i = 0; i < channels; i++)
            {
                AVBufferRef* extBufRef = av_buffer_create(bufptr, bytesPerPlan, ImMatWrapper_AVFrame_buffer_free, this, AV_BUFFER_FLAG_READONLY);
                avfrm->data[i] = bufptr;
                avfrm->buf[i] = extBufRef;
                bufptr += bytesPerPlan;
            }
        }
        else
        {
            int bufsize = avfrm->nb_samples*bytesPerSample*channels;
            avfrm->linesize[0] = bufsize;
            uint8_t* bufptr = (uint8_t*)m_mat.data;
            AVBufferRef* extBufRef = av_buffer_create(bufptr, bufsize, ImMatWrapper_AVFrame_buffer_free, this, AV_BUFFER_FLAG_READONLY);
            avfrm->data[0] = bufptr;
            avfrm->buf[0] = extBufRef;
        }
    }
    avfrm->pts = pts;
    return avfrm;
}

FFOverlayBlender::FFOverlayBlender()
{
    m_cvtMat2Avfrm.SetOutPixelFormat(AV_PIX_FMT_RGBA);
}

FFOverlayBlender::~FFOverlayBlender()
{
    if (m_avfg)
    {
        avfilter_graph_free(&m_avfg);
        m_avfg = nullptr;
    }
    m_bufSrcCtxs.clear();
    m_bufSinkCtxs.clear();
    if (m_filterOutputs)
        avfilter_inout_free(&m_filterOutputs);
    if (m_filterInputs)
        avfilter_inout_free(&m_filterInputs);
}

bool FFOverlayBlender::Init(const std::string& inputFormat, uint32_t w1, uint32_t h1, uint32_t w2, uint32_t h2, int32_t x, int32_t y, bool evalPerFrame)
{
    if (w1 == 0 || h1 == 0)
    {
        m_errMsg = "INVALID argument value for 'w1' or 'h1'!";
        return false;
    }
    if (w2 == 0 || h2 == 0)
    {
        m_errMsg = "INVALID argument value for 'w2' or 'h2'!";
        return false;
    }
    AVPixelFormat inPixfmt = GetAVPixelFormatByName(inputFormat);
    if (inPixfmt == AV_PIX_FMT_NONE)
    {
        m_errMsg = "INVALID argument value for 'inputFormat'!";
        return false;
    }

    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    m_avfg = avfilter_graph_alloc();
    if (!m_avfg)
    {
        m_errMsg = "FAILED to allocate new 'AVFilterGraph'!";
        return false;
    }

    int fferr;
    ostringstream oss;
    oss << w1 << ":" << h1 << ":pix_fmt=" << (int)inPixfmt << ":time_base=1/" << AV_TIME_BASE << ":sar=1:frame_rate=25/1";
    string bufsrcArg = oss.str(); oss.str("");
    AVFilterContext* filterCtx = nullptr;
    string filterName = "base";
    fferr = avfilter_graph_create_filter(&filterCtx, buffersrc, filterName.c_str(), bufsrcArg.c_str(), nullptr, m_avfg);
    if (fferr < 0)
    {
        oss << "FAILED when invoking 'avfilter_graph_create_filter' for INPUT '" << filterName << "'! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    AVFilterInOut* filtInOutPtr = avfilter_inout_alloc();
    if (!filtInOutPtr)
    {
        m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
        return false;
    }
    filtInOutPtr->name       = av_strdup(filterName.c_str());
    filtInOutPtr->filter_ctx = filterCtx;
    filtInOutPtr->pad_idx    = 0;
    filtInOutPtr->next       = nullptr;
    m_filterOutputs = filtInOutPtr;
    m_bufSrcCtxs.push_back(filterCtx);

    filterName = "overlay"; filterCtx = nullptr;
    oss << w2 << ":" << h2 << ":pix_fmt=" << (int)inPixfmt << ":time_base=1/" << AV_TIME_BASE << ":sar=1:frame_rate=25/1";
    bufsrcArg = oss.str(); oss.str("");
    fferr = avfilter_graph_create_filter(&filterCtx, buffersrc, filterName.c_str(), bufsrcArg.c_str(), nullptr, m_avfg);
    if (fferr < 0)
    {
        oss << "FAILED when invoking 'avfilter_graph_create_filter' for INPUT '" << filterName << "'! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    filtInOutPtr = avfilter_inout_alloc();
    if (!filtInOutPtr)
    {
        m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
        return false;
    }
    filtInOutPtr->name       = av_strdup(filterName.c_str());
    filtInOutPtr->filter_ctx = filterCtx;
    filtInOutPtr->pad_idx    = 0;
    filtInOutPtr->next       = nullptr;
    m_filterOutputs->next = filtInOutPtr;
    m_bufSrcCtxs.push_back(filterCtx);

    filterName = "out"; filterCtx = nullptr;
    fferr = avfilter_graph_create_filter(&filterCtx, buffersink, filterName.c_str(), nullptr, nullptr, m_avfg);
    if (fferr < 0)
    {
        oss << "FAILED when invoking 'avfilter_graph_create_filter' for OUTPUT 'out'! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    const AVPixelFormat out_pix_fmts[] = { AV_PIX_FMT_RGBA, (AVPixelFormat)-1 };
    fferr = av_opt_set_int_list(filterCtx, "pix_fmts", out_pix_fmts, -1, AV_OPT_SEARCH_CHILDREN);
    if (fferr < 0)
    {
        oss << "FAILED when invoking 'av_opt_set_int_list' for OUTPUTS! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    filtInOutPtr = avfilter_inout_alloc();
    if (!filtInOutPtr)
    {
        m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
        return false;
    }
    filtInOutPtr->name        = av_strdup(filterName.c_str());
    filtInOutPtr->filter_ctx  = filterCtx;
    filtInOutPtr->pad_idx     = 0;
    filtInOutPtr->next        = nullptr;
    m_filterInputs = filtInOutPtr;
    m_bufSinkCtxs.push_back(filterCtx);

    oss << "[base][overlay] overlay=x=" << x << ":y=" << y << ":format=auto:eof_action=pass:eval=" << (evalPerFrame ? "frame" : "init");
    string filterArgs = oss.str();
    fferr = avfilter_graph_parse_ptr(m_avfg, filterArgs.c_str(), &m_filterInputs, &m_filterOutputs, nullptr);
    if (fferr < 0)
    {
        oss << "FAILED to invoke 'avfilter_graph_parse_ptr'! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }

    fferr = avfilter_graph_config(m_avfg, nullptr);
    if (fferr < 0)
    {
        oss << "FAILED to invoke 'avfilter_graph_config'! fferr=" << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }

    if (m_filterOutputs)
        avfilter_inout_free(&m_filterOutputs);
    if (m_filterInputs)
        avfilter_inout_free(&m_filterInputs);
    m_x = x;
    m_y = y;
    return true;
}

bool FFOverlayBlender::Init()
{
    return Init("rgba", 1920, 1080, 1920, 1080, 0, 0, true);
}

ImGui::ImMat FFOverlayBlender::Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    if (baseImage.empty() || overlayImage.empty())
        return baseImage;

    int fferr;
    char cmdArgs[32] = {0}, cmdRes[128] = {0};
    if (m_x != x)
    {
        snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", x);
        fferr = avfilter_graph_send_command(m_avfg, "overlay", "x", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
        if (fferr < 0)
        {
            ostringstream oss;
            oss << "FAILED to invoke 'avfilter_graph_send_command()' on argument 'x' = " << x
                << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
            m_errMsg = oss.str();
            return ImGui::ImMat();
        }
        m_x = x;
    }
    if (m_y != y)
    {
        snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", y);
        fferr = avfilter_graph_send_command(m_avfg, "overlay", "y", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
        if (fferr < 0)
        {
            ostringstream oss;
            oss << "FAILED to invoke 'avfilter_graph_send_command()' on argument 'y' = " << x
                << "! fferr = " << fferr << ", response = '" << cmdRes <<"'.";
            m_errMsg = oss.str();
            return ImGui::ImMat();
        }
        m_y = y;
    }

    ImGui::ImMat res = baseImage;
    int64_t pts = (m_inputCount++)*AV_TIME_BASE/25;

    ImMatWrapper_AVFrame baseImgWrapper(baseImage, true);
    SelfFreeAVFramePtr baseAvfrmPtr;
    if (baseImage.device != IM_DD_CPU)
    {
        baseAvfrmPtr = AllocSelfFreeAVFramePtr();
        m_cvtMat2Avfrm.ConvertImage(baseImage, baseAvfrmPtr.get(), pts);
    }
    else
    {
        baseAvfrmPtr = baseImgWrapper.GetWrapper(pts);
    }
    fferr = av_buffersrc_add_frame_flags(m_bufSrcCtxs[0], baseAvfrmPtr.get(), AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "FAILED to invoke 'av_buffersrc_add_frame_flags()' for 'base' image! fferr = " << fferr << ".";
        m_errMsg = oss.str();
        return ImGui::ImMat();
    }
    ImMatWrapper_AVFrame ovlyImgWrapper(overlayImage, true);
    SelfFreeAVFramePtr ovlyAvfrmPtr;
    if (overlayImage.device != IM_DD_CPU)
    {
        ovlyAvfrmPtr = AllocSelfFreeAVFramePtr();
        m_cvtMat2Avfrm.ConvertImage(overlayImage, ovlyAvfrmPtr.get(), pts);
    }
    else
    {
        ovlyAvfrmPtr = ovlyImgWrapper.GetWrapper(pts);
    }
    fferr = av_buffersrc_add_frame_flags(m_bufSrcCtxs[1], ovlyAvfrmPtr.get(), AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "FAILED to invoke 'av_buffersrc_add_frame_flags()' for 'overlay' image! fferr = " << fferr << ".";
        m_errMsg = oss.str();
        return ImGui::ImMat();
    }
    SelfFreeAVFramePtr outAvfrmPtr = AllocSelfFreeAVFramePtr();
    fferr = av_buffersink_get_frame(m_bufSinkCtxs[0], outAvfrmPtr.get());
    if (fferr < 0)
    {
        ostringstream oss; 
        oss << "FAILED to invoke 'av_buffersink_get_frame()' for 'out' image! fferr = " << fferr << ".";
        m_errMsg = oss.str();
        return ImGui::ImMat();
    }
    if (!m_cvtInited)
    {
        m_cvtAvfrm2Mat.SetOutSize(outAvfrmPtr->width, outAvfrmPtr->height);
        m_cvtAvfrm2Mat.SetOutColorFormat(baseImage.color_format);
        m_cvtInited = true;
    }
    if (!m_cvtAvfrm2Mat.ConvertImage(outAvfrmPtr.get(), res, baseImage.time_stamp))
    {
        ostringstream oss;
        oss << "FAILED to convert output AVFrame to ImMat! Converter error message is '" << m_cvtAvfrm2Mat.GetError() << "'.";
        m_errMsg = oss.str();
        return ImGui::ImMat();
    }
    return res;
}

ImGui::ImMat FFOverlayBlender::Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage)
{
    return Blend(baseImage, overlayImage, m_x, m_y, overlayImage.w, overlayImage.h);
}

static ImDataType GetImDataTypeByAVSampleFormat(AVSampleFormat smpfmt)
{
    ImDataType dtype = IM_DT_UNDEFINED;
    switch (smpfmt)
    {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        dtype = IM_DT_INT8;
        break;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        dtype = IM_DT_INT16;
        break;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
        dtype = IM_DT_INT32;
        break;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        dtype = IM_DT_FLOAT32;
        break;
    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_DBLP:
        dtype = IM_DT_FLOAT64;
        break;
    case AV_SAMPLE_FMT_S64:
    case AV_SAMPLE_FMT_S64P:
        dtype = IM_DT_INT64;
        break;
    default:
        break;
    }
    return dtype;
}

static AVSampleFormat GetAVSampleFormatByImDataType(ImDataType dtype, bool isPlanar)
{
    AVSampleFormat smpfmt = AV_SAMPLE_FMT_NONE;
    switch (dtype)
    {
    case IM_DT_INT8:
        smpfmt = AV_SAMPLE_FMT_U8;
        break;
    case IM_DT_INT16:
        smpfmt = AV_SAMPLE_FMT_S16;
        break;
    case IM_DT_INT32:
        smpfmt = AV_SAMPLE_FMT_S32;
        break;
    case IM_DT_FLOAT32:
        smpfmt = AV_SAMPLE_FMT_FLT;
        break;
    case IM_DT_FLOAT64:
        smpfmt = AV_SAMPLE_FMT_DBL;
        break;
    case IM_DT_INT64:
        smpfmt = AV_SAMPLE_FMT_S64;
        break;
    default:
        break;
    }
    if (smpfmt != AV_SAMPLE_FMT_NONE && isPlanar)
        smpfmt = av_get_planar_sample_fmt(smpfmt);
    return smpfmt;
}

bool AudioImMatAVFrameConverter::ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& amat, double timestamp)
{
    amat.release();
    ImDataType dtype = GetImDataTypeByAVSampleFormat((AVSampleFormat)avfrm->format);
    bool isPlanar = av_sample_fmt_is_planar((AVSampleFormat)avfrm->format) == 1;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    const int channels = avfrm->channels;
#else
    const int channels = avfrm->ch_layout.nb_channels;
#endif
    amat.create_type(avfrm->nb_samples, (int)1, channels, dtype);
    amat.elempack = isPlanar ? 1 : channels;
    int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)avfrm->format);
    int bytesPerLine = avfrm->nb_samples*bytesPerSample*(isPlanar?1:channels);
    if (isPlanar)
    {
        uint8_t* dstptr = (uint8_t*)amat.data;
        for (int i = 0; i < channels; i++)
        {
            const uint8_t* srcptr = i < 8 ? avfrm->data[i] : avfrm->extended_data[i-8];
            memcpy(dstptr, srcptr, bytesPerLine);
            dstptr += bytesPerLine;
        }
    }
    else
    {
        int totalBytes = bytesPerLine;
        memcpy(amat.data, avfrm->data[0], totalBytes);
    }
    amat.rate = { avfrm->sample_rate, 1 };
    amat.time_stamp = timestamp;
    amat.elempack = isPlanar ? 1 : channels;
    return true;
}

bool AudioImMatAVFrameConverter::ConvertImMatToAVFrame(const ImGui::ImMat& amat, AVFrame* avfrm, int64_t pts)
{
    av_frame_unref(avfrm);
    bool isPlanar = amat.elempack == 1;
    avfrm->format = (int)GetAVSampleFormatByImDataType(amat.type, isPlanar);
    avfrm->nb_samples = amat.w;
    const int channels = amat.c;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    avfrm->channels = channels;
    avfrm->channel_layout = av_get_default_channel_layout(channels);
#else
    av_channel_layout_default(&avfrm->ch_layout, channels);
#endif
    int fferr = av_frame_get_buffer(avfrm, 0);
    if (fferr < 0)
    {
        std::cerr << "FAILED to allocate buffer for audio AVFrame! format=" << av_get_sample_fmt_name((AVSampleFormat)avfrm->format)
                << ", nb_samples=" << avfrm->nb_samples << ", channels=" << channels << ". fferr=" << fferr << "." << std::endl;
        return false;
    }
    int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)avfrm->format);
    int bytesPerLine = avfrm->nb_samples*bytesPerSample*(isPlanar?1:channels);
    if (isPlanar)
    {
        const uint8_t* srcptr = (const uint8_t*)amat.data;
        for (int i = 0; i < channels; i++)
        {
            uint8_t* dstptr = i < 8 ? avfrm->data[i] : avfrm->extended_data[i-8];
            memcpy(dstptr, srcptr, bytesPerLine);
            srcptr += bytesPerLine;
        }
    }
    else
    {
        int totalBytes = bytesPerLine;
        memcpy(avfrm->data[0], amat.data, totalBytes);
    }
    avfrm->sample_rate = amat.rate.num;
    avfrm->pts = pts;
    return true;
}

static MediaInfo::Ratio MediaInfoRatioFromAVRational(const AVRational& src)
{
    return { src.num, src.den };
}

MediaInfo::InfoHolder GenerateMediaInfoByAVFormatContext(const AVFormatContext* avfmtCtx)
{
    MediaInfo::InfoHolder hInfo(new MediaInfo::Info());
    hInfo->url = string(avfmtCtx->url);
    double fftb = av_q2d(FF_AV_TIMEBASE);
    if (avfmtCtx->duration != AV_NOPTS_VALUE)
        hInfo->duration = avfmtCtx->duration*fftb;
    if (avfmtCtx->start_time != AV_NOPTS_VALUE)
        hInfo->startTime = avfmtCtx->start_time*fftb;
    hInfo->streams.reserve(avfmtCtx->nb_streams);
    for (uint32_t i = 0; i < avfmtCtx->nb_streams; i++)
    {
        MediaInfo::StreamHolder hStream;
        AVStream* stream = avfmtCtx->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;
        double streamtb = av_q2d(stream->time_base);
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            auto vidStream = new MediaInfo::VideoStream();
            vidStream->bitRate = codecpar->bit_rate;
            if (stream->start_time != AV_NOPTS_VALUE)
                vidStream->startTime = stream->start_time*streamtb;
            else
                vidStream->startTime = hInfo->startTime;
            if (stream->duration > 0)
                vidStream->duration = stream->duration*streamtb;
            else
                vidStream->duration = hInfo->duration;
            vidStream->timebase = MediaInfoRatioFromAVRational(stream->time_base);
            vidStream->width = codecpar->width;
            vidStream->height = codecpar->height;
            if (stream->sample_aspect_ratio.num > 0 && stream->sample_aspect_ratio.den > 0)
                vidStream->sampleAspectRatio = MediaInfoRatioFromAVRational(stream->sample_aspect_ratio);
            else
                vidStream->sampleAspectRatio = {1, 1};
            vidStream->avgFrameRate = MediaInfoRatioFromAVRational(stream->avg_frame_rate);
            vidStream->realFrameRate = MediaInfoRatioFromAVRational(stream->r_frame_rate);
            auto cdcdesc = avcodec_descriptor_get(codecpar->codec_id);
            string mimeType = cdcdesc && cdcdesc->mime_types ? string(cdcdesc->mime_types[0]) : "";
            string demuxerName(avfmtCtx->iformat->name);
            if (demuxerName.find("image2") != string::npos ||
                demuxerName.find("_pipe") != string::npos ||
                demuxerName.find("mp3") != string::npos ||
                mimeType.find("image/") != string::npos)
                vidStream->isImage = true;
            if (!vidStream->isImage)
            {
                if (stream->nb_frames > 0)
                    vidStream->frameNum = stream->nb_frames;
                else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0)
                    vidStream->frameNum = (uint64_t)(stream->duration*av_q2d(stream->r_frame_rate));
                else if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
                    vidStream->frameNum = (uint64_t)(stream->duration*av_q2d(stream->avg_frame_rate));
            }
            else
            {
                vidStream->frameNum = stream->nb_frames > 0 ? stream->nb_frames : 1;
                if (vidStream->duration < 0) vidStream->duration = 0;
            }
            switch (codecpar->color_trc)
            {
                case AVCOL_TRC_SMPTE2084:
                case AVCOL_TRC_ARIB_STD_B67:
                    vidStream->isHdr = true;
                    break;
                default:
                    vidStream->isHdr = false;
            }
            const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)codecpar->format);
            if (desc && desc->nb_components > 0)
                vidStream->bitDepth = desc->comp[0].depth;
            hStream = MediaInfo::StreamHolder(vidStream);
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            auto audStream = new MediaInfo::AudioStream();
            audStream->bitRate = codecpar->bit_rate;
            if (stream->start_time != AV_NOPTS_VALUE)
                audStream->startTime = stream->start_time*streamtb;
            else
                audStream->startTime = hInfo->startTime;
            if (stream->duration > 0)
                audStream->duration = stream->duration*streamtb;
            else
                audStream->duration = hInfo->duration;
            audStream->timebase = MediaInfoRatioFromAVRational(stream->time_base);
            audStream->channels = codecpar->channels;
            audStream->sampleRate = codecpar->sample_rate;
            audStream->bitDepth = av_get_bytes_per_sample((AVSampleFormat)codecpar->format) << 3;
            hStream = MediaInfo::StreamHolder(audStream);
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            auto subStream = new MediaInfo::SubtitleStream();
            subStream->bitRate = codecpar->bit_rate;
            if (stream->start_time != AV_NOPTS_VALUE)
                subStream->startTime = stream->start_time*streamtb;
            else
                subStream->startTime = hInfo->startTime;
            if (stream->duration > 0)
                subStream->duration = stream->duration*streamtb;
            else
                subStream->duration = hInfo->duration;
            subStream->timebase = MediaInfoRatioFromAVRational(stream->time_base);
            hStream = MediaInfo::StreamHolder(subStream);
        }
        if (hStream)
            hInfo->streams.push_back(hStream);
    }
    return hInfo;
}
