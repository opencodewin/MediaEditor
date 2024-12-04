#pragma once
#include <vector>
#include <list>
#include "immat.h"
#include "imgui.h"
#include "MatUtilsVecTypeDef.h"

namespace MatUtils
{
ImGui::ImMat MakeColor(ImDataType eDtype, double dColorVal);

ImGui::ImMat Contour2Mask(
        const Size2i& szMaskSize, const std::vector<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        ImDataType dtMaskDataType, double dMaskValue, double dNonMaskValue, int iLineType, bool bFilled = true, int iFeatherKsize = 0);
ImGui::ImMat Contour2Mask(
        const Size2i& szMaskSize, const std::list<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        ImDataType dtMaskDataType, double dMaskValue, double dNonMaskValue, int iLineType, bool bFilled = true, int iFeatherKsize = 0);

void DrawMask(
        ImGui::ImMat& mMask, const std::vector<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        double dMaskValue, int iLineType);
void DrawMask(
        ImGui::ImMat& mMask, const std::list<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        double dMaskValue, int iLineType);

void DrawPolygon(ImGui::ImMat& img, const std::vector<Point2l>& aPolygonVertices, int iFixPointShift, const ImGui::ImMat& color, int iLineType);

template<template<class T, class Alloc = std::allocator<T>> class Container>
std::vector<Point2l> ConvertFixPointPolygonVertices(const Container<Point2f>& aPolygonVertices, int iFixPointShift)
{
    const double dFixPointScale = (double)(1LL << iFixPointShift);
    std::vector<Point2l> aPolygonVertices_;
    aPolygonVertices_.reserve(aPolygonVertices.size());
    auto iter = aPolygonVertices.begin();
    while (iter != aPolygonVertices.end())
    {
        const auto& v = *iter++;
        aPolygonVertices_.push_back({(int64_t)((double)v.x*dFixPointScale), (int64_t)((double)v.y*dFixPointScale)});
    }
    return std::move(aPolygonVertices_);
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
void DrawPolygon(ImGui::ImMat& img, const Container<Point2f>& aPolygonVertices, const ImGui::ImMat& color, int iLineType, int iFixPointShift = 8)
{
    if (aPolygonVertices.empty())
        return;
    const auto aPolygonVertices_ = ConvertFixPointPolygonVertices(aPolygonVertices, iFixPointShift);
    DrawPolygon(img, aPolygonVertices_, iFixPointShift, color, iLineType);
}

bool CheckTwoLinesCross(const Point2f v[4], Point2f* pCross);
bool CheckTwoLinesCross(const ImVec2 v[4], ImVec2* pCross);
bool CheckPointOnLine(const ImVec2& po, const ImVec2 v[2]);
};