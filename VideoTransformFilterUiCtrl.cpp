#include <cmath>
#include <vector>
#include <limits>
#include <sstream>
#include <imgui_helper.h>
#include "VideoTransformFilterUiCtrl.h"

#define ICON_ROTATE         u8"\ue437"

using namespace std;
using namespace ImGui;
using namespace MediaCore;
using namespace Logger;

namespace MEC
{
VideoTransformFilterUiCtrl::VideoTransformFilterUiCtrl(VideoTransformFilter::Holder hTransformFilter)
    : m_hTransformFilter(hTransformFilter), m_aUiCornerPoints(4)
{
    m_pLogger = GetLogger("VTransFilterUiCtrl");
    m_u32GradLineColor = IM_COL32(190, 190, 120, 160);
    m_u32ResizeGrabberColor = IM_COL32(240, 100, 100, 160);
    m_u32ResizeGrabberHoveredColor = IM_COL32(210, 190, 190, 255);
    m_u32CropGrabberColor = IM_COL32(80, 80, 240, 160);
    m_u32CropGrabberHoveredColor = IM_COL32(190, 190, 210, 255);
    m_u32RotationGrabberColor = IM_COL32(100, 240, 100, 160);
    m_u32RotationGrabberHoveredColor = IM_COL32(190, 210, 190, 255);
    m_u32GrabberBorderColor = IM_COL32(190, 190, 120, 220);
    m_u32GrabberBorderHoveredColor = IM_COL32(240, 240, 220, 255);
}

bool VideoTransformFilterUiCtrl::Draw(const ImVec2& v2ViewPos, const ImVec2& v2ViewSize, const ImVec2& v2ImageViewPos, const ImVec2& v2ImageViewSize, int64_t i64Tick, bool* pParamChanged)
{
    const auto v2CursorPos = GetCursorPos();
    if (m_bNeedUpdateCornerPoints)
    {
        if (!m_hTransformFilter->CalcCornerPoints(i64Tick, m_aImageCornerPoints))
        {
            m_pLogger->Log(Error) << "Invoke 'VideoTransformFilter::CalcCornerPoints()' FAILED!" << endl;
            return false;
        }
    }

    // apply UI scale
    const ImVec2 v2UiScale(v2ImageViewSize.x/(float)m_hTransformFilter->GetOutWidth(), v2ImageViewSize.y/(float)m_hTransformFilter->GetOutHeight());
    m_aUiCornerPoints[0] = m_aImageCornerPoints[0]*v2UiScale;  // top left
    m_aUiCornerPoints[1] = m_aImageCornerPoints[1]*v2UiScale;  // top right
    m_aUiCornerPoints[2] = m_aImageCornerPoints[2]*v2UiScale;  // bottom right
    m_aUiCornerPoints[3] = m_aImageCornerPoints[3]*v2UiScale;  // bottom left
    // transfer orgin from image view center to normal ui origin
    const ImVec2 v2ImageViewCenter(v2ImageViewPos.x+v2ImageViewSize.x/2, v2ImageViewPos.y+v2ImageViewSize.y/2);
    m_aUiCornerPoints[0] += v2ImageViewCenter; m_aUiCornerPoints[1] += v2ImageViewCenter;
    m_aUiCornerPoints[2] += v2ImageViewCenter; m_aUiCornerPoints[3] += v2ImageViewCenter;
    const ImVec2 v2UiImageCenter = (m_aUiCornerPoints[0]+m_aUiCornerPoints[2])/2;

    // draw grad lines
    vector<ImVec2> aEdgeCenters(4);  // left, top, right, bottom
    aEdgeCenters[0] = (m_aUiCornerPoints[0]+m_aUiCornerPoints[3])/2;
    aEdgeCenters[1] = (m_aUiCornerPoints[0]+m_aUiCornerPoints[1])/2;
    aEdgeCenters[2] = (m_aUiCornerPoints[1]+m_aUiCornerPoints[2])/2;
    aEdgeCenters[3] = (m_aUiCornerPoints[2]+m_aUiCornerPoints[3])/2;
    auto pDrawList = GetWindowDrawList();
    const auto v2MousePos = GetMousePos();
    const ImRect rViewArea(v2ViewPos, v2ViewPos+v2ViewSize);

    // left edge
    pDrawList->AddLine(m_aUiCornerPoints[0], m_aUiCornerPoints[3], m_u32GradLineColor, m_fGradLineThickness);
    // top edge
    pDrawList->AddLine(m_aUiCornerPoints[0], m_aUiCornerPoints[1], m_u32GradLineColor, m_fGradLineThickness);
    // right edge
    pDrawList->AddLine(m_aUiCornerPoints[1], m_aUiCornerPoints[2], m_u32GradLineColor, m_fGradLineThickness);
    // bottom edge
    pDrawList->AddLine(m_aUiCornerPoints[2], m_aUiCornerPoints[3], m_u32GradLineColor, m_fGradLineThickness);
    // draw horizontal cross line
    pDrawList->AddLine(aEdgeCenters[0], aEdgeCenters[2], m_u32GradLineColor, m_fGradLineThickness);
    // draw vertical cross line
    pDrawList->AddLine(aEdgeCenters[1], aEdgeCenters[3], m_u32GradLineColor, m_fGradLineThickness);

    HandleType eHoveredHandleType = m_ePrevHandleType;
    bool bIsHovering = m_ePrevHandleType != HT_NONE;
    auto bIsMouseDown = IsMouseDown(ImGuiMouseButton_Left);
    const auto fMouseDownDur = GetIO().MouseDownDuration[ImGuiMouseButton_Left];
    // if mouse is clicked outside of the curve area, then dragging into this area. Ignore the mouse down event
    if (m_ePrevHandleType == HT_NONE && bIsMouseDown && fMouseDownDur > 0.f)
    {
        bIsMouseDown = false;
        bIsHovering = true;
    }
    const auto fRotationAngle = m_hTransformFilter->GetRotation();
    // draw resize grabbers
    auto fRadiusWithBorder = m_fResizeGrabberRadius+m_fGrabberBorderThickness;
    auto fHoverDetectRadius = fRadiusWithBorder+m_fGrabberHoverDetectExtendRadius;
    ImVec2 v2DetectRectQuad(fHoverDetectRadius, fHoverDetectRadius);
    // top left
    auto v2GrabberCenter = m_aUiCornerPoints[0];
    ImRect rDetectRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bool bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    auto u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, fRadiusWithBorder, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32ResizeGrabberHoveredColor : m_u32ResizeGrabberColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, m_fResizeGrabberRadius, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_TOP_LEFT; }
    // top right
    v2GrabberCenter = m_aUiCornerPoints[1];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, fRadiusWithBorder, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32ResizeGrabberHoveredColor : m_u32ResizeGrabberColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, m_fResizeGrabberRadius, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_TOP_RIGHT; }
    // bottom right
    v2GrabberCenter = m_aUiCornerPoints[2];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, fRadiusWithBorder, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32ResizeGrabberHoveredColor : m_u32ResizeGrabberColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, m_fResizeGrabberRadius, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_BOTTOM_RIGHT; }
    // bottom left
    v2GrabberCenter = m_aUiCornerPoints[3];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, fRadiusWithBorder, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32ResizeGrabberHoveredColor : m_u32ResizeGrabberColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, m_fResizeGrabberRadius, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_BOTTOM_LEFT; }

    // draw crop grabbers
    fRadiusWithBorder = m_fCropGrabberRadius+m_fGrabberBorderThickness;
    fHoverDetectRadius = fRadiusWithBorder+m_fGrabberHoverDetectExtendRadius;
    ImVec2 v2RectGrabberQuad(fRadiusWithBorder, fRadiusWithBorder);
    v2DetectRectQuad = ImVec2(fHoverDetectRadius, fHoverDetectRadius);
    auto fRotationRadian = (-fRotationAngle+45)*M_PI/180.f;
    float fSinA = sin(fRotationRadian);
    float fCosA = cos(fRotationRadian);
    const auto fCropInnerRectOffsetSin = fSinA*m_fCropGrabberRadius;
    const auto fCropInnerRectOffsetCos = fCosA*m_fCropGrabberRadius;
    const auto fCropOutterRectOffsetSin = fSinA*fRadiusWithBorder;
    const auto fCropOutterRectOffsetCos = fCosA*fRadiusWithBorder;
    const ImVec2 aCropRectInnerOffset[4] = {
        {fCropInnerRectOffsetSin, fCropInnerRectOffsetCos}, {fCropInnerRectOffsetCos, -fCropInnerRectOffsetSin},
        {-fCropInnerRectOffsetSin, -fCropInnerRectOffsetCos}, {-fCropInnerRectOffsetCos, fCropInnerRectOffsetSin}};
    const ImVec2 aCropRectOutterOffset[4] = {
        {fCropOutterRectOffsetSin, fCropOutterRectOffsetCos}, {fCropOutterRectOffsetCos, -fCropOutterRectOffsetSin},
        {-fCropOutterRectOffsetSin, -fCropOutterRectOffsetCos}, {-fCropOutterRectOffsetCos, fCropOutterRectOffsetSin}};
    ImVec2 aCropRectPointsCoord[4];
    // left
    v2GrabberCenter = aEdgeCenters[0];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectOutterOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectOutterOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectOutterOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectOutterOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32CropGrabberHoveredColor : m_u32CropGrabberColor;
    v2RectGrabberQuad = ImVec2(m_fCropGrabberRadius, m_fCropGrabberRadius);
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectInnerOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectInnerOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectInnerOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectInnerOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_LEFT; }
    // top
    v2GrabberCenter = aEdgeCenters[1];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectOutterOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectOutterOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectOutterOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectOutterOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32CropGrabberHoveredColor : m_u32CropGrabberColor;
    v2RectGrabberQuad = ImVec2(m_fCropGrabberRadius, m_fCropGrabberRadius);
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectInnerOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectInnerOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectInnerOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectInnerOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_TOP; }
    // right
    v2GrabberCenter = aEdgeCenters[2];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectOutterOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectOutterOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectOutterOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectOutterOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32CropGrabberHoveredColor : m_u32CropGrabberColor;
    v2RectGrabberQuad = ImVec2(m_fCropGrabberRadius, m_fCropGrabberRadius);
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectInnerOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectInnerOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectInnerOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectInnerOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_RIGHT; }
    // bottom
    v2GrabberCenter = aEdgeCenters[3];
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectOutterOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectOutterOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectOutterOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectOutterOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32CropGrabberHoveredColor : m_u32CropGrabberColor;
    v2RectGrabberQuad = ImVec2(m_fCropGrabberRadius, m_fCropGrabberRadius);
    aCropRectPointsCoord[0] = v2GrabberCenter+aCropRectInnerOffset[0]; aCropRectPointsCoord[1] = v2GrabberCenter+aCropRectInnerOffset[1];
    aCropRectPointsCoord[2] = v2GrabberCenter+aCropRectInnerOffset[2]; aCropRectPointsCoord[3] = v2GrabberCenter+aCropRectInnerOffset[3];
    pDrawList->AddConvexPolyFilled(aCropRectPointsCoord, 4, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_BOTTOM; }

    // draw rotation grabber
    // draw rotation indicator line
    if (m_ePrevHandleType == HT_CENTER && v2MousePos != v2UiImageCenter)
    {
        pDrawList->AddLine(v2UiImageCenter, v2MousePos, m_u32RotationGrabberColor, m_fGradLineThickness+1);
    }
    fRadiusWithBorder = m_fRotationGrabberRadius+m_fGrabberBorderThickness;
    fHoverDetectRadius = fRadiusWithBorder+m_fGrabberHoverDetectExtendRadius;
    v2DetectRectQuad = ImVec2(fHoverDetectRadius, fHoverDetectRadius);
    v2GrabberCenter = v2UiImageCenter;
    rDetectRect = ImRect(v2GrabberCenter-v2DetectRectQuad, v2GrabberCenter+v2DetectRectQuad);
    bIsWidgetHovered = !bIsHovering && rDetectRect.Contains(v2MousePos);
    u32WidgetColor = bIsWidgetHovered ? m_u32GrabberBorderHoveredColor : m_u32GrabberBorderColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, fRadiusWithBorder, u32WidgetColor);
    u32WidgetColor = bIsWidgetHovered ? m_u32RotationGrabberHoveredColor : m_u32RotationGrabberColor;
    pDrawList->AddCircleFilled(v2GrabberCenter, m_fRotationGrabberRadius, u32WidgetColor);
    if (bIsWidgetHovered)
    { bIsHovering = true; eHoveredHandleType = HT_CENTER; }

    // check if mouse is inside the image
    if (!bIsHovering)
    {
        if (CheckPointInsidePolygon(v2MousePos, m_aUiCornerPoints))
        { bIsHovering = true; eHoveredHandleType = HT_AREA; }
    }
    // check if mouse is outside of the view area
    if (!rViewArea.Contains(v2MousePos) && m_ePrevHandleType == HT_NONE)
    {
        bIsHovering = false;
        eHoveredHandleType = HT_NONE;
    }

    // render customized mouse shape according to hovered handle type
    if (eHoveredHandleType != HT_NONE)
    {
        switch (eHoveredHandleType)
        {
        case HT_TOP_LEFT:
        case HT_BOTTOM_RIGHT:
            RenderMouseCursor(ICON_FA_ARROWS_UP_DOWN, ImVec2(4, 8), 1.f, fRotationAngle-45);
            break;
        case HT_TOP_RIGHT:
        case HT_BOTTOM_LEFT:
            RenderMouseCursor(ICON_FA_ARROWS_LEFT_RIGHT, ImVec2(8, 8), 1.f, fRotationAngle-45);
            break;
        case HT_LEFT:
        case HT_RIGHT:
            RenderMouseCursor(ICON_FA_ARROWS_LEFT_RIGHT, ImVec2(8, 8), 1.f, fRotationAngle);
            break;
        case HT_TOP:
        case HT_BOTTOM:
            RenderMouseCursor(ICON_FA_ARROWS_UP_DOWN, ImVec2(4, 8), 1.f, fRotationAngle);
            break;
        case HT_CENTER:
            RenderMouseCursor(ICON_ROTATE, ImVec2(8, 8), 1.f, fRotationAngle);
            break;
        case HT_AREA:
            RenderMouseCursor(ICON_FA_UP_DOWN_LEFT_RIGHT, ImVec2(8, 8), 1.f);
            break;
        default:
            break;
        }
    }

    bool bMouseCaptured = false;
    bool bParamChanged = false;
    // handle mouse dragging
    if (bIsMouseDown && bIsHovering)
    {
        const auto v2MouseMoveDelta = m_ePrevHandleType == HT_NONE ? ImVec2(0, 0) : v2MousePos-m_v2PrevMousePos;
        // m_pLogger->Log(WARN) << "---> v2MouseMoveDelta=(" << v2MouseMoveDelta.x << ", " << v2MouseMoveDelta.y << ")" << endl;
        m_v2PrevMousePos = v2MousePos;

        if (m_ePrevHandleType == HT_NONE)
        {
            m_v2OpBeginMousePos = v2MousePos;
            if (eHoveredHandleType == HT_LEFT)
            {
                m_fOpBeginParamVal = m_hTransformFilter->GetCropRatioL(i64Tick);
            }
            else if (eHoveredHandleType == HT_TOP)
            {
                m_fOpBeginParamVal = m_hTransformFilter->GetCropRatioT(i64Tick);
            }
            else if (eHoveredHandleType == HT_RIGHT)
            {
                m_fOpBeginParamVal = m_hTransformFilter->GetCropRatioR(i64Tick);
            }
            else if (eHoveredHandleType == HT_BOTTOM)
            {
                m_fOpBeginParamVal = m_hTransformFilter->GetCropRatioB(i64Tick);
            }
            else if (eHoveredHandleType == HT_TOP_LEFT)
            {
                m_v2OpBeginHandlePos = m_aImageCornerPoints[0];
                m_v2OpBeginAnchorPos = m_aImageCornerPoints[2];
            }
            else if (eHoveredHandleType == HT_TOP_RIGHT)
            {
                m_v2OpBeginHandlePos = m_aImageCornerPoints[1];
                m_v2OpBeginAnchorPos = m_aImageCornerPoints[3];
            }
            else if (eHoveredHandleType == HT_BOTTOM_RIGHT)
            {
                m_v2OpBeginHandlePos = m_aImageCornerPoints[2];
                m_v2OpBeginAnchorPos = m_aImageCornerPoints[0];
            }
            else if (eHoveredHandleType == HT_BOTTOM_LEFT)
            {
                m_v2OpBeginHandlePos = m_aImageCornerPoints[3];
                m_v2OpBeginAnchorPos = m_aImageCornerPoints[1];
            }
            else if (eHoveredHandleType == HT_CENTER)
            {
                m_fOpBeginParamVal = m_hTransformFilter->GetRotation(i64Tick);
            }
        }
        m_ePrevHandleType = eHoveredHandleType;

        if (v2MouseMoveDelta.x != 0 || v2MouseMoveDelta.y != 0)
        {
            if (eHoveredHandleType == HT_AREA)
            {
                const auto v2RevUiScaledMoveDelta = v2MouseMoveDelta/v2UiScale;
                if (!m_hTransformFilter->ChangePosOffset(i64Tick, v2RevUiScaledMoveDelta.x, v2RevUiScaledMoveDelta.y, &bParamChanged))
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangePosOffset()' with arguments: i64Tick=" << i64Tick
                            << ", i32DeltaX=" << v2RevUiScaledMoveDelta.x << ", i32DeltaY=" << v2RevUiScaledMoveDelta.y << "!" << endl;
            }
            else if (eHoveredHandleType == HT_LEFT)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto fMoveDist = sqrt(v2MouseMovementInOp.x*v2MouseMovementInOp.x+v2MouseMovementInOp.y*v2MouseMovementInOp.y);
                auto fMoveRadian = asin(v2MouseMovementInOp.y/fMoveDist); if (v2MouseMovementInOp.x < 0) fMoveRadian = M_PI-fMoveRadian;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fCosA = cos(fMoveRadian-fRotationRadian);
                const auto fCropLDelta = fCosA*fMoveDist;
                const auto v2FinalScale = m_hTransformFilter->GetFinalScale(i64Tick);
                const auto fNewCropRatioL = m_fOpBeginParamVal+fCropLDelta/(m_hTransformFilter->GetInWidth()*v2FinalScale.x);
                if (!m_hTransformFilter->SetCropRatioL(i64Tick, fNewCropRatioL, true, &bParamChanged))
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::SetCropRatioL()' with arguments: i64Tick=" << i64Tick
                            << ", fNewCropRatioL=" << fNewCropRatioL << "!" << endl;
            }
            else if (eHoveredHandleType == HT_TOP)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto fMoveDist = sqrt(v2MouseMovementInOp.x*v2MouseMovementInOp.x+v2MouseMovementInOp.y*v2MouseMovementInOp.y);
                auto fMoveRadian = asin(v2MouseMovementInOp.y/fMoveDist); if (v2MouseMovementInOp.x < 0) fMoveRadian = M_PI-fMoveRadian;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fSinA = sin(fMoveRadian-fRotationRadian);
                const auto fCropTDelta = fSinA*fMoveDist;
                const auto v2FinalScale = m_hTransformFilter->GetFinalScale(i64Tick);
                const auto fNewCropRatioT = m_fOpBeginParamVal+fCropTDelta/(m_hTransformFilter->GetInHeight()*v2FinalScale.y);
                if (!m_hTransformFilter->SetCropRatioT(i64Tick, fNewCropRatioT, true, &bParamChanged))
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::SetCropRatioT()' with arguments: i64Tick=" << i64Tick
                            << ", fNewCropRatioT=" << fNewCropRatioT << "!" << endl;
            }
            else if (eHoveredHandleType == HT_RIGHT)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto fMoveDist = sqrt(v2MouseMovementInOp.x*v2MouseMovementInOp.x+v2MouseMovementInOp.y*v2MouseMovementInOp.y);
                auto fMoveRadian = asin(v2MouseMovementInOp.y/fMoveDist); if (v2MouseMovementInOp.x < 0) fMoveRadian = M_PI-fMoveRadian;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fCosA = cos(fMoveRadian-fRotationRadian);
                const auto fCropRDelta = -fCosA*fMoveDist;
                const auto v2FinalScale = m_hTransformFilter->GetFinalScale(i64Tick);
                const auto fNewCropRatioR = m_fOpBeginParamVal+fCropRDelta/(m_hTransformFilter->GetInWidth()*v2FinalScale.x);
                if (!m_hTransformFilter->SetCropRatioR(i64Tick, fNewCropRatioR, true, &bParamChanged))
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::SetCropRatioR()' with arguments: i64Tick=" << i64Tick
                            << ", fNewCropRatioR=" << fNewCropRatioR << "!" << endl;
            }
            else if (eHoveredHandleType == HT_BOTTOM)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto fMoveDist = sqrt(v2MouseMovementInOp.x*v2MouseMovementInOp.x+v2MouseMovementInOp.y*v2MouseMovementInOp.y);
                auto fMoveRadian = asin(v2MouseMovementInOp.y/fMoveDist); if (v2MouseMovementInOp.x < 0) fMoveRadian = M_PI-fMoveRadian;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fSinA = sin(fMoveRadian-fRotationRadian);
                const auto fCropBDelta = -fSinA*fMoveDist;
                const auto v2FinalScale = m_hTransformFilter->GetFinalScale(i64Tick);
                const auto fNewCropRatioB = m_fOpBeginParamVal+fCropBDelta/(m_hTransformFilter->GetInHeight()*v2FinalScale.y);
                if (!m_hTransformFilter->SetCropRatioB(i64Tick, fNewCropRatioB, true, &bParamChanged))
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::SetCropRatioB()' with arguments: i64Tick=" << i64Tick
                            << ", fNewCropRatioB=" << fNewCropRatioB << "!" << endl;
            }
            else if (eHoveredHandleType == HT_TOP_LEFT)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto v2NewTopLeft = m_v2OpBeginHandlePos+v2MouseMovementInOp;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fSinA = sin(fRotationRadian);
                fCosA = cos(fRotationRadian);
                const auto v2XyDelta = m_v2OpBeginAnchorPos-v2NewTopLeft;
                auto fNewWidth = fCosA*v2XyDelta.x+fSinA*v2XyDelta.y; if (fNewWidth < 0) fNewWidth = 0;
                auto fNewHeight = -fSinA*v2XyDelta.x+fCosA*v2XyDelta.y; if (fNewHeight < 0) fNewHeight = 0;
                if (m_hTransformFilter->ChangeScaleToFitOutputSize(i64Tick, fNewWidth, fNewHeight, &bParamChanged))
                {
                    bParamChanged = true;
                    ImVec2 aNewCornerPoints[4];
                    m_hTransformFilter->CalcCornerPoints(i64Tick, aNewCornerPoints);
                    const auto v2PosOffsetDelta = m_v2OpBeginAnchorPos-aNewCornerPoints[2];
                    if (!m_hTransformFilter->ChangePosOffset(i64Tick, v2PosOffsetDelta.x, v2PosOffsetDelta.y))
                        m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangePosOffset()' [Scale by TopLeft] with arguments: i64Tick=" << i64Tick
                                << ", i32DeltaX=" << v2PosOffsetDelta.x << ", i32DeltaY=" << v2PosOffsetDelta.y << "!" << endl;
                }
                else
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangeScaleToFitOutputSize()' with arguments: i64Tick=" << i64Tick
                            << ", fNewWidth=" << fNewWidth << ", fNewHeight=" << fNewHeight << "!" << endl;
            }
            else if (eHoveredHandleType == HT_TOP_RIGHT)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto v2NewTopRight = m_v2OpBeginHandlePos+v2MouseMovementInOp;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fSinA = sin(fRotationRadian);
                fCosA = cos(fRotationRadian);
                const auto v2XyDelta = m_v2OpBeginAnchorPos-v2NewTopRight;
                auto fNewWidth = -fCosA*v2XyDelta.x-fSinA*v2XyDelta.y; if (fNewWidth < 0) fNewWidth = 0;
                auto fNewHeight = -fSinA*v2XyDelta.x+fCosA*v2XyDelta.y; if (fNewHeight < 0) fNewHeight = 0;
                if (m_hTransformFilter->ChangeScaleToFitOutputSize(i64Tick, fNewWidth, fNewHeight, &bParamChanged))
                {
                    bParamChanged = true;
                    ImVec2 aNewCornerPoints[4];
                    m_hTransformFilter->CalcCornerPoints(i64Tick, aNewCornerPoints);
                    const auto v2PosOffsetDelta = m_v2OpBeginAnchorPos-aNewCornerPoints[3];
                    if (!m_hTransformFilter->ChangePosOffset(i64Tick, v2PosOffsetDelta.x, v2PosOffsetDelta.y))
                        m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangePosOffset()' [Scale by TopRight] with arguments: i64Tick=" << i64Tick
                                << ", i32DeltaX=" << v2PosOffsetDelta.x << ", i32DeltaY=" << v2PosOffsetDelta.y << "!" << endl;
                }
                else
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangeScaleToFitOutputSize()' with arguments: i64Tick=" << i64Tick
                            << ", fNewWidth=" << fNewWidth << ", fNewHeight=" << fNewHeight << "!" << endl;
            }
            else if (eHoveredHandleType == HT_BOTTOM_RIGHT)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto v2NewTopRight = m_v2OpBeginHandlePos+v2MouseMovementInOp;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fSinA = sin(fRotationRadian);
                fCosA = cos(fRotationRadian);
                const auto v2XyDelta = m_v2OpBeginAnchorPos-v2NewTopRight;
                auto fNewWidth = -fCosA*v2XyDelta.x-fSinA*v2XyDelta.y; if (fNewWidth < 0) fNewWidth = 0;
                auto fNewHeight = fSinA*v2XyDelta.x-fCosA*v2XyDelta.y; if (fNewHeight < 0) fNewHeight = 0;
                if (m_hTransformFilter->ChangeScaleToFitOutputSize(i64Tick, fNewWidth, fNewHeight, &bParamChanged))
                {
                    bParamChanged = true;
                    ImVec2 aNewCornerPoints[4];
                    m_hTransformFilter->CalcCornerPoints(i64Tick, aNewCornerPoints);
                    const auto v2PosOffsetDelta = m_v2OpBeginAnchorPos-aNewCornerPoints[0];
                    if (!m_hTransformFilter->ChangePosOffset(i64Tick, v2PosOffsetDelta.x, v2PosOffsetDelta.y))
                        m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangePosOffset()' [Scale by BottomRight] with arguments: i64Tick=" << i64Tick
                                << ", i32DeltaX=" << v2PosOffsetDelta.x << ", i32DeltaY=" << v2PosOffsetDelta.y << "!" << endl;
                }
                else
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangeScaleToFitOutputSize()' with arguments: i64Tick=" << i64Tick
                            << ", fNewWidth=" << fNewWidth << ", fNewHeight=" << fNewHeight << "!" << endl;
            }
            else if (eHoveredHandleType == HT_BOTTOM_LEFT)
            {
                const auto v2MouseMovementInOp = (v2MousePos-m_v2OpBeginMousePos)/v2UiScale;
                const auto v2NewTopRight = m_v2OpBeginHandlePos+v2MouseMovementInOp;
                fRotationRadian = fRotationAngle*M_PI/180.f;
                fSinA = sin(fRotationRadian);
                fCosA = cos(fRotationRadian);
                const auto v2XyDelta = m_v2OpBeginAnchorPos-v2NewTopRight;
                auto fNewWidth = fCosA*v2XyDelta.x+fSinA*v2XyDelta.y; if (fNewWidth < 0) fNewWidth = 0;
                auto fNewHeight = fSinA*v2XyDelta.x-fCosA*v2XyDelta.y; if (fNewHeight < 0) fNewHeight = 0;
                if (m_hTransformFilter->ChangeScaleToFitOutputSize(i64Tick, fNewWidth, fNewHeight, &bParamChanged))
                {
                    bParamChanged = true;
                    ImVec2 aNewCornerPoints[4];
                    m_hTransformFilter->CalcCornerPoints(i64Tick, aNewCornerPoints);
                    const auto v2PosOffsetDelta = m_v2OpBeginAnchorPos-aNewCornerPoints[1];
                    if (!m_hTransformFilter->ChangePosOffset(i64Tick, v2PosOffsetDelta.x, v2PosOffsetDelta.y))
                        m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangePosOffset()' [Scale by BottomLeft] with arguments: i64Tick=" << i64Tick
                                << ", i32DeltaX=" << v2PosOffsetDelta.x << ", i32DeltaY=" << v2PosOffsetDelta.y << "!" << endl;
                }
                else
                    m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::ChangeScaleToFitOutputSize()' with arguments: i64Tick=" << i64Tick
                            << ", fNewWidth=" << fNewWidth << ", fNewHeight=" << fNewHeight << "!" << endl;
            }
            else if (eHoveredHandleType == HT_CENTER)
            {
                if (v2MousePos != v2UiImageCenter)
                {
                    const auto v2MouseMovement = v2MousePos-v2UiImageCenter;
                    auto fMoveVectorRadian = atan2(v2MouseMovement.y, v2MouseMovement.x);
                    if (isinf(m_v2OpBeginAnchorPos.x))
                    {
                        m_v2OpBeginAnchorPos.x = fMoveVectorRadian;
                    }
                    else
                    {
                        const auto fMoveVectorAngleOffset = (fMoveVectorRadian-m_v2OpBeginAnchorPos.x)*180.f/M_PI;
                        const auto fNewRotationAngle = m_fOpBeginParamVal+fMoveVectorAngleOffset;
                        if (m_hTransformFilter->SetRotation(i64Tick, fNewRotationAngle, &bParamChanged) && bParamChanged)
                        {
                            ImVec2 aNewCornerPoints[4];
                            m_hTransformFilter->CalcCornerPoints(i64Tick, aNewCornerPoints);
                            const auto v2NewCenter = (aNewCornerPoints[0]+aNewCornerPoints[2])/2;
                            const auto v2OldCenter = (m_aImageCornerPoints[0]+m_aImageCornerPoints[2])/2;
                            if (v2NewCenter != v2OldCenter)
                            {
                                const auto v2MoveOffset = v2OldCenter-v2NewCenter;
                                m_hTransformFilter->ChangePosOffset(i64Tick, v2MoveOffset.x, v2MoveOffset.y);
                            }
                        }
                        else
                            m_pLogger->Log(Error) << "FAILED to invoke 'VideoTransformFilter::SetRotation()' with arguments: i64Tick=" << i64Tick
                                    << ", fNewRotationAngle=" << fNewRotationAngle << "!" << endl;
                    }
                }
            }
        }
        bMouseCaptured = true;
    }
    else if (!bIsMouseDown)
    {
        m_ePrevHandleType = HT_NONE;
        m_v2OpBeginAnchorPos.x = numeric_limits<float>::infinity();
    }

    if (pParamChanged)
        *pParamChanged = bParamChanged;
    return bMouseCaptured;
}
}
