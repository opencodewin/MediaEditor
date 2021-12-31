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
    #include "libavutil/frame.h"
    #include "libswscale/swscale.h"
}

extern const AVRational MILLISEC_TIMEBASE;
extern const AVRational FF_AV_TIMEBASE;

std::string MillisecToString(int64_t millisec);
std::string TimestampToString(double timestamp);

using SelfFreeAVFramePtr = std::shared_ptr<AVFrame>;
SelfFreeAVFramePtr AllocSelfFreeAVFramePtr();

int AVPixelFormatToImColorFormat(AVPixelFormat pixfmt);
bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp);

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
    float m_outWFactor{0.f}, m_outHFactor{0.f};
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


#include "MediaInfo.h"

MediaInfo::InfoHolder GenerateMediaInfoByAVFormatContext(const AVFormatContext* avfmtCtx);
