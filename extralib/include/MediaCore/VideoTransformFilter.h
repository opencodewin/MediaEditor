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
#include <string>
#include <memory>
#include <vector>
#include <MatUtilsVecTypeDef.h>
#include <ImNewCurve.h>
#include <imgui_curve.h>
#include <ImMaskCreator.h>
#include <imgui_json.h>
#include "SharedSettings.h"

namespace MediaCore
{
    enum AspectFitType
    {
        ASPECT_FIT_TYPE__FIT = 0,
        ASPECT_FIT_TYPE__CROP,
        ASPECT_FIT_TYPE__FILL,
        ASPECT_FIT_TYPE__STRETCH,
    };

    struct VideoClip;

    struct VideoTransformFilter
    {
        using Holder = std::shared_ptr<VideoTransformFilter>;
        static Holder CreateInstance();

        virtual bool Initialize(SharedSettings::Holder hSettings) = 0;
        virtual Holder Clone(SharedSettings::Holder hSettings) = 0;

        virtual const std::string GetFilterName() const = 0;
        virtual uint32_t GetInWidth() const = 0;
        virtual uint32_t GetInHeight() const = 0;
        virtual uint32_t GetOutWidth() const = 0;
        virtual uint32_t GetOutHeight() const = 0;
        virtual MatUtils::Vec2<uint32_t>GetOutSize() const = 0;

        virtual bool SetOutputFormat(const std::string& outputFormat) = 0;
        virtual std::string GetOutputFormat() const = 0;
        virtual bool SetAspectFitType(AspectFitType type) = 0;
        virtual AspectFitType GetAspectFitType() const = 0;

        virtual VideoFrame::Holder FilterImage(VideoFrame::Holder hVfrm, int64_t pos) = 0;

        virtual void ApplyTo(VideoClip* pVClip) = 0;
        virtual void UpdateClipRange() = 0;

        // return the coordinates of four corner points as array { TopLeft, TopRight, BottomRight, BottomLeft },
        // the coordinates use the canvas center as the origin
        virtual bool CalcCornerPoints(int64_t i64Tick, ImVec2 aCornerPoints[4]) const = 0;

        // Position
        virtual bool SetPosOffset(int32_t i32PosOffX, int32_t i32PosOffY) = 0;
        virtual bool SetPosOffsetX(int32_t i32PosOffX) = 0;
        virtual int32_t GetPosOffsetX() const = 0;
        virtual bool SetPosOffsetY(int32_t i32PosOffY) = 0;
        virtual int32_t GetPosOffsetY() const = 0;
        virtual MatUtils::Vec2<int32_t> GetPosOffset() const = 0; 
        virtual MatUtils::Vec2<int32_t> GetPosOffset(int64_t i64Tick) const = 0; 
        virtual bool SetPosOffsetRatio(float fPosOffRatioX, float fPosOffRatioY) = 0;
        virtual bool SetPosOffsetRatioX(float fPosOffRatioX) = 0;
        virtual float GetPosOffsetRatioX() const = 0;
        virtual bool SetPosOffsetRatioY(float fPosOffRatioY) = 0;
        virtual float GetPosOffsetRatioY() const = 0;
        virtual MatUtils::Vec2<float> GetPosOffsetRatio() const = 0; 
        virtual MatUtils::Vec2<float> GetPosOffsetRatio(int64_t i64Tick) const = 0; 
        virtual bool SetPosOffsetRatio(int64_t i64Tick, float fPosOffRatioX, float fPosOffRatioY) = 0;
        virtual bool SetPosOffsetRatioX(int64_t i64Tick, float fPosOffRatioX) = 0;
        virtual float GetPosOffsetRatioX(int64_t i64Tick) const = 0;
        virtual bool SetPosOffsetRatioY(int64_t i64Tick, float fPosOffRatioY) = 0;
        virtual float GetPosOffsetRatioY(int64_t i64Tick) const = 0;
        virtual bool ChangePosOffset(int64_t i64Tick, int32_t i32DeltaX, int32_t i32DeltaY, bool* pParamUpdated = nullptr) = 0;
        virtual void EnableKeyFramesOnPosOffset(bool bEnable) = 0;
        virtual bool IsKeyFramesEnabledOnPosOffset() const = 0;
        virtual ImGui::ImNewCurve::Curve::Holder GetKeyFramesCurveOnPosOffset() const = 0;
        // Crop
        virtual bool SetCrop(uint32_t u32CropL, uint32_t u32CropT, uint32_t u32CropR, uint32_t u32CropB) = 0;
        virtual bool SetCropL(uint32_t u32CropL) = 0;
        virtual uint32_t GetCropL() const = 0;
        virtual bool SetCropT(uint32_t u32CropT) = 0;
        virtual uint32_t GetCropT() const = 0;
        virtual bool SetCropR(uint32_t u32CropR) = 0;
        virtual uint32_t GetCropR() const = 0;
        virtual bool SetCropB(uint32_t u32CropB) = 0;
        virtual uint32_t GetCropB() const = 0;
        virtual bool SetCropRatio(float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual bool SetCropRatioL(float fCropRatioL, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioL() const = 0;
        virtual bool SetCropRatioT(float fCropRatioT, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioT() const = 0;
        virtual bool SetCropRatioR(float fCropRatioR, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioR() const = 0;
        virtual bool SetCropRatioB(float fCropRatioB, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioB() const = 0;
        virtual bool SetCropRatio(int64_t i64Tick, float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual bool SetCropRatioL(int64_t i64Tick, float fCropRatioL, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioL(int64_t i64Tick) const = 0;
        virtual bool SetCropRatioT(int64_t i64Tick, float fCropRatioT, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioT(int64_t i64Tick) const = 0;
        virtual bool SetCropRatioR(int64_t i64Tick, float fCropRatioR, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioR(int64_t i64Tick) const = 0;
        virtual bool SetCropRatioB(int64_t i64Tick, float fCropRatioB, bool bClipValue = false, bool* pParamUpdated = nullptr) = 0;
        virtual float GetCropRatioB(int64_t i64Tick) const = 0;
        virtual bool ChangeCropL(int64_t i64Tick, int32_t i32Delta) = 0;
        virtual bool ChangeCropT(int64_t i64Tick, int32_t i32Delta) = 0;
        virtual bool ChangeCropR(int64_t i64Tick, int32_t i32Delta) = 0;
        virtual bool ChangeCropB(int64_t i64Tick, int32_t i32Delta) = 0;
        virtual void EnableKeyFramesOnCrop(bool bEnable) = 0;
        virtual bool IsKeyFramesEnabledOnCrop() const = 0;
        virtual std::vector<ImGui::ImNewCurve::Curve::Holder> GetKeyFramesCurveOnCrop() const = 0;
        // Scale
        virtual bool SetScale(float fScaleX, float fScaleY) = 0;
        virtual bool SetScaleX(float fScaleX) = 0;
        virtual float GetScaleX() const = 0;
        virtual bool SetScaleY(float fScaleY) = 0;
        virtual float GetScaleY() const = 0;
        virtual bool SetScale(int64_t i64Tick, float fScaleX, float fScaleY) = 0;
        virtual bool SetScaleX(int64_t i64Tick, float fScaleX) = 0;
        virtual float GetScaleX(int64_t i64Tick) const = 0;
        virtual bool SetScaleY(int64_t i64Tick, float fScaleY) = 0;
        virtual float GetScaleY(int64_t i64Tick) const = 0;
        virtual MatUtils::Vec2<float> GetScale() const = 0;
        virtual MatUtils::Vec2<float> GetScale(int64_t i64Tick) const = 0;
        virtual MatUtils::Vec2<float> GetFinalScale(int64_t i64Tick) const = 0;
        virtual bool ChangeScaleToFitOutputSize(int64_t i64Tick, uint32_t u32OutWidth, uint32_t u32OutHeight, bool* pParamUpdated = nullptr) = 0;
        virtual void SetKeepAspectRatio(bool bEnable) = 0;
        virtual bool IsKeepAspectRatio() const = 0;
        virtual void EnableKeyFramesOnScale(bool bEnable) = 0;
        virtual bool IsKeyFramesEnabledOnScale() const = 0;
        virtual ImGui::ImNewCurve::Curve::Holder GetKeyFramesCurveOnScale() const = 0;
        // Rotation
        virtual bool SetRotation(float fAngle) = 0;
        virtual float GetRotation() const = 0;
        virtual bool SetRotation(int64_t i64Tick, float fAngle, bool* pParamUpdated = nullptr) = 0;
        virtual float GetRotation(int64_t i64Tick) const = 0;
        virtual void EnableKeyFramesOnRotation(bool bEnable) = 0;
        virtual bool IsKeyFramesEnabledOnRotation() const = 0;
        virtual ImGui::ImNewCurve::Curve::Holder GetKeyFramesCurveOnRotation() const = 0;
        // Opacity
        virtual bool SetOpacity(float fOpacity) = 0;
        virtual float GetOpacity() const = 0;
        virtual bool SetOpacity(int64_t i64Tick, float fOpacity) = 0;
        virtual float GetOpacity(int64_t i64Tick) const = 0;
        virtual void EnableKeyFramesOnOpacity(bool bEnable) = 0;
        virtual bool IsKeyFramesEnabledOnOpacity() const = 0;
        virtual ImGui::ImNewCurve::Curve::Holder GetKeyFramesCurveOnOpacity() const = 0;
        virtual ImGui::MaskCreator::Holder CreateNewOpacityMask(const std::string& name) = 0;
        virtual int GetOpacityMaskCount() const = 0;
        virtual const ImGui::MaskCreator::Holder GetOpacityMaskCreator(size_t index) const = 0;
        virtual bool RemoveOpacityMask(size_t index) = 0;

        virtual void SetUiStateJson(const imgui_json::value& j) = 0;
        virtual imgui_json::value GetUiStateJson() const = 0;
        virtual imgui_json::value SaveAsJson() const = 0;
        virtual bool LoadFromJson(const imgui_json::value& j) = 0;
        virtual std::string GetError() const = 0;
    };
}