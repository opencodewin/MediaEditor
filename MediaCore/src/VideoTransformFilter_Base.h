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
#include <cmath>
#include <utility>
#include <MatUtilsImVecHelper.h>
#include "VideoClip.h"
#include "VideoTransformFilter.h"

using namespace std;
namespace LibCurve = ImGui::ImNewCurve;

namespace MediaCore
{
class VideoTransformFilter_Base : public VideoTransformFilter
{
public:
    VideoTransformFilter_Base()
    {
        m_hPosOffsetCurve = LibCurve::Curve::CreateInstance("PosOffsetCurve", LibCurve::Linear, {-1,-1,0,0}, {1,1,0,0}, {0,0,0,0}, true);
        m_hPosOffsetCurve->SetClipOutputValue(LibCurve::Curve::FLAGS_CLIP_MINMAX); m_hPosOffsetCurve->SetClipKeyPointValue(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_hPosOffsetCurve->SetClipKeyPointTime(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_aCropCurves.resize(2);
        m_aCropCurves[0] = LibCurve::Curve::CreateInstance("CropCurveLT", LibCurve::Linear, {0,0,0,0}, {1,1,0,0}, {0,0,0,0}, true);
        m_aCropCurves[0]->SetClipOutputValue(LibCurve::Curve::FLAGS_CLIP_MINMAX); m_aCropCurves[0]->SetClipKeyPointValue(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_aCropCurves[1] = LibCurve::Curve::CreateInstance("CropCurveRB", LibCurve::Linear, {0,0,0,0}, {1,1,0,0}, {0,0,0,0}, true);
        m_aCropCurves[1]->SetClipOutputValue(LibCurve::Curve::FLAGS_CLIP_MINMAX); m_aCropCurves[1]->SetClipKeyPointValue(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_aCropCurves[0]->SetClipKeyPointTime(LibCurve::Curve::FLAGS_CLIP_MINMAX); m_aCropCurves[1]->SetClipKeyPointTime(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_hScaleCurve = LibCurve::Curve::CreateInstance("ScaleCurve", LibCurve::Linear, {0,0,0,0}, {4,4,0,0}, {1,1,0,0}, true);
        m_hScaleCurve->SetClipOutputValue(LibCurve::Curve::FLAGS_CLIP_MIN); m_hScaleCurve->SetClipKeyPointValue(LibCurve::Curve::FLAGS_CLIP_MIN);
        m_hScaleCurve->SetClipKeyPointTime(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_hRotationCurve = LibCurve::Curve::CreateInstance("RotationCurve", LibCurve::Linear, {-360,-360,0,0}, {360,360,0,0}, {0,0,0,0}, true);
        m_hRotationCurve->SetClipOutputValue(LibCurve::Curve::FLAGS_CLIP_MINMAX); m_hRotationCurve->SetClipKeyPointValue(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_hRotationCurve->SetClipKeyPointTime(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_hOpacityCurve = LibCurve::Curve::CreateInstance("OpacityCurve", LibCurve::Linear, {0,0,0,0}, {1,1,0,0}, {1,1,0,0}, true);
        m_hOpacityCurve->SetClipOutputValue(LibCurve::Curve::FLAGS_CLIP_MINMAX); m_hOpacityCurve->SetClipKeyPointValue(LibCurve::Curve::FLAGS_CLIP_MINMAX);
        m_hOpacityCurve->SetClipKeyPointTime(LibCurve::Curve::FLAGS_CLIP_MINMAX);
    }

    virtual ~VideoTransformFilter_Base() {}

    Holder Clone(SharedSettings::Holder hSettings) override
    {
        VideoTransformFilter::Holder hNewInst = VideoTransformFilter::CreateInstance();
        if (!hNewInst->Initialize(hSettings))
            return nullptr;
        const auto j = SaveAsJson();
        hNewInst->LoadFromJson(j);
        return hNewInst;
    }

    uint32_t GetInWidth() const override
    { return m_u32InWidth; }

    uint32_t GetInHeight() const override
    { return m_u32InHeight; }

    uint32_t GetOutWidth() const override
    { return m_u32OutWidth; }

    uint32_t GetOutHeight() const override
    { return m_u32OutHeight; }

    MatUtils::Vec2<uint32_t>GetOutSize() const override
    { return {m_u32OutWidth, m_u32OutHeight}; }

    string GetOutputFormat() const override
    { return m_strOutputFormat; }

    bool SetAspectFitType(AspectFitType type) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (type < ASPECT_FIT_TYPE__FIT || type > ASPECT_FIT_TYPE__STRETCH)
        {
            m_strErrMsg = "INVALID argument 'type'!";
            return false;
        }
        if (m_eAspectFitType == type)
            return true;
        m_eAspectFitType = type;
        m_bNeedUpdateScaleParam = true;
        return true;
    }

    AspectFitType GetAspectFitType() const override
    { return m_eAspectFitType; }

    virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos, float& fOpacity) = 0;

    VideoFrame::Holder FilterImage(VideoFrame::Holder hVfrm, int64_t pos) override
    {
        if (!hVfrm)
        {
            m_strErrMsg = "INVALID arguments! 'hVfrm' is null.";
            return nullptr;
        }
        ImGui::ImMat vmat;
        const auto bRet = hVfrm->GetMat(vmat);
        if (!bRet || vmat.empty())
        {
            m_strErrMsg = "FAILED to get ImMat instance from 'hVfrm'!";
            return nullptr;
        }
        float fOpacity = 1.f;
        vmat = this->FilterImage(vmat, pos, fOpacity);
        if (vmat.empty())
            return nullptr;
        auto hOutVfrm = VideoFrame::CreateMatInstance(vmat);
        hOutVfrm->SetOpacity(fOpacity);
        return hOutVfrm;
    }

    void ApplyTo(VideoClip* pVClip) override
    {
        m_i64ClipStartOffset = pVClip->StartOffset();
        m_i64ClipEndOffset = pVClip->EndOffset();
        m_i64ClipDuration = pVClip->Duration();
        m_pOwnerClip = pVClip;
        m_tTimeRange = {0, pVClip->SrcDuration()};
        const ImVec2 v2TimeRange((float)m_tTimeRange.x, (float)m_tTimeRange.y);
        m_hPosOffsetCurve->SetTimeRange(v2TimeRange);
        m_aCropCurves[0]->SetTimeRange(v2TimeRange);
        m_aCropCurves[1]->SetTimeRange(v2TimeRange);
        m_hScaleCurve->SetTimeRange(v2TimeRange);
        m_hRotationCurve->SetTimeRange(v2TimeRange);
        m_hOpacityCurve->SetTimeRange(v2TimeRange);
        const auto tFrameRate = pVClip->GetSharedSettings()->VideoOutFrameRate();
        const pair<uint32_t, uint32_t> tTimeBase(tFrameRate.den, tFrameRate.num);
        m_hPosOffsetCurve->SetTimeBase(tTimeBase);
        m_aCropCurves[0]->SetTimeBase(tTimeBase);
        m_aCropCurves[1]->SetTimeBase(tTimeBase);
        m_hScaleCurve->SetTimeBase(tTimeBase);
        m_hRotationCurve->SetTimeBase(tTimeBase);
        m_hOpacityCurve->SetTimeBase(tTimeBase);
    }

    void UpdateClipRange() override
    {
        if (!m_pOwnerClip)
            return;
        const MatUtils::Vec2<int64_t> tNewTimeRange(0, m_pOwnerClip->SrcDuration());
        if (m_tTimeRange != tNewTimeRange)
        {
            m_tTimeRange = tNewTimeRange;
            const ImVec2 v2TimeRange((float)m_tTimeRange.x, (float)m_tTimeRange.y);
            m_hPosOffsetCurve->SetTimeRange(v2TimeRange);
            m_aCropCurves[0]->SetTimeRange(v2TimeRange);
            m_aCropCurves[1]->SetTimeRange(v2TimeRange);
            m_hScaleCurve->SetTimeRange(v2TimeRange);
            m_hRotationCurve->SetTimeRange(v2TimeRange);
            m_hOpacityCurve->SetTimeRange(v2TimeRange);
        }

        const auto i64NewStartOffset = m_pOwnerClip->StartOffset();
        if (i64NewStartOffset != m_i64ClipStartOffset)
        {
            const LibCurve::KeyPoint::ValType tPanOffset(0, 0, 0, (float)(m_i64ClipStartOffset-i64NewStartOffset));
            m_hPosOffsetCurve->PanKeyPoints(tPanOffset);
            m_aCropCurves[0]->PanKeyPoints(tPanOffset);
            m_aCropCurves[1]->PanKeyPoints(tPanOffset);
            m_hScaleCurve->PanKeyPoints(tPanOffset);
            m_hRotationCurve->PanKeyPoints(tPanOffset);
            m_hOpacityCurve->PanKeyPoints(tPanOffset);
            m_i64ClipStartOffset = i64NewStartOffset;
        }
        m_i64ClipEndOffset = m_pOwnerClip->EndOffset();
        m_i64ClipDuration = m_pOwnerClip->Duration();
    }

    bool CalcCornerPoints(int64_t i64Tick, ImVec2 aCornerPoints[4]) const override
    {
        if (m_u32InWidth == 0 || m_u32InHeight == 0 || m_u32OutWidth == 0 || m_u32OutHeight == 0)
            return false;

        float fTick;
        LibCurve::KeyPoint::ValType tKpVal;
        // Position offset
        fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tPosOffRatio = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        // Crop
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        const auto _u32CropL = (uint32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto _u32CropT = (uint32_t)round((float)m_u32InHeight*tKpVal.y);
        tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        const auto _u32CropR = (uint32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto _u32CropB = (uint32_t)round((float)m_u32InHeight*tKpVal.y);
        uint32_t u32CropL, u32CropT, u32CropR, u32CropB;
        if (_u32CropL+_u32CropR < m_u32InWidth)
        {
            u32CropL = _u32CropL;
            u32CropR = _u32CropR;
        }
        else
        {
            u32CropL = m_u32InWidth-_u32CropR;
            u32CropR = m_u32InWidth-_u32CropL;
        }
        if (_u32CropT+_u32CropB < m_u32InHeight)
        {
            u32CropT = _u32CropT;
            u32CropB = _u32CropB;
        }
        else
        {
            u32CropT = m_u32InHeight-_u32CropB;
            u32CropB = m_u32InHeight-_u32CropT;
        }
        // Scale
        fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        const auto v2FinalScale = MatUtils::ToImVec2(CalcFinalScale(tKpVal.x, tKpVal.y));
        const ImVec2 v2PosOffset(
            (int32_t)round(((float)m_u32InWidth*v2FinalScale.x+(float)m_u32OutWidth)*tPosOffRatio.x/2.f),
            (int32_t)round(((float)m_u32InHeight*v2FinalScale.y+(float)m_u32OutHeight)*tPosOffRatio.y/2.f)
            );
        // Rotation
        fTick = m_bEnableKeyFramesOnRotation ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hRotationCurve->CalcPointVal(fTick, false);
        const auto fRotationAngle = tKpVal.x;

        // calc corner points
        ImVec2 aInCornerPoints[4];  // TopLeft, TopRight, BottomRight, BottomLeft
        aInCornerPoints[0] = ImVec2(u32CropL, u32CropT);
        aInCornerPoints[1] = ImVec2(m_u32InWidth-u32CropR, u32CropT);
        aInCornerPoints[2] = ImVec2(m_u32InWidth-u32CropR, m_u32InHeight-u32CropB);
        aInCornerPoints[3] = ImVec2(u32CropL, m_u32InHeight-u32CropB);
        const ImVec2 v2InCenter((float)m_u32InWidth/2, (float)m_u32InHeight/2);
        // remap the coordinates using input center as origin
        aInCornerPoints[0] -= v2InCenter; aInCornerPoints[1] -= v2InCenter;
        aInCornerPoints[2] -= v2InCenter; aInCornerPoints[3] -= v2InCenter;
        // apply scale
        aInCornerPoints[0] *= v2FinalScale; aInCornerPoints[1] *= v2FinalScale;
        aInCornerPoints[2] *= v2FinalScale; aInCornerPoints[3] *= v2FinalScale;
        // apply rotation
        const auto fRotationRadian = -fRotationAngle*M_PI/180.f;
        const auto fSinA = sin(fRotationRadian);
        const auto fCosA = cos(fRotationRadian);
        aCornerPoints[0].x = fCosA*aInCornerPoints[0].x+fSinA*aInCornerPoints[0].y; aCornerPoints[0].y = -fSinA*aInCornerPoints[0].x+fCosA*aInCornerPoints[0].y;
        aCornerPoints[1].x = fCosA*aInCornerPoints[1].x+fSinA*aInCornerPoints[1].y; aCornerPoints[1].y = -fSinA*aInCornerPoints[1].x+fCosA*aInCornerPoints[1].y;
        aCornerPoints[2].x = fCosA*aInCornerPoints[2].x+fSinA*aInCornerPoints[2].y; aCornerPoints[2].y = -fSinA*aInCornerPoints[2].x+fCosA*aInCornerPoints[2].y;
        aCornerPoints[3].x = fCosA*aInCornerPoints[3].x+fSinA*aInCornerPoints[3].y; aCornerPoints[3].y = -fSinA*aInCornerPoints[3].x+fCosA*aInCornerPoints[3].y;
        // apply position offset
        aCornerPoints[0] += v2PosOffset; aCornerPoints[1] += v2PosOffset;
        aCornerPoints[2] += v2PosOffset; aCornerPoints[3] += v2PosOffset;
        return true;
    }

    // Position
    bool SetPosOffset(int32_t i32PosOffX, int32_t i32PosOffY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetX == i32PosOffX && m_i32PosOffsetY == i32PosOffY)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        if (i32PosOffX > (int32_t)m_u32OutWidth || i32PosOffX < -(int32_t)m_u32OutWidth)
        {
            ostringstream oss; oss << "INVALID argument value PosOffX(" << i32PosOffX << ")! Valid range is [" << -(int32_t)m_u32OutWidth << ", " << m_u32OutWidth << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (i32PosOffY > (int32_t)m_u32OutHeight || i32PosOffY < -(int32_t)m_u32OutHeight)
        {
            ostringstream oss; oss << "INVALID argument value PosOffY(" << i32PosOffY << ")! Valid range is [" << -(int32_t)m_u32OutHeight << ", " << m_u32OutHeight << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fPosOffRatioX = (float)i32PosOffX/(float)m_u32OutWidth;
        const auto fPosOffRatioY = (float)i32PosOffY/(float)m_u32OutHeight;
        return SetPosOffsetRatio(fPosOffRatioX, fPosOffRatioY);
    }

    bool SetPosOffsetX(int32_t i32PosOffX) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetX == i32PosOffX)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        if (i32PosOffX > (int32_t)m_u32OutWidth || i32PosOffX < -(int32_t)m_u32OutWidth)
        {
            ostringstream oss; oss << "INVALID argument value PosOffX(" << i32PosOffX << ")! Valid range is [" << -(int32_t)m_u32OutWidth << ", " << m_u32OutWidth << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fPosOffRatioX = (float)i32PosOffX/(float)m_u32OutWidth;
        return SetPosOffsetRatioX(fPosOffRatioX);
    }

    int32_t GetPosOffsetX() const override
    { return m_i32PosOffsetX; }

    bool SetPosOffsetY(int32_t i32PosOffY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetY == i32PosOffY)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        if (i32PosOffY > (int32_t)m_u32OutHeight || i32PosOffY < -(int32_t)m_u32OutHeight)
        {
            ostringstream oss; oss << "INVALID argument value PosOffY(" << i32PosOffY << ")! Valid range is [" << -(int32_t)m_u32OutHeight << ", " << m_u32OutHeight << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fPosOffRatioY = (float)i32PosOffY/(float)m_u32OutHeight;
        return SetPosOffsetRatioX(fPosOffRatioY);
    }

    int32_t GetPosOffsetY() const override
    { return m_i32PosOffsetY; }

    MatUtils::Vec2<int32_t> GetPosOffset() const override
    { return {m_i32PosOffsetX, m_i32PosOffsetY}; }

    MatUtils::Vec2<int32_t> GetPosOffset(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        const auto i32PosOffX = (int32_t)round((float)m_u32OutWidth*tKpVal.x);
        const auto i32PosOffY = (int32_t)round((float)m_u32OutHeight*tKpVal.y);
        return {i32PosOffX, i32PosOffY};
    }

    bool SetPosOffsetRatio(float fPosOffRatioX, float fPosOffRatioY) override
    { return SetPosOffsetRatio(m_tTimeRange.x, fPosOffRatioX, fPosOffRatioY); }

    bool SetPosOffsetRatioX(float fPosOffRatioX) override
    { return SetPosOffsetRatioX(m_tTimeRange.x, fPosOffRatioX); }

    float GetPosOffsetRatioX() const override
    { return m_fPosOffsetRatioX; }

    bool SetPosOffsetRatioY(float fPosOffRatioY) override
    { return SetPosOffsetRatioY(m_tTimeRange.x, fPosOffRatioY); }

    float GetPosOffsetRatioY() const override
    { return m_fPosOffsetRatioY; }

    bool SetPosOffsetRatio(int64_t i64Tick, float fPosOffRatioX, float fPosOffRatioY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tMaxVal = m_hPosOffsetCurve->GetMaxVal();
        if (fPosOffRatioX < tMinVal.x || fPosOffRatioX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioX(" << fPosOffRatioX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fPosOffRatioY < tMinVal.y || fPosOffRatioY > tMaxVal.y)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioY(" << fPosOffRatioY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnPosOffset)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fPosOffRatioX, fPosOffRatioY, 0.f, (float)i64Tick);
        const auto iRet = m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool SetPosOffsetRatioX(int64_t i64Tick, float fPosOffRatioX) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tMaxVal = m_hPosOffsetCurve->GetMaxVal();
        if (fPosOffRatioX < tMinVal.x || fPosOffRatioX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioX(" << fPosOffRatioX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        tKpVal.x = fPosOffRatioX;
        const auto iRet = m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetPosOffsetRatioX(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        return tKpVal.x;
    }

    bool SetPosOffsetRatioY(int64_t i64Tick, float fPosOffRatioY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tMaxVal = m_hPosOffsetCurve->GetMaxVal();
        if (fPosOffRatioY < tMinVal.y || fPosOffRatioY > tMaxVal.y)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioY(" << fPosOffRatioY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        tKpVal.y = fPosOffRatioY;
        const auto iRet = m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetPosOffsetRatioY(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        return tKpVal.y;
    }

    MatUtils::Vec2<float> GetPosOffsetRatio() const override
    {
        return {m_fPosOffsetRatioX, m_fPosOffsetRatioY};
    }

    MatUtils::Vec2<float> GetPosOffsetRatio(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        return {tKpVal.x, tKpVal.y};
    }

    bool ChangePosOffset(int64_t i64Tick, int32_t i32DeltaX, int32_t i32DeltaY, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (i32DeltaX == 0 && i32DeltaY == 0)
            return true;
        auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        const auto v2FinalScale = MatUtils::ToImVec2(CalcFinalScale(tKpVal.x, tKpVal.y));
        const auto fPosOffRatioDeltaX = (float)i32DeltaX*2.f/((float)m_u32InWidth*v2FinalScale.x+(float)m_u32OutWidth);
        const auto fPosOffRatioDeltaY = (float)i32DeltaY*2.f/((float)m_u32InHeight*v2FinalScale.y+(float)m_u32OutHeight);
        fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        const auto tPosOffMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tPosOffMaxVal = m_hPosOffsetCurve->GetMaxVal();
        tKpVal.x += fPosOffRatioDeltaX; if (tKpVal.x < tPosOffMinVal.x) tKpVal.x = tPosOffMinVal.x; if (tKpVal.x > tPosOffMaxVal.x) tKpVal.x = tPosOffMaxVal.x;
        tKpVal.y += fPosOffRatioDeltaY; if (tKpVal.y < tPosOffMinVal.y) tKpVal.y = tPosOffMinVal.y; if (tKpVal.y > tPosOffMaxVal.y) tKpVal.y = tPosOffMaxVal.y;
        m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (pParamUpdated) *pParamUpdated = true;
        return true;
    }

    void EnableKeyFramesOnPosOffset(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnPosOffset != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hPosOffsetCurve->CalcPointVal(m_tTimeRange.x);
                m_hPosOffsetCurve->ClearAll();
                m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal));
            }
            m_bEnableKeyFramesOnPosOffset = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnPosOffset() const override
    { return m_bEnableKeyFramesOnPosOffset; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnPosOffset() const override
    { return m_hPosOffsetCurve; }

    // Crop
    bool SetCrop(uint32_t u32CropL, uint32_t u32CropT, uint32_t u32CropR, uint32_t u32CropB) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropL == u32CropL && m_u32CropT == u32CropT && m_u32CropR == u32CropR && m_u32CropB == u32CropB)
            return true;
        if (m_u32InWidth > 0 && u32CropL+u32CropR > m_u32InWidth)
        {
            ostringstream oss; oss << "INVALID argument! CropL(" << u32CropL << ") + CropR(" << u32CropR << ") > InWidth(" << m_u32InWidth << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_u32InHeight > 0 && u32CropT+u32CropB > m_u32InHeight)
        {
            ostringstream oss; oss << "INVALID argument! CropT(" << u32CropT << ") + CropB(" << u32CropB << ") > InHeight(" << m_u32InHeight << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropL = u32CropL;
        m_u32CropT = u32CropT;
        m_u32CropR = u32CropR;
        m_u32CropB = u32CropB;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    bool SetCropL(uint32_t u32CropL) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropL == u32CropL)
            return true;
        if (m_u32InWidth > 0 && u32CropL+m_u32CropR > m_u32InWidth)
        {
            ostringstream oss; oss << "INVALID argument! CropL(" << u32CropL << ") + CropR(" << m_u32CropR << ") > InWidth(" << m_u32InWidth << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropL = u32CropL;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropL() const override
    { return m_u32CropL; }

    bool SetCropT(uint32_t u32CropT) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropT == u32CropT)
            return true;
        if (m_u32InHeight > 0 && u32CropT+m_u32CropB > m_u32InHeight)
        {
            ostringstream oss; oss << "INVALID argument! CropT(" << u32CropT << ") + CropB(" << m_u32CropB << ") > InHeight(" << m_u32InHeight << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropT = u32CropT;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropT() const override
    { return m_u32CropT; }

    bool SetCropR(uint32_t u32CropR) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropR == u32CropR)
            return true;
        if (m_u32InWidth > 0 && m_u32CropL+u32CropR > m_u32InWidth)
        {
            ostringstream oss; oss << "INVALID argument! CropL(" << m_u32CropL << ") + CropR(" << u32CropR << ") > InWidth(" << m_u32InWidth << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropR = u32CropR;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropR() const override
    { return m_u32CropR; }

    bool SetCropB(uint32_t u32CropB) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropB == u32CropB)
            return true;
        if (m_u32InHeight > 0 && m_u32CropT+u32CropB > m_u32InHeight)
        {
            ostringstream oss; oss << "INVALID argument! CropT(" << m_u32CropT << ") + CropB(" << u32CropB << ") > InHeight(" << m_u32InHeight << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropB = u32CropB;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropB() const override
    { return m_u32CropB; }

    bool SetCropRatio(float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB, bool bClipValue, bool* pParamUpdated) override
    { return SetCropRatio(m_tTimeRange.x, fCropRatioL, fCropRatioT, fCropRatioR, fCropRatioB, bClipValue, pParamUpdated); }

    bool SetCropRatioL(float fCropRatioL, bool bClipValue, bool* pParamUpdated) override
    { return SetCropRatioL(m_tTimeRange.x, fCropRatioL, bClipValue, pParamUpdated); }

    float GetCropRatioL() const override
    { return m_fCropRatioL; }

    bool SetCropRatioT(float fCropRatioT, bool bClipValue, bool* pParamUpdated) override
    { return SetCropRatioT(m_tTimeRange.x, fCropRatioT, bClipValue, pParamUpdated); }

    float GetCropRatioT() const override
    { return m_fCropRatioT; }

    bool SetCropRatioR(float fCropRatioR, bool bClipValue, bool* pParamUpdated) override
    { return SetCropRatioR(m_tTimeRange.x, fCropRatioR, bClipValue, pParamUpdated); }

    float GetCropRatioR() const override
    { return m_fCropRatioR; }

    bool SetCropRatioB(float fCropRatioB, bool bClipValue, bool* pParamUpdated) override
    { return SetCropRatioB(m_tTimeRange.x, fCropRatioB, bClipValue, pParamUpdated); }

    float GetCropRatioB() const override
    { return m_fCropRatioB; }

    bool SetCropRatio(int64_t i64Tick, float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB, bool bClipValue, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        auto tMinVal = m_aCropCurves[0]->GetMinVal();
        auto tMaxVal = m_aCropCurves[0]->GetMaxVal();
        if (fCropRatioL < tMinVal.x || fCropRatioL > tMaxVal.x)
        {
            if (bClipValue)
            {
                if      (fCropRatioL < tMinVal.x) fCropRatioL = tMinVal.x;
                else if (fCropRatioL > tMaxVal.x) fCropRatioL = tMaxVal.x;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioR(" << fCropRatioL << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (fCropRatioT < tMinVal.y || fCropRatioT > tMaxVal.y)
        {
            if (bClipValue)
            {
                if      (fCropRatioT < tMinVal.y) fCropRatioT = tMinVal.y;
                else if (fCropRatioT > tMaxVal.y) fCropRatioT = tMaxVal.y;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioB(" << fCropRatioT << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        tMinVal = m_aCropCurves[1]->GetMinVal();
        tMaxVal = m_aCropCurves[1]->GetMaxVal();
        if (fCropRatioR < tMinVal.x || fCropRatioR > tMaxVal.x)
        {
            if (bClipValue)
            {
                if      (fCropRatioR < tMinVal.x) fCropRatioR = tMinVal.x;
                else if (fCropRatioR > tMaxVal.x) fCropRatioR = tMaxVal.x;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioR(" << fCropRatioR << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (fCropRatioB < tMinVal.y || fCropRatioB > tMaxVal.y)
        {
            if (bClipValue)
            {
                if      (fCropRatioB < tMinVal.y) fCropRatioB = tMinVal.y;
                else if (fCropRatioB > tMaxVal.y) fCropRatioB = tMaxVal.y;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioB(" << fCropRatioB << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        const bool bParamUpdated0 = tKpVal.x != fCropRatioL || tKpVal.y != fCropRatioT;
        if (bParamUpdated0)
        {
            tKpVal.x = fCropRatioL; tKpVal.y = fCropRatioT;
            auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        const bool bParamUpdated1 = tKpVal.x != fCropRatioR || tKpVal.y != fCropRatioB;
        if (bParamUpdated1)
        {
            tKpVal.x = fCropRatioR; tKpVal.y = fCropRatioB;
            auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated0 || bParamUpdated1;
        return true;
    }

    bool SetCropRatioL(int64_t i64Tick, float fCropRatioL, bool bClipValue, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_aCropCurves[0]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[0]->GetMaxVal();
        if (fCropRatioL < tMinVal.x || fCropRatioL > tMaxVal.x)
        {
            if (bClipValue)
            {
                if      (fCropRatioL < tMinVal.x) fCropRatioL = tMinVal.x;
                else if (fCropRatioL > tMaxVal.x) fCropRatioL = tMaxVal.x;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioR(" << fCropRatioL << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        const bool bParamUpdated = tKpVal.x != fCropRatioL;
        if (bParamUpdated)
        {
            tKpVal.x = fCropRatioL;
            const auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated;
        return true;
    }

    float GetCropRatioL(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        return tKpVal.x;
    }

    bool SetCropRatioT(int64_t i64Tick, float fCropRatioT, bool bClipValue, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_aCropCurves[0]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[0]->GetMaxVal();
        if (fCropRatioT < tMinVal.y || fCropRatioT > tMaxVal.y)
        {
            if (bClipValue)
            {
                if      (fCropRatioT < tMinVal.y) fCropRatioT = tMinVal.y;
                else if (fCropRatioT > tMaxVal.y) fCropRatioT = tMaxVal.y;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioB(" << fCropRatioT << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        const bool bParamUpdated = tKpVal.y != fCropRatioT;
        if (bParamUpdated)
        {
            tKpVal.y = fCropRatioT;
            const auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated;
        return true;
    }

    float GetCropRatioT(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        return tKpVal.y;
    }

    bool SetCropRatioR(int64_t i64Tick, float fCropRatioR, bool bClipValue, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_aCropCurves[1]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[1]->GetMaxVal();
        if (fCropRatioR < tMinVal.x || fCropRatioR > tMaxVal.x)
        {
            if (bClipValue)
            {
                if      (fCropRatioR < tMinVal.x) fCropRatioR = tMinVal.x;
                else if (fCropRatioR > tMaxVal.x) fCropRatioR = tMaxVal.x;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioR(" << fCropRatioR << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        const bool bParamUpdated = tKpVal.x != fCropRatioR;
        if (bParamUpdated)
        {
            tKpVal.x = fCropRatioR;
            const auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated;
        return true;
    }

    float GetCropRatioR(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        return tKpVal.x;
    }

    bool SetCropRatioB(int64_t i64Tick, float fCropRatioB, bool bClipValue, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_aCropCurves[1]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[1]->GetMaxVal();
        if (fCropRatioB < tMinVal.y || fCropRatioB > tMaxVal.y)
        {
            if (bClipValue)
            {
                if      (fCropRatioB < tMinVal.y) fCropRatioB = tMinVal.y;
                else if (fCropRatioB > tMaxVal.y) fCropRatioB = tMaxVal.y;
            }
            else
            {
                ostringstream oss; oss << "INVALID argument vaule CropRatioB(" << fCropRatioB << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        const bool bParamUpdated = tKpVal.y != fCropRatioB;
        if (bParamUpdated)
        {
            tKpVal.y = fCropRatioB;
            const auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated;
        return true;
    }

    float GetCropRatioB(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        return tKpVal.y;
    }

    bool ChangeCropL(int64_t i64Tick, int32_t i32Delta) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        const auto v2FinalScale = MatUtils::ToImVec2(CalcFinalScale(tKpVal.x, tKpVal.y));
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        tKpVal.x += (float)i32Delta/((float)m_u32InWidth*v2FinalScale.x);
        const auto tMinVal = m_aCropCurves[0]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[0]->GetMaxVal();
        if (tKpVal.x < tMinVal.x) tKpVal.x = tMinVal.x;
        if (tKpVal.x > tMaxVal.x) tKpVal.x = tMaxVal.x;
        const auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to change crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool ChangeCropT(int64_t i64Tick, int32_t i32Delta) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        const auto v2FinalScale = MatUtils::ToImVec2(CalcFinalScale(tKpVal.x, tKpVal.y));
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        tKpVal.y += (float)i32Delta/((float)m_u32InHeight*v2FinalScale.y);
        const auto tMinVal = m_aCropCurves[0]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[0]->GetMaxVal();
        if (tKpVal.y < tMinVal.y) tKpVal.y = tMinVal.y;
        if (tKpVal.y > tMaxVal.y) tKpVal.y = tMaxVal.y;
        const auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to change crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool ChangeCropR(int64_t i64Tick, int32_t i32Delta) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        const auto v2FinalScale = MatUtils::ToImVec2(CalcFinalScale(tKpVal.x, tKpVal.y));
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        tKpVal.x += (float)i32Delta/((float)m_u32InWidth*v2FinalScale.x);
        const auto tMinVal = m_aCropCurves[1]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[1]->GetMaxVal();
        if (tKpVal.x < tMinVal.x) tKpVal.x = tMinVal.x;
        if (tKpVal.x > tMaxVal.x) tKpVal.x = tMaxVal.x;
        const auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to change crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool ChangeCropB(int64_t i64Tick, int32_t i32Delta) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        auto fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        const auto v2FinalScale = MatUtils::ToImVec2(CalcFinalScale(tKpVal.x, tKpVal.y));
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        tKpVal.y += (float)i32Delta/((float)m_u32InHeight*v2FinalScale.y);
        const auto tMinVal = m_aCropCurves[1]->GetMinVal();
        const auto tMaxVal = m_aCropCurves[1]->GetMaxVal();
        if (tKpVal.y < tMinVal.y) tKpVal.y = tMinVal.y;
        if (tKpVal.y > tMaxVal.y) tKpVal.y = tMaxVal.y;
        const auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to change crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    void EnableKeyFramesOnCrop(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnCrop != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal0 = m_aCropCurves[0]->CalcPointVal(m_tTimeRange.x);
                m_aCropCurves[0]->ClearAll();
                m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal0));
                const auto tHeadKpVal1 = m_aCropCurves[1]->CalcPointVal(m_tTimeRange.x);
                m_aCropCurves[1]->ClearAll();
                m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal1));
            }
            m_bEnableKeyFramesOnCrop = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnCrop() const override
    { return m_bEnableKeyFramesOnCrop; }

    vector<LibCurve::Curve::Holder> GetKeyFramesCurveOnCrop() const override
    { return m_aCropCurves; }

    // Scale
    bool SetScale(float fScaleX, float fScaleY) override
    { return SetScale(m_tTimeRange.x, fScaleX, fScaleY); }

    bool SetScaleX(float fScaleX) override
    { return SetScaleX(m_tTimeRange.x, fScaleX); }

    float GetScaleX() const override
    { return m_fScaleX; }

    bool SetScaleY(float fScaleY) override
    { return SetScaleY(m_tTimeRange.x, fScaleY); }

    float GetScaleY() const override
    { return m_fScaleY; }

    bool SetScale(int64_t i64Tick, float fScaleX, float fScaleY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_fScaleX == fScaleX && m_fScaleY == fScaleY)
            return true;
        const auto tMinVal = m_hScaleCurve->GetMinVal();
        const auto tMaxVal = m_hScaleCurve->GetMaxVal();
        if (fScaleX < tMinVal.x || fScaleX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value ScaleX(" << fScaleX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fScaleY < tMinVal.x || fScaleY > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value ScaleY(" << fScaleY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnScale)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fScaleX, fScaleY, 0.f, (float)i64Tick);
        const auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool SetScaleX(int64_t i64Tick, float fScaleX) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_fScaleX == fScaleX)
            return true;
        const auto tMinVal = m_hScaleCurve->GetMinVal();
        const auto tMaxVal = m_hScaleCurve->GetMaxVal();
        if (fScaleX < tMinVal.x || fScaleX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value ScaleX(" << fScaleX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        tKpVal.x = fScaleX;
        const auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetScaleX(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        return tKpVal.x;
    }

    bool SetScaleY(int64_t i64Tick, float fScaleY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_fScaleY == fScaleY)
            return true;
        const auto tMinVal = m_hScaleCurve->GetMinVal();
        const auto tMaxVal = m_hScaleCurve->GetMaxVal();
        if (fScaleY < tMinVal.y || fScaleY > tMaxVal.y)
        {
            ostringstream oss; oss << "INVALID argument value ScaleY(" << fScaleY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        tKpVal.y = fScaleY;
        const auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetScaleY(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        return tKpVal.y;
    }

    MatUtils::Vec2<float> GetScale() const override
    {
        return {m_fScaleX, m_fScaleY};
    }

    MatUtils::Vec2<float> GetScale(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        return m_bKeepAspectRatio ? MatUtils::Vec2<float>(tKpVal.x, tKpVal.x) : MatUtils::Vec2<float>(tKpVal.x, tKpVal.y);
    }

    MatUtils::Vec2<float> GetFinalScale(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        return CalcFinalScale(tKpVal.x, tKpVal.y);
    }

    bool ChangeScaleToFitOutputSize(int64_t i64Tick, uint32_t u32OutWidth, uint32_t u32OutHeight, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        auto fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tOrgVal = m_hScaleCurve->CalcPointVal(fTick, false);
        LibCurve::KeyPoint::ValType tKpVal(1.f, 1.f, 0, fTick);
        if (u32OutWidth == 0 && u32OutHeight == 0)
        {
            tKpVal.x = tKpVal.y = 0;
        }
        else
        {
            fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
            auto tCropKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
            const auto u32CropL = (uint32_t)round((float)m_u32InWidth*tCropKpVal.x);
            const auto u32CropT = (uint32_t)round((float)m_u32InHeight*tCropKpVal.y);
            tCropKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
            const auto u32CropR = (uint32_t)round((float)m_u32InWidth*tCropKpVal.x);
            const auto u32CropB = (uint32_t)round((float)m_u32InHeight*tCropKpVal.y);
            const auto u32OrgWidth = u32CropL+u32CropR > m_u32InWidth ? u32CropL+u32CropR-m_u32InWidth : m_u32InWidth-u32CropL-u32CropR;
            const auto u32OrgHeight = u32CropT+u32CropB > m_u32InHeight ? u32CropT+u32CropB-m_u32InHeight : m_u32InHeight-u32CropT-u32CropB;
            const auto v2AspectFitScale = CalcAspectFitScale();
            tKpVal.x = (float)u32OutWidth/(u32OrgWidth*v2AspectFitScale.x);
            tKpVal.y = (float)u32OutHeight/(u32OrgHeight*v2AspectFitScale.y);
        }
        bool bParamUpdated = false;
        if (tKpVal.x != tOrgVal.x || tKpVal.y != tOrgVal.y)
        {
            const auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
            bParamUpdated = true;
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated;
        return true;
    }

    void SetKeepAspectRatio(bool bEnable) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_bKeepAspectRatio != bEnable)
        {
            if (m_fScaleX != m_fScaleY)
                m_bNeedUpdateScaleParam = true;
            m_bKeepAspectRatio = bEnable;
        }
    }

    bool IsKeepAspectRatio() const override
    { return m_bKeepAspectRatio; }

    void EnableKeyFramesOnScale(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnScale != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hScaleCurve->CalcPointVal(m_tTimeRange.x);
                m_hScaleCurve->ClearAll();
                m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal));
            }
            m_bEnableKeyFramesOnScale = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnScale() const override
    { return m_bEnableKeyFramesOnScale; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnScale() const override
    { return m_hScaleCurve; }

    // Rotation
    bool SetRotation(float fAngle) override
    { return SetRotation(m_tTimeRange.x, fAngle, nullptr); }

    float GetRotation() const override
    { return m_fRotateAngle; }

    bool SetRotation(int64_t i64Tick, float fAngle, bool* pParamUpdated) override
    {
        if (pParamUpdated) *pParamUpdated = false;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        // int32_t n = (int32_t)trunc(fAngle/360);
        // fAngle -= n*360;
        auto fTick = m_bEnableKeyFramesOnRotation ? (float)i64Tick : (float)m_tTimeRange.x;
        auto tKpVal = m_hRotationCurve->CalcPointVal(fTick, false);
        const bool bParamUpdated = tKpVal.x != fAngle;
        if (bParamUpdated)
        {
            tKpVal.x = fAngle;
            auto iRet = m_hRotationCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
            if (iRet < 0)
            {
                ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set rotation as (" << tKpVal.x << ") at time tick " << i64Tick << " !";
                m_strErrMsg = oss.str();
                return false;
            }
        }
        if (pParamUpdated) *pParamUpdated = bParamUpdated;
        return true;
    }

    float GetRotation(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnRotation ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hRotationCurve->CalcPointVal(fTick, false);
        return tKpVal.x;
    }

    void EnableKeyFramesOnRotation(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnRotation != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hRotationCurve->CalcPointVal(m_tTimeRange.x);
                m_hRotationCurve->ClearAll();
                m_hRotationCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal));
            }
            m_bEnableKeyFramesOnRotation = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnRotation() const override
    { return m_bEnableKeyFramesOnRotation; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnRotation() const override
    { return m_hRotationCurve; }

    // Opacity
    bool SetOpacity(float opacity) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        m_fOpacity = opacity;
        return true;
    }

    float GetOpacity() const override
    { return m_fOpacity; }

    bool SetOpacity(int64_t i64Tick, float fOpacity) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_fOpacity == fOpacity)
            return true;
        const auto tMinVal = m_hOpacityCurve->GetMinVal();
        const auto tMaxVal = m_hOpacityCurve->GetMaxVal();
        if (fOpacity < tMinVal.x || fOpacity > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value Opacity(" << fOpacity << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnOpacity)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fOpacity, 0.f, 0.f, (float)i64Tick);
        auto iRet = m_hOpacityCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal));
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set opacity as (" << tKpVal.x << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetOpacity(int64_t i64Tick) const override
    {
        const auto fTick = m_bEnableKeyFramesOnOpacity ? (float)i64Tick : (float)m_tTimeRange.x;
        const auto tKpVal = m_hOpacityCurve->CalcPointVal(fTick, false);
        return tKpVal.x;
    }

    void EnableKeyFramesOnOpacity(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnOpacity != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hOpacityCurve->CalcPointVal(m_tTimeRange.x);
                m_hOpacityCurve->ClearAll();
                m_hOpacityCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal));
            }
            m_bEnableKeyFramesOnOpacity = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnOpacity() const override
    { return m_bEnableKeyFramesOnOpacity; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnOpacity() const override
    { return m_hOpacityCurve; }

    ImGui::MaskCreator::Holder CreateNewOpacityMask(const std::string& name) override
    {
        const MatUtils::Size2i szMaskSize(m_pOwnerClip->OutWidth(), m_pOwnerClip->OutHeight());
        auto hMaskCreator = ImGui::MaskCreator::CreateInstance(szMaskSize, name);
        hMaskCreator->SetTickRange(0, m_pOwnerClip->Duration());
        m_ahMaskCreators.push_back(hMaskCreator);
        return hMaskCreator;
    }

    int GetOpacityMaskCount() const override
    {
        return m_ahMaskCreators.size();
    }

    const ImGui::MaskCreator::Holder GetOpacityMaskCreator(size_t index) const override
    {
        const auto szMaskCnt = m_ahMaskCreators.size();
        if (index >= szMaskCnt)
            return nullptr;
        return m_ahMaskCreators[index];
    }

    bool RemoveOpacityMask(size_t index) override
    {
        const auto szMaskCnt = m_ahMaskCreators.size();
        if (index >= szMaskCnt)
            return false;
        auto itDel = m_ahMaskCreators.begin()+index;
        m_ahMaskCreators.erase(itDel);
        return true;
    }

    void SetUiStateJson(const imgui_json::value& j) override
    {
        m_jnUiState = j;
    }

    imgui_json::value GetUiStateJson() const override
    {
        return m_jnUiState;
    }

    imgui_json::value SaveAsJson() const override
    {
        imgui_json::value j;
        j["output_format"] = imgui_json::string(m_strOutputFormat);
        j["aspect_fit_type"] = imgui_json::number((int)m_eAspectFitType);
        j["pos_offset_curve"] = m_hPosOffsetCurve->SaveAsJson();
        j["pos_offset_keyframes_enabled"] = m_bEnableKeyFramesOnPosOffset;
        j["crop_lt_curve"] = m_aCropCurves[0]->SaveAsJson();
        j["crop_rb_curve"] = m_aCropCurves[1]->SaveAsJson();
        j["crop_keyframes_enabled"] = m_bEnableKeyFramesOnCrop;
        j["scale_curve"] = m_hScaleCurve->SaveAsJson();
        j["keep_aspect_ratio"] = m_bKeepAspectRatio;
        j["scale_keyframes_enabled"] = m_bEnableKeyFramesOnScale;
        j["rotation_curve"] = m_hRotationCurve->SaveAsJson();
        j["rotation_keyframes_enabled"] = m_bEnableKeyFramesOnRotation;
        j["opacity_curve"] = m_hOpacityCurve->SaveAsJson();
        j["opacity_keyframes_enabled"] = m_bEnableKeyFramesOnOpacity;
        j["ui_state"] = m_jnUiState;
        imgui_json::array ajnOpacityMasks;
        for (const auto& hMaskCreator : m_ahMaskCreators)
        {
            imgui_json::value jnMask;
            if (hMaskCreator->SaveAsJson(jnMask))
                ajnOpacityMasks.push_back(jnMask);
        }
        if (!ajnOpacityMasks.empty())
            j["opacity_masks"] = ajnOpacityMasks;
        return std::move(j);
    }

    bool LoadFromJson(const imgui_json::value& j) override
    {
        string strAttrName;
        strAttrName = "output_format";
        if (j.contains(strAttrName) && j[strAttrName].is_string())
        {
            if (!SetOutputFormat(j[strAttrName].get<imgui_json::string>()))
                return false;
        }
        strAttrName = "aspect_fit_type";
        if (j.contains(strAttrName) && j[strAttrName].is_number())
        {
            if (!SetAspectFitType((AspectFitType)j[strAttrName].get<imgui_json::number>()))
                return false;
        }
        strAttrName = "pos_offset_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hPosOffsetCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "pos_offset_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnPosOffset = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "crop_lt_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_aCropCurves[0]->LoadFromJson(j[strAttrName]);
        strAttrName = "crop_rb_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_aCropCurves[1]->LoadFromJson(j[strAttrName]);
        strAttrName = "crop_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnCrop = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "scale_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hScaleCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "keep_aspect_ratio";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bKeepAspectRatio = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "scale_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnScale = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "rotation_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hRotationCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "rotation_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnRotation = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "opacity_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hOpacityCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "opacity_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnOpacity = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "ui_state";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_jnUiState = j[strAttrName];
        strAttrName = "opacity_masks";
        if (j.contains(strAttrName) && j[strAttrName].is_array())
        {
            const auto& ajnMasks = j[strAttrName].get<imgui_json::array>();
            for (const auto& jnMask : ajnMasks)
            {
                auto hMaskCreator = ImGui::MaskCreator::LoadFromJson(jnMask);
                if (m_pOwnerClip) hMaskCreator->SetTickRange(0, m_pOwnerClip->Duration());
                m_ahMaskCreators.push_back(hMaskCreator);
            }
        }
        return true;
    }

    string GetError() const override
    { return m_strErrMsg; }

protected:
    bool UpdateParamsByKeyFrames(int64_t i64Tick)
    {
        float fTick;
        LibCurve::KeyPoint::ValType tKpVal;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        // Position offset
        fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false);
        const auto i32PosOffX = (int32_t)round((float)m_u32OutWidth*tKpVal.x);
        const auto i32PosOffY = (int32_t)round((float)m_u32OutHeight*tKpVal.y);
        if (i32PosOffX != m_i32PosOffsetX || i32PosOffY != m_i32PosOffsetY)
        {
            m_i32PosOffsetX = i32PosOffX;
            m_i32PosOffsetY = i32PosOffY;
            m_fPosOffsetRatioX = tKpVal.x;
            m_fPosOffsetRatioY = tKpVal.y;
            m_bNeedUpdatePosOffsetParam = true;
        }
        // Crop
        if (m_bNeedUpdateCropRatioParam)
        {
            const auto fCropRatioL = (float)m_u32CropL/m_u32InWidth;
            const auto fCropRatioT = (float)m_u32CropT/m_u32InHeight;
            const auto fCropRatioR = (float)m_u32CropR/m_u32InWidth;
            const auto fCropRatioB = (float)m_u32CropB/m_u32InHeight;
            m_bNeedUpdateCropRatioParam = false;
            if (!SetCropRatio(fCropRatioL, fCropRatioT, fCropRatioR, fCropRatioB, false, nullptr))
                return false;
        }
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false);
        const auto u32CropL = (uint32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto u32CropT = (uint32_t)round((float)m_u32InHeight*tKpVal.y);
        m_fCropRatioL = tKpVal.x; m_fCropRatioT = tKpVal.y;
        tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false);
        const auto u32CropR = (uint32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto u32CropB = (uint32_t)round((float)m_u32InHeight*tKpVal.y);
        m_fCropRatioR = tKpVal.x; m_fCropRatioB = tKpVal.y;
        if (u32CropL != m_u32CropL || u32CropT != m_u32CropT || u32CropR != m_u32CropR || u32CropB != m_u32CropB)
        {
            m_u32CropL = u32CropL;
            m_u32CropT = u32CropT;
            m_u32CropR = u32CropR;
            m_u32CropB = u32CropB;
            m_bNeedUpdateCropParam = true;
        }
        // Scale
        fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hScaleCurve->CalcPointVal(fTick, false);
        if (tKpVal.x != m_fScaleX || (!m_bKeepAspectRatio && tKpVal.y != m_fScaleY))
        {
            m_fScaleX = tKpVal.x;
            m_fScaleY = tKpVal.y;
            m_bNeedUpdateScaleParam = true;
        }
        // Rotation
        fTick = m_bEnableKeyFramesOnRotation ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hRotationCurve->CalcPointVal(fTick, false);
        if (tKpVal.x != m_fRotateAngle)
        {
            m_fRotateAngle = tKpVal.x;
            m_bNeedUpdateRotationParam = true;
        }
        // Opacity
        fTick = m_bEnableKeyFramesOnOpacity ? (float)i64Tick : (float)m_tTimeRange.x;
        tKpVal = m_hOpacityCurve->CalcPointVal(fTick, false);
        if (tKpVal.x != m_fRotateAngle)
        {
            m_fOpacity = tKpVal.x;
        }

        return true;
    }

    MatUtils::Vec2<float> CalcAspectFitScale() const
    {
        uint32_t u32FitScaleWidth{m_u32InWidth}, u32FitScaleHeight{m_u32InHeight};
        switch (m_eAspectFitType)
        {
            case ASPECT_FIT_TYPE__FIT:
            if (m_u32InWidth*m_u32OutHeight > m_u32InHeight*m_u32OutWidth)
            {
                u32FitScaleWidth = m_u32OutWidth;
                u32FitScaleHeight = (uint32_t)round((float)m_u32InHeight*m_u32OutWidth/m_u32InWidth);
            }
            else
            {
                u32FitScaleHeight = m_u32OutHeight;
                u32FitScaleWidth = (uint32_t)round((float)m_u32InWidth*m_u32OutHeight/m_u32InHeight);
            }
            break;
            case ASPECT_FIT_TYPE__CROP:
            u32FitScaleWidth = m_u32InWidth;
            u32FitScaleHeight = m_u32InHeight;
            break;
            case ASPECT_FIT_TYPE__FILL:
            if (m_u32InWidth*m_u32OutHeight > m_u32InHeight*m_u32OutWidth)
            {
                u32FitScaleHeight = m_u32OutHeight;
                u32FitScaleWidth = (uint32_t)round((float)m_u32InWidth*m_u32OutHeight/m_u32InHeight);
            }
            else
            {
                u32FitScaleWidth = m_u32OutWidth;
                u32FitScaleHeight = (uint32_t)round((float)m_u32InHeight*m_u32OutWidth/m_u32InWidth);
            }
            break;
            case ASPECT_FIT_TYPE__STRETCH:
            u32FitScaleWidth = m_u32OutWidth;
            u32FitScaleHeight = m_u32OutHeight;
            break;
        }
        const auto fAspectFitScaleX = (float)u32FitScaleWidth/m_u32InWidth;
        const auto fAspectFitScaleY = (float)u32FitScaleHeight/m_u32InHeight;
        return {fAspectFitScaleX, fAspectFitScaleY};
    }

    MatUtils::Vec2<float> CalcFinalScale(float fScaleX, float fScaleY) const
    {
        const auto v2AspectFitScale = CalcAspectFitScale();
        const auto fRealScaleRatioX = v2AspectFitScale.x*fScaleX;
        const auto fRealScaleRatioY = v2AspectFitScale.y*(m_bKeepAspectRatio ? fScaleX : fScaleY);
        return {fRealScaleRatioX, fRealScaleRatioY};
    }

protected:
    recursive_mutex m_mtxProcessLock;
    uint32_t m_u32InWidth{0}, m_u32InHeight{0};
    uint32_t m_u32OutWidth{0}, m_u32OutHeight{0};
    string m_strOutputFormat;
    AspectFitType m_eAspectFitType{ASPECT_FIT_TYPE__FIT};
    MatUtils::Vec2<int64_t> m_tTimeRange;
    int32_t m_i32PosOffsetX{0}, m_i32PosOffsetY{0};
    float m_fPosOffsetRatioX{0}, m_fPosOffsetRatioY{0};
    uint32_t m_u32CropL{0}, m_u32CropR{0}, m_u32CropT{0}, m_u32CropB{0};
    float m_fCropRatioL{0}, m_fCropRatioR{0}, m_fCropRatioT{0}, m_fCropRatioB{0};
    float m_fScaleX{1}, m_fScaleY{1};
    bool m_bKeepAspectRatio{false};
    float m_fRotateAngle{0};
    float m_fOpacity{1.f};
    bool m_bNeedUpdatePosOffsetParam{false};
    bool m_bNeedUpdateCropParam{false};
    bool m_bNeedUpdateCropRatioParam{false};
    bool m_bNeedUpdateRotationParam{false};
    bool m_bNeedUpdateScaleParam{true};
    LibCurve::Curve::Holder m_hPosOffsetCurve;
    bool m_bEnableKeyFramesOnPosOffset{false};
    vector<LibCurve::Curve::Holder> m_aCropCurves;
    bool m_bEnableKeyFramesOnCrop{false};
    LibCurve::Curve::Holder m_hScaleCurve;
    bool m_bEnableKeyFramesOnScale{false};
    LibCurve::Curve::Holder m_hRotationCurve;
    bool m_bEnableKeyFramesOnRotation{false};
    LibCurve::Curve::Holder m_hOpacityCurve;
    bool m_bEnableKeyFramesOnOpacity{false};
    vector<ImGui::MaskCreator::Holder> m_ahMaskCreators;
    VideoClip* m_pOwnerClip{nullptr};
    int64_t m_i64ClipStartOffset{0}, m_i64ClipEndOffset{0}, m_i64ClipDuration{0};
    imgui_json::value m_jnUiState;
    string m_strErrMsg;
};
}