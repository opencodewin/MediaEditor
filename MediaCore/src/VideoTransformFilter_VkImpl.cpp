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

#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include <imgui.h>
#include <warpAffine_vulkan.h>
#include <OpacityFilter_vulkan.h>
#include <ColorConvert_vulkan.h>
#include <MatMath.h>
#include "VideoTransformFilter_Base.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
class VideoTransformFilter_VkImpl : public VideoTransformFilter_Base
{
public:
    ~VideoTransformFilter_VkImpl()
    {
        if (m_pWarpAffine)
        {
            delete m_pWarpAffine;
            m_pWarpAffine = nullptr;
        }
        if (m_pOpacityFilter)
        {
            delete m_pOpacityFilter;
            m_pOpacityFilter = nullptr;
        }
    }

    const std::string GetFilterName() const
    {
        return "VideoTransformFilter_VkImpl";
    }

    bool Initialize(SharedSettings::Holder hSettings)
    {
        const auto u32OutWidth = hSettings->VideoOutWidth();
        const auto u32OutHeight = hSettings->VideoOutHeight();
        if (u32OutWidth == 0 || u32OutHeight == 0)
        {
            m_strErrMsg = "INVALID argument! 'VideoOutWidth' and 'VideoOutHeight' must be positive value.";
            return false;
        }
        m_u32OutWidth = u32OutWidth;
        m_u32OutHeight = u32OutHeight;
        m_mAffineMatrix.create_type(3, 2, IM_DT_FLOAT32);
        memset(m_mAffineMatrix.data, 0, m_mAffineMatrix.elemsize*m_mAffineMatrix.total());
        m_mAffineMatrix.at<float>(0, 0) = 1;
        m_mAffineMatrix.at<float>(1, 1) = 1;
        if (!SetOutputFormat("rgba"))
            return false;
        m_bNeedUpdateScaleParam = true;
        return true;
    }


    bool SetOutputFormat(const string& outputFormat)
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (outputFormat != "rgba")
        {
            m_strErrMsg = "ONLY support using 'rgba' as output format!";
            return false;
        }

        m_strOutputFormat = outputFormat;
        return true;
    }

    ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos, float& fOpacity)
    {
        ImGui::ImMat res;
        if (!_filterImage(vmat, res, pos))
        {
            res.release();
            Log(Error) << "VideoTransformFilter_VkImpl::FilterImage() FAILED! " << m_strErrMsg << endl;
        }

        const int64_t i64Tick = pos;
        fOpacity = GetOpacity(i64Tick);
        if (!m_ahMaskCreators.empty())
        {
            ImGui::ImMat mCombinedMask;
            const MatUtils::Size2i szImageSize(res.w, res.h);
            const auto v2WaOffsetRatio = MatUtils::ToImVec2(GetPosOffsetRatio());
            const auto v2WaScale = MatUtils::ToImVec2(GetScale());
            const auto v2OutSize = MatUtils::ToImVec2(GetOutSize());
            const auto v2WaOffset = v2WaOffsetRatio*(v2WaScale+ImVec2(1,1))*v2OutSize/2.f;
            const auto fWaRotAngle = GetRotation();
            const auto szMaskCnt = m_ahMaskCreators.size();
            for (auto i = 0; i < szMaskCnt; i++)
            {
                auto& hMaskCreator = m_ahMaskCreators[i];
                if (szImageSize != hMaskCreator->GetMaskSize())
                    hMaskCreator->ChangeMaskSize(szImageSize, true);
                m_ahMaskCreators[i]->SetMaskWarpAffineParameters(v2WaOffset, v2WaScale, -fWaRotAngle, MatUtils::ToImVec2(szImageSize)/2.f);
                if (!hMaskCreator->IsMaskReady())
                    continue;
                auto mMask = m_ahMaskCreators[i]->GetMask(ImGui::MaskCreator::AA, true, IM_DT_FLOAT32, 1, 0, i64Tick);
                if (mCombinedMask.empty())
                    mCombinedMask = mMask.clone();
                else
                    MatUtils::Max(mCombinedMask, mMask);
            }
            if (!mCombinedMask.empty())
            {
                if (!m_pOpacityFilter) m_pOpacityFilter = new ImGui::OpacityFilter_vulkan();
                const bool bInplace = res.data != vmat.data;
                // int tx = 100, ty = 100;
                // ImGui::ImMat dbgMat;
                // if (res.device == IM_DD_CPU)
                //     dbgMat = res;
                // else
                // {
                //     dbgMat.type = res.type;
                //     dbgMat.device = IM_DD_CPU;
                //     m_tClrCvt.Conv(res, dbgMat);
                // }
                // const int alphaVal0 = (int)dbgMat.at<uint8_t>(tx, ty, 3);
                // const float maskVal = mCombinedMask.at<float>(tx, ty);
                // Log(WARN) << "---> Before 'maskOpacity', at pos(" << tx << ", " << ty << "), alpha=" << alphaVal0 << ", mask=" << maskVal << ", opacity=" << fOpacity << ", inplace=" << bInplace << endl;
                res = m_pOpacityFilter->maskOpacity(res, mCombinedMask, fOpacity, false, bInplace);
                // if (res.device == IM_DD_CPU)
                //     dbgMat = res;
                // else
                // {
                //     dbgMat.type = res.type;
                //     dbgMat.device = IM_DD_CPU;
                //     m_tClrCvt.Conv(res, dbgMat);
                // }
                // const int alphaVal1 = (int)dbgMat.at<uint8_t>(tx, ty, 3);
                // const int alphaExpected = (int)floor((float)alphaVal0*maskVal*fOpacity);
                // Log(WARN) << "<--- After 'maskOpacity', at pos(" << tx << ", " << ty << "), alpha=" << alphaVal1 << ", expected=" << alphaExpected << endl;
                fOpacity = 1.f;
            }
        }

        return res;
    }

private:
    bool _filterImage(const ImGui::ImMat& inMat, ImGui::ImMat& outMat, int64_t pos)
    {
        m_u32InWidth = inMat.w; m_u32InHeight = inMat.h;
        if (!UpdateParamsByKeyFrames(pos))
        {
            Log(Error) << "[VideoTransformFilter_VkImpl::_filterImage] 'UpdateParamsByKeyFrames()' at pos " << pos << " FAILED!" << endl;
            return false;
        }

        if (m_bNeedUpdateScaleParam)
        {
            const auto v2FinalScale = CalcFinalScale(m_fScaleX, m_fScaleY);
            m_fFinalScaleRatioX = v2FinalScale.x;
            m_fFinalScaleRatioY = v2FinalScale.y;
        }
        if (m_bNeedUpdateScaleParam || m_bNeedUpdateRotationParam || m_bNeedUpdatePosOffsetParam)
        {
            const float fScaleFactorX = 1.f/(m_fFinalScaleRatioX+FLT_EPSILON);
            const float fScaleFactorY = 1.f/(m_fFinalScaleRatioY+FLT_EPSILON);
            const float fRotationRadian = m_fRotateAngle/180.f*M_PI;
            const float fAlpha00 = cos(fRotationRadian)*fScaleFactorX;
            const float fAlpha11 = cos(fRotationRadian)*fScaleFactorY;
            const float fBeta01 = sin(fRotationRadian)*fScaleFactorX;
            const float fBeta10 = sin(fRotationRadian)*fScaleFactorY;
            const float fSizeDiffX = (float)m_u32OutWidth-(float)m_u32InWidth;
            const float fSizeDiffY = (float)m_u32OutHeight-(float)m_u32InHeight;
            const float fSizeSumX = (m_u32OutWidth+m_u32InWidth*m_fFinalScaleRatioX)/2.f;
            const float fSizeSumY = (m_u32OutHeight+m_u32InHeight*m_fFinalScaleRatioY)/2.f;
            const float fOffsetRatioX = (float)m_i32PosOffsetX/(float)m_u32OutWidth;
            const float fOffsetX = fOffsetRatioX*fSizeSumX+fSizeDiffX/2.f;
            const float fOffsetRatioY = (float)m_i32PosOffsetY/(float)m_u32OutHeight;
            const float fOffsetY = fOffsetRatioY*fSizeSumY+fSizeDiffY/2.f;
            const float fCenterX = (float)m_u32InWidth/2.f+fOffsetX;
            const float fCenterY = (float)m_u32InHeight/2.f+fOffsetY;
            m_mAffineMatrix.at<float>(0, 0) = fAlpha00;
            m_mAffineMatrix.at<float>(1, 0) = fBeta01;
            m_mAffineMatrix.at<float>(2, 0) = (1-fAlpha00)*fCenterX-fBeta01*fCenterY-fOffsetX;
            m_mAffineMatrix.at<float>(0, 1) = -fBeta10;
            m_mAffineMatrix.at<float>(1, 1) = fAlpha11;
            m_mAffineMatrix.at<float>(2, 1) = fBeta10*fCenterX+(1-fAlpha11)*fCenterY-fOffsetY;
            m_bNeedUpdateScaleParam = m_bNeedUpdateRotationParam = m_bNeedUpdatePosOffsetParam = false;
            UpdatePassThrough();
        }

        if (m_bNeedUpdateCropRatioParam)
        {
            m_u32CropL = (uint32_t)((float)m_u32InWidth*m_fCropRatioL);
            m_u32CropR = (uint32_t)((float)m_u32InWidth*m_fCropRatioR);
            m_u32CropT = (uint32_t)((float)m_u32InHeight*m_fCropRatioT);
            m_u32CropB = (uint32_t)((float)m_u32InHeight*m_fCropRatioB);
            m_bNeedUpdateCropRatioParam = false;
            m_bNeedUpdateCropParam = true;
        }
        if (m_bNeedUpdateCropParam)
        {
            auto l = m_u32CropL;
            auto t = m_u32CropT;
            auto r = m_u32CropR;
            auto b = m_u32CropB;
            if (l+r > m_u32InWidth)
            {
                const auto tmp = l;
                l = m_u32InWidth-r;
                r = m_u32InWidth-tmp;
            }
            if (t+b > m_u32InHeight)
            {
                const auto tmp = t;
                t = m_u32InHeight-b;
                b = m_u32InHeight-tmp;
            }
            m_tCropRect = ImPixel((float)l, (float)t, (float)r, (float)b);
            m_bNeedUpdateCropParam = false;
            UpdatePassThrough();
        }

        if (m_bPassThrough)
            outMat = inMat;
        else
        {
            ImGui::VkMat vkmat; vkmat.type = inMat.type;
            vkmat.w = m_u32OutWidth; vkmat.h = m_u32OutHeight;
            if (!m_pWarpAffine)
                m_pWarpAffine = new ImGui::warpAffine_vulkan();
            m_pWarpAffine->warp(inMat, vkmat, m_mAffineMatrix, m_eInterpMode, ImPixel(0, 0, 0, 0), m_tCropRect);
            outMat = vkmat;
        }
        return true;
    }

    void UpdatePassThrough()
    {
        if (m_fFinalScaleRatioX == 1 && m_fFinalScaleRatioY == 1 &&
            m_u32InWidth == m_u32OutWidth && m_u32InHeight == m_u32OutHeight &&
            m_u32CropL == 0 && m_u32CropT == 0 && m_u32CropR == 0 && m_u32CropB == 0 &&
            m_fRotateAngle == 0 &&
            m_i32PosOffsetX == 0 && m_i32PosOffsetY == 0)
            m_bPassThrough = true;
        else
            m_bPassThrough = false;
    }

private:
    ImGui::warpAffine_vulkan* m_pWarpAffine{nullptr};
    ImGui::ImMat m_mAffineMatrix;
    float m_fFinalScaleRatioX{1}, m_fFinalScaleRatioY{1};
    ImPixel m_tCropRect;
    ImInterpolateMode m_eInterpMode{IM_INTERPOLATE_BICUBIC};
    ImGui::OpacityFilter_vulkan* m_pOpacityFilter{nullptr};
    bool m_bPassThrough{false};
    std::string m_strErrMsg;
};

VideoTransformFilter::Holder CreateVideoTransformFilterInstance_VkImpl()
{
    return VideoTransformFilter::Holder(new VideoTransformFilter_VkImpl(), [] (auto p) {
        VideoTransformFilter_VkImpl* ptr = dynamic_cast<VideoTransformFilter_VkImpl*>(p);
        delete ptr;
    });
}
}
#endif
