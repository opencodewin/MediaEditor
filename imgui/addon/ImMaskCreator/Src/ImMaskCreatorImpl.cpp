#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <list>
#include <vector>
#include <utility>
#include <unordered_map>
#include <cmath>
#include <limits>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <chrono>
#include <imgui_internal.h>
#include <imgui_helper.h>
#include "ImNewCurve.h"
#include "ImMaskCreator.h"
#include "Contour2Mask.h"
#include "MatMath.h"
#include "MatFilter.h"
#include "MatUtilsImVecHelper.h"
#include "CpuUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
namespace json = imgui_json;
namespace LibCurve = ImGui::ImNewCurve;

namespace ImGui
{
class MaskCreatorImpl : public MaskCreator
{
public:
    MaskCreatorImpl(const MatUtils::Size2i& size, const string& name) : m_szMaskSize(size), m_strMaskName(name), m_tMorphCtrl(this)
    {
        m_pLogger = GetLogger("MaskCreator");
        m_v2PointSizeHalf = m_v2PointSize/2;
        m_itMorphCtrlVt = m_itHoveredVertex = m_itSelectedVertex = m_aRoutePointsForUi.end();
        m_aUiWarpAffineMatrix[0][0] = 1; m_aUiWarpAffineMatrix[0][1] = 0; m_aUiWarpAffineMatrix[0][2] = 0;
        m_aUiWarpAffineMatrix[1][0] = 0; m_aUiWarpAffineMatrix[1][1] = 1; m_aUiWarpAffineMatrix[1][2] = 0;
        m_aUiRevWarpAffineMatrix[0][0] = 1; m_aUiRevWarpAffineMatrix[0][1] = 0; m_aUiRevWarpAffineMatrix[0][2] = 0;
        m_aUiRevWarpAffineMatrix[1][0] = 0; m_aUiRevWarpAffineMatrix[1][1] = 1; m_aUiRevWarpAffineMatrix[1][2] = 0;
        m_aMaskWarpAffineMatrix[0][0] = 1; m_aMaskWarpAffineMatrix[0][1] = 0; m_aMaskWarpAffineMatrix[0][2] = 0;
        m_aMaskWarpAffineMatrix[1][0] = 0; m_aMaskWarpAffineMatrix[1][1] = 1; m_aMaskWarpAffineMatrix[1][2] = 0;
        m_aMaskRevWarpAffineMatrix[0][0] = 1; m_aMaskRevWarpAffineMatrix[0][1] = 0; m_aMaskRevWarpAffineMatrix[0][2] = 0;
        m_aMaskRevWarpAffineMatrix[1][0] = 0; m_aMaskRevWarpAffineMatrix[1][1] = 1; m_aMaskRevWarpAffineMatrix[1][2] = 0;
    }

    string GetName() const override
    {
        return m_strMaskName;
    }

    void SetName(const string& name) override
    {
        m_strMaskName = name;
    }

    bool DrawContent(const ImVec2& v2Pos, const ImVec2& v2ViewSize, bool bEditable, int64_t i64Tick) override
    {
        ImRect bb(v2Pos, v2Pos+v2ViewSize);
        if (!ItemAdd(bb, 0))
        {
            ostringstream oss; oss << "DrawContent: FAILED at 'ItemAdd' with item rect (("
                    << bb.Min.x << ", " << bb.Min.y << "), (" << bb.Max.x << ", " << bb.Max.y << "))!";
            m_sErrMsg = oss.str();
            return false;
        }
        m_rWorkArea = bb;
        const ImVec2 v2UiScaleNew(v2ViewSize.x/(float)m_szMaskSize.x, v2ViewSize.y/(float)m_szMaskSize.y);
        if (m_v2UiScale != v2UiScaleNew || m_bUiWarpAffineMatrixChanged || m_bMaskWarpAffineMatrixChanged)
        {
            const auto& fScaleX = v2UiScaleNew.x;
            const auto& fScaleY = v2UiScaleNew.y;
            float (*A)[3] = m_aUiWarpAffineMatrix, (*B)[3] = m_aMaskWarpAffineMatrix;
            m_aUiFinalWarpAffineMatrix[0][0] = fScaleX*(A[0][0]*B[0][0]+A[0][1]*B[1][0]);
            m_aUiFinalWarpAffineMatrix[0][1] = fScaleX*(A[0][0]*B[0][1]+A[0][1]*B[1][1]);
            m_aUiFinalWarpAffineMatrix[0][2] = fScaleX*(A[0][0]*B[0][2]+A[0][1]*B[1][2]+A[0][2]);
            m_aUiFinalWarpAffineMatrix[1][0] = fScaleY*(A[1][0]*B[0][0]+A[1][1]*B[1][0]);
            m_aUiFinalWarpAffineMatrix[1][1] = fScaleY*(A[1][0]*B[0][1]+A[1][1]*B[1][1]);
            m_aUiFinalWarpAffineMatrix[1][2] = fScaleY*(A[1][0]*B[0][2]+A[1][1]*B[1][2]+A[1][2]);
            const auto fRevScaleX = 1/v2UiScaleNew.x;
            const auto fRevScaleY = 1/v2UiScaleNew.y;
            A = m_aUiRevWarpAffineMatrix; B = m_aMaskRevWarpAffineMatrix;
            m_aUiFinalRevWarpAffineMatrix[0][0] = fRevScaleX*(A[0][0]*B[0][0]+A[1][0]*B[0][1]);
            m_aUiFinalRevWarpAffineMatrix[0][1] = fRevScaleY*(A[0][1]*B[0][0]+A[1][1]*B[0][1]);
            m_aUiFinalRevWarpAffineMatrix[0][2] = A[0][2]*B[0][0]+A[1][2]*B[0][1]+B[0][2];
            m_aUiFinalRevWarpAffineMatrix[1][0] = fRevScaleX*(A[0][0]*B[1][0]+A[1][0]*B[1][1]);
            m_aUiFinalRevWarpAffineMatrix[1][1] = fRevScaleY*(A[0][1]*B[1][0]+A[1][1]*B[1][1]);
            m_aUiFinalRevWarpAffineMatrix[1][2] = A[0][2]*B[1][0]+A[1][2]*B[1][1]+B[1][2];
        }
        m_v2UiScale = v2UiScaleNew;
        m_bUiWarpAffineMatrixChanged = false;
        m_bMaskWarpAffineMatrixChanged = false;

        if (m_bKeyFrameEnabled && UpdateContourByKeyFrame(i64Tick, true))
            m_bRouteChanged = true;

        // save hovering state for log
        int iPrevHoverType = HasHoveredVertex() ? m_itHoveredVertex->m_iHoverType : -1;
        auto itPrevHoveredVertex = m_itHoveredVertex;

        // reactions
        const auto v2MousePosAbs = GetMousePos();
        const ImVec2 v2MousePos = FromUiPos(v2MousePosAbs);
        const bool bMouseInWorkArea = m_rWorkArea.Contains(v2MousePosAbs);
        const bool bRemoveKeyDown = IsKeyDown(m_eRemoveVertexKey);
        const bool bInsertKeyDown = IsKeyDown(m_eInsertVertexKey);
        const bool bScaleKeyDown = IsKeyDown(m_eScaleContourKey);
        const bool bIsMouseClicked = IsMouseClicked(ImGuiMouseButton_Left);
        const bool bIsMouseDown = IsMouseDown(ImGuiMouseButton_Left);
        const bool bIsMouseDragging = IsMouseDragging(ImGuiMouseButton_Left);
        bool bMorphChanged = false;
        if (bEditable)
        {
            // check hovering state
            if (!IsMouseDown(ImGuiMouseButton_Left))
            {
                auto iter = m_aRoutePointsForUi.begin();
                while (iter != m_aRoutePointsForUi.end())
                {
                    auto& v = *iter;
                    if (v.CheckGrabberHovering(v2MousePos))
                        break;
                    iter++;
                }
                if (iter != m_aRoutePointsForUi.end())
                {
                    if (HasHoveredVertex() && m_itHoveredVertex != iter)
                        m_itHoveredVertex->QuitHover();
                    if (HasHoveredMorphCtrl())
                        m_tMorphCtrl.QuitHover();
                    m_itHoveredVertex = iter;
                }
                if ((!HasHoveredVertex() || HasHoveredContour()) && IsMorphCtrlShown())
                {
                    if (m_tMorphCtrl.CheckHovering(v2MousePos))
                    {
                        if (HasHoveredContour())
                        {
                            m_itHoveredVertex->QuitHover();
                            m_itHoveredVertex = m_aRoutePointsForUi.end();
                        }
                    }
                }
                if (!HasHoveredSomething())
                {
                    iter = m_aRoutePointsForUi.begin();
                    while (iter != m_aRoutePointsForUi.end())
                    {
                        auto& v = *iter;
                        if (v.CheckContourHovering(v2MousePos))
                            break;
                        iter++;
                    }
                    if (iter != m_aRoutePointsForUi.end())
                        m_itHoveredVertex = iter;
                }
            }

            // set mouse cursor
            if (bMouseInWorkArea)
            {
                if (m_bInScaleMode && !bScaleKeyDown)
                    m_bInScaleMode = false;

                if (m_bInScaleMode)
                    SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                else if (!bIsMouseDragging && bRemoveKeyDown)
                    SetMouseCursor(ImGuiMouseCursor_Minus);
                else if (HasHoveredVertex() && m_itHoveredVertex->m_iHoverType == 4)
                {
                    if (!bIsMouseDragging && bInsertKeyDown)
                        SetMouseCursor(ImGuiMouseCursor_Add);
                    else if (!bIsMouseDown && bScaleKeyDown)
                    {
                        m_bInScaleMode = true;
                        SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    }
                    else
                        SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                }
            }

            lock_guard<mutex> _lk(m_mtxRouteLock);
            if (m_bInScaleMode)
            {
                const auto fMouseWheel = GetIO().MouseWheel;
                if (fMouseWheel < -FLT_EPSILON)
                {
                    ScaleRoute(0.95);
                    m_bRouteChanged = true;
                }
                else if (fMouseWheel > FLT_EPSILON)
                {
                    ScaleRoute(1.05);
                    m_bRouteChanged = true;
                }
            }
            else if (bMouseInWorkArea && bIsMouseClicked)
            {
                if (!bRemoveKeyDown && !HasHoveredVertex() && !m_bRouteCompleted)
                {
                    // add new vertex to the end of vertex list
                    m_aRoutePointsForUi.push_back({this, v2MousePos});
                    auto iter = m_aRoutePointsForUi.end();
                    iter--;
                    m_itHoveredVertex = iter;
                    SelectVertex(iter);
                    iter->UpdateGrabberContainBox();
                    UpdateEdgeVertices(m_aRoutePointsForUi, m_itHoveredVertex);
                    m_bRouteChanged = true;
                }
                else if (HasHoveredVertex())
                {
                    if (bRemoveKeyDown)
                    {
                        if (m_itHoveredVertex->m_iHoverType == 0)
                        {
                            // remove vertex
                            bool bUpdateMorphCtrlIter = m_itHoveredVertex == m_itMorphCtrlVt;
                            auto itNextVt = m_aRoutePointsForUi.erase(m_itHoveredVertex);
                            m_itHoveredVertex = m_aRoutePointsForUi.end();
                            if (m_aRoutePointsForUi.size() <= 2)
                            {
                                m_bRouteCompleted = false;
                                m_itMorphCtrlVt = m_aRoutePointsForUi.end();
                            }
                            if (m_bRouteCompleted && itNextVt == m_aRoutePointsForUi.end())
                                itNextVt = m_aRoutePointsForUi.begin();
                            if (itNextVt != m_aRoutePointsForUi.end())
                            {
                                if (bUpdateMorphCtrlIter)
                                    m_itMorphCtrlVt = itNextVt;
                                UpdateEdgeVertices(m_aRoutePointsForUi, itNextVt);
                                m_bRouteChanged = true;
                            }
                        }
                        else
                        {
                            // disable bezier curve
                            auto& v = *m_itHoveredVertex;
                            v.m_bEnableBezier = false;
                            v.m_bFirstDrag = true;
                            v.m_v2Grabber0Offset = {0.f, 0.f};
                            v.m_v2Grabber1Offset = {0.f, 0.f};
                            v.UpdateGrabberContainBox();
                            UpdateEdgeVertices(m_aRoutePointsForUi, m_itHoveredVertex);
                            m_bRouteChanged = true;
                        }
                    }
                    else if (!m_bRouteCompleted && m_itHoveredVertex == m_aRoutePointsForUi.begin() && m_itHoveredVertex->m_iHoverType == 0 && m_aRoutePointsForUi.size() >= 3)
                    {
                        // complete the contour
                        m_bRouteCompleted = true;
                        auto iterVt = m_aRoutePointsForUi.begin();
                        iterVt++;
                        m_itMorphCtrlVt = iterVt;
                        UpdateEdgeVertices(m_aRoutePointsForUi, m_aRoutePointsForUi.begin());
                        m_bRouteChanged = true;
                    }
                    else if (m_itHoveredVertex->m_iHoverType == 4)
                    {
                        if (bInsertKeyDown)
                        {
                            // insert new vertex on the contour
                            bool bNeedUpdateMorphCtrlIter = m_itHoveredVertex == m_itMorphCtrlVt;
                            m_itHoveredVertex = m_aRoutePointsForUi.insert(m_itHoveredVertex, {this, m_itHoveredVertex->m_v2HoverPointOnContour});
                            if (bNeedUpdateMorphCtrlIter) m_itMorphCtrlVt = m_itHoveredVertex;
                            m_itHoveredVertex->UpdateGrabberContainBox();
                            SelectVertex(m_itHoveredVertex);
                            UpdateEdgeVertices(m_aRoutePointsForUi, m_itHoveredVertex);
                            m_bRouteChanged = true;
                        }
                        else
                        {
                            m_itHoveredVertex->m_v2MoveOriginMousePos = v2MousePos;
                        }
                    }
                    else
                    {
                        SelectVertex(m_itHoveredVertex);
                    }
                }
            }
            if (bIsMouseDragging)
            {
                if (HasHoveredVertex())
                {
                    ImGui::SetNextFrameWantCaptureMouse(true);
                    if (m_itHoveredVertex->m_iHoverType < 4)
                    {
                        if (m_itHoveredVertex->m_bJustAdded || !m_itHoveredVertex->m_bEnableBezier && IsKeyDown(m_eEnableBezierKey))
                        {
                            // enable bezier curve
                            m_itHoveredVertex->m_bEnableBezier = true;
                            m_itHoveredVertex->m_iHoverType = 2;
                        }
                        bool bNeedUpdateContour = false;
                        const auto v2CpPos = m_bKeyFrameEnabled ? MatUtils::ToImVec2(m_itHoveredVertex->m_ptCurrPos) : m_itHoveredVertex->m_v2Pos;
                        const auto v2Grabber0Offset = m_bKeyFrameEnabled ? MatUtils::ToImVec2(m_itHoveredVertex->m_ptCurrGrabber0Offset) : m_itHoveredVertex->m_v2Grabber0Offset;
                        const auto v2Grabber1Offset = m_bKeyFrameEnabled ? MatUtils::ToImVec2(m_itHoveredVertex->m_ptCurrGrabber1Offset) : m_itHoveredVertex->m_v2Grabber1Offset;
                        const auto coordOff = v2MousePos-v2CpPos;
                        if (m_itHoveredVertex->m_bFirstDrag && m_itHoveredVertex->m_bEnableBezier && m_itHoveredVertex->m_v2Grabber1Offset != coordOff)
                        {
                            // 1st-time dragging bezier grabber, change both the grabbers
                            m_itHoveredVertex->m_v2Grabber0Offset = -coordOff;
                            m_itHoveredVertex->m_v2Grabber1Offset = coordOff;
                            if (m_bKeyFrameEnabled)
                            {
                                m_itHoveredVertex->m_ptCurrGrabber0Offset = MatUtils::FromImVec2<float>(-coordOff);
                                m_itHoveredVertex->m_ptCurrGrabber1Offset = MatUtils::FromImVec2<float>(coordOff);
                                LibCurve::KeyPoint::ValType tKpVal(-coordOff.x, -coordOff.y, 0, 0);
                                auto hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(1, hKp);
                                if (i64Tick > 0)
                                {
                                    tKpVal.w = i64Tick;
                                    auto hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                    m_itHoveredVertex->AddKeyPoint(1, hKp);
                                }
                                tKpVal.x = coordOff.x; tKpVal.y = coordOff.y; tKpVal.w = 0;
                                hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(2, hKp);
                                if (i64Tick > 0)
                                {
                                    tKpVal.w = i64Tick;
                                    auto hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                    m_itHoveredVertex->AddKeyPoint(2, hKp);
                                }
                            }
                            bNeedUpdateContour = true;
                        }
                        else if (m_itHoveredVertex->m_iHoverType == 0 && bMouseInWorkArea && m_itHoveredVertex->m_v2Pos != v2MousePos)
                        {
                            // moving the vertex
                            if (!m_bKeyFrameEnabled || i64Tick <= 0)
                                m_itHoveredVertex->m_v2Pos = v2MousePos;
                            if (m_bKeyFrameEnabled)
                            {
                                const LibCurve::KeyPoint::ValType tKpVal(v2MousePos.x, v2MousePos.y, 0, i64Tick);
                                auto hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(0, hKp);
                                m_itHoveredVertex->m_ptCurrPos = MatUtils::FromImVec2<float>(v2MousePos);
                                // const auto strKpPosX = m_itHoveredVertex->m_ahCurves[0]->PrintKeyPointsByDim(LibCurve::DIM_X);
                                // const auto strKpPosY = m_itHoveredVertex->m_ahCurves[0]->PrintKeyPointsByDim(LibCurve::DIM_Y);
                                // m_pLogger->Log(DEBUG) << "Pos X:" << strKpPosX << ", Y:" << strKpPosY << endl;
                            }
                            bNeedUpdateContour = true;
                        }
                        else if (m_itHoveredVertex->m_iHoverType == 1 && m_itHoveredVertex->m_v2Grabber0Offset != coordOff)
                        {
                            // moving bezier grabber0
                            if (!m_bKeyFrameEnabled || i64Tick <= 0)
                            {
                                m_itHoveredVertex->m_v2Grabber0Offset = coordOff;
                                auto c0sqr = coordOff.x*coordOff.x+coordOff.y*coordOff.y;
                                auto& coordOff1 = m_itHoveredVertex->m_v2Grabber1Offset;
                                auto c1sqr = coordOff1.x*coordOff1.x+coordOff1.y*coordOff1.y;
                                auto ratio = sqrt(c1sqr/c0sqr);
                                coordOff1.x = -coordOff.x*ratio;
                                coordOff1.y = -coordOff.y*ratio;
                            }
                            if (m_bKeyFrameEnabled)
                            {
                                LibCurve::KeyPoint::ValType tKpVal(coordOff.x, coordOff.y, 0, i64Tick);
                                auto hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(1, hKp);
                                m_itHoveredVertex->m_ptCurrGrabber0Offset = MatUtils::FromImVec2<float>(coordOff);
                                auto c0sqr = coordOff.x*coordOff.x+coordOff.y*coordOff.y;
                                const auto& coordOff1 = m_itHoveredVertex->m_ptCurrGrabber1Offset;
                                auto c1sqr = coordOff1.x*coordOff1.x+coordOff1.y*coordOff1.y;
                                auto ratio = sqrt(c1sqr/c0sqr);
                                tKpVal.x = -coordOff.x*ratio;
                                tKpVal.y = -coordOff.y*ratio;
                                hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(2, hKp);
                                m_itHoveredVertex->m_ptCurrGrabber1Offset = MatUtils::Point2f(tKpVal.x, tKpVal.y);
                                // const auto strKpPosX = m_itHoveredVertex->m_ahCurves[1]->PrintKeyPointsByDim(LibCurve::DIM_X);
                                // const auto strKpPosY = m_itHoveredVertex->m_ahCurves[1]->PrintKeyPointsByDim(LibCurve::DIM_Y);
                                // m_pLogger->Log(DEBUG) << "G0 X:" << strKpPosX << ", Y:" << strKpPosY << endl;
                            }
                            bNeedUpdateContour = true;
                        }
                        else if (m_itHoveredVertex->m_iHoverType == 2 && m_itHoveredVertex->m_v2Grabber1Offset != coordOff)
                        {
                            // moving bezier grabber1
                            if (!m_bKeyFrameEnabled || i64Tick <= 0)
                            {
                                m_itHoveredVertex->m_v2Grabber1Offset = coordOff;
                                auto c1sqr = coordOff.x*coordOff.x+coordOff.y*coordOff.y;
                                auto& coordOff0 = m_itHoveredVertex->m_v2Grabber0Offset;
                                auto c0sqr = coordOff0.x*coordOff0.x+coordOff0.y*coordOff0.y;
                                auto ratio = sqrt(c0sqr/c1sqr);
                                coordOff0.x = -coordOff.x*ratio;
                                coordOff0.y = -coordOff.y*ratio;
                            }
                            if (m_bKeyFrameEnabled)
                            {
                                LibCurve::KeyPoint::ValType tKpVal(coordOff.x, coordOff.y, 0, i64Tick);
                                auto hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(2, hKp);
                                m_itHoveredVertex->m_ptCurrGrabber1Offset = MatUtils::FromImVec2<float>(coordOff);
                                auto c1sqr = coordOff.x*coordOff.x+coordOff.y*coordOff.y;
                                const auto& coordOff0 = m_itHoveredVertex->m_ptCurrGrabber0Offset;
                                auto c0sqr = coordOff0.x*coordOff0.x+coordOff0.y*coordOff0.y;
                                auto ratio = sqrt(c0sqr/c1sqr);
                                tKpVal.x = -coordOff.x*ratio;
                                tKpVal.y = -coordOff.y*ratio;
                                hKp = LibCurve::KeyPoint::CreateInstance(tKpVal);
                                m_itHoveredVertex->AddKeyPoint(1, hKp);
                                m_itHoveredVertex->m_ptCurrGrabber0Offset = MatUtils::Point2f(tKpVal.x, tKpVal.y);
                                // const auto strKpPosX = m_itHoveredVertex->m_ahCurves[2]->PrintKeyPointsByDim(LibCurve::DIM_X);
                                // const auto strKpPosY = m_itHoveredVertex->m_ahCurves[2]->PrintKeyPointsByDim(LibCurve::DIM_Y);
                                // m_pLogger->Log(DEBUG) << "G1 X:" << strKpPosX << ", Y:" << strKpPosY << endl;
                            }
                            bNeedUpdateContour = true;
                        }
                        if (bNeedUpdateContour)
                        {
                            m_itHoveredVertex->UpdateGrabberContainBox();
                            UpdateEdgeVertices(m_aRoutePointsForUi, m_itHoveredVertex);
                            m_bRouteChanged = true;
                        }
                    }
                    else if (m_itHoveredVertex->m_iHoverType == 4)
                    {
                        if (v2MousePos != m_itHoveredVertex->m_v2MoveOriginMousePos)
                        {
                            const auto v2MoveOffset = v2MousePos-m_itHoveredVertex->m_v2MoveOriginMousePos;
                            MoveRoute(v2MoveOffset, i64Tick);
                            m_bRouteChanged = true;
                            m_itHoveredVertex->m_v2MoveOriginMousePos = v2MousePos;
                        }
                    }
                }
                else if (HasHoveredMorphCtrl())
                {
                    // moving morph controller's root position
                    if (m_tMorphCtrl.m_iHoverType == 0)
                    {
                        ImVec2 v2VertSlope;
                        int iLineIdx = 0;
                        auto ptRootPos = MatUtils::CalcNearestPointOnPloygon(v2MousePos, m_aContourVerticesForUi, &v2VertSlope, &iLineIdx);
                        const auto fCtrlSlope = v2VertSlope.y == 0 ? numeric_limits<float>::infinity() : v2VertSlope.x/v2VertSlope.y;
                        m_itMorphCtrlVt = m_tMorphCtrl.SetPosAndSlope(ptRootPos, fCtrlSlope, iLineIdx);
                    }
                    else if (m_tMorphCtrl.m_iHoverType == 1)
                    {
                        const auto dx = v2MousePos.x-m_tMorphCtrl.m_ptRootPos.x;
                        const auto dy = v2MousePos.y-m_tMorphCtrl.m_ptRootPos.y;
                        float l = sqrt(dx*dx+dy*dy);
                        if (l < MorphController::MIN_CTRL_LENGTH) l = MorphController::MIN_CTRL_LENGTH;
                        if (m_tMorphCtrl.m_fMorphCtrlLength != l)
                        {
                            m_tMorphCtrl.m_fMorphCtrlLength = l;
                            auto iMorphIters = (int)floor(l-MorphController::MIN_CTRL_LENGTH);
                            if (iMorphIters != m_tMorphCtrl.m_iMorphIterations)
                            {
                                m_tMorphCtrl.m_iMorphIterations = iMorphIters;
                                bMorphChanged = true;
                            }
                            const auto& ptOrgRootPos = m_tMorphCtrl.m_ptRootPos;
                            const auto& ptOrgGrabberPos = m_tMorphCtrl.m_ptGrabberPos;
                            const auto& fVertSlope = isinf(m_tMorphCtrl.m_fCtrlSlope) ? 0 : m_tMorphCtrl.m_fCtrlSlope == 0 ? numeric_limits<float>::infinity() : -1/m_tMorphCtrl.m_fCtrlSlope;
                            bool bGrabberSide = isinf(fVertSlope) ? ptOrgGrabberPos.x > ptOrgRootPos.x : ptOrgGrabberPos.y > fVertSlope*(ptOrgGrabberPos.x-ptOrgRootPos.x)+ptOrgRootPos.y;
                            bool bMousePosSide = isinf(fVertSlope) ? v2MousePos.x > ptOrgRootPos.x : v2MousePos.y > fVertSlope*(v2MousePos.x-ptOrgRootPos.x)+ptOrgRootPos.y;
                            if (bGrabberSide != bMousePosSide)
                            {
                                m_tMorphCtrl.m_bInsidePoly = !m_tMorphCtrl.m_bInsidePoly;
                                bMorphChanged = true;
                            }
                            m_tMorphCtrl.CalcGrabberPos();
                        }
                    }
                    else if (m_tMorphCtrl.m_iHoverType == 2)
                    {
                        const auto dx = v2MousePos.x-m_tMorphCtrl.m_ptRootPos.x;
                        const auto dy = v2MousePos.y-m_tMorphCtrl.m_ptRootPos.y;
                        float l = sqrt(dx*dx+dy*dy);
                        const auto fMinFeatherGrabberLength = m_tMorphCtrl.m_fMorphCtrlLength+m_fGrabberRadius*2+m_fHoverDetectExRadius;
                        float fFeatherGrabberLength = l-fMinFeatherGrabberLength;
                        if (fFeatherGrabberLength < 0) fFeatherGrabberLength = 0;
                        if (m_tMorphCtrl.m_fFeatherCtrlLength != fFeatherGrabberLength)
                        {
                            m_tMorphCtrl.m_fFeatherCtrlLength = fFeatherGrabberLength;
                            m_tMorphCtrl.m_iFeatherIterations = (int)floor(fFeatherGrabberLength);
                            bMorphChanged = true;
                            m_tMorphCtrl.CalcGrabberPos();
                        }
                    }
                }
            }
            else if (!IsMouseDown(ImGuiMouseButton_Left))
            {
                if (HasHoveredVertex())
                {
                    if (!m_itHoveredVertex->CheckHovering(v2MousePos))
                    {
                        // quit hovering state
                        m_itHoveredVertex->QuitHover();
                        m_itHoveredVertex = m_aRoutePointsForUi.end();
                    }
                }
                else if (HasHoveredMorphCtrl())
                {
                    if (!m_tMorphCtrl.CheckHovering(v2MousePos))
                    {
                        m_tMorphCtrl.m_bIsHovered = false;
                    }
                }
            }
            if (IsMouseReleased(ImGuiMouseButton_Left) && HasHoveredVertex())
            {
                ImGui::SetNextFrameWantCaptureMouse(false);
                if (m_itHoveredVertex->m_bEnableBezier)
                    m_itHoveredVertex->m_bFirstDrag = false;
                m_itHoveredVertex->m_bJustAdded = false;
            }
        }
        else
        {
            if (HasHoveredContour())
            {
                m_itHoveredVertex->QuitHover();
                m_itHoveredVertex = m_aRoutePointsForUi.end();
            }
            if (HasHoveredMorphCtrl())
            {
                m_tMorphCtrl.QuitHover();
                m_itMorphCtrlVt = m_aRoutePointsForUi.end();
            }
        }

        // log hovering state change
        int iCurrHoverType = HasHoveredVertex() ? m_itHoveredVertex->m_iHoverType : -1;
        bool bHoverStateChanged = iPrevHoverType != iCurrHoverType || itPrevHoveredVertex != m_itHoveredVertex;
        if (bHoverStateChanged)
        {
            m_pLogger->Log(DEBUG) << "Hover state changed: ";
            if (HasHoveredVertex())
            {
                if (m_bKeyFrameEnabled)
                {
                    const auto& rpPos = m_itHoveredVertex->m_ptCurrPos;
                    const auto& rpG0Off = m_itHoveredVertex->m_ptCurrGrabber0Offset;
                    const auto& rpG1Off = m_itHoveredVertex->m_ptCurrGrabber1Offset;
                    m_pLogger->Log(DEBUG) << "RoutePoint(" << rpPos.x << ", " << rpPos.y << "):G0Off(" << rpG0Off.x << ", " << rpG0Off.y
                            << "):G1Off(" << rpG1Off.x << ", " << rpG1Off.y << "), HoverType=" << m_itHoveredVertex->m_iHoverType << "." << endl;
                }
                else
                {
                    const auto& rpPos = m_itHoveredVertex->m_v2Pos;
                    const auto& rpG0Off = m_itHoveredVertex->m_v2Grabber0Offset;
                    const auto& rpG1Off = m_itHoveredVertex->m_v2Grabber1Offset;
                    m_pLogger->Log(DEBUG) << "RoutePoint(" << rpPos.x << ", " << rpPos.y << "):G0Off(" << rpG0Off.x << ", " << rpG0Off.y
                            << "):G1Off(" << rpG1Off.x << ", " << rpG1Off.y << "), HoverType=" << m_itHoveredVertex->m_iHoverType << "." << endl;
                }
            }
            else
            {
                m_pLogger->Log(DEBUG) << "NO hovered object." << endl;
            }
        }

        bool bContourChanged = false;
        if (m_bRouteChanged)
        {
            // refresh contour vertices
            list<ImVec2> aContourVertices;
            if (m_aRoutePointsForUi.size() > 1)
            {
                bool b1stVertex = true;
                auto itRp = m_aRoutePointsForUi.begin();
                while (itRp != m_aRoutePointsForUi.end())
                {
                    if (!m_bRouteCompleted && itRp == m_aRoutePointsForUi.begin())
                    {
                        itRp++;
                        continue;
                    }
                    const auto& v = *itRp++;
                    if (!v.m_aEdgeVertices.empty())
                    {
                        auto itVt0 = v.m_aEdgeVertices.begin();
                        auto itVt1 = itVt0; itVt1++;
                        while (itVt1 != v.m_aEdgeVertices.end())
                        {
                            aContourVertices.push_back(*itVt0++);
                            itVt1++;
                        }
                    }
                }
                if (!m_bRouteCompleted)
                    aContourVertices.push_back(m_aRoutePointsForUi.back().m_v2Pos);
            }
            m_aContourVerticesForUi = aContourVertices;
            CalcMorphCtrlPos();
            UpdateContainBox();

            m_bRouteChanged = false;
            m_bRouteNeedSync = true;
            bContourChanged = true;
        }

        // draw mask trajectory
        ImDrawList* pDrawList = GetWindowDrawList();
        // draw background color if contour is not completed yet
        if (!m_bRouteCompleted)
        {
            const ImU32 u32BgColor{IM_COL32(0, 0, 0, 100)};
            pDrawList->AddRectFilled(bb.Min, bb.Max, u32BgColor);
        }
        {
            auto pointIter = m_aRoutePointsForUi.begin();
            auto prevIter = m_aRoutePointsForUi.end();
            // draw contour
            while (pointIter != m_aRoutePointsForUi.end())
            {
                if (prevIter != m_aRoutePointsForUi.end())
                    DrawContour(pDrawList, *prevIter, *pointIter);
                prevIter = pointIter++;
            }
            if (m_bRouteCompleted)
                DrawContour(pDrawList, *prevIter, m_aRoutePointsForUi.front());
            // draw points
            for (const auto& p : m_aRoutePointsForUi)
                DrawPoint(pDrawList, p);
            if (bEditable)
            {
                // draw morph controller
                if (IsMorphCtrlShown())
                    DrawMorphController(pDrawList);
            }
            // draw hovering point on contour
            if (!bIsMouseDragging && HasHoveredVertex() && m_itHoveredVertex->m_iHoverType == 4 && bInsertKeyDown)
            {
                const auto pointPos = ToUiPos(m_itHoveredVertex->m_v2HoverPointOnContour);
                const auto& offsetSize1 = m_v2PointSizeHalf;
                pDrawList->AddRectFilled(pointPos-offsetSize1, pointPos+offsetSize1, m_u32PointBorderHoverColor);
                const ImVec2 offsetSize2(m_v2PointSizeHalf.x-m_fPointBorderThickness, m_v2PointSizeHalf.y-m_fPointBorderThickness);
                pDrawList->AddRectFilled(pointPos-offsetSize2, pointPos+offsetSize2, m_u32ContourHoverPointColor);
            }
        }
        return m_bRouteCompleted && (bContourChanged || bMorphChanged);
    }

    bool DrawContourPointKeyFrames(int64_t& i64Tick, const RoutePoint* ptContourPoint, uint32_t u32Width) override
    {
        if (!m_bKeyFrameEnabled)
            return false;
        list<RoutePointImpl>::iterator itTargetCp = m_aRoutePointsForUi.end();
        if (ptContourPoint)
            itTargetCp = find_if(m_aRoutePointsForUi.begin(), m_aRoutePointsForUi.end(), [ptContourPoint] (const auto& elem) {
                return ptContourPoint == static_cast<const RoutePoint*>(&elem);
            });

        ostringstream oss; oss << setw(4) << setfill(' ') << "N/A" << "," << setw(4) << setfill(' ') << "N/A";
        string strNA = oss.str();
        const ImVec2 v2PaddingUnit(5.f, 2.f);
        auto v2PointCoordTextSize = CalcTextSize(strNA.c_str());

        const auto v2MousePosAbs = GetMousePos();
        const auto v2ViewPos = GetCursorScreenPos();
        const auto v2MousePos = v2MousePosAbs-v2ViewPos;
        const auto v2AvailSize = GetContentRegionAvail();
        ImVec2 v2ViewSize(u32Width == 0 ? v2AvailSize.x : u32Width, v2PointCoordTextSize.y+v2PaddingUnit.y*2);
        auto v2CursorPos = GetCursorScreenPos();
        ImDrawList* pDrawList = GetWindowDrawList();

        if (itTargetCp != m_aRoutePointsForUi.end())
        {
            // draw point coordinates
            string strTag = "Point Coords:";
            SetCursorScreenPos(v2ViewPos+v2PaddingUnit);
            const ImU32 u32TagColor{IM_COL32(180, 170, 150, 255)};
            TextColored(ImColor(u32TagColor), "%s", strTag.c_str()); SameLine();
            ImVec2 v2TextBgRectLt, v2TextBgRectRb;
            v2TextBgRectLt = v2CursorPos; v2TextBgRectLt.y -= v2PaddingUnit.y;
            v2TextBgRectRb = v2TextBgRectLt+v2PointCoordTextSize+v2PaddingUnit*2;
            pDrawList->AddRectFilled(v2TextBgRectLt, v2TextBgRectRb, m_u32CpkfCoordBgColor, m_fCpkfCoordBgRectRounding);
            const ImU32 u32PointCoordTextColor{IM_COL32(30, 30, 30, 255)};
            SetCursorScreenPos(v2TextBgRectLt+v2PaddingUnit);
            string strDisplayText(strNA);
            if (itTargetCp != m_aRoutePointsForUi.end())
            {
                const auto& cp = *itTargetCp;
                auto tKpVal = cp.m_ahCurves[0]->CalcPointVal(i64Tick);
                oss.str(""); oss << setw(4) << setfill(' ') << (int)roundf(tKpVal.x) << "," << setw(4) << setfill(' ') << (int)roundf(tKpVal.y);
                strDisplayText = oss.str();
            }
            TextColored(ImColor(u32PointCoordTextColor), "%s", strDisplayText.c_str()); SameLine();
            // draw bezier grabber0 offset
            strTag = "Bezier Grabber0:";
            v2CursorPos = GetCursorScreenPos();
            v2CursorPos.x += v2PaddingUnit.x;
            SetCursorScreenPos(v2CursorPos);
            TextColored(ImColor(u32TagColor), "%s", strTag.c_str()); SameLine();
            strDisplayText = strNA;
            if (itTargetCp != m_aRoutePointsForUi.end())
            {
                const auto& cp = *itTargetCp;
                auto tKpVal = cp.m_ahCurves[1]->CalcPointVal(i64Tick);
                oss.str(""); oss << setw(4) << setfill(' ') << showpos << (int)roundf(tKpVal.x) << "," << setw(4) << setfill(' ') << showpos << (int)roundf(tKpVal.y);
                strDisplayText = oss.str();
            }
            v2PointCoordTextSize = CalcTextSize(strDisplayText.c_str());
            v2CursorPos = GetCursorScreenPos();
            v2TextBgRectLt = v2CursorPos; v2TextBgRectLt.y -= v2PaddingUnit.y;
            v2TextBgRectRb = v2TextBgRectLt+v2PointCoordTextSize+v2PaddingUnit*2;
            pDrawList->AddRectFilled(v2TextBgRectLt, v2TextBgRectRb, m_u32CpkfCoordBgColor, m_fCpkfCoordBgRectRounding);
            SetCursorScreenPos(v2TextBgRectLt+v2PaddingUnit);
            TextColored(ImColor(u32PointCoordTextColor), "%s", strDisplayText.c_str()); SameLine();
            // draw bezier grabber1 offset
            strTag = "Bezier Grabber1:";
            v2CursorPos = GetCursorScreenPos();
            v2CursorPos.x += v2PaddingUnit.x;
            SetCursorScreenPos(v2CursorPos);
            TextColored(ImColor(u32TagColor), "%s", strTag.c_str()); SameLine();
            strDisplayText = strNA;
            if (itTargetCp != m_aRoutePointsForUi.end())
            {
                const auto& cp = *itTargetCp;
                auto tKpVal = cp.m_ahCurves[2]->CalcPointVal(i64Tick);
                oss.str(""); oss << setw(4) << setfill(' ') << showpos << (int)roundf(tKpVal.x) << "," << setw(4) << setfill(' ') << showpos << (int)roundf(tKpVal.y);
                strDisplayText = oss.str();
            }
            v2PointCoordTextSize = CalcTextSize(strDisplayText.c_str());
            v2CursorPos = GetCursorScreenPos();
            v2TextBgRectLt = v2CursorPos; v2TextBgRectLt.y -= v2PaddingUnit.y;
            v2TextBgRectRb = v2TextBgRectLt+v2PointCoordTextSize+v2PaddingUnit*2;
            pDrawList->AddRectFilled(v2TextBgRectLt, v2TextBgRectRb, m_u32CpkfCoordBgColor, m_fCpkfCoordBgRectRounding);
            SetCursorScreenPos(v2TextBgRectLt+v2PaddingUnit);
            TextColored(ImColor(u32PointCoordTextColor), "%s", strDisplayText.c_str()); SameLine();
            v2CursorPos.x = v2ViewPos.x;
            v2CursorPos.y = v2TextBgRectRb.y;
            SetCursorScreenPos(v2CursorPos);
        }

        // draw key frame timeline
        list<float> aTicks;
        if (itTargetCp == m_aRoutePointsForUi.end())
        {
            // merge ticks from all contour points
            for (const auto& cp : m_aRoutePointsForUi)
            {
                if (aTicks.empty())
                    aTicks = cp.m_aKpTicks;
                else
                {
                    for (auto t : cp.m_aKpTicks)
                    {
                        auto itIns = find_if(aTicks.begin(), aTicks.end(), [t] (const auto& elem) {
                            return elem >= t;
                        });
                        if (itIns == aTicks.end() || *itIns > t)
                            aTicks.insert(itIns, t);
                    }
                }
            }
        }
        else
        {
            aTicks = itTargetCp->m_aKpTicks;
        }
        // prepare tick range
        auto prTickRange = m_prTickRange;
        if (prTickRange.first >= prTickRange.second && !aTicks.empty())
        {
            prTickRange.first = aTicks.front();
            prTickRange.second = aTicks.back();
        }
        if (prTickRange.second < prTickRange.first+10)
            prTickRange.second = prTickRange.first+10;
        // draw timeline slider
        const auto i64TickLength = prTickRange.second-prTickRange.first;
        const ImVec2 v2TimeLineWdgSize(v2ViewSize);
        const float fTimelineSliderHeight = 5.f;
        const float fKeyFrameIndicatorRadius = 5.f;
        ImVec2 v2SliderRectLt(v2ViewPos.x+fKeyFrameIndicatorRadius+v2PaddingUnit.x, v2CursorPos.y+(v2TimeLineWdgSize.y-fTimelineSliderHeight)/2);
        ImVec2 v2SliderRectRb(v2SliderRectLt.x+v2ViewSize.x-(fKeyFrameIndicatorRadius+v2PaddingUnit.x)*2, v2SliderRectLt.y+fTimelineSliderHeight);
        const auto fSliderWdgWidth = v2SliderRectRb.x-v2SliderRectLt.x;
        const ImU32 u32TimelineSliderBorderColor{IM_COL32(80, 80, 80, 255)};
        pDrawList->AddRect(v2SliderRectLt, v2SliderRectRb, u32TimelineSliderBorderColor, fTimelineSliderHeight/2, 0, 1);
        // draw key point indicators
        const float fKeyFrameIndicatorY = v2CursorPos.y+v2TimeLineWdgSize.y/2;
        const ImU32 u32KeyFrameIndicatorColor{IM_COL32(200, 150, 20, 255)};
        const ImU32 u32KeyFrameIndicatorBorderColor = u32TimelineSliderBorderColor;
        const float fHoverBrightnessIncreament = 0.4;
        ImColor tHoverColor(u32KeyFrameIndicatorColor);
        tHoverColor.Value.x += fHoverBrightnessIncreament; tHoverColor.Value.y += fHoverBrightnessIncreament; tHoverColor.Value.z += fHoverBrightnessIncreament;
        const ImU32 u32KeyFrameIndicatorHoverColor = (ImU32)tHoverColor;
        tHoverColor = ImColor(u32KeyFrameIndicatorBorderColor);
        tHoverColor.Value.x += fHoverBrightnessIncreament; tHoverColor.Value.y += fHoverBrightnessIncreament; tHoverColor.Value.z += fHoverBrightnessIncreament;
        const ImU32 u32KeyFrameIndicatorBorderHoverColor = (ImU32)tHoverColor;
        bool bHasPointAtCurrTick = false;
        bool bHasHoveredTick = false;
        int64_t i64HoveredTick;
        float fHoverDistance = numeric_limits<float>::max();
        auto fHoverDistThresh = fKeyFrameIndicatorRadius+m_fHoverDetectExRadius;
        fHoverDistThresh = fHoverDistThresh*fHoverDistThresh;
        const auto szTickCnt = aTicks.size();
        for (const auto t : aTicks)
        {
            const float fKeyFrameIndicatorX = v2SliderRectLt.x+((double)(t-prTickRange.first)/i64TickLength)*fSliderWdgWidth;
            const ImVec2 v2TickPos(fKeyFrameIndicatorX, fKeyFrameIndicatorY);
            const auto v2Offset = v2TickPos-v2MousePosAbs;
            const float distSqr = v2Offset.x*v2Offset.x+v2Offset.y*v2Offset.y;
            if (distSqr <= fHoverDistThresh && distSqr < fHoverDistance)
            {
                bHasHoveredTick = true;
                i64HoveredTick = t;
                fHoverDistance = distSqr;
            }
            if (t != i64Tick)
            {
                pDrawList->AddCircleFilled(v2TickPos, fKeyFrameIndicatorRadius+1, u32KeyFrameIndicatorBorderColor);
                pDrawList->AddCircleFilled(v2TickPos, fKeyFrameIndicatorRadius, u32KeyFrameIndicatorColor);
            }
            else
                bHasPointAtCurrTick = true;
        }
        if (bHasPointAtCurrTick)
        {
            const float fKeyFrameIndicatorX = v2SliderRectLt.x+((double)(i64Tick-prTickRange.first)/i64TickLength)*fSliderWdgWidth;
            const ImVec2 v2TickPos(fKeyFrameIndicatorX, fKeyFrameIndicatorY);
            pDrawList->AddCircleFilled(v2TickPos, fKeyFrameIndicatorRadius+1, u32KeyFrameIndicatorBorderHoverColor);
            pDrawList->AddCircleFilled(v2TickPos, fKeyFrameIndicatorRadius, u32KeyFrameIndicatorHoverColor);
        }
        if (bHasHoveredTick)
        {
            const float fKeyFrameIndicatorX = v2SliderRectLt.x+((double)(i64HoveredTick-prTickRange.first)/i64TickLength)*fSliderWdgWidth;
            const ImVec2 v2TickPos(fKeyFrameIndicatorX, fKeyFrameIndicatorY);
            pDrawList->AddCircleFilled(v2TickPos, fKeyFrameIndicatorRadius+2, u32KeyFrameIndicatorBorderHoverColor);
            pDrawList->AddCircleFilled(v2TickPos, fKeyFrameIndicatorRadius, u32KeyFrameIndicatorHoverColor);
        }

        const bool bRemoveKeyDown = IsKeyDown(m_eRemoveVertexKey);
        // handle click event on a key-frame indicator
        if (IsMouseClicked(ImGuiMouseButton_Left) && bHasHoveredTick)
        {
            if (bRemoveKeyDown && i64HoveredTick != prTickRange.first)
            {  // remove a key point
                lock_guard<mutex> _lk(m_mtxRouteLock);
                for (auto& cp : m_aRoutePointsForUi)
                    cp.RemoveKeyPoint(-1, (float)i64HoveredTick);
                m_i64PrevUiTick = INT64_MIN;
                m_bRouteChanged = true;
            }
            else
            {
                i64Tick = i64HoveredTick;
            }
        }

        // draw tick indicator
        const ImVec2 v2TickIndicatorPos(v2SliderRectLt.x+((double)(i64Tick-prTickRange.first)/i64TickLength)*fSliderWdgWidth, v2SliderRectRb.y+fKeyFrameIndicatorRadius+2);
        const ImVec2 v2TickIndicatorSize(10, 6);
        const ImU32 u32TickIndicatorColor{IM_COL32(80, 180, 80, 255)};
        if (i64Tick >= prTickRange.first && i64Tick <= prTickRange.second)
        {
            pDrawList->AddTriangleFilled(
                    v2TickIndicatorPos,
                    v2TickIndicatorPos+ImVec2(-v2TickIndicatorSize.x/2, v2TickIndicatorSize.y),
                    v2TickIndicatorPos+ImVec2(v2TickIndicatorSize.x/2, v2TickIndicatorSize.y),
                    u32TickIndicatorColor);
        }

        v2ViewSize.y = v2TickIndicatorPos.y+v2TickIndicatorSize.y-v2ViewPos.y+1;
        ImRect bb(v2ViewPos, v2ViewPos+v2ViewSize);
        if (!ItemAdd(bb, 0))
        {
            ostringstream oss; oss << "DrawSelectedPointKeyFrames: FAILED at 'ItemAdd' with item rect (("
                    << bb.Min.x << ", " << bb.Min.y << "), (" << bb.Max.x << ", " << bb.Max.y << "))!";
            m_sErrMsg = oss.str();
            return false;
        }
        SetCursorScreenPos(ImVec2(v2ViewPos.x, v2ViewPos.y+v2ViewSize.y));

        const bool bMouseInView = bb.Contains(v2MousePosAbs);
        if (bRemoveKeyDown && bMouseInView && i64Tick != prTickRange.first)
            SetMouseCursor(ImGuiMouseCursor_Minus);

        return true;
    }

    void SetMaskWarpAffineMatrix(float aWarpAffineMatrix[2][3], float aRevWarpAffineMatrix[2][3]) override
    {
        if (memcmp(m_aMaskWarpAffineMatrix, aWarpAffineMatrix, sizeof(m_aMaskWarpAffineMatrix)))
        {
            memcpy(m_aMaskWarpAffineMatrix, aWarpAffineMatrix, sizeof(m_aMaskWarpAffineMatrix));
            m_bMaskWarpAffineMatrixChanged = true;
        }
        if (memcmp(m_aMaskRevWarpAffineMatrix, aRevWarpAffineMatrix, sizeof(m_aMaskRevWarpAffineMatrix)))
        {
            memcpy(m_aMaskRevWarpAffineMatrix, aRevWarpAffineMatrix, sizeof(m_aMaskRevWarpAffineMatrix));
            m_bMaskWarpAffineMatrixChanged = true;
        }
        if (m_bMaskWarpAffineMatrixChanged)
            m_bNeedRebuildMaskContourVtx = true;
    }

    void SetMaskWarpAffineParameters(const ImVec2& v2Offset, const ImVec2& v2Scale, float fRotationAngle, const ImVec2& v2Anchor) override
    {
        const auto fRotationRadian = fRotationAngle*M_PI/180.f;
        const auto fSinA = (float)sin(fRotationRadian);
        const auto fCosA = (float)cos(fRotationRadian);
        float aWarpAffineMatrix[2][3], aRevWarpAffineMatrix[2][3];
        aWarpAffineMatrix[0][0] = fCosA*v2Scale.x;
        aWarpAffineMatrix[0][1] = fSinA*v2Scale.y;
        aWarpAffineMatrix[1][0] = -fSinA*v2Scale.x;
        aWarpAffineMatrix[1][1] = fCosA*v2Scale.y;
        aWarpAffineMatrix[0][2] = v2Offset.x+(1-aWarpAffineMatrix[0][0])*v2Anchor.x-aWarpAffineMatrix[0][1]*v2Anchor.y;
        aWarpAffineMatrix[1][2] = v2Offset.y-aWarpAffineMatrix[1][0]*v2Anchor.x+(1-aWarpAffineMatrix[1][1])*v2Anchor.y;
        aRevWarpAffineMatrix[0][0] = fCosA/v2Scale.x;
        aRevWarpAffineMatrix[0][1] = -fSinA/v2Scale.x;
        aRevWarpAffineMatrix[0][2] = -aWarpAffineMatrix[0][2]*aRevWarpAffineMatrix[0][0]-aWarpAffineMatrix[1][2]*aRevWarpAffineMatrix[0][1];
        aRevWarpAffineMatrix[1][0] = fSinA/v2Scale.y;
        aRevWarpAffineMatrix[1][1] = fCosA/v2Scale.y;
        aRevWarpAffineMatrix[1][2] = -aWarpAffineMatrix[0][2]*aRevWarpAffineMatrix[1][0]-aWarpAffineMatrix[1][2]*aRevWarpAffineMatrix[1][1];
        SetMaskWarpAffineMatrix(aWarpAffineMatrix, aRevWarpAffineMatrix);
    }

    void SetUiWarpAffineMatrix(float aWarpAffineMatrix[2][3], float aRevWarpAffineMatrix[2][3]) override
    {
        if (memcmp(m_aUiWarpAffineMatrix, aWarpAffineMatrix, sizeof(m_aUiWarpAffineMatrix)))
        {
            memcpy(m_aUiWarpAffineMatrix, aWarpAffineMatrix, sizeof(m_aUiWarpAffineMatrix));
            m_bUiWarpAffineMatrixChanged = true;
        }
        if (memcmp(m_aUiRevWarpAffineMatrix, aRevWarpAffineMatrix, sizeof(m_aUiRevWarpAffineMatrix)))
        {
            memcpy(m_aUiRevWarpAffineMatrix, aRevWarpAffineMatrix, sizeof(m_aUiRevWarpAffineMatrix));
            m_bUiWarpAffineMatrixChanged = true;
        }
    }

    void SetUiWarpAffineParameters(const ImVec2& v2Offset, const ImVec2& v2Scale, float fRotationAngle, const ImVec2& v2Anchor) override
    {
        const auto fRotationRadian = fRotationAngle*M_PI/180.f;
        const auto fSinA = (float)sin(fRotationRadian);
        const auto fCosA = (float)cos(fRotationRadian);
        float aWarpAffineMatrix[2][3], aRevWarpAffineMatrix[2][3];
        aWarpAffineMatrix[0][0] = fCosA*v2Scale.x;
        aWarpAffineMatrix[0][1] = fSinA*v2Scale.y;
        aWarpAffineMatrix[1][0] = -fSinA*v2Scale.x;
        aWarpAffineMatrix[1][1] = fCosA*v2Scale.y;
        aWarpAffineMatrix[0][2] = v2Offset.x+(1-aWarpAffineMatrix[0][0])*v2Anchor.x-aWarpAffineMatrix[0][1]*v2Anchor.y;
        aWarpAffineMatrix[1][2] = v2Offset.y-aWarpAffineMatrix[1][0]*v2Anchor.x+(1-aWarpAffineMatrix[1][1])*v2Anchor.y;
        aRevWarpAffineMatrix[0][0] = fCosA/v2Scale.x;
        aRevWarpAffineMatrix[0][1] = -fSinA/v2Scale.x;
        aRevWarpAffineMatrix[0][2] = -aWarpAffineMatrix[0][2]*aRevWarpAffineMatrix[0][0]-aWarpAffineMatrix[1][2]*aRevWarpAffineMatrix[0][1];
        aRevWarpAffineMatrix[1][0] = fSinA/v2Scale.y;
        aRevWarpAffineMatrix[1][1] = fCosA/v2Scale.y;
        aRevWarpAffineMatrix[1][2] = -aWarpAffineMatrix[0][2]*aRevWarpAffineMatrix[1][0]-aWarpAffineMatrix[1][2]*aRevWarpAffineMatrix[1][1];
        SetUiWarpAffineMatrix(aWarpAffineMatrix, aRevWarpAffineMatrix);
    }

    bool ChangeMaskSize(const MatUtils::Size2i& size, bool bScaleMask) override
    {
        if (m_szMaskSize != size)
        {
            const ImVec2 v2Scale((float)size.x/m_szMaskSize.x, (float)size.y/m_szMaskSize.y);
            m_szMaskSize = size;
            m_bRouteChanged = true;
            if (bScaleMask)
            {
                for (auto& cp : m_aRoutePointsForUi)
                {
                    cp.m_v2Pos *= v2Scale;
                    cp.m_v2Grabber0Offset *= v2Scale;
                    cp.m_v2Grabber1Offset *= v2Scale;
                    if (m_bKeyFrameEnabled)
                    {
                        const LibCurve::KeyPoint::ValType tScale(v2Scale.x, v2Scale.y, 1.f, 1.f);
                        for (auto i = 0; i < 3; i++)
                            cp.m_ahCurves[i]->ScaleKeyPoints(tScale);
                        cp.UpdateByTick(m_i64PrevUiTick);
                    }
                }
                RefreshAllEdgeVertices(m_aRoutePointsForUi);
                if (IsMorphCtrlShown())
                {
                    m_tMorphCtrl.m_fDistance *= v2Scale.x;
                    m_tMorphCtrl.SetPosAndSlope(m_itMorphCtrlVt, m_tMorphCtrl.m_fDistance);
                }
            }
            m_i64PrevUiTick = INT64_MIN;
            m_i64PrevMaskTick = INT64_MIN;
        }
        return true;
    }

    MatUtils::Size2i GetMaskSize() const override
    {
        return m_szMaskSize;
    }

    ImGui::ImMat GetMask(int iLineType, bool bFilled, ImDataType eDataType, double dMaskValue, double dNonMaskValue, int64_t i64Tick) override
    {
        ImGui::ImMat res;
        if (!m_bRouteCompleted)
            return res;

        auto& aRoutePoints = m_aRoutePointsForMask;
        bool bContourChanged = false;
        if (m_bRouteNeedSync.exchange(false))
        {
            auto itRpMask = aRoutePoints.begin();
            {
                lock_guard<mutex> _lk(m_mtxRouteLock);
                auto itRpUi = m_aRoutePointsForUi.begin();
                while (itRpUi != m_aRoutePointsForUi.end())
                {
                    const auto& tRpUi = *itRpUi++;
                    if (itRpMask == aRoutePoints.end())
                    {
                        RoutePointImpl cp(this, {0, 0});
                        cp.SyncWith(tRpUi);
                        aRoutePoints.push_back(std::move(cp));
                        itRpMask = aRoutePoints.end();
                    }
                    else
                    {
                        itRpMask->SyncWith(tRpUi);
                        itRpMask++;
                    }
                }
            }
            if (itRpMask != aRoutePoints.end())
                aRoutePoints.erase(itRpMask, aRoutePoints.end());
            RefreshAllEdgeVertices(m_aRoutePointsForMask);
            m_i64PrevMaskTick = INT64_MIN;
            bContourChanged = true;
        }

        if (m_bKeyFrameEnabled && UpdateContourByKeyFrame(i64Tick, false))
            bContourChanged = true;

        if (m_bNeedRebuildMaskContourVtx.exchange(false))
            bContourChanged = true;

        if (bContourChanged)
        {
            // refresh contour vertices
            list<MatUtils::Point2f> aContourVertices;
            if (aRoutePoints.size() > 1)
            {
                bool b1stVertex = true;
                auto itRp = aRoutePoints.begin();
                while (itRp != aRoutePoints.end())
                {
                    const auto& v = *itRp++;
                    if (!v.m_aEdgeVertices.empty())
                    {
                        auto itVt0 = v.m_aEdgeVertices.begin();
                        auto itVt1 = itVt0; itVt1++;
                        while (itVt1 != v.m_aEdgeVertices.end())
                        {
                            aContourVertices.push_back(MatUtils::FromImVec2<float>(ToMaskPos(*itVt0++)));
                            itVt1++;
                        }
                    }
                }
            }
            m_aContourVerticesForMask = aContourVertices;
        }

        res = m_mMask;
        const auto iMorphIters = m_tMorphCtrl.m_bInsidePoly ? -m_tMorphCtrl.m_iMorphIterations : m_tMorphCtrl.m_iMorphIterations;
        const auto iFeatherIters = m_tMorphCtrl.m_iFeatherIterations;
        if (bContourChanged || m_mMask.empty() ||
            m_iLastMaskLineType != iLineType || m_bLastMaskFilled != bFilled ||
            m_eLastMaskDataType != eDataType || m_dLastMaskValue != dMaskValue || m_dLastNonMaskValue != dNonMaskValue)
        {
            if ((iMorphIters == 0 && iFeatherIters == 0) || !bFilled)
            {
                m_mMask = MatUtils::Contour2Mask(m_szMaskSize, m_aContourVerticesForMask, {0, 0}, eDataType, dMaskValue, dNonMaskValue, iLineType, bFilled);
                res = m_mMask;
            }
            else
            {
                m_mMask.release();
            }
        }

        if (iMorphIters != 0 || iFeatherIters != 0)
        {
            if (bContourChanged || m_iLastMorphIters != iMorphIters || m_iLastFeatherIters != iFeatherIters ||
                m_iLastMaskLineType != iLineType || m_bLastMaskFilled != bFilled ||
                m_eLastMaskDataType != eDataType || m_dLastMaskValue != dMaskValue || m_dLastNonMaskValue != dNonMaskValue)
            {
                list<MatUtils::Point2f> aMorphContourVertices;
                ImGui::ImMat mColor;
                if (!bFilled)
                {
                    m_mMorphMask = m_mMask.clone();
                    if (iMorphIters != 0)
                    {
                        aMorphContourVertices = iMorphIters < 0 ? ErodeContour(-iMorphIters) : DilateContour(iMorphIters);
                        mColor = MatUtils::MakeColor(res.type, dMaskValue);
                        MatUtils::DrawPolygon(m_mMorphMask, aMorphContourVertices, mColor, iLineType);
                    }
                    if (iFeatherIters > 0)
                    {
                        mColor = MatUtils::MakeColor(res.type, dMaskValue/2);
                        const auto iFeatherIter1 = iMorphIters-iFeatherIters;
                        if (iFeatherIter1 != 0)
                        {
                            aMorphContourVertices = iFeatherIter1 < 0 ? ErodeContour(-iFeatherIter1) : DilateContour(iFeatherIter1);
                            MatUtils::DrawPolygon(m_mMorphMask, aMorphContourVertices, mColor, iLineType);
                        }
                        const auto iFeatherIter2 = iMorphIters+iFeatherIters;
                        if (iFeatherIter2 != 0)
                        {
                            aMorphContourVertices = iFeatherIter2 < 0 ? ErodeContour(-iFeatherIter2) : DilateContour(iFeatherIter2);
                            MatUtils::DrawPolygon(m_mMorphMask, aMorphContourVertices, mColor, iLineType);
                        }
                    }
                }
                else
                {
                    if (iFeatherIters > 0)
                    {
                        aMorphContourVertices = iMorphIters < 0 ? ErodeContour(-iMorphIters) : DilateContour(iMorphIters);
                        const auto iFeatherKsize = 2*iFeatherIters+1;
                        m_mMorphMask = MatUtils::Contour2Mask(m_szMaskSize, aMorphContourVertices, {0, 0}, eDataType, dMaskValue, dNonMaskValue, iLineType, bFilled, iFeatherKsize);
                    }
                    else
                    {
                        aMorphContourVertices = iMorphIters < 0 ? ErodeContour(-iMorphIters) : DilateContour(iMorphIters);
                        m_mMorphMask = MatUtils::Contour2Mask(m_szMaskSize, aMorphContourVertices, {0, 0}, eDataType, dMaskValue, dNonMaskValue, iLineType, bFilled);
                    }
                }
                m_iLastMorphIters = iMorphIters;
                m_iLastFeatherIters = iFeatherIters;
            }
            res = m_mMorphMask;
        }

        m_iLastMaskLineType = iLineType;
        m_bLastMaskFilled = bFilled;
        m_eLastMaskDataType = eDataType;
        m_dLastMaskValue = dMaskValue;
        m_dLastNonMaskValue = dNonMaskValue;
        return res;
    }

    const RoutePoint* GetHoveredPoint() const override
    {
        if (HasHoveredVertex())
            return static_cast<RoutePoint*>(&(*m_itHoveredVertex));
        return nullptr;
    }

    const RoutePoint* GetSelectedPoint() const override
    {
        if (HasSelectedVertex())
            return static_cast<RoutePoint*>(&(*m_itSelectedVertex));
        return nullptr;
    }

    ImVec4 GetContourContainBox() const override
    {
        const auto v2UiBoxMin = ToUiPos(m_rContianBox.Min);
        const auto v2UiBoxMax = ToUiPos(m_rContianBox.Max);
        return ImVec4(v2UiBoxMin.x, v2UiBoxMin.y, v2UiBoxMax.x, v2UiBoxMax.y);
    }

    ImVec2 GetUiScale() const override
    {
        return m_v2UiScale;
    }

    bool IsKeyFrameEnabled() const override
    {
        return m_bKeyFrameEnabled;
    }

    void EnableKeyFrames(bool bEnable) override
    {
        if (m_bKeyFrameEnabled != bEnable)
        {
            if (m_bKeyFrameEnabled)
                UpdateContourByKeyFrame(0, true);
            for (auto& cp : m_aRoutePointsForUi)
                cp.EnableKeyFrames(bEnable);
            m_bKeyFrameEnabled = bEnable;
            if (!bEnable)
                SelectVertex(m_aRoutePointsForUi.end());
            m_i64PrevUiTick = 0;
            m_i64PrevMaskTick = INT64_MIN;
        }
    }

    bool SetTickRange(int64_t i64Start, int64_t i64End) override
    {
        if (i64Start >= i64End)
            return false;
        m_prTickRange.first = i64Start;
        m_prTickRange.second = i64End;
        if (m_bKeyFrameEnabled)
        {
            for (const auto& cp : m_aRoutePointsForUi)
            {
                for (auto i = 0; i < 3; i++)
                    cp.m_ahCurves[i]->SetTimeRange(ImVec2(i64Start, i64End));
            }
        }
        return true;
    }

    bool IsMaskReady() const override
    {
        return m_bRouteCompleted;
    }

    bool SaveAsJson(imgui_json::value& j) const override
    {
        j = imgui_json::value();
        json::array subj;
        for (const auto& cp : m_aRoutePointsForUi)
            subj.push_back(cp.ToJson());
        j["contour_points"] = subj;
        j["morph_controller"] = m_tMorphCtrl.ToJson();
        if (IsMorphCtrlShown())
        {
            int cpidx = 0;
            auto it = m_aRoutePointsForUi.begin();
            while (it != m_itMorphCtrlVt && it != m_aRoutePointsForUi.end())
            {
                cpidx++; it++;
            }
            if (it != m_aRoutePointsForUi.end())
                j["morph_ctrl_cpidx"] = json::number(cpidx);
            else
                j["morph_ctrl_cpidx"] = json::number(-1);
        }
        else
        {
            j["morph_ctrl_cpidx"] = json::number(-1);
        }
        j["name"] = json::string(m_strMaskName);
        j["size"] = MatUtils::ToImVec2(m_szMaskSize);
        j["point_size"] = m_v2PointSize;
        j["point_color"] = json::number(m_u32PointColor);
        j["point_border_color"] = json::number(m_u32PointBorderColor);
        j["point_border_hover_color"] = json::number(m_u32PointBorderHoverColor);
        j["point_border_thickness"] = m_fPointBorderThickness;
        j["contour_hover_point_color"] = json::number(m_u32ContourHoverPointColor);
        j["grabber_radius"] = m_fGrabberRadius;
        j["grabber_border_thickness"] = m_fGrabberBorderThickness;
        j["grabber_color"] = json::number(m_u32GrabberColor);
        j["grabber_border_color"] = json::number(m_u32GrabberBorderColor);
        j["grabber_border_hover_color"] = json::number(m_u32GrabberBorderHoverColor);
        j["grabberline_thickness"] = m_fGrabberLineThickness;
        j["grabberline_color"] = json::number(m_u32GrabberLineColor);
        j["contour_thickness"] = m_fContourThickness;
        j["contour_color"] = json::number(m_u32ContourColor);
        j["contour_hover_detect_ex_radius"] = m_fContourHoverDetectExRadius;
        j["hover_detect_ex_radius"] = m_fHoverDetectExRadius;
        j["contour_complete"] = m_bRouteCompleted;
        j["mask_filled"] = m_bLastMaskFilled;
        j["mask_line_type"] = json::number(m_iLastMaskLineType);
        j["key_frame_enabled"] = m_bKeyFrameEnabled;
        j["tick_range_0"] = json::number(m_prTickRange.first);
        j["tick_range_1"] = json::number(m_prTickRange.second);
        return true;
    }

    bool SaveAsJson(const std::string& filePath) const override
    {
        json::value j;
        if (!SaveAsJson(j))
            return false;
        j.save(filePath);
        return true;
    }

    void LoadFromJson(const json::value& j)
    {
        if (j.contains("name")) m_strMaskName = j["name"].get<json::string>();
        if (j.contains("size")) m_szMaskSize = MatUtils::FromImVec2<int32_t>(j["size"].get<json::vec2>());
        m_v2PointSize = j["point_size"].get<json::vec2>();
        m_u32PointColor = j["point_color"].get<json::number>();
        m_u32PointBorderColor = j["point_border_color"].get<json::number>();
        m_u32PointBorderHoverColor = j["point_border_hover_color"].get<json::number>();
        m_fPointBorderThickness = j["point_border_thickness"].get<json::number>();
        m_u32ContourHoverPointColor = j["contour_hover_point_color"].get<json::number>();
        m_fGrabberRadius = j["grabber_radius"].get<json::number>();
        m_fGrabberBorderThickness = j["grabber_border_thickness"].get<json::number>();
        m_u32GrabberColor = j["grabber_color"].get<json::number>();
        m_u32GrabberBorderColor = j["grabber_border_color"].get<json::number>();
        m_u32GrabberBorderHoverColor = j["grabber_border_hover_color"].get<json::number>();
        m_fGrabberLineThickness = j["grabberline_thickness"].get<json::number>();
        m_u32GrabberLineColor = j["grabberline_color"].get<json::number>();
        m_fContourThickness = j["contour_thickness"].get<json::number>();
        m_u32ContourColor = j["contour_color"].get<json::number>();
        m_fContourHoverDetectExRadius = j["contour_hover_detect_ex_radius"].get<json::number>();
        m_fHoverDetectExRadius = j["hover_detect_ex_radius"].get<json::number>();
        m_bRouteCompleted = j["contour_complete"].get<json::boolean>();
        m_bLastMaskFilled = j["mask_filled"].get<json::boolean>();
        m_iLastMaskLineType = j["mask_line_type"].get<json::number>();
        if (j.contains("key_frame_enabled"))
            m_bKeyFrameEnabled = j["key_frame_enabled"].get<json::boolean>();
        if (j.contains("tick_range_0"))
            m_prTickRange.first = j["tick_range_0"].get<json::number>();
        if (j.contains("tick_range_1"))
            m_prTickRange.second = j["tick_range_1"].get<json::number>();
        m_i64PrevUiTick = m_prTickRange.first;
        const auto& subj = j["contour_points"].get<json::array>();
        for (const auto& elem : subj)
        {
            RoutePointImpl cp(this, {0, 0});
            cp.FromJson(elem);
            m_aRoutePointsForUi.push_back(std::move(cp));
        }
        if (m_bKeyFrameEnabled)
        {
            for (auto& cp : m_aRoutePointsForUi)
                cp.UpdateByTick(m_i64PrevUiTick);
        }
        m_tMorphCtrl.FromJson(j["morph_controller"]);

        m_v2PointSizeHalf = m_v2PointSize/2;
        m_itMorphCtrlVt = m_itHoveredVertex = m_aRoutePointsForUi.end();
        RefreshAllEdgeVertices(m_aRoutePointsForUi);
        int cpidx = j["morph_ctrl_cpidx"].get<json::number>();
        if (cpidx >= 0)
        {
            auto it = m_aRoutePointsForUi.begin();
            for (int i = 0; i < cpidx; i++)
                it++;
            m_itMorphCtrlVt = it;
            m_tMorphCtrl.SetPosAndSlope(it, m_tMorphCtrl.m_fDistance);
        }
    }

    string GetError() const override
    {
        return m_sErrMsg;
    }

    void SetLoggerLevel(Logger::Level l) override
    {
        m_pLogger->SetShowLevels(l);
    }

private:
    struct RoutePointImpl : public RoutePoint
    {
        RoutePointImpl(MaskCreatorImpl* owner, const ImVec2& pos)
            : m_owner(owner)
            , m_v2Pos(pos)
            , m_rGrabberContBox(pos, pos)
            , m_rEdgeContBox(-1.f, -1.f, -1.f, -1.f)
        {
            m_ptCurrPos = MatUtils::FromImVec2<float>(m_v2Pos);
            if (owner->m_bKeyFrameEnabled)
                EnableKeyFrames(true);
        }

        MaskCreatorImpl* m_owner;
        ImVec2 m_v2Pos;
        ImVec2 m_v2Grabber0Offset{0.f, 0.f};
        ImVec2 m_v2Grabber1Offset{0.f, 0.f};
        bool m_bJustAdded{true};  // when point is 1st time added, grabbing op will trigger bezier grabber. after mouse button released, following dragging op will treated as moving
        bool m_bEnableBezier{false};
        bool m_bFirstDrag{true};  // 1st time dragging bezier grabber, both 1&2 grabbers move together
        bool m_bHovered{true};
        int m_iHoverType{0};  // 0: on contour point, 1: on 1st bezier grabber, 2: on 2nd bezier grabber, 4: on contour
        bool m_bSelected{false};
        list<ImVec2> m_aEdgeVertices;
        ImVec2 m_v2HoverPointOnContour;
        ImVec2 m_v2MoveOriginMousePos;
        int m_iHoverPointOnContourIdx;
        ImRect m_rGrabberContBox;
        ImRect m_rEdgeContBox;
        LibCurve::Curve::Holder m_ahCurves[3];
        list<float> m_aKpTicks;
        MatUtils::Point2f m_ptCurrPos;
        MatUtils::Point2f m_ptCurrGrabber0Offset{0, 0}, m_ptCurrGrabber1Offset{0, 0};

        bool AddKeyPoint(int iCurveIdx, LibCurve::KeyPoint::Holder hKp)
        {
            if (iCurveIdx >= 3)
                return false;
            float t;
            if (iCurveIdx < 0)
            {
                auto idx = m_ahCurves[0]->AddPoint(hKp);
                auto hKp = m_ahCurves[0]->GetKeyPoint(idx);
                t = hKp->t;
                m_ahCurves[1]->AddPoint(hKp);
                m_ahCurves[2]->AddPoint(hKp);
            }
            else
            {
                auto idx = m_ahCurves[iCurveIdx]->AddPoint(hKp);
                auto hKp = m_ahCurves[iCurveIdx]->GetKeyPoint(idx);
                t = hKp->t;
            }
            auto itCheck = find_if(m_aKpTicks.begin(), m_aKpTicks.end(), [t] (const auto& elem) {
                return elem >= t;
            });
            if (itCheck == m_aKpTicks.end() || *itCheck > t)
                m_aKpTicks.insert(itCheck, t);
            return true;
        }

        bool RemoveKeyPoint(int iCurveIdx, float t)
        {
            if (iCurveIdx >= 3)
                return false;
            if (iCurveIdx < 0)
            {
                for (auto i = 0; i < 3; i++)
                    m_ahCurves[i]->RemovePoint(t);
            }
            else
            {
                m_ahCurves[iCurveIdx]->RemovePoint(t);
            }
            m_aKpTicks.remove(t);
            return true;
        }

        void SyncWith(const RoutePointImpl& rp)
        {
            m_v2Pos = rp.m_v2Pos;
            m_v2Grabber0Offset = rp.m_v2Grabber0Offset;
            m_v2Grabber1Offset = rp.m_v2Grabber1Offset;
            m_bEnableBezier = rp.m_bEnableBezier;
            if (m_owner->m_bKeyFrameEnabled)
            {
                for (auto i = 0; i < 3; i++)
                    m_ahCurves[i] = rp.m_ahCurves[i]->Clone();
            }
            else
            {
                for (auto i = 0; i < 3; i++)
                    m_ahCurves[i] = nullptr;
            }
        }

        json::value ToJson() const
        {
            json::value j;
            j["pos"] = m_v2Pos;
            j["enable_bezier"] = m_bEnableBezier;
            j["first_drag"] = m_bFirstDrag;
            j["grabber0_offset"] = m_v2Grabber0Offset;
            j["grabber1_offset"] = m_v2Grabber1Offset;
            if (m_ahCurves[0])
                j["curve0"] = m_ahCurves[0]->SaveAsJson();
            if (m_ahCurves[1])
                j["curve1"] = m_ahCurves[1]->SaveAsJson();
            if (m_ahCurves[2])
                j["curve2"] = m_ahCurves[2]->SaveAsJson();
            return std::move(j);
        }

        void FromJson(const json::value& j)
        {
            m_v2Pos = j["pos"].get<json::vec2>();
            m_bEnableBezier = j["enable_bezier"].get<json::boolean>();
            m_bFirstDrag = j["first_drag"].get<json::boolean>();
            m_v2Grabber0Offset = j["grabber0_offset"].get<json::vec2>();
            m_v2Grabber1Offset = j["grabber1_offset"].get<json::vec2>();
            m_bJustAdded = false;
            m_bHovered = false;
            m_iHoverType = -1;
            if (j.contains("curve0"))
                m_ahCurves[0] = LibCurve::Curve::CreateFromJson(j["curve0"]);
            if (j.contains("curve1"))
                m_ahCurves[1] = LibCurve::Curve::CreateFromJson(j["curve1"]);
            if (j.contains("curve2"))
                m_ahCurves[2] = LibCurve::Curve::CreateFromJson(j["curve2"]);
            // rebuild tick array
            list<float> aKpTicks;
            for (int i = 0; i < 3; i++)
            {
                if (!m_ahCurves[i])
                    continue;
                const auto& hCurve = m_ahCurves[i];
                const auto szKpCnt = hCurve->GetKeyPointCount();
                for (auto j = 0; j < szKpCnt; j++)
                {
                    const auto t = hCurve->GetKeyPoint(j)->t;
                    auto itIns = find_if(aKpTicks.begin(), aKpTicks.end(), [t] (const auto& elem) {
                        return elem >= t;
                    });
                    if (itIns == aKpTicks.end() || *itIns > t)
                        aKpTicks.insert(itIns, t);
                }
            }
            m_aKpTicks = std::move(aKpTicks);
            UpdateGrabberContainBox();
        }

        MatUtils::Point2f GetPos(int64_t t) const override
        {
            if (t <= 0 || !m_ahCurves[0])
                return MatUtils::FromImVec2<float>(m_v2Pos);
            const auto tKpVal = m_ahCurves[0]->CalcPointVal(t, false);
            const ImVec2 v2Pos(LibCurve::KeyPoint::GetDimVal(tKpVal, LibCurve::DIM_X), LibCurve::KeyPoint::GetDimVal(tKpVal, LibCurve::DIM_Y));
            return MatUtils::FromImVec2<float>(v2Pos);
        }

        MatUtils::Point2f GetBezierGrabberOffset(int idx, int64_t t) const override
        {
            if (idx < 0 || idx > 1)
                return {-1, -1};
            if (idx == 0)
            {
                if (t <= 0 || !m_ahCurves[1])
                    return MatUtils::FromImVec2<float>(m_v2Grabber0Offset);
                const auto tKpVal = m_ahCurves[1]->CalcPointVal(t, false);
                const ImVec2 v2Offset(LibCurve::KeyPoint::GetDimVal(tKpVal, LibCurve::DIM_X), LibCurve::KeyPoint::GetDimVal(tKpVal, LibCurve::DIM_Y));
                return MatUtils::FromImVec2<float>(v2Offset);
            }
            else
            {
                if (t <= 0 || !m_ahCurves[2])
                    return MatUtils::FromImVec2<float>(m_v2Grabber1Offset);
                const auto tKpVal = m_ahCurves[2]->CalcPointVal(t, false);
                const ImVec2 v2Offset(LibCurve::KeyPoint::GetDimVal(tKpVal, LibCurve::DIM_X), LibCurve::KeyPoint::GetDimVal(tKpVal, LibCurve::DIM_Y));
                return MatUtils::FromImVec2<float>(v2Offset);
            }
        }

        int GetHoverType() const override
        {
            return m_iHoverType;
        }

        bool IsSelected() const override
        {
            return m_bSelected;
        }

        void EnableKeyFrames(bool bEnable)
        {
            if (bEnable)
            {
                const auto& prTickRange = m_owner->m_prTickRange;
                ostringstream oss; oss << "ContourPont_Curve";
                string strCurveName = oss.str();
                {
                    const LibCurve::KeyPoint::ValType tMinVal(0, 0, 0, prTickRange.first);
                    const LibCurve::KeyPoint::ValType tMaxVal(7680, 4320, 0, prTickRange.second);
                    const LibCurve::KeyPoint::ValType tDefaultVal(m_v2Pos.x, m_v2Pos.y, 0, 0);
                    m_ahCurves[0] = LibCurve::Curve::CreateInstance(strCurveName, LibCurve::Smooth, tMinVal, tMaxVal, tDefaultVal);
                }
                {
                    const LibCurve::KeyPoint::ValType tMinVal(-1000, -1000, 0, prTickRange.first);
                    const LibCurve::KeyPoint::ValType tMaxVal(1000, 1000, 0, prTickRange.second);
                    LibCurve::KeyPoint::ValType tDefaultVal(m_v2Grabber0Offset.x, m_v2Grabber0Offset.y, 0, 0);
                    oss.str(""); oss << "Grabber0Offset_Curve";
                    strCurveName = oss.str();
                    m_ahCurves[1] = LibCurve::Curve::CreateInstance(strCurveName, LibCurve::Smooth, tMinVal, tMaxVal, tDefaultVal);
                    tDefaultVal = LibCurve::KeyPoint::ValType(m_v2Grabber1Offset.x, m_v2Grabber1Offset.y, 0, 0);
                    oss.str(""); oss << "Grabber1Offset_Curve";
                    strCurveName = oss.str();
                    m_ahCurves[2] = LibCurve::Curve::CreateInstance(strCurveName, LibCurve::Smooth, tMinVal, tMaxVal, tDefaultVal);
                }
                m_ptCurrPos = MatUtils::FromImVec2<float>(m_v2Pos);
                m_ptCurrGrabber0Offset = MatUtils::FromImVec2<float>(m_v2Grabber0Offset);
                m_ptCurrGrabber1Offset = MatUtils::FromImVec2<float>(m_v2Grabber1Offset);
                auto hKp = LibCurve::KeyPoint::CreateInstance(m_ahCurves[0]->GetDefaultVal());
                AddKeyPoint(0, hKp);
                hKp = LibCurve::KeyPoint::CreateInstance(m_ahCurves[1]->GetDefaultVal());
                AddKeyPoint(1, hKp);
                hKp = LibCurve::KeyPoint::CreateInstance(m_ahCurves[2]->GetDefaultVal());
                AddKeyPoint(2, hKp);
            }
            else
            {
                for (int i = 0; i < 3; i++)
                    m_ahCurves[i] = nullptr;
            }
        }

        bool UpdateByTick(int64_t i64Tick)
        {
            const auto ptCurrPos = GetPos(i64Tick);
            const auto ptCurrGrabber0Offset = GetBezierGrabberOffset(0, i64Tick);
            const auto ptCurrGrabber1Offset = GetBezierGrabberOffset(1, i64Tick);
            const bool bChanged = ptCurrPos != m_ptCurrPos || ptCurrGrabber0Offset != m_ptCurrGrabber0Offset || ptCurrGrabber1Offset != m_ptCurrGrabber1Offset;
            m_ptCurrPos = ptCurrPos;
            m_ptCurrGrabber0Offset = ptCurrGrabber0Offset;
            m_ptCurrGrabber1Offset = ptCurrGrabber1Offset;
            return bChanged;
        }

        MatUtils::Recti GetContainBox() const
        {
            return MatUtils::Recti(
                    m_rGrabberContBox.Min.x < m_rEdgeContBox.Min.x ? m_rGrabberContBox.Min.x : m_rEdgeContBox.Min.x,
                    m_rGrabberContBox.Min.y < m_rEdgeContBox.Min.y ? m_rGrabberContBox.Min.y : m_rEdgeContBox.Min.y,
                    m_rGrabberContBox.Max.x > m_rEdgeContBox.Max.x ? m_rGrabberContBox.Max.x : m_rEdgeContBox.Max.x,
                    m_rGrabberContBox.Max.y > m_rEdgeContBox.Max.y ? m_rGrabberContBox.Max.y : m_rEdgeContBox.Max.y);
        }

        void UpdateGrabberContainBox()
        {
            const auto& pointSizeHalf = m_owner->m_v2PointSizeHalf;
            const auto& grabberRadiusOutter = m_owner->m_fGrabberRadius;
            ImRect result(m_v2Pos-pointSizeHalf, m_v2Pos+pointSizeHalf);
            if (m_bEnableBezier)
            {
                const auto grabber0Center = m_v2Pos+m_v2Grabber0Offset;
                auto c = grabber0Center.x-grabberRadiusOutter;
                if (c < result.Min.x)
                    result.Min.x = c;
                c = grabber0Center.x+grabberRadiusOutter;
                if (c > result.Max.x)
                    result.Max.x = c;
                c = grabber0Center.y-grabberRadiusOutter;
                if (c < result.Min.y)
                    result.Min.y = c;
                c = grabber0Center.y+grabberRadiusOutter;
                if (c > result.Max.y)
                    result.Max.y = c;
                const auto grabber1Center = m_v2Pos+m_v2Grabber1Offset;
                c = grabber1Center.x-grabberRadiusOutter;
                if (c < result.Min.x)
                    result.Min.x = c;
                c = grabber1Center.x+grabberRadiusOutter;
                if (c > result.Max.x)
                    result.Max.x = c;
                c = grabber1Center.y-grabberRadiusOutter;
                if (c < result.Min.y)
                    result.Min.y = c;
                c = grabber1Center.y+grabberRadiusOutter;
                if (c > result.Max.y)
                    result.Max.y = c;
            }
            m_rGrabberContBox = result;
        }

        bool CheckHovering(const ImVec2& mousePos)
        {
            if (CheckGrabberHovering(mousePos))
                return true;
            return CheckContourHovering(mousePos);
        }

        bool CheckGrabberHovering(const ImVec2& mousePos)
        {
            const ImVec2 szDetectExRadius(m_owner->m_fHoverDetectExRadius, m_owner->m_fHoverDetectExRadius);
            const auto& grabberRadius = m_owner->m_fGrabberRadius;
            const auto bKeyFrameEnabled = m_owner->m_bKeyFrameEnabled;
            const auto v2Pos = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrPos) : m_v2Pos;
            const auto v2Grabber0Offset = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrGrabber0Offset) : m_v2Grabber0Offset;
            const auto v2Grabber1Offset = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrGrabber1Offset) : m_v2Grabber1Offset;
            if (m_bEnableBezier)
            {
                const ImVec2 radiusSize(grabberRadius, grabberRadius);
                const auto grabber0Center = v2Pos+v2Grabber0Offset;
                const ImRect grabber0Rect(grabber0Center-radiusSize-szDetectExRadius, grabber0Center+radiusSize+szDetectExRadius);
                if (grabber0Rect.Contains(mousePos))
                {
                    m_bHovered = true;
                    m_iHoverType = 1;
                    return true;
                }
                const auto grabber1Center = v2Pos+v2Grabber1Offset;
                const ImRect grabber1Rect(grabber1Center-radiusSize-szDetectExRadius, grabber1Center+radiusSize+szDetectExRadius);
                if (grabber1Rect.Contains(mousePos))
                {
                    m_bHovered = true;
                    m_iHoverType = 2;
                    return true;
                }
            }
            const auto& pointSizeHalf = m_owner->m_v2PointSizeHalf;
            const ImRect pointRect(v2Pos-pointSizeHalf-szDetectExRadius, v2Pos+pointSizeHalf+szDetectExRadius);
            if (pointRect.Contains(mousePos))
            {
                m_bHovered = true;
                m_iHoverType = 0;
                return true;
            }
            return false;
        }

        bool CheckContourHovering(const ImVec2& mousePos)
        {
            const auto& contourThickness = m_owner->m_fContourThickness;
            const auto& hoverDetectExtension = m_owner->m_fContourHoverDetectExRadius;
            if (!m_aEdgeVertices.empty() && m_rEdgeContBox.Contains(mousePos))
            {
                auto itVt0 = m_aEdgeVertices.begin();
                auto itVt1 = itVt0; itVt1++;
                int idx = 0;
                while (itVt1 != m_aEdgeVertices.end())
                {
                    const auto& v0 = *itVt0++;
                    const auto& v1 = *itVt1++;
                    float minX, minY, maxX, maxY;
                    if (v0.x < v1.x) { minX = v0.x; maxX = v1.x; }
                    else { minX = v1.x; maxX = v0.x; }
                    if (v0.y < v1.y) { minY = v0.y; maxY = v1.y; }
                    else { minY = v1.y; maxY = v0.y; }
                    if (minX != maxX && mousePos.x >= minX && mousePos.x <= maxX)
                    {
                        float crossY = (mousePos.x-v0.x)*(v1.y-v0.y)/(v1.x-v0.x)+v0.y;
                        if (abs(crossY-mousePos.y) < contourThickness+hoverDetectExtension)
                        {
                            m_bHovered = true;
                            m_iHoverType = 4;
                            m_v2HoverPointOnContour = {mousePos.x, crossY};
                            // m_owner->m_pLogger->Log(DEBUG) << "--> mousePos(" << mousePos.x << ", " << mousePos.y << "), crossY-hoverPoint(" << mousePos.x << ", " << crossY << ")" << endl;
                            m_iHoverPointOnContourIdx = idx;
                            return true;
                        }
                        // else
                        // {
                        //     m_owner->m_pLogger->Log(DEBUG) << "[" << i << "] mousePos(" << mousePos.x << ", " << mousePos.y << "), crossY=" << crossY << ", v0(" << v0.x << ", " << v0.y << "), v1(" << v1.x << ", " << v1.y << ")"
                        //             << ", abs(crossY-mousePos.y)=" << abs(crossY-mousePos.y) << endl;
                        // }
                    }
                    if (minY != maxY && mousePos.y >= minY && mousePos.y <= maxY)
                    {
                        float crossX = (mousePos.y-v0.y)*(v1.x-v0.x)/(v1.y-v0.y)+v0.x;
                        if (abs(crossX-mousePos.x) < contourThickness+hoverDetectExtension)
                        {
                            m_bHovered = true;
                            m_iHoverType = 4;
                            m_v2HoverPointOnContour = {crossX, mousePos.y};
                            // m_owner->m_pLogger->Log(DEBUG) << "--> mousePos(" << mousePos.x << ", " << mousePos.y << "), crossX-hoverPoint(" << crossX << ", " << mousePos.y << ")" << endl;
                            m_iHoverPointOnContourIdx = idx;
                            return true;
                        }
                        // else
                        // {
                        //     m_owner->m_pLogger->Log(DEBUG) << "[" << i << "] mousePos(" << mousePos.x << ", " << mousePos.y << "), crossX=" << crossX << ", v0(" << v0.x << ", " << v0.y << "), v1(" << v1.x << ", " << v1.y << ")"
                        //             << ", abs(crossX-mousePos.x)=" << abs(crossX-mousePos.x) << endl;
                        // }
                    }
                    idx++;
                }
            }
            return false;
        }

        void QuitHover()
        {
            m_bHovered = false;
            m_iHoverType = -1;
        }

        void DrawPoint(ImDrawList* pDrawList) const
        {
            const auto bKeyFrameEnabled = m_owner->m_bKeyFrameEnabled;
            const auto v2Pos = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrPos) : m_v2Pos;
            const auto v2Grabber0Offset = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrGrabber0Offset) : m_v2Grabber0Offset;
            const auto v2Grabber1Offset = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrGrabber1Offset) : m_v2Grabber1Offset;
            const auto& pointSizeHalf = m_owner->m_v2PointSizeHalf;
            const auto& pointColor = m_owner->m_u32PointColor;
            const auto& pointBorderThickness = m_owner->m_bKeyFrameEnabled && m_bSelected ? m_owner->m_fPointBorderSelectedThickness : m_owner->m_fPointBorderThickness;
            const auto& pointBorderColor = m_owner->m_u32PointBorderColor;
            const auto& pointBorderHoverColor = m_owner->m_u32PointBorderHoverColor;
            const auto& pointBorderSelectedColor = m_owner->m_u32PointBorderSelectedColor;
            const auto grabberRadiusInner = m_owner->m_fGrabberRadius-m_owner->m_fGrabberBorderThickness;
            const auto& grabberColorInner = m_owner->m_u32GrabberColor;
            const auto& grabberRadiusOutter = m_owner->m_fGrabberRadius;
            const auto& grabberColorOutter = m_owner->m_u32GrabberBorderColor;
            const auto& grabberColorHoverOutter = m_owner->m_u32GrabberBorderHoverColor;
            const auto& lineThickness = m_owner->m_fGrabberLineThickness;
            const auto& lineColor = m_owner->m_u32GrabberLineColor;
            const auto vPoint2UiPos = m_owner->ToUiPos(v2Pos);
            if (m_bEnableBezier)
            {
                const auto grabber0Center = m_owner->ToUiPos(v2Pos+v2Grabber0Offset);
                const auto grabber1Center = m_owner->ToUiPos(v2Pos+v2Grabber1Offset);
                pDrawList->AddLine(vPoint2UiPos, grabber0Center, lineColor, lineThickness);
                pDrawList->AddLine(vPoint2UiPos, grabber1Center, lineColor, lineThickness);
                auto borderColor = m_bHovered && m_iHoverType==1 ? grabberColorHoverOutter : grabberColorOutter;
                pDrawList->AddCircleFilled(grabber0Center, grabberRadiusOutter, borderColor);
                pDrawList->AddCircleFilled(grabber0Center, grabberRadiusInner, grabberColorInner);
                borderColor = m_bHovered && m_iHoverType==2 ? grabberColorHoverOutter : grabberColorOutter;
                pDrawList->AddCircleFilled(grabber1Center, grabberRadiusOutter, borderColor);
                pDrawList->AddCircleFilled(grabber1Center, grabberRadiusInner, grabberColorInner);
            }
            const ImVec2 offsetSize1(pointSizeHalf.x+pointBorderThickness, pointSizeHalf.y+pointBorderThickness);
            auto borderColor = m_bHovered && m_iHoverType==0 ? pointBorderHoverColor : (m_owner->m_bKeyFrameEnabled && m_bSelected ? pointBorderSelectedColor : pointBorderColor);
            pDrawList->AddRectFilled(vPoint2UiPos-offsetSize1, vPoint2UiPos+offsetSize1-ImVec2(1,1), borderColor);
            const auto& offsetSize2 = pointSizeHalf;
            pDrawList->AddRectFilled(vPoint2UiPos-offsetSize2, vPoint2UiPos+offsetSize2, pointColor);
        }

        void CalcEdgeVertices(const RoutePointImpl& prevVt)
        {
            const auto bKeyFrameEnabled = m_owner->m_bKeyFrameEnabled;
            const auto v2PtPos = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrPos) : m_v2Pos;
            const auto v2PrevPtPos = bKeyFrameEnabled ? MatUtils::ToImVec2(prevVt.m_ptCurrPos) : prevVt.m_v2Pos;
            if (!prevVt.m_bEnableBezier && !m_bEnableBezier)
            {
                m_aEdgeVertices.clear();
                m_aEdgeVertices.push_back(v2PrevPtPos);
                m_aEdgeVertices.push_back(v2PtPos);
                UpdateEdgeContianBox();
                return;
            }

            const auto v2Grabber0Offset = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrGrabber0Offset) : m_v2Grabber0Offset;
            const auto v2PrevGrabber1Offset = bKeyFrameEnabled ? MatUtils::ToImVec2(prevVt.m_ptCurrGrabber1Offset) : prevVt.m_v2Grabber1Offset;
            ImVec2 bzVts[] = {v2PrevPtPos, v2PrevGrabber1Offset, v2Grabber0Offset, v2PtPos};
            m_aEdgeVertices = CalcBezierCurve(bzVts);
            UpdateEdgeContianBox();
        }

        void UpdateEdgeContianBox()
        {
            const auto contourHoverRadius = m_owner->m_fContourThickness+m_owner->m_fContourHoverDetectExRadius;
            if (m_aEdgeVertices.empty())
            {
                m_rEdgeContBox = {-1, -1, -1, -1};
                return;
            }
            auto iter = m_aEdgeVertices.begin();
            auto& firstVertex = *iter++;
            ImRect rContainBox = { firstVertex.x-contourHoverRadius, firstVertex.y-contourHoverRadius, firstVertex.x+contourHoverRadius, firstVertex.y+contourHoverRadius };
            while (iter != m_aEdgeVertices.end())
            {
                auto& v = *iter++;
                if (v.x-contourHoverRadius < rContainBox.Min.x)
                    rContainBox.Min.x = v.x-contourHoverRadius;
                if (v.y-contourHoverRadius < rContainBox.Min.y)
                    rContainBox.Min.y = v.y-contourHoverRadius;
                if (v.x+contourHoverRadius > rContainBox.Max.x)
                    rContainBox.Max.x = v.x+contourHoverRadius;
                if (v.y+contourHoverRadius > rContainBox.Max.y)
                    rContainBox.Max.y = v.y+contourHoverRadius;
            }
            m_rEdgeContBox = rContainBox;
        }

        void DrawContour(ImDrawList* pDrawList, const RoutePointImpl& prevVt) const
        {
            const auto& contourColor = m_owner->m_u32ContourColor;
            const auto& contourThickness = m_owner->m_fContourThickness;
            if (!m_aEdgeVertices.empty())
            {
                auto itVt0 = m_aEdgeVertices.begin();
                auto itVt1 = itVt0; itVt1++;
                while (itVt1 != m_aEdgeVertices.end())
                {
                    const auto v0 = m_owner->ToUiPos(*itVt0++);
                    const auto v1 = m_owner->ToUiPos(*itVt1++);
                    pDrawList->AddLine(v0, v1, contourColor, contourThickness);
                }
            }
            else
            {
                const auto bKeyFrameEnabled = m_owner->m_bKeyFrameEnabled;
                const auto v2Pos = bKeyFrameEnabled ? MatUtils::ToImVec2(m_ptCurrPos) : m_v2Pos;
                const auto v2PrevPos = bKeyFrameEnabled ? MatUtils::ToImVec2(prevVt.m_ptCurrPos) : prevVt.m_v2Pos;
                const auto v0 = m_owner->ToUiPos(v2PrevPos);
                const auto v1 = m_owner->ToUiPos(v2Pos);
                pDrawList->AddLine(v0, v1, contourColor, contourThickness);
            }
        }
    };

    struct MorphController
    {
        MaskCreatorImpl* m_owner;
        ImVec2 m_ptRootPos;
        ImVec2 m_ptGrabberPos;
        bool m_bInsidePoly{false};
        float m_fCtrlSlope;
        float m_fMorphCtrlLength;
        float m_fDistance{0.f};
        int m_iMorphIterations{0};
        ImVec2 m_ptFeatherGrabberPos;
        float m_fFeatherCtrlLength{0};
        int m_iFeatherIterations{0};
        bool m_bIsHovered{false};
        int m_iHoverType{-1};

        static const int MIN_CTRL_LENGTH{30};

        MorphController(MaskCreatorImpl* owner)
            : m_owner(owner), m_ptRootPos({-1, -1}), m_fCtrlSlope(0)
            , m_fMorphCtrlLength(MIN_CTRL_LENGTH)
        {}

        json::value ToJson() const
        {
            json::value j;
            j["ctrl_length"] = json::number(m_fMorphCtrlLength);
            j["iterations"] = json::number(m_iMorphIterations);
            j["inside_polygon"] = m_bInsidePoly;
            j["distance"] = json::number(m_fDistance);
            j["feather_ctrl_length"] = json::number(m_fFeatherCtrlLength);
            j["feather_iterations"] = json::number(m_iFeatherIterations);
            return std::move(j);
        }

        void FromJson(const json::value& j)
        {
            m_fMorphCtrlLength = j["ctrl_length"].get<json::number>();
            m_iMorphIterations = j["iterations"].get<json::number>();
            m_bInsidePoly = j["inside_polygon"].get<json::boolean>();
            m_fDistance = j["distance"].get<json::number>();
            m_fFeatherCtrlLength = j["feather_ctrl_length"].get<json::number>();
            m_iFeatherIterations = j["feather_iterations"].get<json::number>();
        }

        void Reset()
        {
            m_ptRootPos = {-1, -1};
            m_fCtrlSlope = 0;
            m_fDistance = 0;
            m_bIsHovered = false;
            m_iHoverType = -1;
        }

        list<RoutePointImpl>::const_iterator SetPosAndSlope(ImVec2& ptRootPos, float fCtrlSlope, int iLineIdx)
        {
            const auto& aRoutePoints = m_owner->m_aRoutePointsForUi;
            const auto& aContourVertices = m_owner->m_aContourVerticesForUi;
            auto itCv = aContourVertices.begin();
            auto itCp = aRoutePoints.begin();
            auto itCpSub1 = itCp->m_aEdgeVertices.begin();
            auto itCpSub2 = itCpSub1; itCpSub2++;
            int idx = 0;
            while (itCv != aContourVertices.end() && idx < iLineIdx)
            {
                idx++; itCv++;
                itCpSub1++; itCpSub2++;
                if (itCpSub2 == itCp->m_aEdgeVertices.end())
                {
                    itCp++; if (itCp == aRoutePoints.end()) itCp = aRoutePoints.begin();
                    itCpSub1 = itCp->m_aEdgeVertices.begin();
                    itCpSub2 = itCpSub1; itCpSub2++;
                }
            }
            assert(*itCv == *itCpSub1);

            auto v2Dist = CalcRootPosDist(*itCp, itCpSub1, ptRootPos);
            const auto fMinDist = m_owner->m_v2PointSizeHalf.x+m_owner->m_fHoverDetectExRadius;
            if (v2Dist.x+v2Dist.y < fMinDist*2)
            {
                auto itSearchCp = itCp;
                itSearchCp++; if (itSearchCp == aRoutePoints.end()) itSearchCp = aRoutePoints.begin();
                ImVec2 ptSearchRootPos, v2SearchDist;
                float fSearchCtrlSlope;
                bool bFoundCp = false;
                while (itSearchCp != itCp)
                {
                    v2SearchDist = CalcRootPosDist(*itSearchCp, fMinDist, ptSearchRootPos, fSearchCtrlSlope);
                    if (v2SearchDist.x+v2SearchDist.y >= fMinDist*2)
                    {
                        bFoundCp = true;
                        break;
                    }
                    itSearchCp++; if (itSearchCp == aRoutePoints.end()) itSearchCp = aRoutePoints.begin();
                }
                if (bFoundCp)
                {
                    ptRootPos = ptSearchRootPos;
                    fCtrlSlope = fSearchCtrlSlope;
                    itCp = itSearchCp;
                    v2Dist = v2SearchDist;
                }
            }
            else if (v2Dist.y < fMinDist)
            {
                auto fTargetDist = v2Dist.x+v2Dist.y-fMinDist;
                v2Dist = CalcRootPosDist(*itCp, fTargetDist, ptRootPos, fCtrlSlope);
            }
            else if (v2Dist.x < fMinDist)
            {
                v2Dist = CalcRootPosDist(*itCp, fMinDist, ptRootPos, fCtrlSlope);
            }

            m_ptRootPos = ptRootPos;
            m_fCtrlSlope = fCtrlSlope;
            m_fDistance =  v2Dist.x;
            CalcGrabberPos();
            return itCp;
        }

        list<RoutePointImpl>::const_iterator SetPosAndSlope(list<RoutePointImpl>::const_iterator itCp, float fTargetDist)
        {
            ImVec2 ptRootPos, v2Dist;
            float fCtrlSlope;
            bool bFoundCp = false;
            const auto fMinDist = m_owner->m_v2PointSizeHalf.x+m_owner->m_fHoverDetectExRadius;
            auto fTargetDist2 = fTargetDist < fMinDist ? fMinDist : fTargetDist;
            v2Dist = CalcRootPosDist(*itCp, fTargetDist2, ptRootPos, fCtrlSlope);
            if (v2Dist.x+v2Dist.y < fMinDist*2)
            {
                auto itSearchCp = itCp;
                itSearchCp++; if (itSearchCp == m_owner->m_aRoutePointsForUi.end()) itSearchCp = m_owner->m_aRoutePointsForUi.begin();
                ImVec2 ptSearchRootPos, v2SearchDist;
                float fSearchCtrlSlope;
                while (itSearchCp != itCp)
                {
                    v2SearchDist = CalcRootPosDist(*itSearchCp, fMinDist, ptSearchRootPos, fSearchCtrlSlope);
                    if (v2SearchDist.x+v2SearchDist.y >= fMinDist*2)
                    {
                        bFoundCp = true;
                        break;
                    }
                    itSearchCp++; if (itSearchCp == m_owner->m_aRoutePointsForUi.end()) itSearchCp = m_owner->m_aRoutePointsForUi.begin();
                }
                if (bFoundCp)
                {
                    ptRootPos = ptSearchRootPos;
                    fCtrlSlope = fSearchCtrlSlope;
                    itCp = itSearchCp;
                    v2Dist = v2SearchDist;
                }
            }
            else
            {
                bFoundCp = true;
                if (fTargetDist < fMinDist)
                {
                    fTargetDist2 = (v2Dist.x+v2Dist.y)/2;
                    v2Dist = CalcRootPosDist(*itCp, fTargetDist2, ptRootPos, fCtrlSlope);
                }
                else if (v2Dist.y < fMinDist)
                {
                    fTargetDist2 = v2Dist.x+v2Dist.y-fMinDist;
                    v2Dist = CalcRootPosDist(*itCp, fTargetDist2, ptRootPos, fCtrlSlope);
                }
            }

            m_ptRootPos = ptRootPos;
            m_fCtrlSlope = fCtrlSlope;
            m_fDistance = v2Dist.x;
            CalcGrabberPos();
            return itCp;
        }

        ImVec2 CalcRootPosDist(const RoutePointImpl& cp, list<ImVec2>::const_iterator itCpSubTarget, const ImVec2& ptRootPos) const 
        {
            float fDist1 = 0, fDist2 = 0;
            auto itCpSub2 = cp.m_aEdgeVertices.begin();
            auto itCpSub1 = cp.m_aEdgeVertices.end();
            while (itCpSub2 != itCpSubTarget)
            {
                if (itCpSub1 != cp.m_aEdgeVertices.end())
                {
                    const auto dx = itCpSub2->x-itCpSub1->x;
                    const auto dy = itCpSub2->y-itCpSub1->y;
                    fDist1 += sqrt(dx*dx+dy*dy);
                }
                itCpSub1 = itCpSub2++;
            }
            {
                const auto dx = ptRootPos.x-itCpSub2->x;
                const auto dy = ptRootPos.y-itCpSub2->y;
                fDist1 += sqrt(dx*dx+dy*dy);
            }
            {
                itCpSub2++;
                const auto dx = ptRootPos.x-itCpSub2->x;
                const auto dy = ptRootPos.y-itCpSub2->y;
                fDist2 = sqrt(dx*dx+dy*dy);
                itCpSub1 = itCpSub2++;
            }
            while (itCpSub2 != cp.m_aEdgeVertices.end())
            {
                const auto dx = itCpSub2->x-itCpSub1->x;
                const auto dy = itCpSub2->y-itCpSub1->y;
                fDist2 += sqrt(dx*dx+dy*dy);
                itCpSub1 = itCpSub2++;
            }
            return ImVec2(fDist1, fDist2);
        }

        ImVec2 CalcRootPosDist(const RoutePointImpl& cp, float fTargetDist1, ImVec2& ptRootPos, float& fVertSlope) const
        {
            float fDist1 = 0, fDist2 = 0;
            auto itCpSub2 = cp.m_aEdgeVertices.begin();
            auto itCpSub1 = cp.m_aEdgeVertices.end();
            while (itCpSub2 != cp.m_aEdgeVertices.end())
            {
                if (itCpSub1 != cp.m_aEdgeVertices.end())
                {
                    const auto dx = itCpSub2->x-itCpSub1->x;
                    const auto dy = itCpSub2->y-itCpSub1->y;
                    const auto l = sqrt(dx*dx+dy*dy);
                    if (fDist1+l <= fTargetDist1)
                        fDist1 += l;
                    else
                    {
                        fDist2 = fDist1+l-fTargetDist1;
                        fDist1 = fTargetDist1;
                        const auto fSlope = itCpSub1->x == itCpSub2->x ? numeric_limits<float>::infinity() : (itCpSub1->y-itCpSub2->y)/(itCpSub1->x-itCpSub2->x);
                        if (isinf(fSlope))
                        {
                            ptRootPos.x = itCpSub1->x;
                            ptRootPos.y = itCpSub2->y+(itCpSub2->y > itCpSub1->y ? -fDist2 : fDist2);
                            fVertSlope = 0;
                        }
                        else
                        {
                            const auto u = 1/sqrt(fSlope*fSlope+1);
                            const auto d = itCpSub1->x < itCpSub2->x ? -fDist2 : fDist2;
                            ptRootPos.x = itCpSub2->x+d*u;
                            ptRootPos.y = itCpSub2->y+d*u*fSlope;
                            fVertSlope = fSlope == 0 ? numeric_limits<float>::infinity() : -1/fSlope;
                        }
                        break;
                    }
                }
                itCpSub1 = itCpSub2++;
            }
            if (itCpSub2 != cp.m_aEdgeVertices.end())
                itCpSub1 = itCpSub2++;
            while (itCpSub2 != cp.m_aEdgeVertices.end())
            {
                const auto dx = itCpSub2->x-itCpSub1->x;
                const auto dy = itCpSub2->y-itCpSub1->y;
                fDist2 += sqrt(dx*dx+dy*dy);
                itCpSub1 = itCpSub2++;
            }
            return ImVec2(fDist1, fDist2);
        }

        void CalcGrabberPos()
        {
            float x, y;
            const auto ratio = 1/sqrt(m_fCtrlSlope*m_fCtrlSlope+1);
            if (isinf(m_fCtrlSlope))
            {
                x = m_ptRootPos.x;
                y = m_ptRootPos.y+1;
            }
            else
            {
                x = m_ptRootPos.x+ratio;
                y = m_ptRootPos.y+ratio*m_fCtrlSlope;
            }
            const auto& aContourVertices = m_owner->m_aContourVerticesForUi;
            bool bIsInside = CheckPointInsidePolygon({x, y}, aContourVertices);
            float fLength = m_fMorphCtrlLength;
            if (m_bInsidePoly^bIsInside)
                fLength = -fLength;
            if (isinf(m_fCtrlSlope))
            {
                x = m_ptRootPos.x;
                y = m_ptRootPos.y+fLength;
            }
            else
            {
                const auto u = fLength*ratio;
                x = m_ptRootPos.x+u;
                y = m_ptRootPos.y+u*m_fCtrlSlope;
            }
            m_ptGrabberPos = {x, y};
            const auto fFeatherGrabberDistance = m_fFeatherCtrlLength+m_owner->m_fGrabberRadius*2+m_owner->m_fHoverDetectExRadius;
            fLength = m_bInsidePoly^bIsInside ? fLength-fFeatherGrabberDistance : fLength+fFeatherGrabberDistance;
            if (isinf(m_fCtrlSlope))
            {
                x = m_ptRootPos.x;
                y = m_ptRootPos.y+fLength;
            }
            else
            {
                const auto u = fLength*ratio;
                x = m_ptRootPos.x+u;
                y = m_ptRootPos.y+u*m_fCtrlSlope;
            }
            m_ptFeatherGrabberPos = {x, y};
        }

        bool CheckHovering(const ImVec2& mousePos)
        {
            const ImVec2 szDetectExRadius(m_owner->m_fHoverDetectExRadius, m_owner->m_fHoverDetectExRadius);
            const auto& pointSizeHalf = m_owner->m_v2PointSizeHalf;
            const auto& grabberRadius = m_owner->m_fGrabberRadius;
            const ImRect rootPosRect(m_ptRootPos-pointSizeHalf-szDetectExRadius, m_ptRootPos+pointSizeHalf+szDetectExRadius);
            if (rootPosRect.Contains(mousePos))
            {
                m_bIsHovered = true;
                m_iHoverType = 0;
                return true;
            }
            const ImVec2 radiusSize(grabberRadius, grabberRadius);
            ImRect grabberRect(m_ptGrabberPos-radiusSize-szDetectExRadius, m_ptGrabberPos+radiusSize+szDetectExRadius);
            if (grabberRect.Contains(mousePos))
            {
                m_bIsHovered = true;
                m_iHoverType = 1;
                return true;
            }
            grabberRect = {m_ptFeatherGrabberPos-radiusSize-szDetectExRadius, m_ptFeatherGrabberPos+radiusSize+szDetectExRadius};
            if (grabberRect.Contains(mousePos))
            {
                m_bIsHovered = true;
                m_iHoverType = 2;
                return true;
            }
            m_bIsHovered = false;
            m_iHoverType = -1;
            return false;
        }

        void QuitHover()
        {
            m_bIsHovered = false;
        }

        void Draw(ImDrawList* pDrawList) const
        {
            const ImVec2 rootPos = m_owner->ToUiPos(m_ptRootPos);
            ImVec2 grabberPos = m_owner->ToUiPos(m_ptFeatherGrabberPos);
            pDrawList->AddLine(rootPos, grabberPos, m_owner->m_u32FeatherGrabberColor, m_owner->m_fContourThickness);
            grabberPos = m_owner->ToUiPos(m_ptGrabberPos);
            pDrawList->AddLine(rootPos, grabberPos, m_owner->m_u32ContourColor, m_owner->m_fContourThickness);
            const auto& offsetSize1 = m_owner->m_v2PointSizeHalf;
            auto borderColor = m_iHoverType == 0 ? m_owner->m_u32PointBorderHoverColor : m_owner->m_u32PointBorderColor;
            pDrawList->AddRectFilled(rootPos-offsetSize1, rootPos+offsetSize1, borderColor);
            const ImVec2 offsetSize2(m_owner->m_v2PointSizeHalf.x-m_owner->m_fPointBorderThickness, m_owner->m_v2PointSizeHalf.y-m_owner->m_fPointBorderThickness);
            pDrawList->AddRectFilled(rootPos-offsetSize2, rootPos+offsetSize2, m_owner->m_u32ContourColor);
            borderColor = m_iHoverType == 1 ? m_owner->m_u32GrabberBorderHoverColor : m_owner->m_u32GrabberBorderColor;
            pDrawList->AddCircleFilled(grabberPos, m_owner->m_fGrabberRadius, borderColor);
            pDrawList->AddCircleFilled(grabberPos, m_owner->m_fGrabberRadius-m_owner->m_fGrabberBorderThickness, m_owner->m_u32GrabberColor);
            grabberPos = m_owner->ToUiPos(m_ptFeatherGrabberPos);
            borderColor = m_iHoverType == 2 ? m_owner->m_u32GrabberBorderHoverColor : m_owner->m_u32GrabberBorderColor;
            pDrawList->AddCircleFilled(grabberPos, m_owner->m_fGrabberRadius, borderColor);
            pDrawList->AddCircleFilled(grabberPos, m_owner->m_fGrabberRadius-m_owner->m_fGrabberBorderThickness, m_owner->m_u32FeatherGrabberColor);
        }
    };

    struct BezierTable
    {
        using Holder = shared_ptr<BezierTable>;

        int m_steps;
        float* m_C;
        chrono::time_point<chrono::system_clock> m_tpLastRefTime;

        BezierTable(int steps) : m_steps(steps)
        {
            m_C = new float[(steps+1)*4];
            for (int step = 0; step <= steps; ++step)
            {
                float t = (float)step/(float)steps;
                m_C[step*4+0] = (1-t)*(1-t)*(1-t);
                m_C[step*4+1] = 3*(1-t)*(1-t)*t;
                m_C[step*4+2] = 3*(1-t)*t*t;
                m_C[step*4+3] = t*t*t;
            }
            m_tpLastRefTime = chrono::system_clock::now();
        }

        ~BezierTable()
        {
            if (m_steps)
                delete [] m_C;
        }

        const float* GetBezierConstant()
        {
            m_tpLastRefTime = chrono::system_clock::now();
            return m_C;
        }

        template <class Rep, class Period>
        bool NotUsedInTime(const chrono::time_point<chrono::system_clock>& tpNow, const chrono::duration<Rep, Period>& dur) const
        {return tpNow-m_tpLastRefTime > dur; }

        const chrono::time_point<chrono::system_clock>& LastRefTime() const
        { return m_tpLastRefTime; }
    };

    static unordered_map<int, BezierTable::Holder> s_mapBezierTables;
    static int s_iKeepBezierTableCountMin;
    static chrono::seconds s_iKeepBezierTableTimeOut;

    static BezierTable::Holder GetBezierTable(int steps)
    {
        BezierTable::Holder hRes;
        auto iter = s_mapBezierTables.find(steps);
        if (iter != s_mapBezierTables.end())
            hRes = iter->second;
        else
        {
            hRes = BezierTable::Holder(new BezierTable(steps));
            s_mapBezierTables[steps] = hRes;
        }

        if (s_mapBezierTables.size() > s_iKeepBezierTableCountMin)
        {
            list<BezierTable::Holder> refTimePriList;
            for (auto& elem : s_mapBezierTables)
                refTimePriList.push_back(elem.second);
            refTimePriList.sort([] (auto& a, auto& b) {
                return a->LastRefTime() < b->LastRefTime();
            });

            auto tpNow = chrono::system_clock::now();
            auto iter = refTimePriList.begin();
            while (iter != refTimePriList.end() && s_mapBezierTables.size() > s_iKeepBezierTableCountMin)
            {
                auto hBt = *iter++;
                if (hBt->NotUsedInTime(tpNow, s_iKeepBezierTableTimeOut))
                    s_mapBezierTables.erase(hBt->m_steps);
            }
        }
        return hRes;
    }

    static list<ImVec2> CalcBezierCurve(ImVec2 v[4])
    {
        auto offsetVt = v[3]-v[0];
        const float distance = sqrt((double)offsetVt.x*offsetVt.x+(double)offsetVt.y*offsetVt.y);
        int log2value = (int)floor(log2(distance));
        if (log2value <= 0)
            return {v[0], v[3]};

        int steps = 1 << (log2value-1);
        auto hBt = GetBezierTable(steps);
        auto C = hBt->GetBezierConstant();
        static const float TRANS_FACTOR = 1e2/sqrt(2);
        static const float INV_TRANS_FACTOR = 1e-2/sqrt(2);
        bool bNeedInvTrans = false;
        if (abs(offsetVt.x) < 1e-2 || abs(offsetVt.y) < 1e-2)
        {
            ImVec2 tmp;
            tmp.x = v[1].x*TRANS_FACTOR-v[1].y*TRANS_FACTOR;
            tmp.y = v[1].x*TRANS_FACTOR+v[1].y*TRANS_FACTOR;
            v[1] = tmp;
            tmp.x = (offsetVt.x+v[2].x)*TRANS_FACTOR-(offsetVt.y+v[2].y)*TRANS_FACTOR;
            tmp.y = (offsetVt.x+v[2].x)*TRANS_FACTOR+(offsetVt.y+v[2].y)*TRANS_FACTOR;
            v[2] = tmp;
            tmp.x = offsetVt.x*TRANS_FACTOR-offsetVt.y*TRANS_FACTOR;
            tmp.y = offsetVt.x*TRANS_FACTOR+offsetVt.y*TRANS_FACTOR;
            offsetVt = tmp;
            v[2].x -= tmp.x;
            v[2].y -= tmp.y;
            bNeedInvTrans = true;
        }
        ImVec2 P[] = {
            { 0.f, 0.f },
            { v[1].x/offsetVt.x, v[1].y/offsetVt.y },
            { 1+v[2].x/offsetVt.x, 1+v[2].y/offsetVt.y },
            { 1.f, 1.f } };
        vector<ImVec2> v2BezierTable(steps+1);
        for (int step = 0; step <= steps; ++step)
        {
            ImVec2 point = {
                C[step*4+0]*P[0].x+C[step*4+1]*P[1].x+C[step*4+2]*P[2].x+C[step*4+3]*P[3].x,
                C[step*4+0]*P[0].y+C[step*4+1]*P[1].y+C[step*4+2]*P[2].y+C[step*4+3]*P[3].y
            };
            v2BezierTable[step] = point;
        }

        const int smoothness = v2BezierTable.size();
        list<ImVec2> res;
        if (bNeedInvTrans)
        {
            for (int i = 0; i < smoothness; i++)
            {
                const auto& p = v2BezierTable[i];
                const ImVec2 tmp(p.x*offsetVt.x, p.y*offsetVt.y);
                const float dx = tmp.x*INV_TRANS_FACTOR+tmp.y*INV_TRANS_FACTOR;
                const float dy = -tmp.x*INV_TRANS_FACTOR+tmp.y*INV_TRANS_FACTOR;
                res.push_back(ImVec2(dx+v[0].x, dy+v[0].y));
            }
        }
        else
        {
            for (int i = 0; i < smoothness; i++)
            {
                const auto& p = v2BezierTable[i];
                res.push_back(ImVec2(p.x*offsetVt.x+v[0].x, p.y*offsetVt.y+v[0].y));
            }
        }
        return res;
    }

    static list<MatUtils::Point2f> CalcBezierCurve(MatUtils::Point2f v[4])
    {
        auto offsetVt = v[3]-v[0];
        const float distance = sqrt((double)offsetVt.x*offsetVt.x+(double)offsetVt.y*offsetVt.y);
        int log2value = (int)floor(log2(distance));
        if (log2value <= 0)
            return {v[0], v[3]};

        int steps = 1 << (log2value-1);
        auto hBt = GetBezierTable(steps);
        auto C = hBt->GetBezierConstant();
        static const float TRANS_FACTOR = 1e2/sqrt(2);
        static const float INV_TRANS_FACTOR = 1e-2/sqrt(2);
        bool bNeedInvTrans = false;
        if (abs(offsetVt.x) < 1e-2 || abs(offsetVt.y) < 1e-2)
        {
            MatUtils::Point2f tmp;
            tmp.x = v[1].x*TRANS_FACTOR-v[1].y*TRANS_FACTOR;
            tmp.y = v[1].x*TRANS_FACTOR+v[1].y*TRANS_FACTOR;
            v[1] = tmp;
            tmp.x = (offsetVt.x+v[2].x)*TRANS_FACTOR-(offsetVt.y+v[2].y)*TRANS_FACTOR;
            tmp.y = (offsetVt.x+v[2].x)*TRANS_FACTOR+(offsetVt.y+v[2].y)*TRANS_FACTOR;
            v[2] = tmp;
            tmp.x = offsetVt.x*TRANS_FACTOR-offsetVt.y*TRANS_FACTOR;
            tmp.y = offsetVt.x*TRANS_FACTOR+offsetVt.y*TRANS_FACTOR;
            offsetVt = tmp;
            v[2].x -= tmp.x;
            v[2].y -= tmp.y;
            bNeedInvTrans = true;
        }
        MatUtils::Point2f P[] = {
            { 0.f, 0.f },
            { v[1].x/offsetVt.x, v[1].y/offsetVt.y },
            { 1+v[2].x/offsetVt.x, 1+v[2].y/offsetVt.y },
            { 1.f, 1.f } };
        vector<MatUtils::Point2f> v2BezierTable(steps+1);
        for (int step = 0; step <= steps; ++step)
        {
            MatUtils::Point2f point = {
                C[step*4+0]*P[0].x+C[step*4+1]*P[1].x+C[step*4+2]*P[2].x+C[step*4+3]*P[3].x,
                C[step*4+0]*P[0].y+C[step*4+1]*P[1].y+C[step*4+2]*P[2].y+C[step*4+3]*P[3].y
            };
            v2BezierTable[step] = point;
        }

        const int smoothness = v2BezierTable.size();
        list<MatUtils::Point2f> res;
        if (bNeedInvTrans)
        {
            for (int i = 0; i < smoothness; i++)
            {
                const auto& p = v2BezierTable[i];
                const MatUtils::Point2f tmp(p.x*offsetVt.x, p.y*offsetVt.y);
                const float dx = tmp.x*INV_TRANS_FACTOR+tmp.y*INV_TRANS_FACTOR;
                const float dy = -tmp.x*INV_TRANS_FACTOR+tmp.y*INV_TRANS_FACTOR;
                res.push_back(MatUtils::Point2f(dx+v[0].x, dy+v[0].y));
            }
        }
        else
        {
            for (int i = 0; i < smoothness; i++)
            {
                const auto& p = v2BezierTable[i];
                res.push_back(MatUtils::Point2f(p.x*offsetVt.x+v[0].x, p.y*offsetVt.y+v[0].y));
            }
        }
        return res;
    }

    static list<ImVec2> CalcBezierConnection(const ImVec2 v[4])
    {
        const double fSlope1 = v[1].x != v[0].x ? ((double)v[1].y-v[0].y)/((double)v[1].x-v[0].x) : numeric_limits<double>::infinity();
        const double fSlope2 = v[2].x != v[3].x ? ((double)v[2].y-v[3].y)/((double)v[2].x-v[3].x) : numeric_limits<double>::infinity();
        const auto vec1 = v[0]-v[1];
        const auto vec2 = v[3]-v[2];
        const auto dotVec12 = vec1.x*vec2.x+vec1.y*vec2.y;
        const auto dotNum = dotVec12*dotVec12;
        const auto dotDen = (vec1.x*vec1.x+vec1.y*vec1.y)*(vec2.x*vec2.x+vec2.y*vec2.y);

        const auto vOffset = v[2]-v[1];
        const double scale = dotVec12 > 0 ? 0.5*(1+dotNum/(dotDen*2)) : 0.5;
        const double fMorphLen = sqrt((double)vOffset.x*vOffset.x+(double)vOffset.y*vOffset.y)*scale;
        float dx1, dy1;
        if (isinf(fSlope1))
        {
            dx1 = 0;
            dy1 = v[1].y > v[0].y ? fMorphLen : -fMorphLen;
        }
        else
        {
            const double ratio = 1/sqrt(1+fSlope1*fSlope1);
            dx1 = v[1].x > v[0].x ? fMorphLen*ratio : -fMorphLen*ratio;
            dy1 = dx1*fSlope1;
        }
        float dx2, dy2;
        if (isinf(fSlope2))
        {
            dx2 = 0;
            dy2 = v[2].y > v[3].y ? fMorphLen : -fMorphLen;
        }
        else
        {
            const double ratio = 1/sqrt(1+fSlope2*fSlope2);
            dx2 = v[2].x > v[3].x ? fMorphLen*ratio : -fMorphLen*ratio;
            dy2 = dx2*fSlope2;
        }
        ImVec2 v2[] = {v[1], {dx1, dy1}, {dx2, dy2}, v[2]};
        return CalcBezierCurve(v2);
    }

    static list<MatUtils::Point2f> CalcBezierConnection(const MatUtils::Point2f v[4])
    {
        const double fSlope1 = v[1].x != v[0].x ? ((double)v[1].y-v[0].y)/((double)v[1].x-v[0].x) : numeric_limits<double>::infinity();
        const double fSlope2 = v[2].x != v[3].x ? ((double)v[2].y-v[3].y)/((double)v[2].x-v[3].x) : numeric_limits<double>::infinity();
        const auto vec1 = v[0]-v[1];
        const auto vec2 = v[3]-v[2];
        const auto dotVec12 = vec1.x*vec2.x+vec1.y*vec2.y;
        const auto dotNum = dotVec12*dotVec12;
        const auto dotDen = (vec1.x*vec1.x+vec1.y*vec1.y)*(vec2.x*vec2.x+vec2.y*vec2.y);

        const auto vOffset = v[2]-v[1];
        const double scale = dotVec12 > 0 ? 0.5*(1+dotNum/(dotDen*2)) : 0.5;
        const double fMorphLen = sqrt((double)vOffset.x*vOffset.x+(double)vOffset.y*vOffset.y)*scale;
        float dx1, dy1;
        if (isinf(fSlope1))
        {
            dx1 = 0;
            dy1 = v[1].y > v[0].y ? fMorphLen : -fMorphLen;
        }
        else
        {
            const double ratio = 1/sqrt(1+fSlope1*fSlope1);
            dx1 = v[1].x > v[0].x ? fMorphLen*ratio : -fMorphLen*ratio;
            dy1 = dx1*fSlope1;
        }
        float dx2, dy2;
        if (isinf(fSlope2))
        {
            dx2 = 0;
            dy2 = v[2].y > v[3].y ? fMorphLen : -fMorphLen;
        }
        else
        {
            const double ratio = 1/sqrt(1+fSlope2*fSlope2);
            dx2 = v[2].x > v[3].x ? fMorphLen*ratio : -fMorphLen*ratio;
            dy2 = dx2*fSlope2;
        }
        MatUtils::Point2f v2[] = {v[1], {dx1, dy1}, {dx2, dy2}, v[2]};
        return CalcBezierCurve(v2);
    }

private:
    void DrawPoint(ImDrawList* pDrawList, const RoutePointImpl& v) const
    {
        v.DrawPoint(pDrawList);
    }

    void DrawContour(ImDrawList* pDrawList, const RoutePointImpl& v0, const RoutePointImpl& v1) const
    {
        v1.DrawContour(pDrawList, v0);
    }

    void DrawMorphController(ImDrawList* pDrawList) const
    {
        m_tMorphCtrl.Draw(pDrawList);
    }

    ImVec2 ToUiPos(const ImVec2& v2Pos, bool bAddOrigin = true) const
    {
        ImVec2 v2UiPos(v2Pos.x*m_aUiFinalWarpAffineMatrix[0][0]+v2Pos.y*m_aUiFinalWarpAffineMatrix[0][1]+m_aUiFinalWarpAffineMatrix[0][2],
                      v2Pos.x*m_aUiFinalWarpAffineMatrix[1][0]+v2Pos.y*m_aUiFinalWarpAffineMatrix[1][1]+m_aUiFinalWarpAffineMatrix[1][2]);
        if (bAddOrigin) v2UiPos += m_rWorkArea.Min;
        return std::move(v2UiPos);
    }

    ImVec2 FromUiPos(const ImVec2& _v2UiPos, bool bSubOrigin = true) const
    {
        const auto v2UiPos = bSubOrigin ? _v2UiPos-m_rWorkArea.Min : _v2UiPos;
        ImVec2 v2Pos(v2UiPos.x*m_aUiFinalRevWarpAffineMatrix[0][0]+v2UiPos.y*m_aUiFinalRevWarpAffineMatrix[0][1]+m_aUiFinalRevWarpAffineMatrix[0][2],
                      v2UiPos.x*m_aUiFinalRevWarpAffineMatrix[1][0]+v2UiPos.y*m_aUiFinalRevWarpAffineMatrix[1][1]+m_aUiFinalRevWarpAffineMatrix[1][2]);
        return std::move(v2Pos);
    }

    ImVec2 ToMaskPos(const ImVec2& v2Pos) const
    {
        ImVec2 v2MaskPos(v2Pos.x*m_aMaskWarpAffineMatrix[0][0]+v2Pos.y*m_aMaskWarpAffineMatrix[0][1]+m_aMaskWarpAffineMatrix[0][2],
                      v2Pos.x*m_aMaskWarpAffineMatrix[1][0]+v2Pos.y*m_aMaskWarpAffineMatrix[1][1]+m_aMaskWarpAffineMatrix[1][2]);
        return std::move(v2MaskPos);
    }

    ImVec2 FromMaskPos(const ImVec2& v2MaskPos) const
    {
        ImVec2 v2Pos(v2MaskPos.x*m_aMaskRevWarpAffineMatrix[0][0]+v2MaskPos.y*m_aMaskRevWarpAffineMatrix[0][1]+m_aMaskRevWarpAffineMatrix[0][2],
                      v2MaskPos.x*m_aMaskRevWarpAffineMatrix[1][0]+v2MaskPos.y*m_aMaskRevWarpAffineMatrix[1][1]+m_aMaskRevWarpAffineMatrix[1][2]);
        return std::move(v2Pos);
    }

    void UpdateContainBox()
    {
        if (m_aRoutePointsForUi.size() < 2)
        {
            m_rContianBox = {-1, -1, -1, -1};
            return;
        }
        auto it = m_aRoutePointsForUi.begin();
        it++;
        m_rContianBox = it->m_rEdgeContBox;
        it++;
        while (it != m_aRoutePointsForUi.end())
        {
            const auto& r = it->m_rEdgeContBox;
            if (r.Min.x < m_rContianBox.Min.x)
                m_rContianBox.Min.x = r.Min.x;
            if (r.Max.x > m_rContianBox.Max.x)
                m_rContianBox.Max.x = r.Max.x;
            if (r.Min.y < m_rContianBox.Min.y)
                m_rContianBox.Min.y = r.Min.y;
            if (r.Max.y > m_rContianBox.Max.y)
                m_rContianBox.Max.y = r.Max.y;
            it++;
        }
        if (m_bRouteCompleted)
        {
            const auto& r = m_aRoutePointsForUi.front().m_rEdgeContBox;
            if (r.Min.x < m_rContianBox.Min.x)
                m_rContianBox.Min.x = r.Min.x;
            if (r.Max.x > m_rContianBox.Max.x)
                m_rContianBox.Max.x = r.Max.x;
            if (r.Min.y < m_rContianBox.Min.y)
                m_rContianBox.Min.y = r.Min.y;
            if (r.Max.y > m_rContianBox.Max.y)
                m_rContianBox.Max.y = r.Max.y;
        }
        m_v2Center.x = (m_rContianBox.Min.x+m_rContianBox.Max.x)/2;
        m_v2Center.y = (m_rContianBox.Min.y+m_rContianBox.Max.y)/2;
    }

    void UpdateEdgeVertices(list<RoutePointImpl>& aRoutePoints, list<RoutePointImpl>::iterator itCurrVt, bool bUpdateNextVt = true)
    {
        if (m_bRouteCompleted && itCurrVt == aRoutePoints.begin())
        {
            itCurrVt->CalcEdgeVertices(aRoutePoints.back());
        }
        else if (itCurrVt != aRoutePoints.begin())
        {
            auto itPrevVt = itCurrVt;
            itPrevVt--;
            itCurrVt->CalcEdgeVertices(*itPrevVt);
        }

        if (bUpdateNextVt)
        {
            auto itNextVt = itCurrVt;
            itNextVt++;
            if (m_bRouteCompleted && itNextVt == aRoutePoints.end())
            {
                itNextVt = aRoutePoints.begin();
            }
            if (itNextVt != aRoutePoints.end())
            {
                itNextVt->CalcEdgeVertices(*itCurrVt);
            }
        }
    }

    void MoveRoute(const ImVec2& v2MoveOffset, int64_t i64Tick)
    {
        if (fabs(v2MoveOffset.x) < FLT_EPSILON && fabs(v2MoveOffset.y) < FLT_EPSILON)
            return;
        const bool bKeyFrameEnabled = m_bKeyFrameEnabled;
        const LibCurve::KeyPoint::ValType tPanOffset(v2MoveOffset.x, v2MoveOffset.y, 0, 0);
        if (!bKeyFrameEnabled || i64Tick <= 0)
        {
            for (auto& rp : m_aRoutePointsForUi)
                rp.m_v2Pos += v2MoveOffset;
        }
        else
        {
            for (auto& rp : m_aRoutePointsForUi)
            {
                auto tKpVal = rp.m_ahCurves[0]->CalcPointVal(i64Tick);
                tKpVal += tPanOffset;
                rp.AddKeyPoint(0, LibCurve::KeyPoint::CreateInstance(tKpVal));
            }
        }
        RefreshAllEdgeVertices(m_aRoutePointsForUi);
        if (bKeyFrameEnabled)
            m_i64PrevUiTick = INT64_MIN;
    }

    void ScaleRoute(float fScale)
    {
        const bool bKeyFrameEnabled = m_bKeyFrameEnabled;
        const LibCurve::KeyPoint::ValType tScale(fScale, fScale, 1, 1);
        const LibCurve::KeyPoint::ValType tOrigin(m_v2Center.x, m_v2Center.y, 0, 0);
        for (auto& rp : m_aRoutePointsForUi)
        {
            const auto off = rp.m_v2Pos-m_v2Center;
            rp.m_v2Pos = m_v2Center+off*fScale;
            if (bKeyFrameEnabled)
                rp.m_ahCurves[0]->ScaleKeyPoints(tScale, tOrigin);
        }
        RefreshAllEdgeVertices(m_aRoutePointsForUi);
        if (bKeyFrameEnabled)
            m_i64PrevUiTick = INT64_MIN;
    }

    void CalcMorphCtrlPos()
    {
        auto iterVt = m_itMorphCtrlVt;
        if (iterVt == m_aRoutePointsForUi.end() || iterVt->m_aEdgeVertices.size() < 2)
        {
            m_tMorphCtrl.Reset();
            m_itMorphCtrlVt = m_aRoutePointsForUi.end();
            return;
        }
        m_itMorphCtrlVt = m_tMorphCtrl.SetPosAndSlope(iterVt, m_tMorphCtrl.m_fDistance);
    }

    bool HasHoveredVertex() const
    {
        return m_itHoveredVertex != m_aRoutePointsForUi.end();
    }

    bool HasSelectedVertex() const
    {
        return m_itSelectedVertex != m_aRoutePointsForUi.end();
    }

    void SelectVertex(list<RoutePointImpl>::iterator itCp)
    {
        if (m_itSelectedVertex != m_aRoutePointsForUi.end())
            m_itSelectedVertex->m_bSelected = false;
        m_itSelectedVertex = itCp;
        if (itCp != m_aRoutePointsForUi.end())
            itCp->m_bSelected = true;
    }

    bool HasHoveredContour() const
    {
        return m_itHoveredVertex != m_aRoutePointsForUi.end() && m_itHoveredVertex->m_iHoverType == 4;
    }

    bool HasHoveredMorphCtrl() const
    {
        return m_itMorphCtrlVt != m_aRoutePointsForUi.end() && m_tMorphCtrl.m_bIsHovered;
    }

    bool HasHoveredSomething() const
    {
        return HasHoveredVertex() || HasHoveredMorphCtrl();
    }

    bool IsMorphCtrlShown() const
    {
        return m_itMorphCtrlVt != m_aRoutePointsForUi.end();
    }

    void RefreshAllEdgeVertices(list<RoutePointImpl>& aRoutePoints)
    {
        auto itVt = aRoutePoints.begin();
        while (itVt != aRoutePoints.end())
        {
            itVt->UpdateGrabberContainBox();
            UpdateEdgeVertices(aRoutePoints, itVt);
            itVt++; if (itVt == aRoutePoints.end()) break;
            itVt->UpdateGrabberContainBox();
            itVt++;
        }
    }

    MatUtils::Point2f CalcEdgeVerticalPoint(const MatUtils::Point2f& v2Root, double fVertSlope, float fLen, bool bInside)
    {
        double dx, dy;
        const double ratio = 1/sqrt(1+fVertSlope*fVertSlope);
        const double fTestUnit = fLen > 0 ? 1e-1 : -1e-1;
        if (isinf(fVertSlope))
        {
            dx = 0;
            dy = fTestUnit;
        }
        else
        {
            dx = fTestUnit*ratio;
            dy = dx*fVertSlope;
        }
        const bool bIsTestUnitInside = MatUtils::CheckPointInsidePolygon(MatUtils::Point2f(v2Root.x+dx, v2Root.y+dy), m_aContourVerticesForMask);
        if (bIsTestUnitInside^bInside)
            fLen = -fLen;
        if (isinf(fVertSlope))
        {
            dx = 0;
            dy = fLen;
        }
        else
        {
            dx = fLen*ratio;
            dy = dx*fVertSlope;
        }
        return MatUtils::Point2f(v2Root.x+dx, v2Root.y+dy);
    }

    MatUtils::Point2f CalcEdgeVerticalPoint(const MatUtils::Point2f& v2Start, const MatUtils::Point2f& v2End, float fLen, bool bLeft)
    {
        const double fVertSlope = v2Start.y != v2End.y ? ((double)v2Start.x-(double)v2End.x)/((double)v2End.y-(double)v2Start.y) : numeric_limits<double>::infinity();
        const double ratio = 1/sqrt(1+fVertSlope*fVertSlope);
        float dx, dy;
        if (isinf(fVertSlope))
        {
            dx = 0;
            dy = bLeft ^ (v2End.x > v2Start.x) ? fLen : -fLen;
        }
        else if (fVertSlope == 0)
        {
            dx = bLeft ^ (v2End.y < v2Start.y) ? fLen : -fLen;
            dy = 0;
        }
        else
        {
            if ((v2End.x > v2Start.x) ^ (bLeft ^ (fVertSlope > 0)))
                fLen = -fLen;
            dx = fLen*ratio;
            dy = dx*fVertSlope;
        }
        return MatUtils::Point2f(v2End.x+dx, v2End.y+dy);
    }

    void CalcTwoCrossPoints(const ImVec2& v0, const ImVec2& v1, const ImVec2& v2, float fParaDist, ImVec2& cp1, ImVec2& cp2)
    {
        // for l1 (v0->v1), calculate two parallel lines that has distance 'iDilateSize', as l1p1 (on the left side) and l1p2 (on the right side)
        const double A1 = (double)v1.y-v0.y;
        const double B1 = (double)v0.x-v1.x;
        const double C1 = (double)v1.x*v0.y-(double)v0.x*v1.y;
        const double D1 = fParaDist*sqrt(A1*A1+B1*B1);
        double C1p1, C1p2;  // l1p1: A1*x+B1*y+C1p1=0, l1p2: A1*x+B1*y+C1p2=0
        if (v1.x == v0.x)
        {
            C1p1 = C1-D1;
            C1p2 = C1+D1;
        }
        else
        {
            C1p1 = C1-D1;
            C1p2 = C1+D1;
        }
        // for l2 (v1->v2), calculate two parallel lines that has distance 'iDilateSize', as l2p1 (on the left side) and l2p2 (on the right side)
        const double A2 = v2.y-v1.y;
        const double B2 = v1.x-v2.x;
        const double C2 = v2.x*v1.y-v1.x*v2.y;
        const double D2 = fParaDist*sqrt(A2*A2+B2*B2);
        double C2p1, C2p2;  // l2p1: A2*x+B2*y+C2p1=0, l2p2: A2*x+B2*y+C2p2=0
        if (v2.x == v1.x)
        {
            C2p1 = C2-D2;
            C2p2 = C2+D2;
        }
        else
        {
            C2p1 = C2-D2;
            C2p2 = C2+D2;
        }
        // calculate two cross points: l1p1 X l2p1, and l1p2 X l2p2. One of these two points is outside of contour
        const double den = A1*B2-A2*B1;
        cp1.x = (B1*C2p1-B2*C1p1)/den;
        cp1.y = (A2*C1p1-A1*C2p1)/den;
        cp2.x = (B1*C2p2-B2*C1p2)/den;
        cp2.y = (A2*C1p2-A1*C2p2)/den;
    }

    list<MatUtils::Point2f> DilateContour(int iDilateSize)
    {
        if (iDilateSize <= 0)
            return m_aContourVerticesForMask;

        list<MatUtils::Point2f> aVts;
        auto itCv = m_aContourVerticesForMask.begin();
        auto itCvNext = itCv; itCvNext++;
        const float fTestLen = 1e-1;
        bool bIs1stVt = true, bPreV2Inside;
        while (itCv != m_aContourVerticesForMask.end())
        {
            const auto& v1 = *itCv;
            const auto& v2 = *itCvNext;

            bool bV1VertLeftInside = bPreV2Inside;
            if (bIs1stVt)
            {
                const auto v1VertLeft = CalcEdgeVerticalPoint(v2, v1, fTestLen, false);
                bV1VertLeftInside = MatUtils::CheckPointInsidePolygon(v1VertLeft, m_aContourVerticesForMask);
                bIs1stVt = false;
            }
#if 0
            const auto v2VertLeft = CalcEdgeVerticalPoint(v1, v2, fTestLen, true);
            const bool bV2VertLeftInside = MatUtils::CheckPointInsidePolygon(v2VertLeft, m_aContourVerticesForMask);
            if (bV1VertLeftInside^bV2VertLeftInside)
                m_pLogger->Log(DEBUG) << "Edge two ends have different inside-bility! v1(" << v1.x << "," << v1.y << ") inside(" << bV1VertLeftInside
                    << "), v2(" << v2.x << "," << v2.y << ") inside(" << bV2VertLeftInside << ")." << endl;
            bPreV2Inside = bV2VertLeftInside;
#else
            bPreV2Inside = bV1VertLeftInside;
#endif

            const auto v1Morph = CalcEdgeVerticalPoint(v2, v1, iDilateSize, bV1VertLeftInside);
#if 0
            const auto v2Morph = CalcEdgeVerticalPoint(v1, v2, iDilateSize, !bV2VertLeftInside);
#else
            const auto v2Morph = CalcEdgeVerticalPoint(v1, v2, iDilateSize, !bV1VertLeftInside);
#endif
            if (aVts.empty())
            {
                aVts.push_back(v1Morph);
                aVts.push_back(v2Morph);
            }
            else
            {
                auto itBack = aVts.end(); itBack--;
                const auto& vTail0 = *itBack--;
                const auto& vTail1 = *itBack;
                if (vTail0 != v1Morph)
                {
                    const MatUtils::Point2f v[] = {vTail1, vTail0, v1Morph, v2Morph};
                    auto aBzVts = CalcBezierConnection(v);
                    auto itIns = aBzVts.begin(); itIns++;
                    aVts.insert(aVts.end(), itIns, aBzVts.end());
                }
                aVts.push_back(v2Morph);
            }

            itCv++;
            itCvNext++;
            if (itCvNext == m_aContourVerticesForMask.end()) itCvNext = m_aContourVerticesForMask.begin();
        }
        if (!aVts.empty())
        {
            const auto& vTail0 = aVts.back();
            const auto& vHead0 = aVts.front();
            if (vTail0 != vHead0)
            {
                auto itVt = aVts.end(); itVt--; itVt--;
                const auto& vTail1 = *itVt;
                itVt = aVts.begin(); itVt++;
                const auto& vHead1 = *itVt;
                const MatUtils::Point2f v[] = {vTail1, vTail0, vHead0, vHead1};
                auto aBzVts = CalcBezierConnection(v);
                auto itIns0 = aBzVts.begin(); itIns0++;
                auto itIns1 = aBzVts.end(); itIns1--;
                aVts.insert(aVts.end(), itIns0, itIns1);
            }
            else
            {
                aVts.pop_back();
            }

            // remove invalid points caused by contour cross
            list<MatUtils::Point2f> aValidVts;
            auto itChk0 = aVts.begin();
            auto itChk1 = itChk0; itChk1++;
            bool bPrevChk1Valid = CheckPointValidOnDilateContour(*itChk0, iDilateSize);
            while (itChk0 != aVts.end())
            {
                auto vChk0 = *itChk0;
                const auto& vChk1 = *itChk1;
                const double dSlopeChk = vChk0.x != vChk1.x ? ((double)vChk1.y-vChk0.y)/((double)vChk1.x-vChk0.x) : numeric_limits<double>::infinity();

                bool bChk0Valid, bChk1Valid;
                bChk0Valid = bPrevChk1Valid;
                bChk1Valid = CheckPointValidOnDilateContour(vChk1, iDilateSize);

                MatUtils::Point2f v2CrossPoint;
                auto itSch0 = aVts.begin();
                auto itSch1 = itSch0; itSch1++;
                while (itSch0 != aVts.end())
                {
                    if (itSch0 != itChk0)
                    {
                        bool bFoundCross = false;
                        const auto& vSch0 = *itSch0;
                        const auto& vSch1 = *itSch1;
                        const double dSlopeSch = vSch0.x != vSch1.x ? ((double)vSch1.y-vSch0.y)/((double)vSch1.x-vSch0.x) : numeric_limits<double>::infinity();
#if 0
                        if (itSch1 == itChk0)
                        {
                            if (isinf(dSlopeChk) && isinf(dSlopeSch))
                            {
                                if ((vSch0.y>vSch1.y) ^ (vChk0.y>vChk1.y))
                                {
                                    v2CrossPoint.x = vChk0.x;
                                    v2CrossPoint.y = (vSch0.y>vSch1.y) ^ (vChk1.y>vSch0.y) ? vChk1.y : vSch0.y;
                                    bFoundCross = true;
                                }
                            }
                            else if (dSlopeChk == dSlopeSch)
                            {
                                if ((vSch0.x>vSch1.x) ^ (vChk0.x>vChk1.x))
                                {
                                    if ((vSch0.x>vSch1.x) ^ (vChk1.x>vSch0.x))
                                    {
                                        v2CrossPoint.x = vChk1.x;
                                        v2CrossPoint.y = vChk1.y;
                                    }
                                    else
                                    {
                                        v2CrossPoint.x = vSch0.x;
                                        v2CrossPoint.y = vSch0.y;
                                    }
                                    bFoundCross = true;
                                }
                            }
                            if (bFoundCross)
                            {
                                aVts.erase(itSch1);  // remove 'vSch1' (also is 'vChk0')
                                itChk0 = itSch0;
                                itSch1 = itChk1;
                            }
                        }
                        else if (itSch0 == itChk1)
                        {
                            const double dSlopeSch = vSch0.x != vSch1.x ? ((double)vSch1.y-vSch0.y)/((double)vSch1.x-vSch0.x) : numeric_limits<double>::infinity();
                            if (isinf(dSlopeChk) && isinf(dSlopeSch))
                            {
                                if ((vSch0.y>vSch1.y) ^ (vChk0.y>vChk1.y))
                                {
                                    v2CrossPoint.x = vChk0.x;
                                    v2CrossPoint.y = (vSch0.y>vSch1.y) ^ (vChk0.y>vSch1.y) ? vSch1.y : vChk0.y;
                                    bFoundCross = true;
                                }
                            }
                            else if (dSlopeChk == dSlopeSch)
                            {
                                if ((vSch0.x>vSch1.x) ^ (vChk0.x>vChk1.x))
                                {
                                    if ((vSch0.x>vSch1.x) ^ (vChk0.x>vSch1.x))
                                    {
                                        v2CrossPoint.x = vSch1.x;
                                        v2CrossPoint.y = vSch1.y;
                                    }
                                    else
                                    {
                                        v2CrossPoint.x = vChk0.x;
                                        v2CrossPoint.y = vChk0.y;
                                    }
                                    bFoundCross = true;
                                }
                            }
                            if (bFoundCross)
                            {
                                aVts.erase(itSch0);  // remove 'vSch0' (also is 'vChk1')
                                itSch0 = itChk0;
                                itChk1 = itSch1;
                            }
                        }
                        else
#endif
                        {
                            if (isinf(dSlopeChk) && isinf(dSlopeSch))
                            {
                                // TODO: check two lines have overlapped part
                            }
                            else if (dSlopeChk == dSlopeSch)
                            {
                                // TODO: check two lines have overlapped part
                            }
                            else
                            {
                                // check if two lines have cross point
                                const MatUtils::Point2f aPoints[] = { vChk0, vChk1, vSch0, vSch1 };
                                if (MatUtils::CheckTwoLinesCross(aPoints, &v2CrossPoint))
                                {
                                    if (v2CrossPoint != vChk1)
                                    {
                                        if (v2CrossPoint != vChk0)
                                        {
                                            if (bChk0Valid)
                                                aValidVts.push_back(vChk0);
                                            vChk0 = v2CrossPoint;
                                        }
                                        bChk0Valid = CheckPointValidOnDilateContour(vChk0, iDilateSize);
                                    }
                                }
                            }
                        }
                    }

                    itSch0++; itSch1++;
                    if (itSch1 == aVts.end()) itSch1 = aVts.begin();
                }

                if (bChk0Valid)
                    aValidVts.push_back(vChk0);
                bPrevChk1Valid = bChk1Valid;
                itChk0++; itChk1++;
                if (itChk1 == aVts.end()) itChk1 = aVts.begin();
            }
            aVts.swap(aValidVts);
        }
        return std::move(aVts);
    }

    list<MatUtils::Point2f> ErodeContour(int iDilateSize)
    {
        if (iDilateSize <= 0)
            return m_aContourVerticesForMask;

        list<MatUtils::Point2f> aVts;
        auto itCv = m_aContourVerticesForMask.begin();
        auto itCvNext = itCv; itCvNext++;
        const float fTestLen = 1e-1;
        bool bIs1stVt = true, bPreV2Inside;
        while (itCv != m_aContourVerticesForMask.end())
        {
            const auto& v1 = *itCv;
            const auto& v2 = *itCvNext;

            bool bV1VertLeftInside = bPreV2Inside;
            if (bIs1stVt)
            {
                const auto v1VertLeft = CalcEdgeVerticalPoint(v2, v1, fTestLen, false);
                bV1VertLeftInside = MatUtils::CheckPointInsidePolygon(v1VertLeft, m_aContourVerticesForMask);
                bIs1stVt = false;
            }
            bPreV2Inside = bV1VertLeftInside;

            const auto v1Morph = CalcEdgeVerticalPoint(v2, v1, iDilateSize, !bV1VertLeftInside);
            const auto v2Morph = CalcEdgeVerticalPoint(v1, v2, iDilateSize, bV1VertLeftInside);

            if (aVts.empty())
            {
                aVts.push_back(v1Morph);
                aVts.push_back(v2Morph);
            }
            else
            {
                auto itBack = aVts.end(); itBack--;
                const auto& vTail0 = *itBack--;
                const auto& vTail1 = *itBack;
                if (vTail0 != v1Morph)
                {
                    const MatUtils::Point2f v[] = {vTail1, vTail0, v1Morph, v2Morph};
                    auto aBzVts = CalcBezierConnection(v);
                    auto itIns = aBzVts.begin(); itIns++;
                    aVts.insert(aVts.end(), itIns, aBzVts.end());
                }
                aVts.push_back(v2Morph);
            }

            itCv++;
            itCvNext++;
            if (itCvNext == m_aContourVerticesForMask.end()) itCvNext = m_aContourVerticesForMask.begin();
        }
        if (!aVts.empty())
        {
            const auto& vTail0 = aVts.back();
            const auto& vHead0 = aVts.front();
            if (vTail0 != vHead0)
            {
                auto itVt = aVts.end(); itVt--; itVt--;
                const auto& vTail1 = *itVt;
                itVt = aVts.begin(); itVt++;
                const auto& vHead1 = *itVt;
                const MatUtils::Point2f v[] = {vTail1, vTail0, vHead0, vHead1};
                auto aBzVts = CalcBezierConnection(v);
                auto itIns0 = aBzVts.begin(); itIns0++;
                auto itIns1 = aBzVts.end(); itIns1--;
                aVts.insert(aVts.end(), itIns0, itIns1);
            }
            else
            {
                aVts.pop_back();
            }

            // remove invalid points caused by contour cross
            list<MatUtils::Point2f> aValidVts;
            auto itChk0 = aVts.begin();
            auto itChk1 = itChk0; itChk1++;
            bool bPrevChk1Valid = CheckPointValidOnErodeContour(*itChk0, iDilateSize);
            while (itChk0 != aVts.end())
            {
                auto vChk0 = *itChk0;
                const auto& vChk1 = *itChk1;
                const double dSlopeChk = vChk0.x != vChk1.x ? ((double)vChk1.y-vChk0.y)/((double)vChk1.x-vChk0.x) : numeric_limits<double>::infinity();

                bool bChk0Valid, bChk1Valid;
                bChk0Valid = bPrevChk1Valid;
                bChk1Valid = CheckPointValidOnErodeContour(vChk1, iDilateSize);

                MatUtils::Point2f v2CrossPoint;
                auto itSch0 = aVts.begin();
                auto itSch1 = itSch0; itSch1++;
                while (itSch0 != aVts.end())
                {
                    if (itSch0 != itChk0)
                    {
                        bool bFoundCross = false;
                        const auto& vSch0 = *itSch0;
                        const auto& vSch1 = *itSch1;
                        const double dSlopeSch = vSch0.x != vSch1.x ? ((double)vSch1.y-vSch0.y)/((double)vSch1.x-vSch0.x) : numeric_limits<double>::infinity();
                        if (isinf(dSlopeChk) && isinf(dSlopeSch))
                        {
                            // TODO: check two lines have overlapped part
                        }
                        else if (dSlopeChk == dSlopeSch)
                        {
                            // TODO: check two lines have overlapped part
                        }
                        else
                        {
                            // check if two lines have cross point
                            const MatUtils::Point2f aPoints[] = { vChk0, vChk1, vSch0, vSch1 };
                            if (MatUtils::CheckTwoLinesCross(aPoints, &v2CrossPoint))
                            {
                                if (v2CrossPoint != vChk1)
                                {
                                    if (v2CrossPoint != vChk0)
                                    {
                                        if (bChk0Valid)
                                            aValidVts.push_back(vChk0);
                                        vChk0 = v2CrossPoint;
                                    }
                                    bChk0Valid = CheckPointValidOnErodeContour(vChk0, iDilateSize);
                                }
                            }
                        }
                    }

                    itSch0++; itSch1++;
                    if (itSch1 == aVts.end()) itSch1 = aVts.begin();
                }

                if (bChk0Valid)
                    aValidVts.push_back(vChk0);
                bPrevChk1Valid = bChk1Valid;
                itChk0++; itChk1++;
                if (itChk1 == aVts.end()) itChk1 = aVts.begin();
            }
            aVts.swap(aValidVts);
        }
        return std::move(aVts);
    }

    bool CheckPointValidOnDilateContour(const MatUtils::Point2f& v, float fDilateSize)
    {
        float fDistToOrgContour;
        MatUtils::CalcNearestPointOnPloygon(v, m_aContourVerticesForMask, nullptr, nullptr, &fDistToOrgContour);
        bool bIsInside = MatUtils::CheckPointInsidePolygon(v, m_aContourVerticesForMask);
        return fDistToOrgContour >= fDilateSize-1e-2 && !bIsInside;
    }

    bool CheckPointValidOnErodeContour(const MatUtils::Point2f& v, float fDilateSize)
    {
        float fDistToOrgContour;
        MatUtils::CalcNearestPointOnPloygon(v, m_aContourVerticesForMask, nullptr, nullptr, &fDistToOrgContour);
        bool bIsInside = MatUtils::CheckPointInsidePolygon(v, m_aContourVerticesForMask);
        return fDistToOrgContour >= fDilateSize-1e-2 && bIsInside;
    }

    list<MatUtils::Point2f> ConvertPointsType(const list<ImVec2>& vts)
    {
        const int iLoopCnt = vts.size();
        list<MatUtils::Point2f> res;
        auto itVt = vts.begin();
        while (itVt != vts.end())
            res.push_back(MatUtils::FromImVec2<float>(*itVt++));
        return res;
    }

    bool UpdateContourByKeyFrame(int64_t i64Tick, bool bIsUiContour)
    {
        auto& aRoutePoints = bIsUiContour ? m_aRoutePointsForUi : m_aRoutePointsForMask;
        auto& i64PrevTick = bIsUiContour ? m_i64PrevUiTick : m_i64PrevMaskTick;
        bool bUpdated = false;
        // update contour points if key-frame is enabled
        if (m_bKeyFrameEnabled && i64Tick >= 0 && i64Tick != i64PrevTick && !aRoutePoints.empty())
        {
            const auto szCpCnt = aRoutePoints.size();
            vector<bool> aCpChanged(szCpCnt);
            auto itCp = aRoutePoints.begin();
            for (auto i = 0; i < szCpCnt; i++)
            {
                auto& cp = *itCp++;
                aCpChanged[i] = cp.UpdateByTick(i64Tick);
            }
            itCp = aRoutePoints.begin();
            bool bChanged1, bChanged2;
            bChanged2 = aCpChanged[0];
            for (auto i = 0; i < szCpCnt; i++, itCp++)
            {
                bChanged1 = bChanged2;
                bChanged2 = i+1 == szCpCnt ? aCpChanged[0] : aCpChanged[i+1];
                if (bChanged1)
                {
                    UpdateEdgeVertices(aRoutePoints, itCp, !bChanged2);
                    bUpdated = true;
                }
            }
            i64PrevTick = i64Tick;
        }
        return bUpdated;
    }

private:
    string m_strMaskName;
    MatUtils::Size2i m_szMaskSize;
    ImRect m_rWorkArea{{-1, -1}, {-1, -1}};
    ImVec2 m_v2UiScale{1.0f, 1.0f};
    float m_aMaskWarpAffineMatrix[2][3], m_aUiWarpAffineMatrix[2][3], m_aUiFinalWarpAffineMatrix[2][3];
    float m_aMaskRevWarpAffineMatrix[2][3], m_aUiRevWarpAffineMatrix[2][3], m_aUiFinalRevWarpAffineMatrix[2][3];
    bool m_bMaskWarpAffineMatrixChanged{true}, m_bUiWarpAffineMatrixChanged{true};
    bool m_bWarpAffinePassThrough{true}, m_bWarpAffineOnlyPan{false};
    list<RoutePointImpl> m_aRoutePointsForUi;
    list<ImVec2> m_aContourVerticesForUi;
    list<RoutePointImpl> m_aRoutePointsForMask;
    list<MatUtils::Point2f> m_aContourVerticesForMask;
    atomic_bool m_bRouteNeedSync{true}, m_bNeedRebuildMaskContourVtx{false};
    mutex m_mtxRouteLock;
    ImVec2 m_v2PointSize{5.f, 5.f}, m_v2PointSizeHalf;
    ImU32 m_u32PointColor{IM_COL32(40, 170, 40, 255)};
    ImU32 m_u32ContourHoverPointColor{IM_COL32(80, 80, 80, 255)};
    float m_fPointBorderThickness{2.5f}, m_fPointBorderSelectedThickness{3.5f};
    ImU32 m_u32PointBorderColor{IM_COL32(150, 150, 150, 255)};
    ImU32 m_u32PointBorderHoverColor{IM_COL32(240, 240, 240, 255)};
    ImU32 m_u32PointBorderSelectedColor{IM_COL32(240, 180, 50, 255)};
    float m_fGrabberRadius{4.5f}, m_fGrabberBorderThickness{1.5f};
    ImU32 m_u32GrabberColor{IM_COL32(80, 80, 150, 255)};
    ImU32 m_u32GrabberBorderColor{IM_COL32(150, 150, 150, 255)};
    ImU32 m_u32GrabberBorderHoverColor{IM_COL32(240, 240, 240, 255)};
    float m_fGrabberLineThickness{2.f};
    ImU32 m_u32GrabberLineColor{IM_COL32(80, 80, 150, 255)};
    float m_fContourThickness{3.f};
    ImU32 m_u32ContourColor{IM_COL32(40, 40, 170, 255)};
    ImU32 m_u32FeatherGrabberColor{IM_COL32(200, 150, 80, 255)};
    ImRect m_rContianBox{{-1, -1}, {-1, -1}};
    ImVec2 m_v2Center;
    float m_fContourHoverDetectExRadius{4.f}, m_fHoverDetectExRadius{5.f};
    list<RoutePointImpl>::iterator m_itHoveredVertex;
    list<RoutePointImpl>::iterator m_itSelectedVertex;
    MorphController m_tMorphCtrl;
    list<RoutePointImpl>::const_iterator m_itMorphCtrlVt;
    ImGuiKey m_eRemoveVertexKey{ImGuiKey_LeftAlt};
    ImGuiKey m_eInsertVertexKey{ImGuiKey_LeftCtrl};
    ImGuiKey m_eScaleContourKey{ImGuiKey_LeftShift};
    string m_strRemoveVertexCursorIcon{u8"\ue15d"};
    ImGuiKey m_eEnableBezierKey{ImGuiKey_LeftCtrl};
    bool m_bInScaleMode{false};
    bool m_bRouteCompleted{false};
    bool m_bRouteChanged{false};
    bool m_bLastMaskFilled{false};
    int m_iLastMaskLineType{0};
    float m_fCpkfViewFixedHeight{20.f};
    float m_fCpkfCoordBgRectRounding{3.f};
    ImU32 m_u32CpkfCoordBgColor{IM_COL32(100, 100, 100, 255)};
    ImDataType m_eLastMaskDataType{IM_DT_UNDEFINED};
    double m_dLastMaskValue{0}, m_dLastNonMaskValue{0};
    ImGui::ImMat m_mMask, m_mMorphMask;
    // ImGui::ImMat m_mMorphKernel;
    int m_iLastMorphIters{0}, m_iLastFeatherIters{0};
    bool m_bKeyFrameEnabled{false};
    int64_t m_i64PrevUiTick{0}, m_i64PrevMaskTick{0};
    pair<int64_t, int64_t> m_prTickRange{0, 0};
    string m_sErrMsg;
    ALogger* m_pLogger;
};

unordered_map<int, MaskCreatorImpl::BezierTable::Holder> MaskCreatorImpl::s_mapBezierTables;
int MaskCreatorImpl::s_iKeepBezierTableCountMin = 10;
chrono::seconds MaskCreatorImpl::s_iKeepBezierTableTimeOut = chrono::seconds(30);

static const auto MASK_CREATOR_DELETER = [] (MaskCreator* p) {
    MaskCreatorImpl* ptr = dynamic_cast<MaskCreatorImpl*>(p);
    delete ptr;
};

MaskCreator::Holder MaskCreator::CreateInstance(const MatUtils::Size2i& size, const string& name)
{
    return MaskCreator::Holder(new MaskCreatorImpl(size, name), MASK_CREATOR_DELETER);
}

void MaskCreator::GetVersion(int& major, int& minor, int& patch, int& build)
{
    major = ImMaskCreator_VERSION_MAJOR;
    minor = ImMaskCreator_VERSION_MINOR;
    patch = ImMaskCreator_VERSION_PATCH;
    build = ImMaskCreator_VERSION_BUILD;
}

MaskCreator::Holder MaskCreator::LoadFromJson(const imgui_json::value& j)
{
    MaskCreator::Holder hInst = CreateInstance({0, 0});
    MaskCreatorImpl* pInst = dynamic_cast<MaskCreatorImpl*>(hInst.get());
    pInst->LoadFromJson(j);
    return hInst;
}

MaskCreator::Holder MaskCreator::LoadFromJson(const string& filePath)
{
    auto res = json::value::load(filePath);
    if (!res.second)
        return nullptr;
    MaskCreator::Holder hInst = CreateInstance({0, 0});
    MaskCreatorImpl* pInst = dynamic_cast<MaskCreatorImpl*>(hInst.get());
    pInst->LoadFromJson(res.first);
    return hInst;
}
}