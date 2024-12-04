#if 0
#include <memory>
#include <filesystem>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cassert>
#include "MatIo.h"
extern "C"
{
    #include "libavutil/pixdesc.h"
    #include "libavutil/hwcontext.h"
    #include "libavutil/avutil.h"
    #include "libavutil/opt.h"
    #include "libavutil/channel_layout.h"
#if LIBAVCODEC_VERSION_MAJOR > 58 || (LIBAVCODEC_VERSION_MAJOR == 58 && LIBAVCODEC_VERSION_MINOR >= 78)
    #include "libavcodec/codec_desc.h"
#endif
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
    #include "libavcodec/avcodec.h"
}

using namespace std;
namespace fs = std::filesystem;

namespace MatUtils
{
const AVRational MILLISEC_TIMEBASE = { 1, 1000 };

using AVFrameHolder = shared_ptr<AVFrame>;

static const auto _AVFRAME_HOLDER_DELETER = [] (AVFrame* p) {
    if (p)
        av_frame_free(&p);
};

AVFrameHolder CreateAVFrameHolder()
{
    return AVFrameHolder(av_frame_alloc(), _AVFRAME_HOLDER_DELETER);
}

AVFrameHolder CloneAVFrameHolder(const AVFrame* pAvfrm)
{
    return AVFrameHolder(av_frame_clone(pAvfrm), _AVFRAME_HOLDER_DELETER);
}

using AVPacketHolder = shared_ptr<AVPacket>;

static const auto _AVPACKET_HOLDER_DELETER = [] (AVPacket* p) {
    if (p)
        av_packet_free(&p);
};

AVPacketHolder CreateAVPacketHolder()
{
    return AVPacketHolder(av_packet_alloc(), _AVPACKET_HOLDER_DELETER);
}


static AVPixelFormat GetPixelFormatByColorFormat(ImColorFormat clrfmt, ImDataType dtype)
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

static AVColorSpace GetAVColorSpaceByImColorSpace(ImColorSpace imclrspc)
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

static AVColorRange GetAVColorRangeByImColorRange(ImColorRange imclrrng)
{
    AVColorRange clrrng = AVCOL_RANGE_UNSPECIFIED;
    if (imclrrng == IM_CR_FULL_RANGE)
        clrrng = AVCOL_RANGE_JPEG;
    else if (imclrrng == IM_CR_NARROW_RANGE)
        clrrng = AVCOL_RANGE_MPEG;
    return clrrng;
}

AVFrameHolder ConvertMatToAVFrame(const ImGui::ImMat& m, int64_t pts = INT64_MIN)
{
    assert(m.device == IM_DD_CPU);

    AVPixelFormat pixfmt = GetPixelFormatByColorFormat(m.color_format, m.type);
    if (pixfmt == AV_PIX_FMT_NONE)
    {
        cerr << "CANNOT deduce AVPixelFormat from ImColorFormat(" << m.color_format << ") and ImDataType(" << m.type << ")!" << endl;
        return nullptr;
    }

    AVFrameHolder hAvfrm = CreateAVFrameHolder();
    auto pAvfrm = hAvfrm.get();
    pAvfrm->width = m.w;
    pAvfrm->height = m.h;
    pAvfrm->format = (int)pixfmt;
    int fferr;
    fferr = av_frame_get_buffer(pAvfrm, 0);
    if (fferr < 0)
    {
        cerr << "FAILED to invoke 'av_frame_get_buffer()'! fferr=" << fferr << "." << endl;
        return nullptr;
    }

    const auto pPixfmtDesc = av_pix_fmt_desc_get(pixfmt);
    const bool bIsRgb = (pPixfmtDesc->flags&AV_PIX_FMT_FLAG_RGB) > 0;
    int iCh;
    if (bIsRgb)
    {
        iCh = pPixfmtDesc->nb_components;
    }
    else
    {
        if (m.color_format == IM_CF_YUV444)
            iCh = 3;
        else
            iCh = 2;
    }
    const uint8_t* pPrevSrc;
    for (int i = 0; i < m.c; i++)
    {
        int iChW = m.w;
        int iChH = m.h;
        if (!bIsRgb && i > 0)
        {
            iChW >>= pPixfmtDesc->log2_chroma_w;
            iChH >>= pPixfmtDesc->log2_chroma_h;
        }
        if (i < pPixfmtDesc->nb_components && pPixfmtDesc->comp[i].plane == i)
        {
            uint8_t* pDst = pAvfrm->data[i]+pPixfmtDesc->comp[i].offset;
            const uint8_t* pSrc;
            if (i < iCh)
                pSrc = (const uint8_t*)m.channel(i).data;
            else
                pSrc = pPrevSrc;
            int iSrcLineSize = iChW*pPixfmtDesc->comp[i].step;
            for (int j = 0; j < iChH; j++)
            {
                memcpy(pDst, pSrc, iSrcLineSize);
                pSrc += iSrcLineSize;
                pDst += pAvfrm->linesize[i];
            }
            pPrevSrc = pSrc;
        }
    }


    pAvfrm->colorspace = GetAVColorSpaceByImColorSpace(m.color_space);
    pAvfrm->color_range = GetAVColorRangeByImColorRange(m.color_range);
    pAvfrm->pts = pts != INT64_MIN ? pts : (int64_t)(m.time_stamp*1000);
    return hAvfrm;
}

struct SavePngContext
{
    ~SavePngContext()
    {
        if (pPngEncCtx)
            avcodec_free_context(&pPngEncCtx);
    }

    AVCodecContext* pPngEncCtx{nullptr};
};

bool SaveAsPng(const ImGui::ImMat& m, const string& _savePath)
{
    const static string logTag = "[SaveAsPng]";
    const auto eDtype = m.color_format;
    if (eDtype != IM_CF_ABGR && eDtype != IM_CF_ARGB && eDtype != IM_CF_BGRA && eDtype != IM_CF_RGBA &&
        eDtype != IM_CF_BGR && eDtype != IM_CF_RGB && eDtype != IM_CF_GRAY)
    {
        cerr << logTag << "Only support RGB(A) and GRAY format!" << endl;
        return false;
    }
    fs::path pngPath(_savePath);
    auto fileExt = pngPath.extension().string();
    transform(fileExt.begin(), fileExt.end(), fileExt.begin(), [] (unsigned char c) { return tolower(c); });
    if (fileExt.empty())
        pngPath /= ".png";
    else if (fileExt != ".png")
        cerr << logTag << "WARNING! Save path is NOT ended with png extension. '" << _savePath << "'." << endl;

    auto hAvfrm = ConvertMatToAVFrame(m);
    if (!hAvfrm)
    {
        cerr << logTag << "FAILED to convert input ImMat to AVFrame!" << endl;
        return false;
    }

    auto pCdc = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!pCdc)
    {
        cerr << logTag << "FAILED to find encoder for 'png'!" << endl;
        return false;
    }

    SavePngContext tSavePngCtx;
    tSavePngCtx.pPngEncCtx = avcodec_alloc_context3(pCdc);
    if (!tSavePngCtx.pPngEncCtx)
    {
        cerr << logTag << "FAILED to allocate encoder context for 'png'!" << endl;
        return false;
    }
    tSavePngCtx.pPngEncCtx->width = hAvfrm->width;
    tSavePngCtx.pPngEncCtx->height = hAvfrm->height;
    tSavePngCtx.pPngEncCtx->pix_fmt = (AVPixelFormat)hAvfrm->format;
    tSavePngCtx.pPngEncCtx->time_base.num = 1;
    tSavePngCtx.pPngEncCtx->time_base.den = 1;
    int fferr;
    fferr = avcodec_open2(tSavePngCtx.pPngEncCtx, pCdc, nullptr);
    if (fferr < 0)
    {
        cerr << logTag << "FAILED to open 'png' encoder! fferr=" << fferr << "." << endl;
        return false;
    }

    fferr = avcodec_send_frame(tSavePngCtx.pPngEncCtx, hAvfrm.get());
    if (fferr < 0)
    {
        cerr << logTag << "FAILED to invoke 'avcodec_send_frame()'! fferr=" << fferr << "." << endl;
        return false;
    }
    auto hAvpkt = CreateAVPacketHolder();
    fferr = avcodec_receive_packet(tSavePngCtx.pPngEncCtx, hAvpkt.get());
    if (fferr < 0)
    {
        cerr << logTag << "FAILED to invoke 'avcodec_receive_packet()'! fferr=" << fferr << "." << endl;
        return false;
    }

    ofstream ofs(pngPath, ios::out|ios::binary);
    ofs.write((const char*)hAvpkt->data, hAvpkt->size);
    ofs.flush();
    ofs.close();

    return true;
}
}
#endif

#include <imgui.h>
#include "MatIo.h"
using namespace std;
namespace MatUtils
{
bool SaveAsPng(const ImGui::ImMat& m, const std::string& savePath)
{
    const static string logTag = "[SaveAsPng]";
    const auto eDtype = m.color_format;
    if (eDtype != IM_CF_ABGR && eDtype != IM_CF_ARGB && eDtype != IM_CF_BGRA && eDtype != IM_CF_RGBA &&
        eDtype != IM_CF_BGR && eDtype != IM_CF_RGB && eDtype != IM_CF_GRAY)
    {
        cerr << logTag << "Only support RGB(A) and GRAY format!" << endl;
        return false;
    }
    stbi_write_png(savePath.c_str(), m.w, m.h, m.c, m.data, m.w * m.c);
    return true;
}
} // namespace MatUtils