#pragma once
#include <vector>
#include <imgui.h>
#include <Logger.h>
#include <MediaCore/VideoTransformFilter.h>

namespace MEC
{
class VideoTransformFilterUiCtrl
{
public:
    VideoTransformFilterUiCtrl(MediaCore::VideoTransformFilter::Holder hTransformFilter);

    VideoTransformFilterUiCtrl() = delete;
    VideoTransformFilterUiCtrl(const VideoTransformFilterUiCtrl&) = delete;

    bool Draw(const ImVec2& v2ViewPos, const ImVec2& v2ViewSize, const ImVec2& v2ImageViewPos, const ImVec2& v2ImageViewSize, int64_t i64Tick, bool* pParamChanged);

    enum HandleType : int
    {
        HT_NONE = 0,
        HT_TOP_LEFT,
        HT_TOP,
        HT_TOP_RIGHT,
        HT_LEFT,
        HT_CENTER,
        HT_RIGHT,
        HT_BOTTOM_LEFT,
        HT_BOTTOM,
        HT_BOTTOM_RIGHT,
        HT_AREA,
    };

    void SetLogLevel(Logger::Level l)
    { m_pLogger->SetShowLevels(l); }

private:
    Logger::ALogger* m_pLogger;
    MediaCore::VideoTransformFilter::Holder m_hTransformFilter;
    ImVec2 m_aImageCornerPoints[4];
    std::vector<ImVec2> m_aUiCornerPoints;
    bool m_bNeedUpdateCornerPoints{true};
    ImU32 m_u32GradLineColor;
    float m_fGradLineThickness{1.5f};
    ImU32 m_u32ResizeGrabberColor, m_u32ResizeGrabberHoveredColor;
    float m_fResizeGrabberRadius{5.f};
    ImU32 m_u32CropGrabberColor, m_u32CropGrabberHoveredColor;
    float m_fCropGrabberRadius{5.f}, m_fCropGrabberRounding{2.f};
    ImU32 m_u32RotationGrabberColor, m_u32RotationGrabberHoveredColor;
    float m_fRotationGrabberRadius{5.f};
    ImU32 m_u32GrabberBorderColor, m_u32GrabberBorderHoveredColor;
    float m_fGrabberBorderThickness{1.5};
    float m_fGrabberHoverDetectExtendRadius{5.f};
    ImVec2 m_v2PrevMousePos;
    ImVec2 m_v2OpBeginHandlePos, m_v2OpBeginAnchorPos, m_v2OpBeginMousePos;
    float m_fOpBeginParamVal;
    HandleType m_ePrevHandleType{HT_NONE};
};
}
