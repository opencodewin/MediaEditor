#pragma once
#include "VideoTransformFilter_Base.h"
#include "FFUtils.h"

namespace DataLayer
{
    class VideoTransformFilter_FFImpl : public VideoTransformFilter_Base
    {
    public:
        virtual ~VideoTransformFilter_FFImpl();
        const std::string GetFilterName() const override;
        bool Initialize(uint32_t outWidth, uint32_t outHeight) override;
        bool SetOutputFormat(const std::string& outputFormat) override;
        bool SetRotationAngle(double angle) override;
        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override;
        bool SetPositionOffsetH(int32_t value) override;
        bool SetPositionOffsetV(int32_t value) override;
        ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override;

    private:
        AVFilterGraph* CreateFilterGraph(const std::string& filterArgs, uint32_t w, uint32_t h, AVPixelFormat inputPixfmt, AVFilterContext** inputCtx, AVFilterContext** outputCtx);
        bool ConvertInMatToAVFrame(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr);
        bool PerformCropStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr);
        bool PerformScaleStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr);
        bool PerformRotateStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr);
        bool PerformPositionStage(const ImGui::ImMat& inMat, SelfFreeAVFramePtr& avfrmPtr);
        bool _filterImage(const ImGui::ImMat& inMat, ImGui::ImMat& outMat, int64_t pos);

    private:
        uint32_t m_diagonalLen{0}, m_scaleSafePadding{2};
        AVPixelFormat m_unifiedInputPixfmt{AV_PIX_FMT_RGBA};
        AVPixelFormat m_unifiedOutputPixfmt{AV_PIX_FMT_NONE};
        ScaleType m_scaleType{SCALE_TYPE__FIT};
        AVRational m_inputFrameRate{25, 1};
        int32_t m_inputCount{0};

        ImMatToAVFrameConverter m_mat2frmCvt;
        AVFrameToImMatConverter m_frm2matCvt;

        AVFilterGraph* m_scaleFg{nullptr};
        AVFilterContext* m_scaleInputCtx{nullptr};
        AVFilterContext* m_scaleOutputCtx{nullptr};
        double m_realScaleRatioH{1}, m_realScaleRatioV{1};
        uint32_t m_scaleOutputRoiW{0}, m_scaleOutputRoiH{0};
        uint32_t m_scaleInputW{0}, m_scaleInputH{0};
        int32_t m_scaleInputOffX{0}, m_scaleInputOffY{0};
        int32_t m_posOffCompH{0}, m_posOffCompV{0};

        AVFilterGraph* m_rotateFg{nullptr};
        AVFilterContext* m_rotateInputCtx{nullptr};
        AVFilterContext* m_rotateOutputCtx{nullptr};
        uint32_t m_rotInW{0}, m_rotInH{0};
    };
}