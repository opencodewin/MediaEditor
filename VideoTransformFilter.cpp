#include "VideoTransformFilter.h"
#include "VideoTransformFilter_FFImpl.h"
#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include "VideoTransformFilter_VulkanImpl.h"
#endif

namespace DataLayer
{
    class VideoTransformFilter_Delegate : public VideoTransformFilter
    {
    public:
        VideoTransformFilter_Delegate()
        {
#if IMGUI_VULKAN_SHADER
            if (m_useVulkan)
                m_filter = &m_vkImpl;
            else
                m_filter = &m_ffImpl;
#else
            m_filter = &m_ffImpl;
#endif
        }

        const std::string GetFilterName() const override
        {
            return m_filter->GetFilterName();
        }

        VideoFilterHolder Clone() override
        {
            return m_filter->Clone();
        }

        void ApplyTo(VideoClip* clip) override
        {
            m_filter->ApplyTo(clip);
        }

        ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override
        {
            return m_filter->FilterImage(vmat, pos);
        }

        bool Initialize(uint32_t outWidth, uint32_t outHeight) override
        {
            return m_filter->Initialize(outWidth, outHeight);
        }

        bool SetOutputFormat(const std::string& outputFormat) override
        {
            return m_filter->SetOutputFormat(outputFormat);
        }

        bool SetScaleType(ScaleType type) override
        {
            return m_filter->SetScaleType(type);
        }

        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override
        {
            return m_filter->SetPositionOffset(offsetH, offsetV);
        }

        bool SetPositionOffsetH(int32_t value) override
        {
            return m_filter->SetPositionOffsetH(value);
        }

        bool SetPositionOffsetV(int32_t value) override
        {
            return m_filter->SetPositionOffsetV(value);
        }

        bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) override
        {
            return m_filter->SetCropMargin(left, top, right, bottom);
        }

        bool SetCropMarginL(uint32_t value) override
        {
            return m_filter->SetCropMarginL(value);
        }

        bool SetCropMarginT(uint32_t value) override
        {
            return m_filter->SetCropMarginT(value);
        }

        bool SetCropMarginR(uint32_t value) override
        {
            return m_filter->SetCropMarginR(value);
        }

        bool SetCropMarginB(uint32_t value) override
        {
            return m_filter->SetCropMarginB(value);
        }

        bool SetRotationAngle(double angle) override
        {
            return m_filter->SetRotationAngle(angle);
        }

        bool SetScaleH(double scale) override
        {
            return m_filter->SetScaleH(scale);
        }

        bool SetScaleV(double scale) override
        {
            return m_filter->SetScaleV(scale);
        }

        bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override
        {
            return m_filter->SetKeyPoint(keypoint);
        }

        uint32_t GetInWidth() const override
        {
            return m_filter->GetInWidth();
        }

        uint32_t GetInHeight() const override
        {
            return m_filter->GetInHeight();
        }

        uint32_t GetOutWidth() const override
        {
            return m_filter->GetOutWidth();
        }

        uint32_t GetOutHeight() const override
        {
            return m_filter->GetOutHeight();
        }

        std::string GetOutputFormat() const override
        {
            return m_filter->GetOutputFormat();
        }

        ScaleType GetScaleType() const override
        {
            return m_filter->GetScaleType();
        }

        int32_t GetPositionOffsetH() const override
        {
            return m_filter->GetPositionOffsetH();
        }

        int32_t GetPositionOffsetV() const override
        {
            return m_filter->GetPositionOffsetV();
        }

        uint32_t GetCropMarginL() const override
        {
            return m_filter->GetCropMarginL();
        }

        uint32_t GetCropMarginT() const override
        {
            return m_filter->GetCropMarginT();
        }

        uint32_t GetCropMarginR() const override
        {
            return m_filter->GetCropMarginR();
        }

        uint32_t GetCropMarginB() const override
        {
            return m_filter->GetCropMarginB();
        }

        double GetRotationAngle() const override
        {
            return m_filter->GetRotationAngle();
        }

        double GetScaleH() const override
        {
            return m_filter->GetScaleH();
        }

        double GetScaleV() const override
        {
            return m_filter->GetScaleV();
        }

        ImGui::KeyPointEditor* GetKeyPoint() override
        {
            return m_filter->GetKeyPoint();
        }

        std::string GetError() const override
        {
            return m_filter->GetError();
        }

    private:
        VideoTransformFilter* m_filter;
        VideoTransformFilter_FFImpl m_ffImpl;
#if IMGUI_VULKAN_SHADER
        VideoTransformFilter_VulkanImpl m_vkImpl;
        bool m_useVulkan{true};
#endif
    };

    VideoTransformFilter* NewVideoTransformFilter()
    {
        return new VideoTransformFilter_Delegate();
    }
}
