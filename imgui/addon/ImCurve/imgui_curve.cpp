#include "imgui_curve.h"

// CurveEdit from https://github.com/CedricGuillemet/ImGuizmo
template <typename T>
static T __tween_bounceout(const T& p) { return (p) < 4 / 11.0 ? (121 * (p) * (p)) / 16.0 : (p) < 8 / 11.0 ? (363 / 40.0 * (p) * (p)) - (99 / 10.0 * (p)) + 17 / 5.0 : (p) < 9 / 10.0 ? (4356 / 361.0 * (p) * (p)) - (35442 / 1805.0 * (p)) + 16061 / 1805.0 : (54 / 5.0 * (p) * (p)) - (513 / 25.0 * (p)) + 268 / 25.0; }
const char * curveTypeName[] = {
        "Hold", "Step", "Linear", "Smooth",
        "QuadIn", "QuadOut", "QuadInOut", 
        "CubicIn", "CubicOut", "CubicInOut", 
        "SineIn", "SineOut", "SineInOut",
        "ExpIn", "ExpOut", "ExpInOut",
        "CircIn", "CircOut", "CircInOut",
        "ElasticIn", "ElasticOut", "ElasticInOut",
        "BackIn", "BackOut", "BackInOut",
        "BounceIn", "BounceOut", "BounceInOut"};

namespace ImGui
{

const ImVec4 ImCurveEdit::ZERO_POINT = ImVec4(0,0,0,0);

int ImCurveEdit::GetCurveTypeName(char**& list)
{
    list = (char **)curveTypeName;
    return IM_ARRAYSIZE(curveTypeName);
}

float ImCurveEdit::smoothstep(float edge0, float edge1, float t, CurveType type)
{
    const double s = 1.70158f;
    const double s2 = 1.70158f * 1.525f;
    t = ImClamp((t - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    auto t2 = t - 1;
    switch (type)
    {
        case Hold:
            return (0);
        case Step:
            return (t > .5);
        case Linear:
            return t;
        case Smooth:
            return t * t * (3 - 2 * t);
        case QuadIn:
            return t * t;
        case QuadOut:
            return -(t * (t - 2));
        case QuadInOut:
            return (t < 0.5) ? (2 * t * t) : ((-2 * t * t) + (4 * t) - 1);
        case CubicIn:
            return t * t * t;
        case CubicOut:
            return (t - 1) * (t - 1) * (t - 1) + 1;
        case CubicInOut:
            return (t < 0.5) ? (4 * t * t * t) : (.5 * ((2 * t) - 2) * ((2 * t) - 2) * ((2 * t) - 2) + 1);
        case SineIn:
            return sin((t - 1) * M_PI_2) + 1;
        case SineOut:
            return sin(t * M_PI_2);
        case SineInOut:
            return 0.5 * (1 - cos(t * M_PI));
        case ExpIn:
            return (t == 0.0) ? t : pow(2, 10 * (t - 1));
        case ExpOut:
            return (t == 1.0) ? t : 1 - pow(2, -10 * t);
        case ExpInOut:
            if (t == 0.0 || t == 1.0) return t;
            if (t < 0.5) { return 0.5 * pow(2, (20 * t) - 10); }
            else { return -0.5 * pow(2, (-20 * t) + 10) + 1; }
        case CircIn:
            return 1 - sqrt(1 - (t * t));
        case CircOut:
            return sqrt((2 - t) * t);
        case CircInOut:
            if (t < 0.5) { return 0.5 * (1 - sqrt(1 - 4 * (t * t)));}
            else { return 0.5 * (sqrt(-((2 * t) - 3) * ((2 * t) - 1)) + 1); }
        case ElasticIn:
            return sin(13 * M_PI_2 * t) * pow(2, 10 * (t - 1));
        case ElasticOut:
            return sin(-13 * M_PI_2 * (t + 1)) * pow(2, -10 * t) + 1;
        case ElasticInOut:
            if (t < 0.5) { return 0.5 * sin(13 * M_PI_2 * (2 * t)) * pow(2, 10 * ((2 * t) - 1)); }
            else { return 0.5 * (sin(-13 * M_PI_2 * ((2 * t - 1) + 1)) * pow(2, -10 * (2 * t - 1)) + 2); }
        case BackIn:
            return t * t * ((s + 1) * t - s);
        case BackOut:
            return 1.f * (t2 * t2 * ((s + 1) * t2 + s) + 1);
        case BackInOut:
            if (t < 0.5) { auto p2 = t * 2; return 0.5 * p2 * p2 * (p2 * s2 + p2 - s2);}
            else { auto p = t * 2 - 2; return 0.5 * (2 + p * p * (p * s2 + p + s2)); }
        case BounceIn:
            return 1 - __tween_bounceout(1 - t);
        case BounceOut:
            return __tween_bounceout(t);
        case BounceInOut:
            if (t < 0.5) { return 0.5 * (1 - __tween_bounceout(1 - t * 2)); }
            else { return 0.5 * __tween_bounceout((t * 2 - 1)) + 0.5; }
        default:
            return t;
    }
}

float ImCurveEdit::distance(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

float ImCurveEdit::distance(float x, float y, float x1, float y1, float x2, float y2)
{
    float A = x - x1;
    float B = y - y1;
    float C = x2 - x1;
    float D = y2 - y1;
    float dot = A * C + B * D;
    float len_sq = C * C + D * D;
    float param = -1.f;
    if (len_sq > FLT_EPSILON)
        param = dot / len_sq;
    float xx, yy;
    if (param < 0.f) {
        xx = x1;
        yy = y1;
    }
    else if (param > 1.f) {
        xx = x2;
        yy = y2;
    }
    else {
       xx = x1 + param * C;
       yy = y1 + param * D;
    }
    float dx = x - xx;
    float dy = y - yy;
    return sqrtf(dx * dx + dy * dy);
}

int ImCurveEdit::DrawPoint(ImDrawList* draw_list, ImVec2 pos, const ImVec2 size, const ImVec2 offset, bool edited)
{
    int ret = 0;
    ImGuiIO& io = GetIO();
    static const ImVec2 localOffsets[4] = { ImVec2(1,0), ImVec2(0,1), ImVec2(-1,0), ImVec2(0,-1) };
    ImVec2 offsets[4];
    for (int i = 0; i < 4; i++)
    {
       offsets[i] = pos * size + localOffsets[i] * 4.5f + offset;
    }
    const ImVec2 center = pos * size + offset;
    const ImRect anchor(center - ImVec2(5, 5), center + ImVec2(5, 5));
    draw_list->AddConvexPolyFilled(offsets, 4, 0xFF000000);
    if (anchor.Contains(io.MousePos))
    {
        ret = 1;
        if (IsMouseDown(ImGuiMouseButton_Left))
            ret = 2;
    }
    if (edited)
        draw_list->AddPolyline(offsets, 4, 0xFFFFFFFF, true, 3.0f);
    else if (ret)
        draw_list->AddPolyline(offsets, 4, 0xFF80B0FF, true, 2.0f);
    else
        draw_list->AddPolyline(offsets, 4, 0xFF0080FF, true, 2.0f);
    return ret;
}

bool ImCurveEdit::Edit(
        ImDrawList* draw_list, Delegate* delegate, const ImVec2& size, unsigned int id, bool editable, float& cursor_pos, float firstTime, float lastTime, 
        unsigned int flags, const ImRect* clippingRect, bool* changed, ValueDimension eDim)
{
    bool hold = false;
    bool curve_changed = false;
    bool draw_timeline = flags & CURVE_EDIT_FLAG_DRAW_TIMELINE;
    const float timeline_height = 30.f;
    if (!delegate) return hold;
    ImGuiIO& io = GetIO();
    bool bEnableDelete = IsKeyDown(ImGuiKey_LeftShift) && (io.KeyMods == ImGuiMod_Shift);
    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    PushStyleColor(ImGuiCol_Border, 0);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    BeginChildFrame(id, size);
    delegate->focused = IsWindowFocused();
    ImVec2 window_pos = GetCursorScreenPos();
    if (!draw_list) draw_list = GetWindowDrawList();
    if (clippingRect)
        draw_list->PushClipRect(clippingRect->Min, clippingRect->Max, true);

    ImVec2 edit_size = size - ImVec2(0, 4) - (draw_timeline ? ImVec2(0, timeline_height) : ImVec2(0, 0));
    const ImVec2 offset = window_pos + ImVec2(0.f, edit_size.y + 2) + (draw_timeline ? ImVec2(0, timeline_height) : ImVec2(0, 0));
    const ImVec2 ssize(edit_size.x, -edit_size.y);
    const ImRect container(offset + ImVec2(0.f, ssize.y), offset + ImVec2(ssize.x, 0.f));
    auto vmin = delegate->GetMin();
    auto vmax = delegate->GetMax();
    ImVec2 _vmin = ImVec2(firstTime, GetDimVal(vmin, eDim));
    ImVec2 _vmax = ImVec2(lastTime, GetDimVal(vmax, eDim));
    ImVec2 _range = _vmax - _vmin;
    const ImVec2 viewSize(edit_size.x, -edit_size.y);
    const size_t curveCount = delegate->GetCurveCount();
    const ImVec2 sizeOfPixel = ImVec2(1.f, 1.f) / viewSize;
    bool point_selected = delegate->overSelectedPoint && IsMouseDown(ImGuiMouseButton_Left);
    bool curve_selected = (flags & CURVE_EDIT_FLAG_MOVE_CURVE) && (delegate->movingCurve != -1) && IsMouseDown(ImGuiMouseButton_Left);
    auto pointToRange = [&](ImVec2 pt) { return (pt - _vmin) / _range; };
    auto rangeToPoint = [&](ImVec2 pt) { return (pt * _range) + _vmin; };

    // draw timeline and mark
    if (draw_timeline)
    {
        float duration = _vmax.x - _vmin.x;
        float msPixelWidth = size.x / (duration + FLT_EPSILON);
        ImVec2 headerSize(size.x, (float)timeline_height);
        InvisibleButton("CurveTimelineBar", headerSize);
        draw_list->AddRectFilled(window_pos, window_pos + headerSize, IM_COL32( 33,  33,  38, 255), 0);
        ImRect movRect(window_pos, window_pos + headerSize);
        if (editable && !delegate->MovingCurrentTime && movRect.Contains(io.MousePos) && IsMouseDown(ImGuiMouseButton_Left) && !point_selected && !curve_selected)
        {
            delegate->MovingCurrentTime = true;
        }
        if (delegate->MovingCurrentTime && duration > 0)
        {
            auto oldPos = cursor_pos;
            auto newPos = (int64_t)((io.MousePos.x - movRect.Min.x) / msPixelWidth) + _vmin.x;
            if (newPos < _vmin.x)
                newPos = _vmin.x;
            if (newPos >= _vmax.x)
                newPos = _vmax.x;
            cursor_pos = newPos;
        }
        if (!IsMouseDown(ImGuiMouseButton_Left))
        {
            delegate->MovingCurrentTime = false;
        }
        
        int64_t modTimeCount = 10;
        int timeStep = 1;
        while ((modTimeCount * msPixelWidth) < 100)
        {
            modTimeCount *= 10;
            timeStep *= 10;
        };
        int halfModTime = modTimeCount / 2;
        auto drawLine = [&](int64_t i, int regionHeight)
        {
            bool baseIndex = ((i % modTimeCount) == 0) || (i == 0 || i == duration);
            bool halfIndex = (i % halfModTime) == 0;
            int px = (int)window_pos.x + int(i * msPixelWidth);
            int timeStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
            int timeEnd = baseIndex ? regionHeight : timeline_height;
            if (px <= (size.x + window_pos.x) && px >= window_pos.x)
            {
                draw_list->AddLine(ImVec2((float)px, window_pos.y + (float)timeStart), ImVec2((float)px, window_pos.y + (float)timeEnd - 1), halfIndex ? IM_COL32(255, 255, 255, 255) : IM_COL32(128, 128, 128, 255), halfIndex ? 2 : 1);
            }
            if (baseIndex && px >= window_pos.x)
            {
                std::string time_str = ImGuiHelper::MillisecToString(i + _vmin.x, 2);
                SetWindowFontScale(0.8);
                draw_list->AddText(ImVec2((float)px + 3.f, window_pos.y), IM_COL32(224, 224, 224, 255), time_str.c_str());
                SetWindowFontScale(1.0);
            }
        };
        for (auto i = 0; i < duration; i+= timeStep)
        {
            drawLine(i, timeline_height);
        }
        drawLine(0, timeline_height);
        drawLine(duration, timeline_height);
        // cursor Arrow
        draw_list->PushClipRect(movRect.Min - ImVec2(32, 0), movRect.Max + ImVec2(32, 0));
        const float arrowWidth = draw_list->_Data->FontSize;
        float arrowOffset = window_pos.x + (cursor_pos - vmin.x) * msPixelWidth - arrowWidth * 0.5f;
        RenderArrow(draw_list, ImVec2(arrowOffset, window_pos.y), IM_COL32(255, 255, 255, 192), ImGuiDir_Down);
        SetWindowFontScale(0.8);
        auto time_str = ImGuiHelper::MillisecToString(cursor_pos, 2);
        ImVec2 str_size = CalcTextSize(time_str.c_str(), nullptr, true);
        float strOffset = window_pos.x + (cursor_pos - _vmin.x) * msPixelWidth - str_size.x * 0.5f;
        ImVec2 str_pos = ImVec2(strOffset, window_pos.y + 10);
        draw_list->AddRectFilled(str_pos + ImVec2(-3, 0), str_pos + str_size + ImVec2(3, 3), IM_COL32(128, 128, 128, 144), 2.0, ImDrawFlags_RoundCornersAll);
        draw_list->AddText(str_pos, IM_COL32(255, 255, 255, 255), time_str.c_str());
        SetWindowFontScale(1.0);
        draw_list->PopClipRect();
        // draw cursor line
        if (cursor_pos >= _vmin.x && cursor_pos <= _vmax.x)
        {
            auto pt = pointToRange(ImVec2(cursor_pos, 0)) * viewSize + offset;
            draw_list->AddLine(pt - ImVec2(0.5, 0), pt - ImVec2(0.5, edit_size.y), IM_COL32(255, 255, 255, 128), 2);
        }
    }

    // handle zoom and VScroll
    if (flags & CURVE_EDIT_FLAG_SCROLL_V)
    {
        if (editable && container.Contains(io.MousePos))
        {
            if (fabsf(io.MouseWheel) > FLT_EPSILON)
            {
                const float r = (io.MousePos.y - offset.y) / ssize.y;
                float ratioY = ImLerp(vmin.y, vmax.y, r);
                auto scaleValue = [&](float v) {
                    v -= ratioY;
                    v *= (1.f - io.MouseWheel * 0.05f);
                    v += ratioY;
                    return v;
                };
                SetDimVal(vmin, scaleValue(_vmin.y), eDim);
                SetDimVal(vmax, scaleValue(_vmax.y), eDim);
                delegate->SetMin(vmin);
                delegate->SetMax(vmax);
                curve_changed = true;
            }
            if (!delegate->scrollingV && IsMouseDown(2))
            {
                delegate->scrollingV = true;
            }
        }
    }

    if ((flags & CURVE_EDIT_FLAG_SCROLL_V) && delegate->scrollingV && editable)
    {
        float deltaH = io.MouseDelta.y * _range.y * sizeOfPixel.y;
        SetDimVal(vmin, _vmin.y-deltaH, eDim);
        SetDimVal(vmax, _vmax.y-deltaH, eDim);
        delegate->SetMin(vmin);
        delegate->SetMax(vmax);
        if (!IsMouseDown(2))
            delegate->scrollingV = false;
        curve_changed = true;
    }

    // draw graticule line
    const float graticule_height = edit_size.y / 10.f;
    for (int i = 0; i <= 10; i++)
    {
        draw_list->AddLine(offset + ImVec2(0, - graticule_height * i), offset + ImVec2(edit_size.x, - graticule_height * i), delegate->GetGraticuleColor(), 1.0f);
    }

    bool overCurveOrPoint = false;
    int localOverCurve = -1;
    int localOverPoint = -1;
    // make sure highlighted curve is rendered last
    std::vector<int> curvesIndex(curveCount);
    for (size_t c = 0; c < curveCount; c++)
        curvesIndex[c] = int(c);
    int highLightedCurveIndex = -1;
    if (delegate->overCurve != -1 && curveCount)
    {
        ImSwap(curvesIndex[delegate->overCurve], curvesIndex[curveCount - 1]);
        highLightedCurveIndex = delegate->overCurve;
    }

    for (size_t cur = 0; cur < curveCount; cur++)
    {
        int c = curvesIndex[cur];
        if (!delegate->IsVisible(c))
            continue;
        const size_t ptCount = delegate->GetCurvePointCount(c);
        if (ptCount < 1)
            continue;
        const KeyPoint* pts = delegate->GetPoints(c);
        ImU32 curveColor = delegate->GetCurveColor(c);
        float curve_width = 1.3f;
        if ((c == highLightedCurveIndex && delegate->selectedPoints.empty() && !delegate->selectingQuad) || delegate->movingCurve == c)
            curve_width = 2.6f;
        for (size_t p = 0; p < ptCount - 1; p++)
        {
            const auto p1 = pointToRange(pts[p].GetVec2PointByDim(eDim));
            const auto p2 = pointToRange(pts[p + 1].GetVec2PointByDim(eDim));
            CurveType ctype = delegate->GetCurvePointType(c, p + 1);
            size_t subStepCount = distance(p1.x, p1.y, p2.x, p2.y);
            if (subStepCount <= 1) subStepCount = 100;
            subStepCount = ImClamp(subStepCount, (size_t)10, (size_t)100);
            float step = 1.f / float(subStepCount - 1);
            for (size_t substep = 0; substep < subStepCount - 1; substep++)
            {
                float t = float(substep) * step;
                const ImVec2 sp1 = ImLerp(p1, p2, t);
                const ImVec2 sp2 = ImLerp(p1, p2, t + step);

                const float rt1 = smoothstep(p1.x, p2.x, sp1.x, ctype);
                const float rt2 = smoothstep(p1.x, p2.x, sp2.x, ctype);

                const ImVec2 pos1 = ImVec2(sp1.x, ImLerp(p1.y, p2.y, rt1)) * viewSize + offset;
                const ImVec2 pos2 = ImVec2(sp2.x, ImLerp(p1.y, p2.y, rt2)) * viewSize + offset;

                if (distance(io.MousePos.x, io.MousePos.y, pos1.x, pos1.y, pos2.x, pos2.y) < 8.f)
                {
                    localOverCurve = int(c);
                    delegate->overCurve = int(c);
                }
                draw_list->AddLine(pos1, pos2, curveColor, curve_width);
            } // substep
        }// point loop

        for (size_t p = 0; p < ptCount; p++)
        {
            SelPoint point(c, p);
            const auto p_ = pointToRange(pts[p].GetVec2PointByDim(eDim));
            const bool edited = delegate->selectedPoints.find(point) != delegate->selectedPoints.end() && delegate->movingCurve == -1/* && !scrollingV*/;
            const int drawState = DrawPoint(draw_list, p_, viewSize, offset, edited);
            if (editable && drawState && delegate->movingCurve == -1 && !delegate->selectingQuad)
            {
                overCurveOrPoint = true;
                delegate->overSelectedPoint = true;
                delegate->overCurve = -1;
                if (drawState == 2)
                {
                    if (!io.KeyShift && delegate->selectedPoints.find(point) == delegate->selectedPoints.end())
                        delegate->selectedPoints.clear();
                    delegate->selectedPoints.insert(point);
                }
                localOverPoint = (int)p;
            }
        }
    }  // curves loop

    if (localOverCurve == -1)
        delegate->overCurve = -1;

    // move selection
    if (editable && delegate->overSelectedPoint && IsMouseDown(ImGuiMouseButton_Left))
    {
        if ((fabsf(io.MouseDelta.x) > 0.f || fabsf(io.MouseDelta.y) > 0.f) && !delegate->selectedPoints.empty())
        {
            if (!delegate->pointsMoved)
            {
                delegate->BeginEdit(0);
                delegate->mousePosOrigin = io.MousePos;
                delegate->originalPoints.resize(delegate->selectedPoints.size());
                int index = 0;
                for (auto& sel : delegate->selectedPoints)
                {
                    const KeyPoint* pts = delegate->GetPoints(sel.curveIndex);
                    delegate->originalPoints[index++] = pts[sel.pointIndex];
                }
            }
            delegate->pointsMoved = true;
            hold = true;
            auto prevSelection = delegate->selectedPoints;
            int originalIndex = 0;
            for (auto& sel : prevSelection)
            {
                const size_t ptCount = delegate->GetCurvePointCount(sel.curveIndex);
                ImVec2 p = rangeToPoint(pointToRange(delegate->originalPoints[originalIndex].GetVec2PointByDim(eDim)) + (io.MousePos - delegate->mousePosOrigin) * sizeOfPixel);
                p = delegate->AlignValueByDim(p, eDim);
                const ImVec2 dimMin(vmin.w, GetDimVal(vmin, eDim));
                const ImVec2 dimMax(vmax.w, GetDimVal(vmax, eDim));
                if (flags & CURVE_EDIT_FLAG_VALUE_LIMITED)
                {
                    if (p.x < dimMin.x) p.x = dimMin.x;
                    if (p.y < dimMin.y) p.y = dimMin.y;
                    if (p.x > dimMax.x) p.x = dimMax.x;
                    if (p.y > dimMax.y) p.y = dimMax.y;
                }
                if (flags & CURVE_EDIT_FLAG_DOCK_BEGIN_END)
                {
                    if (sel.pointIndex == 0)
                    {
                        p.x = dimMin.x;
                    }
                    else if (sel.pointIndex == ptCount - 1)
                    {
                        p.x = dimMax.x;
                    }
                }
                const CurveType ctype = delegate->originalPoints[originalIndex].type;
                auto value_range = fabs(delegate->GetCurveValueRangeByDim(sel.curveIndex, eDim)); 
                p.y = (p.y * value_range) + GetDimVal(delegate->GetCurveMin(sel.curveIndex), eDim);
                const int newIndex = delegate->EditPointByDim(sel.curveIndex, sel.pointIndex, p, ctype, eDim);
                if (localOverPoint == -1 && BeginTooltip())
                {
                    Text("%.2f", p.y);
                    EndTooltip();
                }
                if (newIndex != sel.pointIndex)
                {
                    delegate->selectedPoints.erase(sel);
                    delegate->selectedPoints.insert({ sel.curveIndex, newIndex });
                }
                originalIndex++;
            }
            curve_changed = true;
        }
    }

    if (delegate->overSelectedPoint && !IsMouseDown(ImGuiMouseButton_Left))
    {
        delegate->overSelectedPoint = false;
        if (delegate->pointsMoved)
        {
            delegate->pointsMoved = false;
            delegate->EndEdit();
        }
    }

    // add point with double left click 
    if (editable && delegate->overCurve != -1 && io.MouseDoubleClicked[0])
    {
        ImVec2 np = rangeToPoint((io.MousePos - offset) / viewSize);
        np = delegate->AlignValueByDim(np, eDim);
        const CurveType ctype = delegate->GetCurveType(delegate->overCurve);
        auto value_range = delegate->GetCurveValueRangeByDim(delegate->overCurve, eDim);
        auto point_value = delegate->GetValue(delegate->overCurve, np.x);
        delegate->BeginEdit(delegate->overCurve);
        ImVec4 newPointVal(delegate->GetCurveDefault(delegate->overCurve));
        SetDimVal(newPointVal, np.y, eDim);
        newPointVal.w = np.x;
        delegate->AddPoint(delegate->overCurve, newPointVal, ctype, false);
        delegate->EndEdit();
        curve_changed = true;
    }

    // draw value in tooltip
    if (editable && localOverCurve != -1 && localOverPoint != -1 && BeginTooltip())
    {
        auto value_range = delegate->GetCurveValueRangeByDim(localOverCurve, eDim); 
        const KeyPoint* pts = delegate->GetPoints(localOverCurve);
        const ImVec2 p = pointToRange(pts[localOverPoint].GetVec2PointByDim(eDim));
        Text("%.2f", p.y * value_range + delegate->GetCurveMinByDim(localOverCurve, eDim));
        if (localOverPoint != 0 && localOverPoint != delegate->GetCurvePointCount(localOverCurve) - 1)
            TextUnformatted("Delete key with L-shift and L-click");
        EndTooltip();
    }

    // delete point with right click
    if (editable && localOverCurve !=-1 && localOverPoint != -1 && IsMouseClicked(ImGuiMouseButton_Left) && editable && bEnableDelete)
    {
        bool deletable = true;
        if (flags & CURVE_EDIT_FLAG_KEEP_BEGIN_END)
        {
            const size_t ptCount = delegate->GetCurvePointCount(localOverCurve);
            if (localOverPoint == 0 || localOverPoint == ptCount - 1)
                deletable = false;
        }
        if (deletable)
        {
            delegate->BeginEdit(localOverCurve);
            delegate->DeletePoint(localOverCurve, localOverPoint);
            delegate->EndEdit();
            auto selected_point = std::find_if(delegate->selectedPoints.begin(), delegate->selectedPoints.end(), [&](const SelPoint& point) {
                return point.curveIndex == localOverCurve && point.pointIndex == localOverPoint;
            });
            if (selected_point != delegate->selectedPoints.end())
                delegate->selectedPoints.erase(selected_point);
            curve_changed = true;
        }
    }

    // move curve
    if (editable && (flags & CURVE_EDIT_FLAG_MOVE_CURVE))
    {
        if (delegate->movingCurve != -1)
        {
            auto value_range = delegate->GetCurveValueRangeByDim(delegate->movingCurve, eDim);
            const size_t ptCount = delegate->GetCurvePointCount(delegate->movingCurve);
            const KeyPoint* pts = delegate->GetPoints(delegate->movingCurve);
            if (!delegate->pointsMoved)
            {
                delegate->mousePosOrigin = io.MousePos;
                delegate->pointsMoved = true;
                delegate->originalPoints.resize(ptCount);
                for (size_t index = 0; index < ptCount; index++)
                {
                    delegate->originalPoints[index] = pts[index];
                }
            }
            if (ptCount >= 1)
            {
                for (size_t ptIdx = 0; ptIdx < ptCount; ptIdx++)
                {
                    ImVec2 pt = rangeToPoint(pointToRange(delegate->originalPoints[ptIdx].GetVec2PointByDim(eDim)) + (io.MousePos - delegate->mousePosOrigin) * sizeOfPixel);
                    const ImVec2 dimMin(vmin.w, GetDimVal(vmin, eDim));
                    const ImVec2 dimMax(vmax.w, GetDimVal(vmax, eDim));
                    if (flags & CURVE_EDIT_FLAG_VALUE_LIMITED)
                    {
                        if (pt.x < dimMin.x) pt.x = dimMin.x;
                        if (pt.y < dimMin.y) pt.y = dimMin.y;
                        if (pt.x > dimMax.x) pt.x = dimMax.x;
                        if (pt.y > dimMax.y) pt.y = dimMax.y;
                    }
                    if (flags & CURVE_EDIT_FLAG_DOCK_BEGIN_END)
                    {
                        if (ptIdx == 0)
                        {
                            pt.x = dimMin.x;
                        }
                        else if (ptIdx == ptCount - 1)
                        {
                            pt.x = dimMax.x;
                        }
                    }
                    pt.y = pt.y * value_range + delegate->GetCurveMinByDim(delegate->movingCurve, eDim);
                    delegate->EditPointByDim(delegate->movingCurve, int(ptIdx), pt, delegate->originalPoints[ptIdx].type, eDim);
                }
                hold = true;
                curve_changed = true;
            }
            if (!IsMouseDown(ImGuiMouseButton_Left))
            {
                delegate->movingCurve = -1;
                delegate->pointsMoved = false;
                delegate->EndEdit();
            }
        }
        if (delegate->movingCurve == -1 && delegate->overCurve != -1 && IsMouseClicked(ImGuiMouseButton_Left) && !delegate->selectingQuad)
        {
            delegate->movingCurve = delegate->overCurve;
            delegate->BeginEdit(delegate->overCurve);
        }
    }

    // quad selection
    if (editable && delegate->selectingQuad)
    {
        const ImVec2 bmin = ImMin(delegate->quadSelection, io.MousePos);
        const ImVec2 bmax = ImMax(delegate->quadSelection, io.MousePos);
        draw_list->AddRectFilled(bmin, bmax, IM_COL32(255, 255, 0, 64), 1.f);
        draw_list->AddRect(bmin, bmax, IM_COL32(255,255,0,255), 1.f);
        const ImRect selectionQuad(bmin, bmax);
        if (!IsMouseDown(ImGuiMouseButton_Left))
        {
            if (!io.KeyShift)
                delegate->selectedPoints.clear();
            // select everythnig is quad
            for (size_t c = 0; c < curveCount; c++)
            {
                if (!delegate->IsVisible(c))
                    continue;

                const size_t ptCount = delegate->GetCurvePointCount(c);
                if (ptCount < 1)
                    continue;

                const KeyPoint* pts = delegate->GetPoints(c);
                for (size_t ptIdx = 0; ptIdx < ptCount; ptIdx++)
                {
                    const ImVec2 center = pointToRange(pts[ptIdx].GetVec2PointByDim(eDim)) * viewSize + offset;
                    if (selectionQuad.Contains(center))
                        delegate->selectedPoints.insert({ int(c), int(ptIdx) });
                }
            }
            // done
            delegate->selectingQuad = false;
        }
    }
    if (editable && !overCurveOrPoint && IsMouseClicked(ImGuiMouseButton_Left) && !delegate->selectingQuad && delegate->movingCurve == -1 && !delegate->overSelectedPoint && container.Contains(io.MousePos))
    {
        delegate->selectingQuad = true;
        delegate->quadSelection = io.MousePos;
    }

    if (clippingRect)
        draw_list->PopClipRect();

    EndChildFrame();
    PopStyleColor(2);
    PopStyleVar();
    if (changed)
        *changed = curve_changed;
    return hold | delegate->selectingQuad;
}

bool ImCurveEdit::Edit(
        ImDrawList* draw_list, Delegate* delegate, const ImVec2& size, unsigned int id, bool editable, float& cursor_pos,
        unsigned int flags, const ImRect* clippingRect, bool* changed, ValueDimension eDim)
{
    float firstTime = 0;
    float lastTime = delegate ? delegate->GetMax().w : 0;
    return ImCurveEdit::Edit(draw_list, delegate, size, id, editable, cursor_pos, firstTime, lastTime, flags, clippingRect, changed, eDim);
}

float ImCurveEdit::GetDimVal(const ImVec4& v, ValueDimension eDim)
{
    if (eDim == DIM_X)
        return v.x;
    else if (eDim == DIM_Y)
        return v.y;
    else if (eDim == DIM_Z)
        return v.z;
    else
        return v.w;
}

void ImCurveEdit::SetDimVal(ImVec4& v, float f, ValueDimension eDim)
{
    if (eDim == DIM_X)
        v.x = f;
    else if (eDim == DIM_Y)
        v.y = f;
    else if (eDim == DIM_Z)
        v.z = f;
    else
        v.w = f;
}

KeyPointEditor& KeyPointEditor::operator=(const KeyPointEditor& kpEditor)
{
    mCurves.clear();
    for (const auto& curve : kpEditor.mCurves)
    {
        auto curve_index = AddCurve(curve.name, curve.type, curve.color, curve.visible, curve.m_min, curve.m_max, curve.m_default);
        for (const auto& p : curve.points)
            AddPoint(curve_index, p.val, p.type, false);
    }
    mMin = kpEditor.mMin;
    mMax = kpEditor.mMax;
    BackgroundColor = kpEditor.BackgroundColor;
    GraticuleColor = kpEditor.GraticuleColor;
    return *this;
}

void KeyPointEditor::Load(const imgui_json::value& keypoint)
{
    if (!keypoint.is_object())
        return;
    // keypoint global
    if (keypoint.contains("Min"))
    {
        auto& val = keypoint["Min"];
        if (val.is_vec4()) SetMin(val.get<imgui_json::vec4>());
    }
    if (keypoint.contains("Max"))
    {
        auto& val = keypoint["Max"];
        if (val.is_vec4()) SetMax(val.get<imgui_json::vec4>());
    }

    // Clear Curves
    Clear();

    // keypoint curve
    const imgui_json::array* curveArray = nullptr;
    if (imgui_json::GetPtrTo(keypoint, "Curves", curveArray))
    {
        for (auto& curve : *curveArray)
        {
            if (!curve.is_object()) continue;
            std::string name = "";
            int ctype = -1;
            ImU32 color = 0;
            bool visible = false;
            ImVec4 _min;
            ImVec4 _max;
            ImVec4 _default;
            int64_t _id = -1;
            int64_t _sub_id = -1;
            if (curve.contains("Name"))
            {
                auto& val = curve["Name"];
                if (val.is_string()) name = val.get<imgui_json::string>();
            }
            if (curve.contains("Type"))
            {
                auto& val = curve["Type"];
                if (val.is_number()) ctype = val.get<imgui_json::number>();
            }
            if (curve.contains("Color"))
            {
                auto& val = curve["Color"];
                if (val.is_number()) color = val.get<imgui_json::number>();
            }
            if (curve.contains("Visible"))
            {
                auto& val = curve["Visible"];
                if (val.is_boolean()) visible = val.get<imgui_json::boolean>();
            }
            if (curve.contains("Min"))
            {
                auto& val = curve["Min"];
                if (val.is_vec4()) _min = val.get<imgui_json::vec4>();
            }
            if (curve.contains("Max"))
            {
                auto& val = curve["Max"];
                if (val.is_vec4()) _max = val.get<imgui_json::vec4>();
            }
            if (curve.contains("Default"))
            {
                auto& val = curve["Default"];
                if (val.is_number()) _default = val.get<imgui_json::vec4>();
            }
            if (curve.contains("ID"))
            {
                auto& val = curve["ID"];
                if (val.is_number()) _id = val.get<imgui_json::number>();
            }
            if (curve.contains("SubID"))
            {
                auto& val = curve["SubID"];
                if (val.is_number()) _sub_id = val.get<imgui_json::number>();
            }
            if (!name.empty())
            {
                auto curve_index = AddCurve(name, (ImCurveEdit::CurveType)ctype, color, visible, _min, _max, _default, _id, _sub_id);
                const imgui_json::array* pointArray = nullptr;
                if (imgui_json::GetPtrTo(curve, "KeyPoints", pointArray))
                {
                    for (auto& point : *pointArray)
                    {
                        if (!point.is_object()) continue;
                        ImCurveEdit::KeyPoint p;
                        if (point.contains("Val"))
                        {
                            auto& val = point["Val"];
                            if (val.is_vec4()) p.val = val.get<imgui_json::vec4>();
                        }
                        if (point.contains("Type"))
                        {
                            auto& val = point["Type"];
                            if (val.is_number()) p.type = (ImCurveEdit::CurveType)val.get<imgui_json::number>();
                        }
                        if (p.type != ImCurveEdit::CurveType::UnKnown)
                        {
                            AddPoint(curve_index, p.val, p.type, false);
                        }
                    }
                }
            }
        }
    }
}

void KeyPointEditor::Save(imgui_json::value& keypoint)
{
    keypoint["Min"] = imgui_json::vec4(GetMin());
    keypoint["Max"] = imgui_json::vec4(GetMax());
    imgui_json::value curves;
    for (int i = 0; i < GetCurveCount(); i++)
    {
        imgui_json::value curve;
        curve["Name"] = GetCurveName(i);
        curve["Type"] = imgui_json::number(GetCurveType(i));
        curve["Color"] = imgui_json::number(GetCurveColor(i));
        curve["Visible"] = imgui_json::boolean(IsVisible(i));
        curve["Min"] = imgui_json::vec4(GetCurveMin(i));
        curve["Max"] = imgui_json::vec4(GetCurveMax(i));
        curve["Default"] = imgui_json::vec4(GetCurveDefault(i));
        curve["ID"] = imgui_json::number(GetCurveID(i));
        curve["SubID"] = imgui_json::number(GetCurveSubID(i));
        // save curve key point
        imgui_json::value points;
        for (int p = 0; p < GetCurvePointCount(i); p++)
        {
            imgui_json::value point;
            auto pt = mCurves[i].points[p];
            point["Val"] = imgui_json::vec4(pt.val);
            point["Type"] = imgui_json::number(pt.type);
            points.push_back(point);
        }
        curve["KeyPoints"] = points;
        curves.push_back(curve);
    }
    keypoint["Curves"] = curves;
}

ImCurveEdit::CurveType KeyPointEditor::GetCurvePointType(size_t curveIndex, size_t point) const 
{
    if (curveIndex < mCurves.size())
    {
        if (point < mCurves[curveIndex].points.size())
        {
            return mCurves[curveIndex].points[point].type;
        }
    }
    return ImCurveEdit::CurveType::Hold;
}

ImCurveEdit::KeyPoint KeyPointEditor::GetPoint(size_t curveIndex, size_t pointIndex)
{
    ImCurveEdit::KeyPoint kp;
    if (curveIndex >= mCurves.size())
        return kp;
    const auto& points = mCurves[curveIndex].points;
    if (pointIndex >= points.size())
        return kp;
    const auto& point = points[pointIndex];
    kp.val = point.val*GetCurveValueRange(curveIndex)+GetCurveMin(curveIndex);
    kp.t = point.t;
    kp.type = point.type;
    return kp;
}

int KeyPointEditor::EditPoint(size_t curveIndex, size_t pointIndex, const ImVec4& value, ImCurveEdit::CurveType ctype)
{
    if (curveIndex < mCurves.size())
    {
        auto& points = mCurves[curveIndex].points;
        auto pointCnt = points.size();
        if (pointIndex < pointCnt)
        {
            const auto& valueRange = GetCurveValueRange(curveIndex);
            auto pointValue = (value - GetCurveMin(curveIndex)) / (valueRange + FLT_EPSILON);
            auto& point = points[pointIndex];
            point.val = pointValue;
            point.type = ctype;
            point.t = value.w;
            SortValues(curveIndex);
            for (size_t i = 0; i < pointCnt; i++)
            {
                if (points[i].t == value.w)
                    return (int)i;
            }
        }
    }
    return -1;
}

int KeyPointEditor::EditPointByDim(size_t curveIndex, size_t pointIndex, const ImVec2& value, ImCurveEdit::CurveType ctype, ImCurveEdit::ValueDimension eDim)
{
    if (curveIndex < mCurves.size())
    {
        auto& points = mCurves[curveIndex].points;
        auto pointCnt = points.size();
        if (pointIndex < pointCnt)
        {
            const auto valueRange = GetCurveValueRangeByDim(curveIndex, eDim);
            const auto dimValue = (value.y - GetCurveMinByDim(curveIndex, eDim)) / (valueRange + FLT_EPSILON);
            auto& point = points[pointIndex];
            ImCurveEdit::SetDimVal(point.val, dimValue, eDim);
            point.type = ctype;
            point.t = value.x;
            SortValues(curveIndex);
            for (auto i = 0; i < pointCnt; i++)
            {
                if (points[i].t == value.x)
                    return (int)i;
            }
        }
    }
    return -1;
}

static inline float _align_value(float v, float num, float den)
{
    const int64_t i = (int64_t)floor((double)v*num/den);
    return round(i*den/num);
}

ImVec4 KeyPointEditor::AlignValue(const ImVec4& value) const
{
    ImVec4 res;
    if (mDimAlign[0].x > 0 && mDimAlign[0].y > 0)
        res.x = _align_value(value.x, mDimAlign[0].x, mDimAlign[0].y);
    if (mDimAlign[1].x > 0 && mDimAlign[1].y > 0)
        res.y = _align_value(value.y, mDimAlign[1].x, mDimAlign[1].y);
    if (mDimAlign[2].x > 0 && mDimAlign[2].y > 0)
        res.z = _align_value(value.z, mDimAlign[2].x, mDimAlign[2].y);
    if (mDimAlign[3].x > 0 && mDimAlign[3].y > 0)
        res.w = _align_value(value.z, mDimAlign[3].x, mDimAlign[3].y);
    return res;
}

ImVec2 KeyPointEditor::AlignValueByDim(const ImVec2& value, ImCurveEdit::ValueDimension eDim) const
{
    ImVec2 res(value);
    if (mDimAlign[3].x > 0 && mDimAlign[3].y > 0)
        res.x = _align_value(value.x, mDimAlign[3].x, mDimAlign[3].y);
    if (eDim == ImCurveEdit::DIM_X && mDimAlign[0].x > 0 && mDimAlign[0].y > 0)
        res.y = _align_value(value.y, mDimAlign[0].x, mDimAlign[0].y);
    if (eDim == ImCurveEdit::DIM_Y && mDimAlign[1].x > 0 && mDimAlign[1].y > 0)
        res.y = _align_value(value.y, mDimAlign[1].x, mDimAlign[1].y);
    if (eDim == ImCurveEdit::DIM_Z && mDimAlign[2].x > 0 && mDimAlign[2].y > 0)
        res.y = _align_value(value.y, mDimAlign[2].x, mDimAlign[2].y);
    return res;
}

void KeyPointEditor::SetCurveAlign(const ImVec2& align, ImCurveEdit::ValueDimension eDim)
{
    if (eDim == ImCurveEdit::DIM_X)
        mDimAlign[0] = align;
    else if (eDim == ImCurveEdit::DIM_Y)
        mDimAlign[1] = align;
    else if (eDim == ImCurveEdit::DIM_Z)
        mDimAlign[2] = align;
    else
        mDimAlign[3] = align;
}

void KeyPointEditor::AddPoint(size_t curveIndex, const ImVec4& value, ImCurveEdit::CurveType ctype, bool bNeedNormalize)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    auto iter = std::find_if(curve.points.begin(), curve.points.end(), [value](const auto& item) {
        return value.w == item.t;
    });
    if (iter == curve.points.end())
    {
        if (bNeedNormalize)
        {
            ImVec4 nval = (value - curve.m_min) / (curve.m_valueRangeAbs + FLT_EPSILON);
            nval.w = value.w;
            curve.points.push_back({nval, ctype});
        }
        else
            curve.points.push_back({value, ctype});
        SortValues(curveIndex);
    }
}

void KeyPointEditor::AddPointByDim(size_t curveIndex, const ImVec2& value, ImCurveEdit::CurveType ctype, ImCurveEdit::ValueDimension eDim, bool bNeedNormalize)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    auto iter = std::find_if(curve.points.begin(), curve.points.end(), [value](const auto& item) {
        return value.x == item.t;
    });
    if (iter == curve.points.end())
    {
        ImVec4 v4Val(curve.m_default);
        ImCurveEdit::SetDimVal(v4Val, value.y, eDim);
        if (bNeedNormalize)
            v4Val = (v4Val - curve.m_min) / (curve.m_valueRangeAbs + FLT_EPSILON);
        v4Val.w = value.x;
        curve.points.push_back({v4Val, ctype});
        SortValues(curveIndex);
    }
}

void KeyPointEditor::ClearPoints(size_t curveIndex)
{
    if (curveIndex < mCurves.size())
    {
        mCurves[curveIndex].points.clear();
    }
}

void KeyPointEditor::DeletePoint(size_t curveIndex, size_t pointIndex)
{
    if (curveIndex < mCurves.size())
    {
        if (pointIndex < mCurves[curveIndex].points.size())
        {
            auto iter = mCurves[curveIndex].points.begin() + pointIndex;
            mCurves[curveIndex].points.erase(iter);
        }
    }
}

int KeyPointEditor::AddCurve(
        const std::string& name, ImCurveEdit::CurveType ctype, ImU32 color, bool visible,
        const ImVec4& _min, const ImVec4& _max, const ImVec4& _default, int64_t _id, int64_t _sub_id)
{
    auto new_key = ImCurveEdit::Curve(name, ctype, color, visible, _min, _max, _default);
    new_key.m_id = _id;
    new_key.m_sub_id = _sub_id;
    mCurves.push_back(new_key);
    return mCurves.size() - 1;
}

int KeyPointEditor::AddCurveByDim(
        const std::string& name, ImCurveEdit::CurveType ctype, ImU32 color, bool visible, ImCurveEdit::ValueDimension eDim,
        float _min, float _max, float _default, int64_t _id, int64_t _sub_id)
{
    ImVec4 minVal, maxVal(1, 1, 1, 1), defaultVal;
    ImCurveEdit::SetDimVal(minVal, _min, eDim);
    ImCurveEdit::SetDimVal(maxVal, _max, eDim);
    ImCurveEdit::SetDimVal(defaultVal, _default, eDim);
    return AddCurve(name, ctype, color, visible, minVal, maxVal, defaultVal, _id, _sub_id);
}

void KeyPointEditor::DeleteCurve(size_t curveIndex)
{
    if (curveIndex < mCurves.size())
    {
        auto iter = mCurves.begin() + curveIndex;
        mCurves.erase(iter);
    }
}

void KeyPointEditor::DeleteCurve(const std::string& name)
{
    int index = GetCurveIndex(name);
    if (index != -1)
    {
        DeleteCurve(index);
    }
}

int KeyPointEditor::GetCurveIndex(const std::string& name)
{
    int index = -1;
    auto iter = std::find_if(mCurves.begin(), mCurves.end(), [name](const ImCurveEdit::Curve& key)
    {
        return key.name == name;
    });
    if (iter != mCurves.end())
    {
        index = iter - mCurves.begin();
    }
    return index;
}

int KeyPointEditor::GetCurveIndex(int64_t id)
{
    int index = -1;
    auto iter = std::find_if(mCurves.begin(), mCurves.end(), [id](const ImCurveEdit::Curve& key)
    {
        return key.m_id != -1 && key.m_id == id;
    });
    if (iter != mCurves.end())
    {
        index = iter - mCurves.begin();
    }
    return index;
}

const ImCurveEdit::Curve* KeyPointEditor::GetCurve(const std::string& name)
{
    int index = -1;
    auto iter = std::find_if(mCurves.begin(), mCurves.end(), [name](const ImCurveEdit::Curve& key)
    {
        return key.name == name;
    });
    if (iter != mCurves.end())
    {
        index = iter - mCurves.begin();
    }
    if (index == -1)
        return nullptr;
    return &mCurves[index];
}

const ImCurveEdit::Curve* KeyPointEditor::GetCurve(size_t curveIndex)
{
    if (curveIndex < mCurves.size())
        return &mCurves[curveIndex];
    return nullptr;
}

ImVec4 KeyPointEditor::GetPointValue(size_t curveIndex, float t)
{
    if (curveIndex >= mCurves.size())
        return ImVec4();
    const auto& curve = mCurves[curveIndex];
    const auto& minValue = curve.m_min;
    const auto& valueRange = curve.m_valueRangeAbs;
    auto value = GetValue(curveIndex, t);
    value.x = (value.x - minValue.x) / (valueRange.x + FLT_EPSILON);
    value.y = (value.y - minValue.y) / (valueRange.y + FLT_EPSILON);
    value.z = (value.z - minValue.z) / (valueRange.z + FLT_EPSILON);
    return value;
}

ImVec4 KeyPointEditor::GetValue(size_t curveIndex, float t)
{
    ImVec4 res;
    if (curveIndex >= mCurves.size())
        return res;
    const auto& curve = mCurves[curveIndex];
    res = curve.m_default; res.w = t;

    auto range = GetMax() - GetMin() + ImVec4(0, 0, 0, 1); 
    const auto& valueRangeAbs = curve.m_valueRangeAbs;
    auto pointToRange = [&](const ImVec4& pt) { return (pt - GetMin()) / range; };

    const auto& points = curve.points;
    const auto ptCount = points.size();
    if (ptCount <= 0)
        return res;
    if (ptCount < 2 || t <= points[0].t)
    {
        res = pointToRange(points[0].val) * valueRangeAbs + curve.m_min;
        res.w = t;
    }
    else if (t >= points[ptCount-1].t)
    {
        res = pointToRange(points[ptCount-1].val) * valueRangeAbs + curve.m_min;
        res.w = t;
    }
    else
    {
        int found_index = -1;
        for (int i = 0; i < ptCount - 1; i++)
        {
            if (t >= points[i].t && t < points[i + 1].t)
            {
                found_index = i;
                break;
            }
        }
        if (found_index != -1)
        {
            const auto& p1 = points[found_index];
            const auto& p2 = points[found_index + 1];
            ImCurveEdit::CurveType type = (t - p1.t) < (p2.t - t) ? p1.type : p2.type;
            const float rt = ImCurveEdit::smoothstep(p1.t, p2.t, t, type);
            const auto val1 = pointToRange(p1.val);
            const auto val2 = pointToRange(p2.val);
            const auto v = ImLerp(val1, val2, rt);
            res = v * valueRangeAbs + curve.m_min;
            res.w = t;
        }
    }
    return res;
}

float KeyPointEditor::GetValueByDim(size_t curveIndex, float t, ImCurveEdit::ValueDimension eDim)
{
    return ImCurveEdit::GetDimVal(GetValue(curveIndex, t), eDim);
}

void KeyPointEditor::SetCurvePointDefault(size_t curveIndex, size_t pointIndex)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& points = mCurves[curveIndex].points;
    if (pointIndex >= points.size())
        return;
    const auto pointValDefault = (GetCurveDefault(curveIndex) - GetCurveMin(curveIndex)) / (GetCurveValueRange(curveIndex) + FLT_EPSILON);
    const auto ctypeDefault = GetCurveType(curveIndex);
    for (auto& point : points)
    {
        const auto t = point.t;
        point.val = pointValDefault;
        point.type = ctypeDefault;
        point.t = t;
    }
}

void KeyPointEditor::MoveTo(float t)
{
    float offset = t - mMin.w;
    float length = fabs(mMax.w - mMin.w);
    mMin.w = t;
    mMax.w = mMin.w + length;
    for (auto& curve : mCurves)
    {
        for (auto& point : curve.points)
            point.t += offset;
    }
}

void KeyPointEditor::SetMin(const ImVec4& vmin, bool dock)
{
    if (vmin == mMin)
        return;
    if (dock)
    {
        const auto curveCnt = mCurves.size();
        for (auto i = 0; i < curveCnt; i++)
        {
            // first compute the begin value
            const auto valueBegin = vmin.w > mMin.w ? GetPointValue(i, vmin.w) : GetPointValue(i, mMin.w);
            // second delete out of range points, keep begin end
            auto& points = mCurves[i].points;
            if (points.size() > 2)
            {
                auto iter = points.begin() + 1;
                auto iterEnd = points.end() - 1;
                while (iter != iterEnd)
                {
                    if (iter->t < vmin.w || iter->x < vmin.x || iter->y < vmin.y || iter->z < vmin.z)
                    {
                        iter = points.erase(iter);
                        iterEnd = points.end() - 1;
                    }
                    else
                        ++iter;
                }
            }
            // update the start point
            if (points.size() > 0)
                points[0].val = valueBegin;
        }
    }
    mMin = vmin;
}

void KeyPointEditor::SetMax(const ImVec4& vmax, bool dock)
{
    if (vmax == mMax)
        return;
    if (dock)
    {
        const auto curveCnt = mCurves.size();
        for (auto i = 0; i < curveCnt; i++)
        {
            // first compute the end value
            const auto valueEnd = GetPointValue(i, vmax.w);
            // second delete out of range points, keep begin end
            auto& points = mCurves[i].points;
            if (points.size() > 2)
            {
                auto iter = points.begin() + 1;
                auto iterEnd = points.end() - 1;
                while (iter != iterEnd)
                {
                    if (iter->t > vmax.w || iter->x > vmax.x || iter->y > vmax.y || iter->z > vmax.z)
                    {
                        iter = points.erase(iter);
                        iterEnd = points.end() - 1;
                    }
                    else
                        ++iter;
                }
            }
            // update the end point
            if (points.size() > 0)
            {
                points[points.size()-1].val = valueEnd;
                // std::cout << "valueEnd (" << valueEnd.x << ", " << valueEnd.y << ", " << valueEnd.z << ", " << valueEnd.w << ")" << std::endl;
            }
        }
    }
    mMax = vmax;
}

void KeyPointEditor::SetCurveMin(size_t curveIndex, const ImVec4& _min)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    curve.m_min = _min;
    auto tmp = curve.m_max-_min;
    auto& valueRangeAbs = curve.m_valueRangeAbs;
    valueRangeAbs.x = fabs(tmp.x);
    valueRangeAbs.y = fabs(tmp.y);
    valueRangeAbs.z = fabs(tmp.z);
    valueRangeAbs.w = 1.f;
}

void KeyPointEditor::SetCurveMax(size_t curveIndex, const ImVec4& _max)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    curve.m_max = _max;
    auto tmp = _max-curve.m_min;
    auto& valueRangeAbs = curve.m_valueRangeAbs;
    valueRangeAbs.x = fabs(tmp.x);
    valueRangeAbs.y = fabs(tmp.y);
    valueRangeAbs.z = fabs(tmp.z);
    valueRangeAbs.w = 1.f;
}

void KeyPointEditor::SetCurveDefault(size_t curveIndex, const ImVec4& _default)
{
    if (curveIndex >= mCurves.size())
        return;
    mCurves[curveIndex].m_default = _default;
}

void KeyPointEditor::SetCurveMinByDim(size_t curveIndex, float _min, ImCurveEdit::ValueDimension eDim)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    ImCurveEdit::SetDimVal(curve.m_min, _min, eDim);
    auto tmp = fabs(ImCurveEdit::GetDimVal(curve.m_max, eDim)-_min);
    ImCurveEdit::SetDimVal(curve.m_valueRangeAbs, tmp, eDim);
}

void KeyPointEditor::SetCurveMaxByDim(size_t curveIndex, float _max, ImCurveEdit::ValueDimension eDim)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    ImCurveEdit::SetDimVal(curve.m_max, _max, eDim);
    auto tmp = fabs(_max-ImCurveEdit::GetDimVal(curve.m_min, eDim));
    ImCurveEdit::SetDimVal(curve.m_valueRangeAbs, tmp, eDim);
}

void KeyPointEditor::SetCurveDefaultByDim(size_t curveIndex, float _default, ImCurveEdit::ValueDimension eDim)
{
    if (curveIndex >= mCurves.size())
        return;
    auto& curve = mCurves[curveIndex];
    ImCurveEdit::SetDimVal(curve.m_default, _default, eDim);
}

ImVec4 KeyPointEditor::GetPrevPoint(float pos)
{
    ImVec4 res;
    const auto curveCnt = mCurves.size();
    float pointPos = FLT_MAX;
    for (auto curIdx = 0; curIdx < curveCnt; curIdx++)
    {
        const auto& curve = mCurves[curIdx];
        const auto& points = curve.points;
        const auto pointCnt = points.size();
        for (auto ptIdx = pointCnt - 1; ptIdx >= 0; ptIdx--)
        {
            const auto& p = points[ptIdx];
            if (p.t > pos)
                continue;
            if (p.t == pos && ptIdx != 0)
                continue;
            if (p.t < pointPos)
            {
                res = p.val;
                pointPos = p.t;
                break;
            }
        }
    }
    return res;
}

ImVec4 KeyPointEditor::GetNextPoint(float pos)
{
    ImVec4 res;
    const auto curveCnt = mCurves.size();
    float pointPos = FLT_MIN;
    for (auto curIdx = 0; curIdx < curveCnt; curIdx++)
    {
        const auto& curve = mCurves[curIdx];
        const auto& points = curve.points;
        const auto pointCnt = points.size();
        for (auto ptIdx = 0; ptIdx < pointCnt; ptIdx++)
        {
            const auto& p = points[ptIdx];
            if (p.t < pos)
                continue;
            if (p.t == pos && ptIdx != pointCnt - 1)
                continue;
            if (p.t > pointPos)
            {
                res = p.val;
                pointPos = p.t;
                break;
            }
        }
    }
    return res;
}

void KeyPointEditor::SortValues(size_t curveIndex)
{
    if (curveIndex < mCurves.size())
    {
        auto& curve = mCurves[curveIndex];
        std::sort(curve.points.begin(), curve.points.end(), [](const auto& a, const auto& b) { return a.t < b.t; });
    }
}

bool ImCurveEditKeyByDim(const std::string& label, ImCurveEdit::Curve* pCurve, ImCurveEdit::ValueDimension eDim, const std::string& name, float _min, float _max, float _default, float space)
{
    if (!pCurve || name.empty() || label.empty() || pCurve->m_id == -1)
        return false;
    SameLine(space);
    const std::string idStr = std::to_string(pCurve->m_id);
    const std::string fullLable = label + "@" + idStr;
    const std::string curveName = name + "@" + idStr;
    if (DiamondButton(fullLable.c_str(), false)) 
    {
        pCurve->name = curveName;
        ImCurveEdit::SetDimVal(pCurve->m_min, _min, eDim);
        ImCurveEdit::SetDimVal(pCurve->m_max, _max, eDim);
        ImCurveEdit::SetDimVal(pCurve->m_default, _default, eDim);
        return true;
    }
    ShowTooltipOnHover("Add Curve");
    return false;
}

bool ImCurveCheckEditKeyByDim(const std::string& label, ImCurveEdit::Curve* pCurve, ImCurveEdit::ValueDimension eDim, bool &check, const std::string& name, float _min, float _max, float _default, float space)
{
    if (!pCurve || name.empty() || label.empty() || pCurve->m_id == -1)
        return false;
    SameLine(space);
    const std::string idStr = std::to_string(pCurve->m_id);
    const std::string fullLable = label + "@" + idStr;
    const std::string curveName = name + "@" + idStr;
    if (check)
    {
        if (DiamondButton(fullLable.c_str(), true)) 
        {
            check = false;
            return true;
        }
        ShowTooltipOnHover("Remove Curve");
    }
    else
    {
        if (DiamondButton(fullLable.c_str(), false)) 
        {
            pCurve->name = curveName;
            ImCurveEdit::SetDimVal(pCurve->m_min, _min, eDim);
            ImCurveEdit::SetDimVal(pCurve->m_max, _max, eDim);
            ImCurveEdit::SetDimVal(pCurve->m_default, _default, eDim);
            check = true;
            return true;
        }
        ShowTooltipOnHover("Add Curve");
    }
    return false;
}

bool ImCurveCheckEditKeyWithIDByDim(const std::string& label, ImCurveEdit::Curve* pCurve, ImCurveEdit::ValueDimension eDim, bool check, const std::string& name, float _min, float _max, float _default, int64_t subid, float space)
{
    if (!pCurve || name.empty() || label.empty() || pCurve->m_id == -1)
        return false;
    SameLine(space);
    const std::string idStr = std::to_string(pCurve->m_id);
    const std::string fullLable = label + "@" + idStr;
    const std::string curveName = name + "@" + idStr;
    if (check)
    {
        if (DiamondButton(fullLable.c_str(), true)) 
        {
            pCurve->name = curveName;
            pCurve->m_sub_id = subid;
            pCurve->checked = false;
            return true;
        }
        ShowTooltipOnHover("Remove Curve");
    }
    else
    {
        if (DiamondButton(fullLable.c_str(), false)) 
        {
            pCurve->name = curveName;
            ImCurveEdit::SetDimVal(pCurve->m_min, _min, eDim);
            ImCurveEdit::SetDimVal(pCurve->m_max, _max, eDim);
            ImCurveEdit::SetDimVal(pCurve->m_default, _default, eDim);
            pCurve->m_sub_id = subid;
            pCurve->checked = true;
            return true;
        }
        ShowTooltipOnHover("Add Curve");
    }
    return false;
}

}  // ~ namespace ImGui
