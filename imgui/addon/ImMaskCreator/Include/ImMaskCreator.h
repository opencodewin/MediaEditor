#pragma once
#include <memory>
#include <string>
#include <imgui.h>
#include <immat.h>
#include <imgui_json.h>
#include "MatUtilsVecTypeDef.h"
#include "Logger.h"

namespace ImGui
{
struct RoutePoint
{
    virtual MatUtils::Point2f GetPos(int64_t t = 0) const = 0;
    virtual MatUtils::Point2f GetBezierGrabberOffset(int idx, int64_t t = 0) const = 0;
    virtual int GetHoverType() const = 0;
    virtual bool IsSelected() const = 0;
};

struct MaskCreator
{
    enum LineType
    {
        CONNECT8 = 0,
        CONNECT4,
        AA,
        CAPSULE_SDF,
    };

    using Holder = std::shared_ptr<MaskCreator>;
    IMGUI_API static Holder CreateInstance(const MatUtils::Size2i& size, const std::string& name = "");
    IMGUI_API static void GetVersion(int& major, int& minor, int& patch, int& build);

    virtual std::string GetName() const = 0;
    virtual void SetName(const std::string& name) = 0;
    virtual bool DrawContent(const ImVec2& v2Pos, const ImVec2& v2ViewSize, bool bEditable = true, int64_t i64Tick = 0) = 0;
    virtual bool DrawContourPointKeyFrames(int64_t& i64Tick, const RoutePoint* ptContourPoint = nullptr, uint32_t u32Width = 0) = 0;
    virtual void SetMaskWarpAffineMatrix(float aWarpAffineMatrix[2][3], float aRevWarpAffineMatrix[2][3]) = 0;
    virtual void SetMaskWarpAffineParameters(const ImVec2& v2Offset, const ImVec2& v2Scale, float fRotationAngle, const ImVec2& v2Anchor = ImVec2(0,0)) = 0;
    virtual void SetUiWarpAffineMatrix(float aWarpAffineMatrix[2][3], float aRevWarpAffineMatrix[2][3]) = 0;
    virtual void SetUiWarpAffineParameters(const ImVec2& v2Offset, const ImVec2& v2Scale, float fRotationAngle, const ImVec2& v2Anchor = ImVec2(0,0)) = 0;
    virtual bool ChangeMaskSize(const MatUtils::Size2i& size, bool bScaleMask = false) = 0;
    virtual MatUtils::Size2i GetMaskSize() const = 0;
    virtual ImGui::ImMat GetMask(int iLineType, bool bFilled = true, ImDataType eDataType = IM_DT_INT8, double dMaskValue = 255, double dNonMaskValue = 0, int64_t i64Tick = 0) = 0;
    virtual const RoutePoint* GetHoveredPoint() const = 0;
    virtual const RoutePoint* GetSelectedPoint() const = 0;
    virtual ImVec4 GetContourContainBox() const = 0;
    virtual ImVec2 GetUiScale() const = 0;
    virtual bool IsKeyFrameEnabled() const = 0;
    virtual void EnableKeyFrames(bool bEnable) = 0;
    virtual bool SetTickRange(int64_t i64Start, int64_t i64End) = 0;
    virtual bool IsMaskReady() const = 0;

    virtual bool SaveAsJson(imgui_json::value& j) const = 0;
    virtual bool SaveAsJson(const std::string& filePath) const = 0;
    IMGUI_API static Holder LoadFromJson(const imgui_json::value& j);
    IMGUI_API static Holder LoadFromJson(const std::string& filePath);

    virtual std::string GetError() const = 0;
    virtual void SetLoggerLevel(Logger::Level l) = 0;
};
}