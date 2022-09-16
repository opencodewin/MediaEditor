#pragma once
#include "VidoeTransformFilter.h"
#include <warpAffine_vulkan.h>

namespace DataLayer
{
    class VideoTransformFilter_VulkanImpl : public VideoTransformFilter
    {
    public:
        bool Initialize(uint32_t outWidth, uint32_t outHeight, const std::string& outputFormat) override;
        bool SetScaleType(ScaleType type) override;
        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override;
        bool SetPositionOffsetH(int32_t value) override;
        bool SetPositionOffsetV(int32_t value) override;
        bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) override;
        bool SetCropMarginL(uint32_t value) override;
        bool SetCropMarginT(uint32_t value) override;
        bool SetCropMarginR(uint32_t value) override;
        bool SetCropMarginB(uint32_t value) override;
        bool SetRotationAngle(double angle) override;
        bool SetScaleH(double scale) override;
        bool SetScaleV(double scale) override;
        bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override;

        uint32_t GetInWidth() const override;
        uint32_t GetInHeight() const override;
        uint32_t GetOutWidth() const override;
        uint32_t GetOutHeight() const override;
        std::string GetOutputPixelFormat() const override;
        ScaleType GetScaleType() const override;
        int32_t GetPositionOffsetH() const override;
        int32_t GetPositionOffsetV() const override;
        uint32_t GetCropMarginL() const override;
        uint32_t GetCropMarginT() const override;
        uint32_t GetCropMarginR() const override;
        uint32_t GetCropMarginB() const override;
        double GetRotationAngle() const override;
        double GetScaleH() const override;
        double GetScaleV() const override;
        ImGui::KeyPointEditor* GetKeyPoint() override;

        std::string GetError() const override
        { return m_errMsg; }

    private:
        ImGui::warpAffine_vulkan m_warpAffine;
        std::string m_errMsg;
    };
}