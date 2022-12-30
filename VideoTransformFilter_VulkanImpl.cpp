#include <imgui.h>
#if IMGUI_VULKAN_SHADER
#include "VideoTransformFilter_VulkanImpl.h"
#include "Logger.h"
#include <cmath>

using namespace std;
using namespace Logger;

namespace DataLayer
{
    const std::string VideoTransformFilter_VulkanImpl::GetFilterName() const
    {
        return "VideoTransformFilter_VulkanImpl";
    }

    bool VideoTransformFilter_VulkanImpl::Initialize(uint32_t outWidth, uint32_t outHeight)
    {
        if (outWidth == 0 || outHeight == 0)
        {
            m_errMsg = "INVALID argument! 'outWidth' and 'outHeight' must be positive value.";
            return false;
        }
        m_outWidth = outWidth;
        m_outHeight = outHeight;
        m_affineMat.create_type(3, 2, IM_DT_FLOAT32);
        memset(m_affineMat.data, 0, m_affineMat.elemsize*m_affineMat.total());
        m_affineMat.at<float>(0, 0) = 1;
        m_affineMat.at<float>(1, 1) = 1;

        if (!SetOutputFormat("rgba"))
        {
            return false;
        }
        m_needUpdateScaleParam = true;
        return true;
    }


    bool VideoTransformFilter_VulkanImpl::SetOutputFormat(const string& outputFormat)
    {
        lock_guard<recursive_mutex> lk(m_processLock);
        if (outputFormat != "rgba")
        {
            m_errMsg = "ONLY support using 'rgba' as output format!";
            return false;
        }

        m_outputFormat = outputFormat;
        return true;
    }

    ImGui::ImMat VideoTransformFilter_VulkanImpl::FilterImage(const ImGui::ImMat& vmat, int64_t pos)
    {
        ImGui::ImMat res;
        if (!_filterImage(vmat, res, pos))
        {
            res.release();
            Log(Error) << "FilterImage() FAILED! " << m_errMsg << endl;
        }
        return res;
    }

    bool VideoTransformFilter_VulkanImpl::_filterImage(const ImGui::ImMat& inMat, ImGui::ImMat& outMat, int64_t pos)
    {
        m_inWidth = inMat.w; m_inHeight = inMat.h;

        // check and update key points values
        for (int i = 0; i < m_keyPoints.GetCurveCount(); i++)
        {
            auto name = m_keyPoints.GetCurveName(i);
            auto value = m_keyPoints.GetValue(i, pos);
            if (name == "CropMarginL")
                SetCropMarginL(value);
            else if (name == "CropMarginT")
                SetCropMarginT(value);
            else if (name == "CropMarginR")
                SetCropMarginR(value);
            else if (name == "CropMarginB")
                SetCropMarginB(value);
            else if (name == "Scale")
            {
                SetScaleH(value);
                SetScaleV(value);
            }
            else if (name == "ScaleH")
                SetScaleH(value);
            else if (name == "ScaleV")
                SetScaleV(value);
            else if (name == "RotateAngle")
                SetRotationAngle(value);
            else if (name == "PositionOffsetH")
                SetPositionOffsetH(value);
            else if (name == "PositionOffsetV")
                SetPositionOffsetV(value);
            else
                Log(WARN) << "UNKNOWN curve name '" << name << "', value=" << value << "." << endl;
        }

        if (m_needUpdateScaleParam)
        {
            uint32_t fitScaleWidth{m_inWidth}, fitScaleHeight{m_inHeight};
            switch (m_scaleType)
            {
                case SCALE_TYPE__FIT:
                if (m_inWidth*m_outHeight > m_inHeight*m_outWidth)
                {
                    fitScaleWidth = m_outWidth;
                    fitScaleHeight = (uint32_t)round((double)m_inHeight*m_outWidth/m_inWidth);
                }
                else
                {
                    fitScaleHeight = m_outHeight;
                    fitScaleWidth = (uint32_t)round((double)m_inWidth*m_outHeight/m_inHeight);
                }
                break;
                case SCALE_TYPE__CROP:
                fitScaleWidth = m_inWidth;
                fitScaleHeight = m_inHeight;
                break;
                case SCALE_TYPE__FILL:
                if (m_inWidth*m_outHeight > m_inHeight*m_outWidth)
                {
                    fitScaleHeight = m_outHeight;
                    fitScaleWidth = (uint32_t)round((double)m_inWidth*m_outHeight/m_inHeight);
                }
                else
                {
                    fitScaleWidth = m_outWidth;
                    fitScaleHeight = (uint32_t)round((double)m_inHeight*m_outWidth/m_inWidth);
                }
                break;
                case SCALE_TYPE__STRETCH:
                fitScaleWidth = m_outWidth;
                fitScaleHeight = m_outHeight;
                break;
            }
            m_realScaleRatioH = (double)fitScaleWidth/m_inWidth*m_scaleRatioH;
            m_realScaleRatioV = (double)fitScaleHeight/m_inHeight*m_scaleRatioV;
        }
        if (m_needUpdateScaleParam || m_needUpdateRotateParam || m_needUpdatePositionParam)
        {
            float _x_scale = 1.f / (m_realScaleRatioH + FLT_EPSILON);
            float _y_scale = 1.f / (m_realScaleRatioV + FLT_EPSILON);
            float _angle = m_rotateAngle / 180.f * M_PI;
            float alpha_00 = cos(_angle) * _x_scale;
            float alpha_11 = cos(_angle) * _y_scale;
            float beta_01 = sin(_angle) * _x_scale;
            float beta_10 = sin(_angle) * _y_scale;
            float x_diff = m_outWidth - m_inWidth;
            float y_diff = m_outHeight - m_inHeight;
            float _x_diff = (m_outWidth + m_inWidth * m_realScaleRatioH) / 2.f;
            float _y_diff = (m_outHeight + m_inHeight * m_realScaleRatioV) / 2.f;
            float x_offset = (float)m_posOffsetH / (float)m_outWidth;
            float _x_offset = x_offset * _x_diff + x_diff / 2;
            float y_offset = (float)m_posOffsetV / (float)m_outHeight;
            float _y_offset = y_offset * _y_diff + y_diff / 2;
            int center_x = m_inWidth / 2 + _x_offset;
            int center_y = m_inHeight / 2 + _y_offset;
            m_affineMat.at<float>(0, 0) =  alpha_00;
            m_affineMat.at<float>(1, 0) = beta_01;
            m_affineMat.at<float>(2, 0) = (1 - alpha_00) * center_x - beta_01 * center_y - _x_offset;
            m_affineMat.at<float>(0, 1) = -beta_10;
            m_affineMat.at<float>(1, 1) = alpha_11;
            m_affineMat.at<float>(2, 1) = beta_10 * center_x + (1 - alpha_11) * center_y - _y_offset;
            m_needUpdateScaleParam = m_needUpdateRotateParam = m_needUpdatePositionParam = false;
            UpdatePassThrough();
            m_cropperX = ((int32_t)m_inWidth-(int32_t)m_outWidth)/2;
            m_cropperY = ((int32_t)m_inHeight-(int32_t)m_outHeight)/2;
        }
        if (m_needUpdateCropParam)
        {
            float _l = (float)m_cropL;
            float _t = (float)m_cropT;
            float _r = (float)m_cropR;
            float _b = (float)m_cropB;
            if (_l+_r > m_inWidth) { float tmp = _l; _l = m_inWidth-_r; _r = m_inWidth-tmp; }
            if (_t+_b > m_inHeight) { float tmp = _t; _t = m_inHeight-_b; _b = m_inHeight-tmp; }
            m_cropRect = {_l, _t, _r, _b};
            m_needUpdateCropParam = false;
            UpdatePassThrough();
        }

        if (m_passThrough)
            outMat = inMat;
        else
        {
            ImGui::VkMat vkmat; vkmat.type = IM_DT_INT8;
            m_warpAffine.filter(inMat, vkmat, m_affineMat, m_interpMode, ImPixel(0, 0, 0, 0), m_cropRect);
            vkmat.time_stamp = inMat.time_stamp;
            vkmat.rate = inMat.rate;
            vkmat.flags = inMat.flags;
            outMat = vkmat;
        }

        if (m_cropperX != 0 || m_cropperY != 0 || m_inWidth != m_outWidth || m_inHeight != m_outHeight)
        {
            ImGui::VkMat vkmat;
            vkmat.type = IM_DT_INT8;
            vkmat.w = m_outWidth;
            vkmat.h = m_outHeight;
            int srcX = m_cropperX>=0 ? m_cropperX : 0;
            int srcY = m_cropperY>=0 ? m_cropperY : 0;
            int srcW = srcX+m_outWidth>m_inWidth ? (int)m_inWidth-srcX : m_outWidth;
            int srcH = srcY+m_outHeight>m_inHeight ? (int)m_inHeight-srcY : m_outHeight;
            int dstX = m_cropperX>=0 ? 0 : -m_cropperX;
            int dstY = m_cropperY>=0 ? 0 : -m_cropperY;
            m_cropper.cropto(outMat, vkmat, srcX, srcY, srcW, srcH, dstX, dstY);
            vkmat.time_stamp = outMat.time_stamp;
            vkmat.rate = outMat.rate;
            vkmat.flags = outMat.flags;
            outMat = vkmat;
        }
        return true;
    }

    void VideoTransformFilter_VulkanImpl::UpdatePassThrough()
    {
        if (m_realScaleRatioH == 1 && m_realScaleRatioV == 1 &&
            m_cropL == 0 && m_cropT == 0 && m_cropR == 0 && m_cropB == 0 &&
            m_rotateAngle == 0 &&
            m_posOffsetH == 0 && m_posOffsetV == 0)
            m_passThrough = true;
        else
            m_passThrough = false;
    }
}
#endif
