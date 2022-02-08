#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <imconfig.h>
#include <immat.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
extern "C"
{
    #include "libavformat/avformat.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/frame.h"
    #include "libswscale/swscale.h"
}

#if LIBAVFORMAT_VERSION_MAJOR >= 59
typedef const AVCodec*      AVCodecPtr;
#else
typedef AVCodec*            AVCodecPtr;
#endif

extern const AVRational MILLISEC_TIMEBASE;
extern const AVRational FF_AV_TIMEBASE;

std::string MillisecToString(int64_t millisec);
std::string TimestampToString(double timestamp);

bool IsHwFrame(const AVFrame* avfrm);
bool HwFrameToSwFrame(AVFrame* swfrm, const AVFrame* hwfrm);

using SelfFreeAVFramePtr = std::shared_ptr<AVFrame>;
SelfFreeAVFramePtr AllocSelfFreeAVFramePtr();
SelfFreeAVFramePtr CloneSelfFreeAVFramePtr(const AVFrame* avfrm);

int AVPixelFormatToImColorFormat(AVPixelFormat pixfmt);
bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp);
bool ConvertImMatToAVFrame(const ImGui::ImMat& vmat, AVFrame* avfrm, int64_t pts);

class AVFrameToImMatConverter
{
public:
    AVFrameToImMatConverter();
    ~AVFrameToImMatConverter();

    AVFrameToImMatConverter(const AVFrameToImMatConverter&) = delete;
    AVFrameToImMatConverter(AVFrameToImMatConverter&&) = default;
    AVFrameToImMatConverter& operator=(const AVFrameToImMatConverter&) = delete;

    bool SetOutSize(uint32_t width, uint32_t height);
    bool SetOutColorFormat(ImColorFormat clrfmt);
    bool SetResizeInterpolateMode(ImInterpolateMode interp);
    bool ConvertImage(const AVFrame* avfrm, ImGui::ImMat& outMat, double timestamp);

    uint32_t GetOutWidth() const { return m_outWidth; }
    uint32_t GetOutHeight() const { return m_outHeight; }
    ImColorFormat GetOutColorFormat() const { return m_outClrFmt; }
    ImInterpolateMode GetResizeInterpolateMode() const { return m_resizeInterp; }

    std::string GetError() const { return m_errMsg; }

private:
    uint32_t m_outWidth{0}, m_outHeight{0};
    ImColorFormat m_outClrFmt{IM_CF_RGBA};
    ImInterpolateMode m_resizeInterp{IM_INTERPOLATE_AREA};
#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan* m_imgClrCvt{nullptr};
    ImGui::Resize_vulkan* m_imgRsz{nullptr};
#endif
    bool m_useVulkanComponents;
    SwsContext* m_swsCtx{nullptr};
    int m_swsFlags{0};
    int m_swsInWidth{0}, m_swsInHeight{0};
    AVPixelFormat m_swsInFormat{AV_PIX_FMT_NONE};
    AVPixelFormat m_swsOutFormat{AV_PIX_FMT_RGBA};
    AVColorSpace m_swsClrspc{AVCOL_SPC_RGB};
    bool m_passThrough{false};
    std::string m_errMsg;
};

class ImMatToAVFrameConverter
{
public:
    ImMatToAVFrameConverter();
    ~ImMatToAVFrameConverter();

    ImMatToAVFrameConverter(const ImMatToAVFrameConverter&) = delete;
    ImMatToAVFrameConverter(ImMatToAVFrameConverter&&) = default;
    ImMatToAVFrameConverter& operator=(const ImMatToAVFrameConverter&) = delete;

    bool SetOutSize(uint32_t width, uint32_t height);
    bool SetOutPixelFormat(AVPixelFormat pixfmt);
    bool SetOutColorSpace(AVColorSpace clrspc);
    bool SetOutColorRange(AVColorRange clrrng);
    bool SetResizeInterpolateMode(ImInterpolateMode interp);
    bool ConvertImage(ImGui::ImMat& inMat, AVFrame* avfrm, int64_t pts);

    uint32_t GetOutWidth() const { return m_outWidth; }
    uint32_t GetOutHeight() const { return m_outHeight; }
    AVPixelFormat GetOutPixelFormat() const { return m_outPixfmt; }
    AVColorSpace GetOutColorSpace() const { return m_outClrspc; }
    AVColorRange GetOutColorRange() const { return m_outClrrng; }
    ImInterpolateMode GetResizeInterpolateMode() const { return m_resizeInterp; }

    std::string GetError() const { return m_errMsg; }

private:
    uint32_t m_outWidth{0}, m_outHeight{0};
    AVPixelFormat m_outPixfmt{AV_PIX_FMT_YUV420P};
    AVColorSpace m_outClrspc{AVCOL_SPC_BT709};
    AVColorRange m_outClrrng{AVCOL_RANGE_MPEG};
    const AVPixFmtDescriptor* m_pixDesc{nullptr};
    uint8_t m_outBitsPerPix{0};
    ImInterpolateMode m_resizeInterp{IM_INTERPOLATE_AREA};
#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan* m_imgClrCvt{nullptr};
    ImGui::Resize_vulkan* m_imgRsz{nullptr};
    ImColorFormat m_outMatClrfmt;
    bool m_isClrfmtMatched{false};
    ImColorSpace m_outMatClrspc;
    ImColorRange m_outMatClrrng;
#endif
    bool m_useVulkanComponents;
    SwsContext* m_swsCtx{nullptr};
    int m_swsFlags{0};
    int m_swsInWidth{0}, m_swsInHeight{0};
    AVPixelFormat m_swsInFormat{AV_PIX_FMT_NONE};
    bool m_passThrough{false};
    std::string m_errMsg;
};


#include "MediaInfo.h"

MediaInfo::InfoHolder GenerateMediaInfoByAVFormatContext(const AVFormatContext* avfmtCtx);
