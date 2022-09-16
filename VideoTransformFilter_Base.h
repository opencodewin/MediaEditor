#pragma once
#include "VideoTransformFilter.h"
#include <mutex>

namespace DataLayer
{
    class VideoTransformFilter_Base : public VideoTransformFilter
    {
    public:
        uint32_t GetInWidth() const override
        { return m_inWidth; }

        uint32_t GetInHeight() const override
        { return m_inHeight; }

        uint32_t GetOutWidth() const override
        { return m_outWidth; }

        uint32_t GetOutHeight() const override
        { return m_outHeight; }

        ScaleType GetScaleType() const override
        { return m_scaleType; }

        int32_t GetPositionOffsetH() const override
        { return m_posOffsetH; }

        int32_t GetPositionOffsetV() const override
        { return m_posOffsetV; }

        uint32_t GetCropMarginL() const override
        { return m_cropL; }

        uint32_t GetCropMarginT() const override
        { return m_cropT; }

        uint32_t GetCropMarginR() const override
        { return m_cropR; }

        uint32_t GetCropMarginB() const override
        { return m_cropB; }

        double GetRotationAngle() const override
        { return m_rotateAngle; }

        double GetScaleH() const override
        { return m_scaleRatioH; }

        double GetScaleV() const override
        { return m_scaleRatioV; }

        ImGui::KeyPointEditor* GetKeyPoint() override
        { return &m_keyPoints; }

        bool SetScaleType(ScaleType type) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (type < SCALE_TYPE__FIT || type > SCALE_TYPE__STRETCH)
            {
                m_errMsg = "INVALID argument 'type'!";
                return false;
            }
            if (m_scaleType == type)
                return true;
            m_scaleType = type;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_posOffsetH == offsetH && m_posOffsetV == offsetV)
                return true;
            m_posOffsetH = offsetH;
            m_posOffsetV = offsetV;
            m_needUpdatePositionParam = true;
            return true;
        }

        bool SetPositionOffsetH(int32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_posOffsetH == value)
                return true;
            m_posOffsetH = value;
            m_needUpdatePositionParam = true;
            return true;
        }

        bool SetPositionOffsetV(int32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_posOffsetV == value)
                return true;
            m_posOffsetV = value;
            m_needUpdatePositionParam = true;
            return true;
        }

        bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropL == left && m_cropT == top && m_cropR == right && m_cropB == bottom)
                return true;
            m_cropL = left;
            m_cropT = top;
            m_cropR = right;
            m_cropB = bottom;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginL(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropL == value)
                return true;
            m_cropL = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginT(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropT == value)
                return true;
            m_cropT = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginR(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropR == value)
                return true;
            m_cropR = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginB(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropB == value)
                return true;
            m_cropB = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetRotationAngle(double angle) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            int32_t n = (int32_t)trunc(angle/360);
            angle -= n*360;
            if (m_rotateAngle == angle)
                return true;
            m_rotateAngle = angle;
            m_needUpdateRotateParam = true;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetScaleH(double scale) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_scaleRatioH == scale)
                return true;
            m_scaleRatioH = scale;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetScaleV(double scale) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_scaleRatioV == scale)
                return true;
            m_scaleRatioV = scale;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            m_keyPoints = keypoint;
            return true;
        }

        void ApplyTo(VideoClip* clip) override
        {}

        VideoFilterHolder Clone() override
        { return nullptr; }

        std::string GetError() const override
        { return m_errMsg; }

    protected:
        uint32_t m_inWidth{0}, m_inHeight{0};
        uint32_t m_outWidth{0}, m_outHeight{0};
        ScaleType m_scaleType{SCALE_TYPE__FIT};
        uint32_t m_cropL{0}, m_cropR{0}, m_cropT{0}, m_cropB{0};
        uint32_t m_cropRectX{0}, m_cropRectY{0}, m_cropRectW{0}, m_cropRectH{0};
        double m_scaleRatioH{1}, m_scaleRatioV{1};
        double m_rotateAngle{0};
        int32_t m_posOffsetH{0}, m_posOffsetV{0};
        ImGui::KeyPointEditor m_keyPoints;
        std::recursive_mutex m_processLock;
        bool m_needUpdateScaleParam{true};
        bool m_needUpdatePositionParam{false};
        bool m_needUpdateCropParam{false};
        bool m_needUpdateRotateParam{false};
        std::string m_errMsg;
    };
}