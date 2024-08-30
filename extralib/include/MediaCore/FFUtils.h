/*
    Copyright (c) 2023-2024 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <imconfig.h>
#include <immat.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#include <ColorConvert_vulkan.h>
#include <Resize_vulkan.h>
#endif
extern "C"
{
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/samplefmt.h"
    #include "libavutil/frame.h"
    #include "libswscale/swscale.h"
    #include "libavfilter/avfilter.h"
}

#include "MediaCore.h"
#include "MediaData.h"
#include "HwaccelManager.h"

#if LIBAVFORMAT_VERSION_MAJOR >= 59
typedef const AVCodec*      AVCodecPtr;
#else
typedef AVCodec*            AVCodecPtr;
#endif

#define DONOT_CACHE_HWAVFRAME 1

MEDIACORE_API extern const AVRational MILLISEC_TIMEBASE;
MEDIACORE_API extern const AVRational MICROSEC_TIMEBASE;
MEDIACORE_API extern const AVRational FF_AV_TIMEBASE;

MEDIACORE_API std::string MillisecToString(int64_t millisec);
MEDIACORE_API std::string TimestampToString(double timestamp);

MEDIACORE_API bool IsHwFrame(const AVFrame* avfrm);
MEDIACORE_API bool TransferHwFrameToSwFrame(AVFrame* swfrm, const AVFrame* hwfrm);

using SelfFreeAVFramePtr = std::shared_ptr<AVFrame>;
MEDIACORE_API SelfFreeAVFramePtr AllocSelfFreeAVFramePtr();
MEDIACORE_API SelfFreeAVFramePtr CloneSelfFreeAVFramePtr(const AVFrame* avfrm);
MEDIACORE_API SelfFreeAVFramePtr WrapSelfFreeAVFramePtr(AVFrame* avfrm);
using SelfFreeAVPacketPtr = std::shared_ptr<AVPacket>;
MEDIACORE_API SelfFreeAVPacketPtr AllocSelfFreeAVPacketPtr();
MEDIACORE_API SelfFreeAVPacketPtr CloneSelfFreeAVPacketPtr(const AVPacket* avpkt);
MEDIACORE_API SelfFreeAVPacketPtr WrapSelfFreeAVPacketPtr(AVPacket* avpkt);

MEDIACORE_API AVPixelFormat GetAVPixelFormatByName(const std::string& name);
MEDIACORE_API AVSampleFormat GetAVSampleFormatByDataType(ImDataType dataType, bool isPlanar);
MEDIACORE_API ImColorFormat ConvertPixelFormatToColorFormat(AVPixelFormat pixfmt);
MEDIACORE_API ImDataType GetDataTypeFromSampleFormat(AVSampleFormat smpfmt);
MEDIACORE_API bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp);
MEDIACORE_API bool MapAVFrameToImMat(const AVFrame* avfrm, std::vector<ImGui::ImMat>& vmat, double timestamp);
MEDIACORE_API bool ConvertImMatToAVFrame(const ImGui::ImMat& vmat, AVFrame* avfrm, int64_t pts);
MEDIACORE_API AVPixelFormat ConvertColorFormatToPixelFormat(ImColorFormat clrfmt, ImDataType dtype);

class MEDIACORE_API AVFrameToImMatConverter
{
public:
    AVFrameToImMatConverter();
    ~AVFrameToImMatConverter();

    AVFrameToImMatConverter(const AVFrameToImMatConverter&) = delete;
    AVFrameToImMatConverter(AVFrameToImMatConverter&&) = default;
    AVFrameToImMatConverter& operator=(const AVFrameToImMatConverter&) = delete;

    bool SetOutSize(uint32_t width, uint32_t height);
    bool SetOutColorFormat(ImColorFormat clrfmt);
    bool SetOutDataType(ImDataType dtype);
    bool SetResizeInterpolateMode(ImInterpolateMode interp);
    bool ConvertImage(const AVFrame* avfrm, ImGui::ImMat& outMat, double timestamp);

    uint32_t GetOutWidth() const { return m_outWidth; }
    uint32_t GetOutHeight() const { return m_outHeight; }
    ImColorFormat GetOutColorFormat() const { return m_outClrFmt; }
    ImDataType GetOutDataType() const { return m_outDataType; }
    ImInterpolateMode GetResizeInterpolateMode() const { return m_resizeInterp; }

    void SetUseVulkanConverter(bool use) { m_useVulkanComponents = use; }

    std::string GetError() const { return m_errMsg; }

private:
    uint32_t m_outWidth{0}, m_outHeight{0};
    ImColorFormat m_outClrFmt{IM_CF_RGBA};
    ImDataType m_outDataType{IM_DT_INT8};
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

class MEDIACORE_API ImMatToAVFrameConverter
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

class MEDIACORE_API ImMatWrapper_AVFrame
{
public:
    ImMatWrapper_AVFrame(bool isVideo = true) : m_isVideo(isVideo) {}
    ImMatWrapper_AVFrame(const ImGui::ImMat& mat, bool isVideo) : m_mat(mat), m_isVideo(isVideo) {}
    void SetMat(const ImGui::ImMat& mat) { m_mat = mat; }
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

    ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage);
    ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, int32_t x, int32_t y);

    std::string GetError() const { return m_errMsg; }

private:
    bool SetupFilterGraph(AVPixelFormat pixfmt, uint32_t w1, uint32_t h1, uint32_t w2, uint32_t h2, int32_t x, int32_t y, bool evalPerFrame);
    void ReleaseFilterGraph();

private:
    AVFilterGraph* m_avfg{nullptr};
    AVFilterInOut* m_filterOutputs{nullptr};
    AVFilterInOut* m_filterInputs{nullptr};
    std::vector<AVFilterContext*> m_bufSrcCtxs;
    std::vector<AVFilterContext*> m_bufSinkCtxs;
    uint32_t m_baseImgW{0}, m_baseImgH{0}, m_ovlyImgW{0}, m_ovlyImgH{0};
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
    AudioImMatAVFrameConverter() = default;

    bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& amat, double timestamp);
    bool ConvertImMatToAVFrame(const ImGui::ImMat& amat, AVFrame* avfrm, int64_t pts);
};

namespace FFUtils
{
// Find and open a video decoder
struct OpenVideoDecoderOptions
{
    std::string designatedDecoderName;
    bool onlyUseSoftwareDecoder{false};
    AVHWDeviceType useHardwareType{AV_HWDEVICE_TYPE_NONE};
    bool preferHwOutputPixfmt{true};
    AVPixelFormat useHwOutputPixfmt{AV_PIX_FMT_NONE};
    AVPixelFormat forceOutputPixfmt{AV_PIX_FMT_NONE};
    MediaCore::HwaccelManager::Holder hHwaMgr;
};
struct OpenVideoDecoderResult
{
    AVCodecContext* decCtx{nullptr};
    AVHWDeviceType hwDevType{AV_HWDEVICE_TYPE_NONE};
    SelfFreeAVFramePtr probeFrame;
    std::string errMsg;
};
bool OpenVideoDecoder(const AVFormatContext* pAvfmtCtx, int videoStreamIndex, OpenVideoDecoderOptions* options, OpenVideoDecoderResult* result, bool needValidation = true);

// A function to copy pcm data from one buffer to another, with the considering of sample format and buffer state
uint32_t CopyPcmDataEx(uint8_t channels, uint8_t bytesPerSample, uint32_t copySamples,
    bool isDstPlanar,       uint8_t** ppDst, uint32_t dstOffsetSamples,
    bool isSrcPlanar, const uint8_t** ppSrc, uint32_t srcOffsetSamples);

MEDIACORE_API MediaCore::VideoFrame::Holder CreateVideoFrameFromAVFrame(const AVFrame* pAvfrm, int64_t pos);
MEDIACORE_API MediaCore::VideoFrame::Holder CreateVideoFrameFromAVFrame(SelfFreeAVFramePtr hAvfrm, int64_t pos);

struct FFFilterGraph
{
    using Holder = std::shared_ptr<FFFilterGraph>;
    static Holder CreateInstance(const std::string& strName = "");

    virtual MediaCore::ErrorCode Initialize(const std::string& strFgArgs, const MediaCore::Ratio& tFrameRate, MediaCore::VideoFrame::NativeData::Type eOutputNativeType = MediaCore::VideoFrame::NativeData::MAT) = 0;
    virtual MediaCore::ErrorCode SendFrame(MediaCore::VideoFrame::Holder hVfrm) = 0;
    virtual MediaCore::ErrorCode ReceiveFrame(MediaCore::VideoFrame::Holder& hVfrm) = 0;

    virtual std::string GetError() const = 0;
};
}

#include "MediaInfo.h"

MediaCore::MediaInfo::Holder GenerateMediaInfoByAVFormatContext(const AVFormatContext* avfmtCtx);
