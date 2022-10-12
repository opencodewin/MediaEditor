#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
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
    #include "libavfilter/avfilter.h"
}

#if LIBAVFORMAT_VERSION_MAJOR >= 59
typedef const AVCodec*      AVCodecPtr;
#else
typedef AVCodec*            AVCodecPtr;
#endif

extern const AVRational MILLISEC_TIMEBASE;
extern const AVRational MICROSEC_TIMEBASE;
extern const AVRational FF_AV_TIMEBASE;

std::string MillisecToString(int64_t millisec);
std::string TimestampToString(double timestamp);

bool IsHwFrame(const AVFrame* avfrm);
bool HwFrameToSwFrame(AVFrame* swfrm, const AVFrame* hwfrm);

using SelfFreeAVFramePtr = std::shared_ptr<AVFrame>;
SelfFreeAVFramePtr AllocSelfFreeAVFramePtr();
SelfFreeAVFramePtr CloneSelfFreeAVFramePtr(const AVFrame* avfrm);
SelfFreeAVFramePtr WrapSelfFreeAVFramePtr(AVFrame* avfrm);

AVPixelFormat GetAVPixelFormatByName(const std::string& name);
ImColorFormat ConvertPixelFormatToColorFormat(AVPixelFormat pixfmt);
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

    void SetUseVulkanConverter(bool use) { m_useVulkanComponents = use; }

    std::string GetError() const { return m_errMsg; }

private:
    uint32_t m_outWidth{0}, m_outHeight{0};
    ImColorFormat m_outClrFmt{IM_CF_RGBA};
    ImInterpolateMode m_resizeInterp{IM_INTERPOLATE_AREA};
#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan* m_imgClrCvt{nullptr};
    ImGui::Resize_vulkan* m_imgRsz{nullptr};
    bool m_outputCpuMat{false};
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
    bool ConvertImage(const ImGui::ImMat& inMat, AVFrame* avfrm, int64_t pts);

    uint32_t GetOutWidth() const { return m_outWidth; }
    uint32_t GetOutHeight() const { return m_outHeight; }
    AVPixelFormat GetOutPixelFormat() const { return m_outPixfmt; }
    AVColorSpace GetOutColorSpace() const { return m_outClrspc; }
    AVColorRange GetOutColorRange() const { return m_outClrrng; }
    ImInterpolateMode GetResizeInterpolateMode() const { return m_resizeInterp; }

    void SetUseVulkanConverter(bool use) { m_useVulkanComponents = use; }

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

class ImMatWrapper_AVFrame
{
public:
    ImMatWrapper_AVFrame(ImGui::ImMat& mat, bool isVideo) : m_mat(mat), m_isVideo(isVideo) {}
    SelfFreeAVFramePtr GetWrapper(int64_t pts = 0);

private:
    ImGui::ImMat m_mat;
    bool m_isVideo;
};

class FFOverlayBlender
{
public:
    FFOverlayBlender();
    FFOverlayBlender(const FFOverlayBlender&) = delete;
    FFOverlayBlender(FFOverlayBlender&&) = default;
    FFOverlayBlender& operator=(const FFOverlayBlender&) = delete;
    ~FFOverlayBlender();

    bool Init(const std::string& inputFormat, uint32_t w1, uint32_t h1, uint32_t w2, uint32_t h2, int32_t x, int32_t y, bool evalPerFrame);
    ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage);
    bool Init();
    ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, int32_t x, int32_t y, uint32_t w, uint32_t h);

    std::string GetError() const { return m_errMsg; }

private:
    AVFilterGraph* m_avfg{nullptr};
    AVFilterInOut* m_filterOutputs{nullptr};
    AVFilterInOut* m_filterInputs{nullptr};
    std::vector<AVFilterContext*> m_bufSrcCtxs;
    std::vector<AVFilterContext*> m_bufSinkCtxs;
    int32_t m_x{0}, m_y{0};
    int64_t m_inputCount{0};
    ImMatToAVFrameConverter m_cvtMat2Avfrm;
    AVFrameToImMatConverter m_cvtAvfrm2Mat;
    bool m_cvtInited{false};

    std::string m_errMsg;
};

class AudioImMatAVFrameConverter
{
public:
    AudioImMatAVFrameConverter(uint32_t sampleRate) : m_sampleRate(sampleRate) {}
    uint32_t SampleRate() const { return m_sampleRate; }

    bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& amat, double timestamp);
    bool ConvertImMatToAVFrame(const ImGui::ImMat& amat, AVFrame* avfrm, int64_t pts);

private:
    uint32_t m_sampleRate;
};

#include "MediaInfo.h"

MediaInfo::InfoHolder GenerateMediaInfoByAVFormatContext(const AVFormatContext* avfmtCtx);
