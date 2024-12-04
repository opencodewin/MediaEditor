#include "imgui_extra_widget.h"
#include "imgui_helper.h"
#include "imgui_fft.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <array>
#include <imgui.h>
#include <stdio.h>

static const ImS32          IM_S32_MIN = INT_MIN;    // (-2147483647 - 1), (0x80000000);
static const ImS32          IM_S32_MAX = INT_MAX;    // (2147483647), (0x7FFFFFFF)
static const ImU32          IM_U32_MIN = 0;
static const ImU32          IM_U32_MAX = UINT_MAX;   // (0xFFFFFFFF)
#ifdef LLONG_MIN
static const ImS64          IM_S64_MIN = LLONG_MIN;  // (-9223372036854775807ll - 1ll);
static const ImS64          IM_S64_MAX = LLONG_MAX;  // (9223372036854775807ll);
#else
static const ImS64          IM_S64_MIN = -9223372036854775807LL - 1;
static const ImS64          IM_S64_MAX = 9223372036854775807LL;
#endif
static const ImU64          IM_U64_MIN = 0;
#ifdef ULLONG_MAX
static const ImU64          IM_U64_MAX = ULLONG_MAX; // (0xFFFFFFFFFFFFFFFFull);
#else
static const ImU64          IM_U64_MAX = (2ULL * 9223372036854775807LL + 1);
#endif

inline static tm GetCurrentDate() {
    time_t now;time(&now);
    return *localtime(&now);
}
// Tip: we modify tm (its fields can even be negative!) and then we call this method to retrieve a valid date
inline static void RecalculateDateDependentFields(tm& date)    {
    date.tm_isdst=-1;   // This tries to detect day time savings too
    time_t tmp = mktime(&date);
    date=*localtime(&tmp);
}
inline static tm GetDateZero() {
    tm date;
    memset(&date,0,sizeof(tm));     // Mandatory for emscripten. Please do not remove!
    date.tm_isdst=-1;
    date.tm_sec=0;		//	 Seconds.	[0-60] (1 leap second)
    date.tm_min=0;		//	 Minutes.	[0-59]
    date.tm_hour=0;		//	 Hours.	[0-23]
    date.tm_mday=0;		//	 Day.		[1-31]
    date.tm_mon=0;		//	 Month.	[0-11]
    date.tm_year=0;		//	 Year	- 1900.
    date.tm_wday=0;     //	 Day of week.	[0-6]
    date.tm_yday=0;		//	 Days in year.[0-365]
    return date;
}
inline static void UpdateDate(struct tm& date)
{
    date.tm_isdst = -1;   // This tries to detect day time savings too
    time_t tmp = mktime(&date);
    date = *localtime(&tmp);
}
inline static int DaysInMonth(struct tm& date)
{
    const static int num_days_per_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int days_in_month = num_days_per_month[date.tm_mon];
    if (days_in_month == 28)
    {
        const int year = 1900+date.tm_year;
        const bool bis = ((year%4)==0) && ((year%100)!=0 || (year%400)==0);
        if (bis) days_in_month = 29;
    }
    return days_in_month;
}

inline static bool SameMonth(struct tm& d1, struct tm& d2)
{
    return d1.tm_mon == d2.tm_mon && d1.tm_year == d2.tm_year;
}


template<typename TYPE>
static const char* ImAtoi(const char* src, TYPE* output)
{
    int negative = 0;
    if (*src == '-') { negative = 1; src++; }
    if (*src == '+') { src++; }
    TYPE v = 0;
    while (*src >= '0' && *src <= '9')
        v = (v * 10) + (*src++ - '0');
    *output = negative ? -v : v;
    return src;
}

template<typename TYPE, typename SIGNEDTYPE>
TYPE ImGui::RoundScalarWithFormatKnobT(const char* format, ImGuiDataType data_type, TYPE v)
{
    const char* fmt_start = ImParseFormatFindStart(format);
    if (fmt_start[0] != '%' || fmt_start[1] == '%') // Don't apply if the value is not visible in the format string
        return v;
    char v_str[64];
    ImFormatString(v_str, IM_ARRAYSIZE(v_str), fmt_start, v);
    const char* p = v_str;
    while (*p == ' ')
        p++;
    if (data_type == ImGuiDataType_Float || data_type == ImGuiDataType_Double)
        v = (TYPE)ImAtof(p);
    else
        ImAtoi(p, (SIGNEDTYPE*)&v);
    return v;
}

template<typename TYPE, typename FLOATTYPE>
float ImGui::SliderCalcRatioFromValueT(ImGuiDataType data_type, TYPE v, TYPE v_min, TYPE v_max, float power, float linear_zero_pos)
{
    if (v_min == v_max)
        return 0.0f;

    const bool is_power = (power != 1.0f) && (data_type == ImGuiDataType_Float || data_type == ImGuiDataType_Double);
    const TYPE v_clamped = (v_min < v_max) ? ImClamp(v, v_min, v_max) : ImClamp(v, v_max, v_min);
    if (is_power)
    {
        if (v_clamped < 0.0f)
        {
            const float f = 1.0f - (float)((v_clamped - v_min) / (ImMin((TYPE)0, v_max) - v_min));
            return (1.0f - ImPow(f, 1.0f/power)) * linear_zero_pos;
        }
        else
        {
            const float f = (float)((v_clamped - ImMax((TYPE)0, v_min)) / (v_max - ImMax((TYPE)0, v_min)));
            return linear_zero_pos + ImPow(f, 1.0f/power) * (1.0f - linear_zero_pos);
        }
    }

    // Linear slider
    return (float)((FLOATTYPE)(v_clamped - v_min) / (FLOATTYPE)(v_max - v_min));
}

// FIXME: Move some of the code into SliderBehavior(). Current responsability is larger than what the equivalent DragBehaviorT<> does, we also do some rendering, etc.
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
bool ImGui::SliderBehaviorKnobT(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, TYPE* v, const TYPE v_min, const TYPE v_max, const char* format, float power, ImGuiSliderFlags flags, ImRect* out_grab_bb)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
    const bool is_decimal = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
    const bool is_power = (power != 1.0f) && is_decimal;

    const float grab_padding = 2.0f;
    const float slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;
    float grab_sz = style.GrabMinSize;
    SIGNEDTYPE v_range = (v_min < v_max ? v_max - v_min : v_min - v_max);
    if (!is_decimal && v_range >= 0)                                             // v_range < 0 may happen on integer overflows
        grab_sz = ImMax((float)(slider_sz / (v_range + 1)), style.GrabMinSize);  // For integer sliders: if possible have the grab size represent 1 unit
    grab_sz = ImMin(grab_sz, slider_sz);
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    // For power curve sliders that cross over sign boundary we want the curve to be symmetric around 0.0f
    float linear_zero_pos;   // 0.0->1.0f
    if (is_power && v_min * v_max < 0.0f)
    {
        // Different sign
        const FLOATTYPE linear_dist_min_to_0 = ImPow(v_min >= 0 ? (FLOATTYPE)v_min : -(FLOATTYPE)v_min, (FLOATTYPE)1.0f / power);
        const FLOATTYPE linear_dist_max_to_0 = ImPow(v_max >= 0 ? (FLOATTYPE)v_max : -(FLOATTYPE)v_max, (FLOATTYPE)1.0f / power);
        linear_zero_pos = (float)(linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0));
    }
    else
    {
        // Same sign
        linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
    }

    // Process interacting with the slider
    bool value_changed = false;
    if (g.ActiveId == id)
    {
        bool set_new_value = false;
        float clicked_t = 0.0f;
        if (g.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            if (!g.IO.MouseDown[0])
            {
                ClearActiveID();
            }
            else
            {
                const float mouse_abs_pos = g.IO.MousePos[axis];
                clicked_t = (slider_usable_sz > 0.0f) ? ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_usable_sz, 0.0f, 1.0f) : 0.0f;
                if (axis == ImGuiAxis_Y)
                    clicked_t = 1.0f - clicked_t;
                set_new_value = true;
            }
        }
        else if (g.ActiveIdSource == ImGuiInputSource_Keyboard || g.ActiveIdSource == ImGuiInputSource_Gamepad)
        {
            const bool tweak_slow = IsKeyDown((g.NavInputSource == ImGuiInputSource_Gamepad) ? ImGuiKey_NavGamepadTweakSlow : ImGuiKey_NavKeyboardTweakSlow);
            const bool tweak_fast = IsKeyDown((g.NavInputSource == ImGuiInputSource_Gamepad) ? ImGuiKey_NavGamepadTweakFast : ImGuiKey_NavKeyboardTweakFast);
            const float tweak_factor = tweak_slow ? 1.0f / 1.0f : tweak_fast ? 10.0f : 1.0f;
            float delta = GetNavTweakPressedAmount(axis) * tweak_factor;
            //const ImVec2 delta2 = GetNavInputAmount2d(ImGuiNavDirSourceFlags_Keyboard | ImGuiNavDirSourceFlags_PadDPad, ImGuiNavReadMode_RepeatFast, 0.0f, 0.0f);
            //float delta = (axis == ImGuiAxis_X) ? delta2.x : -delta2.y;
            if (g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
            {
                ClearActiveID();
            }
            else if (delta != 0.0f)
            {
                clicked_t = SliderCalcRatioFromValueT<TYPE,FLOATTYPE>(data_type, *v, v_min, v_max, power, linear_zero_pos);
                const int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 0;
                if ((decimal_precision > 0) || is_power)
                {
                    delta /= 100.0f;    // Gamepad/keyboard tweak speeds in % of slider bounds
                    if (tweak_slow)
                        delta /= 10.0f;
                }
                else
                {
                    if ((v_range >= -100.0f && v_range <= 100.0f) || tweak_slow)
                        delta = ((delta < 0.0f) ? -1.0f : +1.0f) / (float)v_range; // Gamepad/keyboard tweak speeds in integer steps
                    else
                        delta /= 100.0f;
                }
                if (tweak_fast)
                    delta *= 10.0f;
                set_new_value = true;
                if ((clicked_t >= 1.0f && delta > 0.0f) || (clicked_t <= 0.0f && delta < 0.0f)) // This is to avoid applying the saturation when already past the limits
                    set_new_value = false;
                else
                    clicked_t = ImSaturate(clicked_t + delta);
            }
        }

        if (set_new_value)
        {
            TYPE v_new;
            if (is_power)
            {
                // Account for power curve scale on both sides of the zero
                if (clicked_t < linear_zero_pos)
                {
                    // Negative: rescale to the negative range before powering
                    float a = 1.0f - (clicked_t / linear_zero_pos);
                    a = ImPow(a, power);
                    v_new = ImLerp(ImMin(v_max, (TYPE)0), v_min, a);
                }
                else
                {
                    // Positive: rescale to the positive range before powering
                    float a;
                    if (ImFabs(linear_zero_pos - 1.0f) > 1.e-6f)
                        a = (clicked_t - linear_zero_pos) / (1.0f - linear_zero_pos);
                    else
                        a = clicked_t;
                    a = ImPow(a, power);
                    v_new = ImLerp(ImMax(v_min, (TYPE)0), v_max, a);
                }
            }
            else
            {
                // Linear slider
                if (is_decimal)
                {
                    v_new = ImLerp(v_min, v_max, clicked_t);
                }
                else
                {
                    // For integer values we want the clicking position to match the grab box so we round above
                    // This code is carefully tuned to work with large values (e.g. high ranges of U64) while preserving this property..
                    FLOATTYPE v_new_off_f = (v_max - v_min) * clicked_t;
                    TYPE v_new_off_floor = (TYPE)(v_new_off_f);
                    TYPE v_new_off_round = (TYPE)(v_new_off_f + (FLOATTYPE)0.5);
                    if (v_new_off_floor < v_new_off_round)
                        v_new = v_min + v_new_off_round;
                    else
                        v_new = v_min + v_new_off_floor;
                }
            }

            // Round to user desired precision based on format string
            v_new = RoundScalarWithFormatKnobT<TYPE,SIGNEDTYPE>(format, data_type, v_new);

            // Apply result
            if (*v != v_new)
            {
                *v = v_new;
                value_changed = true;
            }
        }
    }

    if (slider_sz < 1.0f)
    {
        *out_grab_bb = ImRect(bb.Min, bb.Min);
    }
    else
    {
        // Output grab position so it can be displayed by the caller
        float grab_t = SliderCalcRatioFromValueT<TYPE, FLOATTYPE>(data_type, *v, v_min, v_max, power, linear_zero_pos);
        if (axis == ImGuiAxis_Y)
            grab_t = 1.0f - grab_t;
        const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
        if (axis == ImGuiAxis_X)
            *out_grab_bb = ImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
        else
            *out_grab_bb = ImRect(bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
    }

    return value_changed;
}

// For 32-bits and larger types, slider bounds are limited to half the natural type range.
// So e.g. an integer Slider between INT_MAX-10 and INT_MAX will fail, but an integer Slider between INT_MAX/2-10 and INT_MAX/2 will be ok.
// It would be possible to lift that limitation with some work but it doesn't seem to be worth it for sliders.
bool ImGui::SliderBehavior(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, void* v, const void* v_min, const void* v_max, const char* format, float power, ImGuiSliderFlags flags, ImRect* out_grab_bb)
{
    switch (data_type)
    {
    case ImGuiDataType_S8:  { ImS32 v32 = (ImS32)*(ImS8*)v;  bool r = SliderBehaviorKnobT<ImS32, ImS32, float >(bb, id, ImGuiDataType_S32, &v32, *(const ImS8*)v_min,  *(const ImS8*)v_max,  format, power, flags, out_grab_bb); if (r) *(ImS8*)v  = (ImS8)v32;  return r; }
    case ImGuiDataType_U8:  { ImU32 v32 = (ImU32)*(ImU8*)v;  bool r = SliderBehaviorKnobT<ImU32, ImS32, float >(bb, id, ImGuiDataType_U32, &v32, *(const ImU8*)v_min,  *(const ImU8*)v_max,  format, power, flags, out_grab_bb); if (r) *(ImU8*)v  = (ImU8)v32;  return r; }
    case ImGuiDataType_S16: { ImS32 v32 = (ImS32)*(ImS16*)v; bool r = SliderBehaviorKnobT<ImS32, ImS32, float >(bb, id, ImGuiDataType_S32, &v32, *(const ImS16*)v_min, *(const ImS16*)v_max, format, power, flags, out_grab_bb); if (r) *(ImS16*)v = (ImS16)v32; return r; }
    case ImGuiDataType_U16: { ImU32 v32 = (ImU32)*(ImU16*)v; bool r = SliderBehaviorKnobT<ImU32, ImS32, float >(bb, id, ImGuiDataType_U32, &v32, *(const ImU16*)v_min, *(const ImU16*)v_max, format, power, flags, out_grab_bb); if (r) *(ImU16*)v = (ImU16)v32; return r; }
    case ImGuiDataType_S32:
        IM_ASSERT(*(const ImS32*)v_min >= IM_S32_MIN/2 && *(const ImS32*)v_max <= IM_S32_MAX/2);
        return SliderBehaviorKnobT<ImS32, ImS32, float >(bb, id, data_type, (ImS32*)v,  *(const ImS32*)v_min,  *(const ImS32*)v_max,  format, power, flags, out_grab_bb);
    case ImGuiDataType_U32:
        IM_ASSERT(*(const ImU32*)v_max <= IM_U32_MAX/2);
        return SliderBehaviorKnobT<ImU32, ImS32, float >(bb, id, data_type, (ImU32*)v,  *(const ImU32*)v_min,  *(const ImU32*)v_max,  format, power, flags, out_grab_bb);
    case ImGuiDataType_S64:
        IM_ASSERT(*(const ImS64*)v_min >= IM_S64_MIN/2 && *(const ImS64*)v_max <= IM_S64_MAX/2);
        return SliderBehaviorKnobT<ImS64, ImS64, double>(bb, id, data_type, (ImS64*)v,  *(const ImS64*)v_min,  *(const ImS64*)v_max,  format, power, flags, out_grab_bb);
    case ImGuiDataType_U64:
        IM_ASSERT(*(const ImU64*)v_max <= IM_U64_MAX/2);
        return SliderBehaviorKnobT<ImU64, ImS64, double>(bb, id, data_type, (ImU64*)v,  *(const ImU64*)v_min,  *(const ImU64*)v_max,  format, power, flags, out_grab_bb);
    case ImGuiDataType_Float:
        IM_ASSERT(*(const float*)v_min >= -FLT_MAX/2.0f && *(const float*)v_max <= FLT_MAX/2.0f);
        return SliderBehaviorKnobT<float, float, float >(bb, id, data_type, (float*)v,  *(const float*)v_min,  *(const float*)v_max,  format, power, flags, out_grab_bb);
    case ImGuiDataType_Double:
        IM_ASSERT(*(const double*)v_min >= -DBL_MAX/2.0f && *(const double*)v_max <= DBL_MAX/2.0f);
        return SliderBehaviorKnobT<double,double,double>(bb, id, data_type, (double*)v, *(const double*)v_min, *(const double*)v_max, format, power, flags, out_grab_bb);
    case ImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
    return false;
}

void ImGui::UvMeter(char const *label, ImVec2 const &size, int *value, int v_min, int v_max, int steps, int* stack, int* count, float background, std::map<float, float> segment)
{
    UvMeter(ImGui::GetWindowDrawList(), label, size, value, v_min, v_max, steps, stack, count, background, segment);
}

void ImGui::UvMeter(ImDrawList *draw_list, char const *label, ImVec2 const &size, int *value, int v_min, int v_max, int steps, int* stack, int* count, float background, std::map<float, float> segment)
{
    float fvalue = (float)*value;
    float *fstack = nullptr;
    float _f = 0.f;
    if (stack) { fstack = &_f; *fstack = (float)*stack; }
    UvMeter(draw_list, label, size, &fvalue, (float)v_min, (float)v_max, steps, fstack, count, background, segment);
    *value = (int)fvalue;
    if (stack) *stack = (int)*fstack;
}

void ImGui::UvMeter(char const *label, ImVec2 const &size, float *value, float v_min, float v_max, int steps, float* stack, int* count, float background, std::map<float, float> segment)
{
    UvMeter(ImGui::GetWindowDrawList(), label, size, value, v_min, v_max, steps, stack, count, background, segment);
}

void ImGui::UvMeter(ImDrawList *draw_list, char const *label, ImVec2 const &size, float *value, float v_min, float v_max, int steps, float* stack, int* count, float background, std::map<float, float> segment)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(label, size);
    float steps_size = (v_max - v_min) / (float)steps;
    if (stack && count)
    {
        if (*value > *stack) 
        {
            *stack = *value;
            *count = 0;
        }
        else
        {
            *(count) += 1;
            if (*count > 10)
            {
                *stack -= steps_size / 2;
                if (*stack < v_min) *stack = v_min;
            }
        }
        if (*stack)
            ImGui::UpdateData();
    }

    if (size.y > size.x)
    {
        float stepHeight = size.y / (v_max - v_min + 1);
        auto y = pos.y + size.y;
        auto hue = 0.4f;
        auto sat = 1.0f;
        auto lum = 0.6f;
        for (float i = v_min; i <= v_max; i += steps_size)
        {
            if (segment.empty())
                hue = 0.4f - (i / (v_max - v_min)) / 2.0f;
            else
            {
                for (const auto value : segment)
                {
                    if (i <= value.first)
                    {
                        hue = value.second;
                        break;
                    }
                }
            }
            sat = (*value < i ? 0.8 : 1.0f);
            lum = (*value < i ? background : 1.0f);
            draw_list->AddRectFilled(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y - (stepHeight * steps_size - 1)), static_cast<ImU32>(ImColor::HSV(hue, sat, lum)));
            y = pos.y + size.y - (i * stepHeight);
        }
        if (stack && count)
        {
            draw_list->AddLine(ImVec2(pos.x, pos.y + size.y - (*stack * stepHeight)), ImVec2(pos.x + size.x, pos.y + size.y - (*stack * stepHeight)), IM_COL32_WHITE, 2.f);
        }
    }
    else
    {
        float stepWidth = size.x / (v_max - v_min + 1);
        auto x = pos.x;
        auto hue = 0.4f;
        auto sat = 0.6f;
        auto lum = 0.6f;
        for (float i = v_min; i <= v_max; i += steps_size)
        {
            hue = 0.4f - (i / (v_max - v_min)) / 2.0f;
            sat = (*value < i ? 0.0f : 0.6f);
            lum = (*value < i ? 0.0f : 0.6f);
            draw_list->AddRectFilled(ImVec2(x, pos.y), ImVec2(x + (stepWidth * steps_size - 1), pos.y + size.y), static_cast<ImU32>(ImColor::HSV(hue, sat, lum)));
            x = pos.x + (i * stepWidth);
        }
        if (stack && count)
        {
            draw_list->AddLine(ImVec2(pos.x + (*stack * stepWidth), pos.y), ImVec2(pos.x + (*stack * stepWidth), pos.y + size.y), IM_COL32_WHITE, 2.f);
        }
    }
}

bool ImGui::Fader(const char *label, const ImVec2 &size, int *v, const int v_min, const int v_max, const char *format, float power)
{
    ImGuiDataType data_type = ImGuiDataType_S32;
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, nullptr, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const ImRect bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(bb, style.FramePadding.y);
    if (!ItemAdd(frame_bb, id))
        return false;

    IM_ASSERT(data_type >= 0 && data_type < ImGuiDataType_COUNT);
    if (format == nullptr)
        format = "%d";

    const bool hovered = ItemHoverable(frame_bb, id, ImGuiItemFlags_None);
    if ((hovered && g.IO.MouseClicked[0]) || g.NavActivateId == id)
    {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        //        g.ActiveIdAllowNavDirFlags = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, v, &v_min, &v_max, format, power, ImGuiSliderFlags_Vertical, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    ImRect gutter;
    gutter.Min = grab_bb.Min;
    gutter.Max = ImVec2(frame_bb.Max.x - 2.0f, frame_bb.Max.y - 2.0f);
    auto w = ((gutter.Max.x - gutter.Min.x) - 4.0f) / 2.0f;
    gutter.Min.x += w;
    gutter.Max.x -= w;
    window->DrawList->AddRectFilled(gutter.Min, gutter.Max, GetColorU32(ImGuiCol_ButtonActive), style.GrabRounding);

    // Render grab
    window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(ImGuiCol_Text), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    // For the vertical slider we allow centered text to overlap the frame padding
    char value_buf[64];
    snprintf(value_buf, 64, format, int(*v * power)); // modify by dicky reduce warning
    const char *value_buf_end = value_buf + strlen(value_buf);
    RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, value_buf, value_buf_end, nullptr, ImVec2(0.5f, 0.0f));
    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    return value_changed;
}

///////////////////////////////////////////////////////////////////////////////
// New Knob controllor 
static void draw_circle(ImVec2 center, float _size, bool filled, int segments, float radius, ImGui::ColorSet& color)
{
    float circle_radius = _size * radius;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 _color = ImGui::GetColorU32(ImGui::IsItemActive() ? color.active : ImGui::IsItemHovered(0) ? color.hovered : color.base);
    if (filled)
        draw_list->AddCircleFilled(center, circle_radius, _color, segments);
    else
        draw_list->AddCircle(center, circle_radius, _color, segments);
}

static void bezier_arc(ImVec2 center, ImVec2 start, ImVec2 end, ImVec2& c1, ImVec2 & c2)
{
    float ax = start[0] - center[0];
    float ay = start[1] - center[1];
    float bx = end[0] - center[0];
    float by = end[1] - center[1];
    float q1 = ax * ax + ay * ay;
    float q2 = q1 + ax * bx + ay * by;
    float k2 = (4.0 / 3.0) * (sqrt(2.0 * q1 * q2) - q2) / (ax * by - ay * bx);
    c1 = ImVec2(center[0] + ax - k2 * ay, center[1] + ay + k2 * ax);
    c2 = ImVec2(center[0] + bx + k2 * by, center[1] + by - k2 * bx);
}

static void draw_arc1(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImU32 color, int num_segments)
{
    ImVec2 start = {center[0] + cos(start_angle) * radius, center[1] + sin(start_angle) * radius};
    ImVec2 end = {center[0] + cos(end_angle) * radius, center[1] + sin(end_angle) * radius};
    ImVec2 c1, c2;
    bezier_arc(center, start, end, c1, c2);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddBezierCubic(start, c1, c2, end, color, thickness, num_segments);
}

static void draw_arc(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImU32 color, int num_segments, int8_t bezier_count)
{
    float overlap = thickness * radius * 0.00001 * IM_PI;
    float delta = end_angle - start_angle;
    float bez_step = 1.0 / (float)bezier_count;
    float mid_angle = start_angle + overlap;
    for (int i = 0; i < bezier_count - 1; i++)
    {
        float mid_angle2 = delta * bez_step + mid_angle;
        draw_arc1(center, radius, mid_angle - overlap, mid_angle2 + overlap, thickness, color, num_segments);
        mid_angle = mid_angle2;
    }
    draw_arc1(center, radius, mid_angle - overlap, end_angle, thickness, color, num_segments);
}

static void draw_arc(ImVec2 center, float _radius, float _size, float radius, float start_angle, float end_angle, int segments, int8_t bezier_count, ImGui::ColorSet& color)
{
    float track_radius = _radius * radius;
    float track_size = _size * radius * 0.5 + 0.0001;
    ImU32 _color = ImGui::GetColorU32(ImGui::IsItemActive() ? color.active : ImGui::IsItemHovered(0) ? color.hovered : color.base);
    draw_arc(center, track_radius, start_angle, end_angle, track_size, _color, segments, bezier_count);
}

static void draw_dot(ImVec2 center, float _radius, float _size, float radius, float _angle, bool filled, int segments, ImGui::ColorSet& color)
{
    float dot_size = _size * radius;
    float dot_radius = _radius * radius;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 _color = ImGui::GetColorU32(ImGui::IsItemActive() ? color.active : ImGui::IsItemHovered(0) ? color.hovered : color.base);
    if (filled)
        draw_list->AddCircleFilled(
                ImVec2(center[0] + cos(_angle) * dot_radius, center[1] + sin(_angle) * dot_radius),
                dot_size, _color, segments);
    else
        draw_list->AddCircle(
                ImVec2(center[0] + cos(_angle) * dot_radius, center[1] + sin(_angle) * dot_radius),
                dot_size, _color, segments);
}

static void draw_tick(ImVec2 center, float radius, float start, float end, float width, float _angle, ImGui::ColorSet& color)
{
    float tick_start = start * radius;
    float tick_end = end * radius;
    float _angle_cos = cos(_angle);
    float _angle_sin = sin(_angle);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 _color = ImGui::GetColorU32(ImGui::IsItemActive() ? color.active : ImGui::IsItemHovered(0) ? color.hovered : color.base);
    draw_list->AddLine(
            ImVec2(center[0] + _angle_cos * tick_end, center[1] + _angle_sin * tick_end),
            ImVec2(center[0] + _angle_cos * tick_start, center[1] + _angle_sin * tick_start),
            _color,
            width * radius);
}

bool ImGui::Knob(char const *label, float *p_value, float v_min, float v_max, float v_step, float v_default, float size,
                ColorSet circle_color, ColorSet wiper_color, ColorSet track_color, ColorSet tick_color,
                ImGuiKnobType type, char const *format, int tick_steps)
{
    ImGuiStyle &style = ImGui::GetStyle();
    float line_height = ImGui::GetTextLineHeight();
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 ItemSize = ImVec2(size, size + line_height * 2 + style.ItemInnerSpacing.y * 2 + 4);
    float radius = std::fmin(ItemSize.x, ItemSize.y) / 2.0f;
    bool is_no_limit = isnan(v_min) || isnan(v_max);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
    std::string ViewID = "###" + std::string(label) + "_KNOB_VIEW_CONTORL_";
    ImGui::BeginChild(ViewID.c_str(), ItemSize, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    auto textSize = CalcTextSize(label, label_end);
    if (textSize.x > 0)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddText(ImVec2(pos.x + ((ItemSize.x / 2) - (textSize.x / 2)), pos.y + style.ItemInnerSpacing.y), ImGui::GetColorU32(ImGuiCol_Text), label, label_end);
        center.y += line_height + 4;
    }

    ImGui::InvisibleButton(label, ImVec2(radius * 2, radius * 2 + line_height));

    float step = isnan(v_step) ? (v_max - v_min) / 200.f : v_step;
    bool value_changed = false;
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemHovered();
    if (is_hovered || (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (is_active && io.MouseDelta.y != 0.0f)
    {
        *p_value -= io.MouseDelta.y * step;
        if (!is_no_limit)
        {
            if (*p_value < v_min)
                *p_value = v_min;
            if (*p_value > v_max)
                *p_value = v_max;
        }
        value_changed = true;
    }

    if (is_hovered && !is_active && io.MouseWheel != 0)
    {
        *p_value += io.MouseWheel * step;
        if (!is_no_limit)
        {
            if (*p_value < v_min)
                *p_value = v_min;
            if (*p_value > v_max)
                *p_value = v_max;
        }
        value_changed = true;
    }

    if (is_active && ImGui::IsMouseDoubleClicked(0))
    {
        if (*p_value != v_default)
        {
            *p_value = v_default;
            value_changed = true;
        }
    }

    float angle_min = IM_PI * 0.75;
    float angle_max = IM_PI * 2.25;
    float t = (*p_value - v_min) / (v_max - v_min);
    if (is_no_limit)
    {
        angle_min = -IM_PI * 0.5;
        angle_max = IM_PI * 1.5;
        t = *p_value;
    }
    float angle = angle_min + (angle_max - angle_min) * t;

    switch (type)
    {
        case IMKNOB_TICK:
            draw_circle(center, 0.7, true, 32, radius, circle_color);
            draw_tick(center, radius, 0.4, 0.7, 0.1, angle, wiper_color);
        break;
        case IMKNOB_TICK_DOT:
            draw_circle(center, 0.85, true, 32, radius, circle_color);
            draw_dot(center, 0.6, 0.12, radius, angle, true, 12, wiper_color);
        break;
        case IMKNOB_TICK_WIPER:
            draw_circle(center, 0.6, true, 32, radius, circle_color);
            if (!is_no_limit)
            {
                draw_arc(center, 0.85, 0.25, radius, angle_min, angle_max, 16, 2, track_color);
                if (t > 0.01)
                    draw_arc(center, 0.85, 0.27, radius, angle_min, angle, 16, 2, wiper_color);
            }
            else
            {
                draw_circle(center, 0.75, false, 32, radius, circle_color);
            }
            draw_tick(center, radius, 0.4, 0.6, 0.1, angle, wiper_color);
        break;
        case IMKNOB_WIPER:
            draw_circle(center, 0.7, true, 32, radius, circle_color);
            if (!is_no_limit)
            {
                draw_arc(center, 0.8, 0.41, radius, angle_min, angle_max, 16, 2, track_color);
                if (t > 0.01)
                    draw_arc(center, 0.8, 0.43, radius, angle_min, angle, 16, 2, wiper_color);
            }
            else
            {
                draw_circle(center, 0.9, false, 32, radius, circle_color);
                draw_tick(center, radius, 0.75, 0.9, 0.1, angle, wiper_color);
            }
        break;
        case IMKNOB_WIPER_TICK:
            draw_circle(center, 0.6, true, 32, radius, circle_color);
            if (!is_no_limit)
            {
                draw_arc(center, 0.85, 0.41, radius, angle_min, angle_max, 16, 2, track_color);
            }
            else
            {
                draw_circle(center, 0.75, false, 32, radius, circle_color);
                draw_circle(center, 0.9, false, 32, radius, circle_color);
            }
            draw_tick(center, radius, 0.75, 0.9, 0.1, angle, wiper_color);
        break;
        case IMKNOB_WIPER_DOT:
            draw_circle(center, 0.6, true, 32, radius, circle_color);
            if (!is_no_limit)
            {
                draw_arc(center, 0.85, 0.41, radius, angle_min, angle_max, 16, 2, track_color);
            }
            else
            {
                draw_circle(center, 0.80, false, 32, radius, circle_color);
                draw_circle(center, 0.95, false, 32, radius, circle_color);
            }
            draw_dot(center, 0.85, 0.1, radius, angle, true, 12, wiper_color);
        break;
        case IMKNOB_WIPER_ONLY:
            if (!is_no_limit)
            {
                draw_arc(center, 0.8, 0.41, radius, angle_min, angle_max, 32, 2, track_color);
                if (t > 0.01)
                    draw_arc(center, 0.8, 0.43, radius, angle_min, angle, 16, 2, wiper_color);
            }
            else
            {
                draw_circle(center, 0.75, false, 32, radius, circle_color);
                draw_circle(center, 0.90, false, 32, radius, circle_color);
                draw_tick(center, radius, 0.75, 0.9, 0.1, angle, wiper_color);
            }
        break;
        case IMKNOB_STEPPED_TICK:
            for (int i = 0; i < tick_steps; i++)
            {
                float a = (float)i / (float)(tick_steps - 1);
                float angle = angle_min + (angle_max - angle_min) * a;
                draw_tick(center, radius, 0.7, 0.9, 0.04, angle, tick_color);
            }
            draw_circle(center, 0.6, true, 32, radius, circle_color);
            draw_tick(center, radius, 0.4, 0.7, 0.1, angle, wiper_color);
        break;
        case IMKNOB_STEPPED_DOT:
            for (int i = 0; i < tick_steps; i++)
            {
                float a = (float)i / (float)(tick_steps - 1);
                float angle = angle_min + (angle_max - angle_min) * a;
                draw_tick(center, radius, 0.7, 0.9, 0.04, angle, tick_color);
            }
            draw_circle(center, 0.6, true, 32, radius, circle_color);
            draw_dot(center, 0.4, 0.12, radius, angle, true, 12, wiper_color);
        break;
        case IMKNOB_SPACE:
            draw_circle(center, 0.3 - t * 0.1, true, 16, radius, circle_color);
            if (t > 0.01 && !is_no_limit)
            {
                draw_arc(center, 0.4, 0.15, radius, angle_min - 1.0, angle - 1.0, 16, 2, wiper_color);
                draw_arc(center, 0.6, 0.15, radius, angle_min + 1.0, angle + 1.0, 16, 2, wiper_color);
                draw_arc(center, 0.8, 0.15, radius, angle_min + 3.0, angle + 3.0, 16, 2, wiper_color);
            }
        break;
        default:
        break;
    }

    ImGui::PushItemWidth(size);
    std::string DragID = "###" + std::string(label) + "_KNOB_DRAG_CONTORL_";
    if (is_no_limit)
    {
        ImGui::DragFloat(DragID.c_str(), p_value, step, FLT_MIN, FLT_MAX, format);
    }
    else
    {
        ImGui::DragFloat(DragID.c_str(), p_value, step, v_min, v_max, format);
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
    return value_changed;
}

void ImGui::RoundProgressBar(float radius, float *p_value, float v_min, float v_max, ColorSet bar_color, ColorSet progress_color, ColorSet text_color)
{
    char label[20] = {0};
    ImVec4 base_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ColorSet back_color = {base_color, base_color, base_color};
    float percentage = (*p_value - v_min) / (v_max - v_min) * 100.f;
    snprintf(label, 20, "%02.1f%%", percentage);  // modify by dicky reduce warning
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
    ImGui::InvisibleButton("##round_progress_bar", ImVec2(radius * 2, radius * 2));
    float angle_min = IM_PI * 0.75;
    float angle_max = IM_PI * 2.25;
    float t = percentage / 100.0;
    angle_min = -IM_PI * 0.5;
    angle_max = IM_PI * 1.5;
    t = percentage / 100.f;
    float angle = angle_min + (angle_max - angle_min) * t;
    draw_circle(center, 0.95, true, 32, radius, bar_color);
    draw_circle(center, 0.80, true, 32, radius, back_color);
    for (float s = angle_min; s < angle; s += IM_PI / 72)
        draw_dot(center, 0.87, 0.1, radius, s, true, 12, progress_color);
    draw_dot(center, 0.87, 0.1, radius, angle, true, 12, progress_color);

    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const char* text_end = label + strlen(label);
    auto draw_size = ImGui::CalcTextSize(label);
    float font_scale = radius / draw_size.x;
    draw_size *= font_scale;
    draw_list->AddTextComplex(center - draw_size * ImVec2(0.5, 0.6), label, font_scale, IM_COL32(255, 255, 255, 255), 1.f, IM_COL32(255, 255, 255, 255), ImVec2(3, 3), ImGui::GetColorU32(bar_color.base));
}

// Splitter
bool ImGui::Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size, float delay)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 1.0, delay);
}

// Based on the code from: https://github.com/benoitjacquier/imgui
inline static bool ColorChooserInternal(ImVec4 *pColorOut,bool supportsAlpha,bool showSliders,ImGuiWindowFlags extra_flags=0,bool* pisAnyItemActive=NULL,float windowWidth = 180/*,bool isCombo = false*/)
{
    bool colorSelected = false;
    if (pisAnyItemActive) *pisAnyItemActive=false;
    //const bool isCombo = (extra_flags&ImGuiWindowFlags_ComboBox);

    ImVec4 color = pColorOut ? *pColorOut : ImVec4(0,0,0,1);
    if (!supportsAlpha) color.w=1.f;

    ImGuiContext& g = *GImGui;
    const float smallWidth = windowWidth/9.f;

    static const ImU32 black = ImGui::ColorConvertFloat4ToU32(ImVec4(0,0,0,1));
    static const ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1,1,1,1));
    static float hue, sat, val;

    ImGui::ColorConvertRGBtoHSV( color.x, color.y, color.z, hue, sat, val );


    ImGuiWindow* colorWindow = ImGui::GetCurrentWindow();

    const float quadSize = windowWidth - smallWidth - colorWindow->WindowPadding.x*2;
    //if (isCombo) ImGui::SetCursorPosX(ImGui::GetCursorPos().x+colorWindow->WindowPadding.x);
    // Hue Saturation Value
    if (ImGui::BeginChild("ValueSaturationQuad##ValueSaturationQuadColorChooser", ImVec2(quadSize, quadSize), false,extra_flags ))
    //ImGui::BeginGroup();
    {
        const int step = 5;
        ImVec2 pos = ImVec2(0, 0);
        ImGuiWindow* window = ImGui::GetCurrentWindow();

        ImVec4 c00(1, 1, 1, 1);
        ImVec4 c10(1, 1, 1, 1);
        ImVec4 c01(1, 1, 1, 1);
        ImVec4 c11(1, 1, 1, 1);
        for (int y = 0; y < step; y++) {
            for (int x = 0; x < step; x++) {
                float s0 = (float)x / (float)step;
                float s1 = (float)(x + 1) / (float)step;
                float v0 = 1.0 - (float)(y) / (float)step;
                float v1 = 1.0 - (float)(y + 1) / (float)step;


                ImGui::ColorConvertHSVtoRGB(hue, s0, v0, c00.x, c00.y, c00.z);
                ImGui::ColorConvertHSVtoRGB(hue, s1, v0, c10.x, c10.y, c10.z);
                ImGui::ColorConvertHSVtoRGB(hue, s0, v1, c01.x, c01.y, c01.z);
                ImGui::ColorConvertHSVtoRGB(hue, s1, v1, c11.x, c11.y, c11.z);

                window->DrawList->AddRectFilledMultiColor(window->Pos + pos, window->Pos + pos + ImVec2(quadSize / step, quadSize / step),
                                                        ImGui::ColorConvertFloat4ToU32(c00),
                                                        ImGui::ColorConvertFloat4ToU32(c10),
                                                        ImGui::ColorConvertFloat4ToU32(c11),
                                                        ImGui::ColorConvertFloat4ToU32(c01));

                pos.x += quadSize / step;
            }
            pos.x = 0;
            pos.y += quadSize / step;
        }

        window->DrawList->AddCircle(window->Pos + ImVec2(sat, 1-val)*quadSize, 4, val<0.5f?white:black, 4);

        const ImGuiID id = window->GetID("ValueSaturationQuad");
        ImRect bb(window->Pos, window->Pos + window->Size);
        bool hovered, held;
        /*bool pressed = */ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_NoKeyModifiers);///*false,*/ false);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (held)   {
            ImVec2 pos = g.IO.MousePos - window->Pos;
            sat = ImSaturate(pos.x / (float)quadSize);
            val = 1 - ImSaturate(pos.y / (float)quadSize);
            ImGui::ColorConvertHSVtoRGB(hue, sat, val, color.x, color.y, color.z);
            colorSelected = true;
        }

    }
    ImGui::EndChild();	// ValueSaturationQuad
    //ImGui::EndGroup();

    ImGui::SameLine();

    //if (isCombo) ImGui::SetCursorPosX(ImGui::GetCursorPos().x+colorWindow->WindowPadding.x+quadSize);

    //Vertical tint
    if (ImGui::BeginChild("Tint##TintColorChooser", ImVec2(smallWidth, quadSize), false,extra_flags))
    //ImGui::BeginGroup();
    {
        const int step = 8;
        const int width = (int)smallWidth;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 pos(0, 0);
        ImVec4 c0(1, 1, 1, 1);
        ImVec4 c1(1, 1, 1, 1);
        for (int y = 0; y < step; y++) {
            float tint0 = (float)(y) / (float)step;
            float tint1 = (float)(y + 1) / (float)step;
            ImGui::ColorConvertHSVtoRGB(tint0, 1.0, 1.0, c0.x, c0.y, c0.z);
            ImGui::ColorConvertHSVtoRGB(tint1, 1.0, 1.0, c1.x, c1.y, c1.z);

            window->DrawList->AddRectFilledMultiColor(window->Pos + pos, window->Pos + pos + ImVec2(width, quadSize / step),
                                                    ImGui::ColorConvertFloat4ToU32(c0),
                                                    ImGui::ColorConvertFloat4ToU32(c0),
                                                    ImGui::ColorConvertFloat4ToU32(c1),
                                                    ImGui::ColorConvertFloat4ToU32(c1));

            pos.y += quadSize / step;
        }

        window->DrawList->AddCircle(window->Pos + ImVec2(smallWidth*0.5f, hue*quadSize), 4, black, 4);
        //window->DrawList->AddLine(window->Pos + ImVec2(0, hue*quadSize), window->Pos + ImVec2(width, hue*quadSize), ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)));
        bool hovered, held;
        const ImGuiID id = window->GetID("Tint");
        ImRect bb(window->Pos, window->Pos + window->Size);
        /*bool pressed = */ImGui::ButtonBehavior(bb, id, &hovered, &held,ImGuiButtonFlags_NoKeyModifiers);// /*false,*/ false);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (held)
        {

            ImVec2 pos = g.IO.MousePos - window->Pos;
            hue = ImClamp( pos.y / (float)quadSize, 0.0f, 1.0f );
            ImGui::ColorConvertHSVtoRGB( hue, sat, val, color.x, color.y, color.z );
            colorSelected = true;
        }

    }
    ImGui::EndChild(); // "Tint"
    //ImGui::EndGroup();

    if (showSliders)
    {
        //Sliders
        //ImGui::PushItemHeight();
        //if (isCombo) ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x+colorWindow->WindowPadding.x,ImGui::GetCursorPos().y+colorWindow->WindowPadding.y+quadSize));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Sliders");
        static bool useHsvSliders = false;
        static const char* btnNames[2] = {"to HSV","to RGB"};
        const int index = useHsvSliders?1:0;
        ImGui::SameLine();
        if (ImGui::SmallButton(btnNames[index])) useHsvSliders=!useHsvSliders;

        ImGui::Separator();
        const ImVec2 sliderSize = /*isCombo ? ImVec2(-1,quadSize) : */ImVec2(-1,-1);
        if (ImGui::BeginChild("Sliders##SliderColorChooser", sliderSize, false,extra_flags))
        {
            {
                int r = ImSaturate( useHsvSliders ? hue : color.x )*255.f;
                int g = ImSaturate( useHsvSliders ? sat : color.y )*255.f;
                int b = ImSaturate( useHsvSliders ? val : color.z )*255.f;
                int a = ImSaturate( color.w )*255.f;

                static const char* names[2][3]={{"R","G","B"},{"H","S","V"}};
                bool sliderMoved = false;
                sliderMoved|= ImGui::SliderInt(names[index][0], &r, 0, 255);
                sliderMoved|= ImGui::SliderInt(names[index][1], &g, 0, 255);
                sliderMoved|= ImGui::SliderInt(names[index][2], &b, 0, 255);
                sliderMoved|= (supportsAlpha && ImGui::SliderInt("A", &a, 0, 255));
                if (sliderMoved)
                {
                    colorSelected = true;
                    color.x = (float)r/255.f;
                    color.y = (float)g/255.f;
                    color.z = (float)b/255.f;
                    if (useHsvSliders)  ImGui::ColorConvertHSVtoRGB(color.x,color.y,color.z,color.x,color.y,color.z);
                    if (supportsAlpha) color.w = (float)a/255.f;
                }
                //ColorConvertRGBtoHSV(s_color.x, s_color.y, s_color.z, tint, sat, val);*/
                if (pisAnyItemActive) *pisAnyItemActive|=sliderMoved;
            }
        }
        ImGui::EndChild();
    }

    if (colorSelected && pColorOut) *pColorOut = color;

    return colorSelected;
}

bool ImGui::ColorCombo(const char* label,ImVec4 *pColorOut,bool supportsAlpha,float width,bool closeWhenMouseLeavesIt)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float itemWidth = width>=0 ? width : ImGui::CalcItemWidth();
    const ImVec2 label_size = ImGui::CalcTextSize(label);
    const float color_quad_size = (g.FontSize + style.FramePadding.x);
    const float arrow_size = (g.FontSize + style.FramePadding.x * 2.0f);
    ImVec2 totalSize = ImVec2(label_size.x+color_quad_size+arrow_size, label_size.y) + style.FramePadding*2.0f;
    if (totalSize.x < itemWidth) totalSize.x = itemWidth;
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + totalSize);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max);// + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id)) return false;
    const float windowWidth = frame_bb.Max.x - frame_bb.Min.x - style.FramePadding.x;


    ImVec4 color = pColorOut ? *pColorOut : ImVec4(0,0,0,1);
    if (!supportsAlpha) color.w=1.f;

    const bool hovered = ItemHoverable(frame_bb, id, ImGuiItemFlags_None);

    const ImRect value_bb(frame_bb.Min, frame_bb.Max - ImVec2(arrow_size, 0.0f));
    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    RenderFrame(frame_bb.Min,ImVec2(frame_bb.Min.x+color_quad_size,frame_bb.Max.y), ImColor(style.Colors[ImGuiCol_Text]), true, style.FrameRounding);
    RenderFrame(ImVec2(frame_bb.Min.x+1,frame_bb.Min.y+1), ImVec2(frame_bb.Min.x+color_quad_size-1,frame_bb.Max.y-1),
                ImGui::ColorConvertFloat4ToU32(ImVec4(color.x,color.y,color.z,1.f)),
                true, style.FrameRounding);

    RenderFrame(ImVec2(frame_bb.Max.x-arrow_size, frame_bb.Min.y), frame_bb.Max, GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button), true, style.FrameRounding); // FIXME-ROUNDING
    RenderArrow(window->DrawList,ImVec2(frame_bb.Max.x-arrow_size, frame_bb.Min.y) + style.FramePadding,GetColorU32(ImGuiCol_Text), ImGuiDir_Down);

    RenderTextClipped(ImVec2(frame_bb.Min.x+color_quad_size,frame_bb.Min.y) + style.FramePadding, value_bb.Max, label, NULL, NULL);

    if (hovered)
    {
        SetHoveredID(id);
        if (g.IO.MouseClicked[0])
        {
            ClearActiveID();
            if (ImGui::IsPopupOpen(id,0))
            {
                ClosePopupToLevel(g.OpenPopupStack.Size - 1,true);
            }
            else
            {
                FocusWindow(window);
                ImGui::OpenPopup(label);
            }
        }
        static ImVec4 copiedColor(1,1,1,1);
        static const ImVec4* pCopiedColor = NULL;
        if (g.IO.MouseClicked[1]) { // right-click (copy color)
            copiedColor = color;
            pCopiedColor = &copiedColor;
            //fprintf(stderr,"Copied\n");
        }
        else if (g.IO.MouseClicked[2] && pCopiedColor && pColorOut) { // middle-click (paste color)
            pColorOut->x = pCopiedColor->x;
            pColorOut->y = pCopiedColor->y;
            pColorOut->z = pCopiedColor->z;
            if (supportsAlpha) pColorOut->w = pCopiedColor->w;
            color = *pColorOut;
            //fprintf(stderr,"Pasted\n");
        }
    }

    bool value_changed = false;
    if (ImGui::IsPopupOpen(id,0))
    {
        ImRect popup_rect(ImVec2(frame_bb.Min.x, frame_bb.Max.y), ImVec2(frame_bb.Max.x, frame_bb.Max.y));
        //popup_rect.Max.y = ImMin(popup_rect.Max.y, g.IO.DisplaySize.y - style.DisplaySafeAreaPadding.y); // Adhoc height limit for Combo. Ideally should be handled in Begin() along with other popups size, we want to have the possibility of moving the popup above as well.
        ImGui::SetNextWindowPos(popup_rect.Min);
        ImGui::SetNextWindowSize(ImVec2(windowWidth,-1));//popup_rect.GetSize());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,2));

        bool mustCloseCombo = false;
        const ImGuiWindowFlags flags =  0;//ImGuiWindowFlags_Modal;//ImGuiWindowFlags_ComboBox;  // ImGuiWindowFlags_ComboBox is no more available... what now ?
        if (ImGui::BeginPopup(label, flags))
        {
            bool comboItemActive = false;
            value_changed = ColorChooserInternal(pColorOut,supportsAlpha,false,flags,&comboItemActive,windowWidth/*,true*/);
            if (closeWhenMouseLeavesIt && !comboItemActive)
            {
                const float distance = g.FontSize*1.75f;//1.3334f;//24;
                //fprintf(stderr,"%1.f",distance);
                ImVec2 pos = ImGui::GetWindowPos();pos.x-=distance;pos.y-=distance;
                ImVec2 size = ImGui::GetWindowSize();
                size.x+=2.f*distance;
                size.y+=2.f*distance+windowWidth*8.f/9.f;   // problem: is seems that ImGuiWindowFlags_ComboBox does not return the full window height
                const ImVec2& mousePos = ImGui::GetIO().MousePos;
                if (mousePos.x<pos.x || mousePos.y<pos.y || mousePos.x>pos.x+size.x || mousePos.y>pos.y+size.y) {
                    mustCloseCombo = true;
                    //fprintf(stderr,"Leaving ColorCombo: pos(%1f,%1f) size(%1f,%1f)\n",pos.x,pos.y,size.x,size.y);
                }
            }
            ImGui::EndPopup();
        }
        if (mustCloseCombo && ImGui::IsPopupOpen(id,0)) {
            ClosePopupToLevel(g.OpenPopupStack.Size - 1,true);
        }
        ImGui::PopStyleVar(3);
    }
    return value_changed;
}

// Based on the code from: https://github.com/benoitjacquier/imgui
bool ImGui::ColorChooser(bool* open,ImVec4 *pColorOut,bool supportsAlpha)
{
    static bool lastOpen = false;
    static const ImVec2 windowSize(175,285);

    if (open && !*open) {lastOpen=false;return false;}
    if (open && *open && *open!=lastOpen) {
        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
        ImGui::SetNextWindowSize(windowSize);
        lastOpen=*open;
    }

    //ImGui::OpenPopup("Color Chooser##myColorChoserPrivate");
    bool colorSelected = false;

    ImGuiWindowFlags WindowFlags = 0;
    //WindowFlags |= ImGuiWindowFlags_NoTitleBar;
    WindowFlags |= ImGuiWindowFlags_NoResize;
    //WindowFlags |= ImGuiWindowFlags_NoMove;
    WindowFlags |= ImGuiWindowFlags_NoScrollbar;
    WindowFlags |= ImGuiWindowFlags_NoCollapse;
    WindowFlags |= ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,2));

    if (open) ImGui::SetNextWindowFocus();
    //if (ImGui::BeginPopupModal("Color Chooser##myColorChoserPrivate",open,WindowFlags))
    //if (ImGui::Begin("Color Chooser##myColorChoserPrivate",open,windowSize,-1.f,WindowFlags)) // Old API
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Color Chooser##myColorChoserPrivate",open,WindowFlags))
    {
        colorSelected = ColorChooserInternal(pColorOut,supportsAlpha,true);
        //ImGui::EndPopup();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    return colorSelected;
}

// ToggleButton
bool ImGui::ToggleButton(const char* str_id, bool* v)
{
    bool valueChange = false;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float height = ImGui::GetFrameHeight() * 0.8;
    float width = height * 1.6f;
    float radius = height * 0.50f;

    ImGui::InvisibleButton(str_id, ImVec2(width, height));
    if (ImGui::IsItemClicked())
    {
        *v = !*v;
        valueChange = true;
    }

    float t = *v ? 1.0f : 0.0f;

    ImGuiContext& g = *GImGui;
    float ANIM_SPEED = 0.08f;
    if (g.LastActiveId == g.CurrentWindow->GetID(str_id))
    {
        float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
        t = *v ? (t_anim) : (1.0f - t_anim);
    }

    ImU32 col_bg;
    if (ImGui::IsItemHovered())
        col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.78f, 0.78f, 0.78f, 1.0f), ImVec4(0.64f, 0.83f, 0.34f, 1.0f), t));
    else
        col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), ImVec4(0.56f, 0.83f, 0.26f, 1.0f), t));

    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));
    return valueChange;
}

bool ImGui::ToggleButton(const char *str_id, bool *v, const ImVec2 &size)
{
    bool valueChange = false;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton(str_id, size);
    if (ImGui::IsItemClicked())
    {
        *v = !*v;
        valueChange = true;
    }

    ImU32 col_tint = ImGui::GetColorU32((*v ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_Border)));
    ImU32 col_bg = ImGui::GetColorU32(ImGui::GetColorU32(ImGuiCol_WindowBg));
    if (ImGui::IsItemHovered())
    {
        col_bg = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    }
    if (ImGui::IsItemActive() || *v)
    {
        col_bg = ImGui::GetColorU32(ImGuiCol_Button);
    }

    draw_list->AddRectFilled(pos, pos + size, ImGui::GetColorU32(col_bg));

    auto textSize = ImGui::CalcTextSize(str_id);
    draw_list->AddText(ImVec2(pos.x + (size.x - textSize.x) / 2, pos.y), col_tint, str_id);

    return valueChange;
}

bool ImGui::BulletToggleButton(const char* label, bool* v, ImVec2 &pos, ImVec2 &size)
{
    bool valueChange = false;

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 old_pos = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(label, size);
    if (ImGui::IsItemClicked())
    {
        *v = !*v;
        valueChange = true;
    }
    pos += size / 2;
    if (*v)
    {
        draw_list->AddCircleFilled(pos, draw_list->_Data->FontSize * 0.20f, IM_COL32(255, 0, 0, 255), 8);
    }
    else
    {
        draw_list->AddCircleFilled(pos, draw_list->_Data->FontSize * 0.20f, IM_COL32(128, 128, 128, 255), 8);
    }
    ImGui::SetCursorScreenPos(old_pos);
    return valueChange;
}

// RotateButton
bool ImGui::RotateButton(const char* label, const ImVec2& size_arg, int rotate)
{
    int flags = ImGuiButtonFlags_None;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    // TODO::Dicky Calc item size need check rotate
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    if (g.LogEnabled)
        LogSetNextTextDecoration("[", "]");
    
    int rotation_start_index = draw_list->VtxBuffer.Size;
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);
    if (rotate != 90)
    {
        float rad = M_PI / 180 * (90 - rotate);
        ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX); // bounds
        auto& buf = draw_list->VtxBuffer;
        float s = sin(rad), c = cos(rad);
        for (int i = rotation_start_index; i < buf.Size; i++)
            l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);
        ImVec2 center = ImVec2((l.x + u.x) / 2, (l.y + u.y) / 2);
        center = ImRotate(center, s, c) - center;
        for (int i = rotation_start_index; i < buf.Size; i++)
            buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
    }
    return pressed;
}

// Input with int64
bool ImGui::InputInt64(const char* label, int64_t* v, int step, int step_fast, ImGuiInputTextFlags flags)
{
    // Hexadecimal input provided as a convenience but the flag name is awkward. Typically you'd use InputText() to parse your own data, if you want to handle prefixes.
    const char* format = (flags & ImGuiInputTextFlags_CharsHexadecimal) ? "%16X" : "%lld";
    return InputScalar(label, ImGuiDataType_S64, (void*)v, (void*)(step > 0 ? &step : NULL), (void*)(step_fast > 0 ? &step_fast : NULL), format, flags);
}

// CheckButton
bool ImGui::CheckButton(const char* label, bool* pvalue, ImVec4 CheckButtonColor, bool useSmallButton, float checkedStateAlphaMult) {
    bool rv = false;
    const bool tmp = pvalue ? *pvalue : false;
    if (tmp) ImGui::PushStyleColor(ImGuiCol_Button, CheckButtonColor);
    if (useSmallButton) {if (ImGui::SmallButton(label)) {if (pvalue) *pvalue=!(*pvalue);rv=true;}}
    else if (ImGui::Button(label)) {if (pvalue) *pvalue=!(*pvalue);rv=true;}
    if (tmp) ImGui::PopStyleColor();
    return rv;
}

// TODO::Jimmy add in
// RotateCheckButton
bool ImGui::RotateCheckButton(const char* label, bool* pvalue, ImVec4 CheckButtonColor, int rotate, ImVec2 CheckButtonSize, float checkedStateAlphaMult) {
    bool rv = false;
    const bool tmp = pvalue ? *pvalue : false;
    if (tmp) ImGui::PushStyleColor(ImGuiCol_Button, CheckButtonColor);
    if (ImGui::RotateButton(label, CheckButtonSize, rotate)) {if (pvalue) *pvalue=!(*pvalue);rv=true;}
    if (tmp) ImGui::PopStyleColor();
    return rv;
}
// TODO::Jimmy add out

// ColoredButtonV1: code posted by @ocornut here: https://github.com/ocornut/imgui/issues/4722
// [Button rounding depends on the FrameRounding Style property (but can be overridden with the last argument)]
bool ImGui::ColoredButton(const char* label, const ImVec2& size_arg, ImU32 text_color, ImU32 bg_color_1, ImU32 bg_color_2, float frame_rounding_override, float disabled_brightness)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    ImGuiButtonFlags flags = ImGuiButtonFlags_None;
    if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
    bool disabled = ImGui::IsItemDisabled();
    if (disabled) { ImGui::ColorBrightnessU32(bg_color_1, disabled_brightness); ImGui::ColorBrightnessU32(bg_color_2, disabled_brightness); ImGui::ColorBrightnessU32(text_color, disabled_brightness); }
    // Render
    const bool is_gradient = bg_color_1 != bg_color_2;
    if (held || hovered)
    {
        // Modify colors (ultimately this can be prebaked in the style)
        float h_increase = (held && hovered) ? 0.02f : 0.02f;
        float v_increase = (held && hovered) ? 0.20f : 0.07f;

        ImVec4 bg1f = ImGui::ColorConvertU32ToFloat4(bg_color_1);
        ImGui::ColorConvertRGBtoHSV(bg1f.x, bg1f.y, bg1f.z, bg1f.x, bg1f.y, bg1f.z);
        bg1f.x = ImMin(bg1f.x + h_increase, 1.0f);
        bg1f.z = ImMin(bg1f.z + v_increase, 1.0f);
        ImGui::ColorConvertHSVtoRGB(bg1f.x, bg1f.y, bg1f.z, bg1f.x, bg1f.y, bg1f.z);
        bg_color_1 = GetColorU32(bg1f);
        if (is_gradient)
        {
            ImVec4 bg2f = ImGui::ColorConvertU32ToFloat4(bg_color_2);
            ImGui::ColorConvertRGBtoHSV(bg2f.x, bg2f.y, bg2f.z, bg2f.x, bg2f.y, bg2f.z);
            bg2f.z = ImMin(bg2f.z + h_increase, 1.0f);
            bg2f.z = ImMin(bg2f.z + v_increase, 1.0f);
            ImGui::ColorConvertHSVtoRGB(bg2f.x, bg2f.y, bg2f.z, bg2f.x, bg2f.y, bg2f.z);
            bg_color_2 = ImGui::GetColorU32(bg2f);
        }
        else
        {
            bg_color_2 = bg_color_1;
        }
    }
    ImGui::RenderNavHighlight(bb, id);

#if 0
    // V1 : faster but prevents rounding
    window->DrawList->AddRectFilledMultiColor(bb.Min, bb.Max, bg_color_1, bg_color_1, bg_color_2, bg_color_2);
    if (g.Style.FrameBorderSize > 0.0f)
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, g.Style.FrameBorderSize);
#endif

    // V2
    const float frameRounding = frame_rounding_override>=0.f ? frame_rounding_override : g.Style.FrameRounding;
    int vert_start_idx = window->DrawList->VtxBuffer.Size;
    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_color_1, frameRounding);
    int vert_end_idx = window->DrawList->VtxBuffer.Size;
    if (is_gradient)
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(window->DrawList, vert_start_idx, vert_end_idx, bb.Min, bb.GetBL(), bg_color_1, bg_color_2);
    if (g.Style.FrameBorderSize > 0.0f)
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), frameRounding, 0, g.Style.FrameBorderSize);

    if (g.LogEnabled)
        ImGui::LogSetNextTextDecoration("[", "]");
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);
    ImGui::PopStyleColor();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

// ProgressBar
float ImGui::ProgressBar(const char *optionalPrefixText, float value, const float minValue, const float maxValue, 
                        const char *format, const ImVec2 &sizeOfBarWithoutTextInPixels, 
                        const ImVec4 &colorLeft, const ImVec4 &colorRight, const ImVec4 &colorBorder)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiWindow* window = GetCurrentWindow();
    ImVec2 item_pos = ImGui::GetCursorPos();
    if (value<minValue) value=minValue;
    else if (value>maxValue) value = maxValue;
    const float valueFraction = (maxValue==minValue) ? 1.0f : ((value-minValue)/(maxValue-minValue));
    const bool needsPercConversion = strstr(format,"%%")!=NULL;

    ImVec2 size = sizeOfBarWithoutTextInPixels;
    if (size.x<=0) size.x = ImGui::GetWindowWidth()*0.25f;
    if (size.y<=0) size.y = ImGui::GetTextLineHeightWithSpacing(); // or without

    const ImFontAtlas* fontAtlas = ImGui::GetIO().Fonts;

    if (optionalPrefixText && strlen(optionalPrefixText) > 0)
    {
        // Hide anything after a '##' string
        const char* text_display_end = FindRenderedTextEnd(optionalPrefixText);
        if (optionalPrefixText != text_display_end)
        {
            ImVec2 overlay_size = CalcTextSize(optionalPrefixText, text_display_end);
            window->DrawList->AddText(g.Font, g.FontSize, ImGui::GetCursorScreenPos(), GetColorU32(ImGuiCol_Text), optionalPrefixText, text_display_end);
            ImGui::SetCursorPos(item_pos + ImVec2(overlay_size.x + style.ItemInnerSpacing.x, 0));
        }
    }

    if (valueFraction>0)
    {
        ImGui::Image(fontAtlas->TexID,ImVec2(size.x*valueFraction,size.y), fontAtlas->TexUvWhitePixel,fontAtlas->TexUvWhitePixel,colorLeft,colorBorder);
    }
    if (valueFraction<1)
    {
        if (valueFraction>0) ImGui::SameLine(0,0);
        ImGui::Image(fontAtlas->TexID,ImVec2(size.x*(1.f-valueFraction),size.y), fontAtlas->TexUvWhitePixel,fontAtlas->TexUvWhitePixel,colorRight,colorBorder);
    }
    ImGui::SameLine();

    ImGui::Text(format,needsPercConversion ? (valueFraction*100.f+0.0001f) : value);
    return valueFraction;
}

//-------------------------------------------------------------------------
bool ImGui::SpinScaler(const char* label, ImGuiDataType data_type, void* data_ptr, const void* step, const void* step_fast, const char* format, ImGuiInputTextFlags flags)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;

	if (format == NULL)
		format = DataTypeGetInfo(data_type)->PrintFmt;

	char buf[64];
	DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, data_ptr, format);

	bool value_changed = false;
	if ((flags & (ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsScientific)) == 0)
		flags |= ImGuiInputTextFlags_CharsDecimal;
	flags |= ImGuiInputTextFlags_AutoSelectAll;
	flags |= ImGuiInputTextFlags_NoMarkEdited;  // We call MarkItemEdited() ourselve by comparing the actual data rather than the string.

	if (step != NULL)
	{
		const float button_size = GetFrameHeight();

		BeginGroup(); // The only purpose of the group here is to allow the caller to query item data e.g. IsItemActive()
		PushID(label);
		SetNextItemWidth(ImMax(1.0f, CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
		if (InputText("", buf, IM_ARRAYSIZE(buf), flags)) // PushId(label) + "" gives us the expected ID from outside point of view
			//value_changed = DataTypeApplyOpFromText(buf, g.InputTextState.InitialTextA.Data, data_type, data_ptr, format);
            value_changed = DataTypeApplyFromText(buf, data_type, data_ptr, format);

		// Step buttons
		const ImVec2 backup_frame_padding = style.FramePadding;
		style.FramePadding.x = style.FramePadding.y;
		ImGuiButtonFlags button_flags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
		if (flags & ImGuiInputTextFlags_ReadOnly)
			button_flags |= ImGuiItemFlags_Disabled;
		SameLine(0, style.ItemInnerSpacing.x);

        // start diffs
		float frame_height = GetFrameHeight();
		float arrow_size = std::floor(frame_height * .45f);
		float arrow_spacing = frame_height - 2.0f * arrow_size;

		BeginGroup();
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{g.Style.ItemSpacing.x, arrow_spacing});

		// save/change font size to draw arrow buttons correctly
		float org_font_size = GetDrawListSharedData()->FontSize;
		GetDrawListSharedData()->FontSize = arrow_size;

		if (ArrowButtonEx("+", ImGuiDir_Up, ImVec2(arrow_size, arrow_size), button_flags))
		{
			DataTypeApplyOp(data_type, '+', data_ptr, data_ptr, g.IO.KeyCtrl && step_fast ? step_fast : step);
			value_changed = true;
		}

		if (ArrowButtonEx("-", ImGuiDir_Down, ImVec2(arrow_size, arrow_size), button_flags))
		{
			DataTypeApplyOp(data_type, '-', data_ptr, data_ptr, g.IO.KeyCtrl && step_fast ? step_fast : step);
			value_changed = true;
		}

		// restore font size
		GetDrawListSharedData()->FontSize = org_font_size;

		PopStyleVar(1);
		EndGroup();
        // end diffs

		const char* label_end = FindRenderedTextEnd(label);
		if (label != label_end)
		{
			SameLine(0, style.ItemInnerSpacing.x);
			TextEx(label, label_end);
		}
		style.FramePadding = backup_frame_padding;

		PopID();
		EndGroup();
	}
	else
	{
		if (InputText(label, buf, IM_ARRAYSIZE(buf), flags))
			//value_changed = DataTypeApplyOpFromText(buf, g.InputTextState.InitialTextA.Data, data_type, data_ptr, format);
            value_changed = DataTypeApplyFromText(buf, data_type, data_ptr, format);
	}
	if (value_changed)
		MarkItemEdited(g.LastItemData.ID);

	return value_changed;
}

bool ImGui::SpinInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags)
{
	// Hexadecimal input provided as a convenience but the flag name is awkward. Typically you'd use InputText() to parse your own data, if you want to handle prefixes.
	const char* format = (flags & ImGuiInputTextFlags_CharsHexadecimal) ? "%08X" : "%d";
	return SpinScaler(label, ImGuiDataType_S32, (void*)v, (void*)(step>0 ? &step : NULL), (void*)(step_fast>0 ? &step_fast : NULL), format, flags);
}

bool ImGui::SpinFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CharsScientific;
	return SpinScaler(label, ImGuiDataType_Float, (void*)v, (void*)(step>0.0f ? &step : NULL), (void*)(step_fast>0.0f ? &step_fast : NULL), format, flags);
}

bool ImGui::SpinDouble(const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CharsScientific;
	return SpinScaler(label, ImGuiDataType_Double, (void*)v, (void*)(step>0.0 ? &step : NULL), (void*)(step_fast>0.0 ? &step_fast : NULL), format, flags);
}

// ImGui Date Chooser widget
bool ImGui::InputDate(const char* label, struct tm& date, const char* format, bool sunday_first, bool close_on_leave, const char* left_button, const char* right_button, const char* today_button)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    static ImGuiID last_open_combo_id = 0;
    static const int name_buf_size = 64;
    static char day_names[7][name_buf_size] = {""};
    static char month_names[12][name_buf_size] = {""};
    static int max_month_width_index = -1;

    // Initialize day/months names
    if (max_month_width_index == -1)
    {
        struct tm tmp = GetDateZero();
        for (int i=0;i<7;i++)
        {
            tmp.tm_wday = i;
            char* day_name = &day_names[i][0];
            strftime(day_name, name_buf_size, "%A", &tmp);
            // Warning: This part won't work for multibyte UTF-8 chars:
            // We want to keep the first letter only here, and we want it to be uppercase (because some locales provide it lowercase)
            if (strlen(day_name) == 0)
            {
                static const char fallbacks[7] = {'S','M','T','W','T','F','S'};
                day_name[0] = fallbacks[i];
                day_name[1] = '\0';
            }
            else 
            {
                day_name[0] = toupper(day_name[0]);
                day_name[1] = '\0';
            }
        }
        float max_month_width = 0;
        for (int i = 0; i < 12; i++)
        {
            tmp.tm_mon = i;
            char* month_name = &month_names[i][0];
            strftime(month_name, name_buf_size, "%B", &tmp);
            if (strlen(month_name) == 0)
            {
                static const char* fallbacks[12] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
                strcpy(month_name, fallbacks[i]);
            }
            else month_name[0] = toupper(month_name[0]);
            float mw = CalcTextSize(month_name).x;
            if (max_month_width < mw)
            {
                max_month_width = mw;
                max_month_width_index = i;
            }
        }
    }

    // Const values from inputs
    const char* arrow_left = left_button ? left_button : "<";
    const char* arrow_right = right_button ? right_button : ">";
    const char* today = today_button ? today_button : "o"; // "⦿";
    char currentText[128];
    strftime(currentText, sizeof(currentText), format, &date);

    // Const values for layout
    const float arrow_left_width  = CalcTextSize(arrow_left).x;
    const float arrow_right_width = CalcTextSize(arrow_right).x;
    const ImVec2 space = GetStyle().ItemSpacing;
    const float month_width = arrow_left_width + CalcTextSize(month_names[max_month_width_index]).x + arrow_right_width + 2*space.x;

    // Use a fixed popup size to avoid any layout stretching policy
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const float year_width = arrow_left_width + CalcTextSize("9990").x + arrow_right_width + 2*space.x;
    const float calendar_width = month_width + year_width + CalcTextSize(today).x + 3*space.x + 30.f;
    const float textbox_width = label_size.x + CalcTextSize(currentText).x;
    const float w = calendar_width > textbox_width ? calendar_width : textbox_width;
    const float h = 8 * label_size.y + 7 * space.y + 2 * style.FramePadding.y + 30;
    SetNextWindowSize(ImVec2(w, h));
    SetNextWindowSizeConstraints(ImVec2(w, h), ImVec2(w, h));

    bool value_changed = false;
    if (!BeginCombo(label, currentText, 0))
        return value_changed;

    const ImGuiID id = window->GetID(label);
    static struct tm d = GetDateZero();
    if (last_open_combo_id != id)
    {
        last_open_combo_id = id;
        d = date.tm_mday == 0 ? GetCurrentDate() : date;
        d.tm_mday = 1; // move at 1st day of the month: mandatory for the algo
        UpdateDate(d); // now d.tm_wday is correct
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    Spacing();

    static const ImVec4 transparent(1,1,1,0);
    PushStyleColor(ImGuiCol_Button,transparent);

    PushID(label);
    int cid = 0;

    // Month selector
    PushID(cid++);
    if (SmallButton(arrow_left))
    {
        d.tm_mon -= 1;
        UpdateDate(d);
    }
    SameLine();
    Text("%s", month_names[d.tm_mon]);
    SameLine(month_width + ImGui::GetStyle().WindowPadding.x);
    if (SmallButton(arrow_right))
    {
        d.tm_mon += 1;
        UpdateDate(d);
    }
    PopID();

    SameLine(ImGui::GetWindowWidth() - month_width - 2*ImGui::GetStyle().WindowPadding.x);

    // Year selector
    PushID(cid++);
    if (SmallButton(arrow_left))
    {
        d.tm_year -= 1;
        if(d.tm_year < 0)
            d.tm_year = 0;
        UpdateDate(d);
    }
    SameLine();
    Text("%d", 1900+d.tm_year);
    SameLine();
    if (SmallButton(arrow_right))
    {
        d.tm_year += 1;
        UpdateDate(d);
    }
    PopID();

    SameLine();

    // Today selector
    PushID(cid++);
    if (SmallButton(today))
    {
        d = GetCurrentDate();
        d.tm_mday = 1;
        UpdateDate(d);
    }
    PopID();

    Spacing();

    PushStyleColor(ImGuiCol_ButtonHovered,style.Colors[ImGuiCol_HeaderHovered]);
    PushStyleColor(ImGuiCol_ButtonActive,style.Colors[ImGuiCol_HeaderActive]);

    Separator();

    bool date_clicked = false;

    IM_ASSERT(d.tm_mday == 1);    // Otherwise the algo does not work
    // Display items
    char cur_day_str[4] = "";
    int days_in_month = DaysInMonth(d);
    struct tm cur_date = GetCurrentDate();
    PushID(cid++);
    for (int dwi = 0; dwi < 7; dwi++)
    {
        int dw = sunday_first ? dwi : (dwi+1)%7;
        bool sunday = dw == 0;

        BeginGroup(); // one column per day
        // -- Day name
        if (sunday) // style sundays in red
        {
            const ImVec4& tc = style.Colors[ImGuiCol_Text];
            const float l = (tc.x+tc.y+tc.z)*0.33334f;
            PushStyleColor(ImGuiCol_Text,ImVec4(l*2.f>1?1:l*2.f,l*.5f,l*.5f,tc.w));
        }
        Text(" %s",day_names[dw]);
        Spacing();
        // -- Day numbers
        int cur_day = dw-d.tm_wday; // tm_wday is in [0,6]. For this to work here d must point to the 1st day of the month: i.e.: d.tm_mday = 1;
        for (int row = 0; row < 6; row++)
        {
            int cday = cur_day + 7*row;
            if (cday >= 0 && cday<days_in_month)
            {
                PushID(cid+7*row+dwi);
                if (!sunday_first && (dw > 0 && d.tm_wday == 0 && row == 0)) // 1st == synday case
                    TextUnformatted(" ");
                if (cday <9 ) snprintf(cur_day_str, 4," %.1d",(unsigned char)cday+1);
                else snprintf(cur_day_str, 4,"%.2d",(unsigned char)cday+1);

                // Highligth input date and today
                bool is_today = cday+1 == cur_date.tm_mday && SameMonth(cur_date, d);
                bool is_input_date = cday+1 == date.tm_mday && SameMonth(date, d);
                if (is_input_date)
                    PushStyleColor(ImGuiCol_Button,style.Colors[ImGuiCol_HeaderActive]);
                else if (is_today)
                    PushStyleColor(ImGuiCol_Button,style.Colors[ImGuiCol_TableHeaderBg]);

                // Day number as a button
                if (SmallButton(cur_day_str))
                {
                    date_clicked = true;
                    struct tm date_out = d;
                    date_out.tm_mday = cday+1;
                    UpdateDate(date_out);
                    if (difftime(mktime(&date_out), mktime(&date)) != 0)
                    {
                        date = date_out;
                        value_changed = true;
                    }
                }
                if (is_today || is_input_date)
                    PopStyleColor();
                PopID();
            } else if ((sunday_first || !sunday) && cday<days_in_month)
                TextUnformatted(" ");
        }
        if (sunday) // sundays
            PopStyleColor();
        EndGroup();

        // horizontal space between columns
        if (dwi!=6)
            SameLine(ImGui::GetWindowWidth()-(6-dwi)*(ImGui::GetWindowWidth()/7.f));
    }
    PopID();

    PopID(); // ImGui::PushID(label)

    PopStyleColor(2);
    PopStyleColor();
    PopStyleVar();

    bool close_combo = date_clicked;
    if (close_on_leave && !close_combo) {
        const float distance = g.FontSize*1.75f;
        ImVec2 pos = GetWindowPos();
        pos.x -= distance; pos.y -= distance;
        ImVec2 size = GetWindowSize();
        size.x += 2.f*distance; size.y += 2.f*distance;
        const ImVec2& mouse_pos = GetIO().MousePos;
        if (mouse_pos.x<pos.x || mouse_pos.y<pos.y || mouse_pos.x>pos.x+size.x || mouse_pos.y>pos.y+size.y) {
            close_combo = true;
        }
    }
    if (close_combo)
        CloseCurrentPopup();

    EndCombo();

    return value_changed;
}

bool ImGui::InputTime(const char* label, struct tm& date, bool with_seconds, bool close_on_leave)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    bool value_changed = false;
    PushID(label);
    PushMultiItemsWidths(with_seconds ? 3 : 2, CalcItemWidth());
    {
        PushID(0);
        value_changed |= DragInt("", &date.tm_hour, 1, 0, 23, "%d", ImGuiSliderFlags_AlwaysClamp);
        PopID();
        PopItemWidth();
        SameLine(0, 0); Text(":"); SameLine(0, 0);
        PushID(1);
        value_changed |= DragInt("", &date.tm_min, 1, 0, 59, "%d", ImGuiSliderFlags_AlwaysClamp);
        PopID();
        PopItemWidth();

        if (with_seconds)
        {
            SameLine(0, 0); Text(":"); SameLine(0, 0);
            PushID(2);
            value_changed |= DragInt("", &date.tm_sec, 1, 0, 59, "%d", ImGuiSliderFlags_AlwaysClamp);
            PopID();
            PopItemWidth();
        }
    }
    PopID();
    return value_changed;
}

bool ImGui::InputDateTime(const char* label, struct tm& date, const char* dateformat, bool sunday_first, bool with_seconds, bool close_on_leave, const char* left_button, const char* right_button, const char* today_button)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const float min_time_width  = (with_seconds?3:2)*CalcTextSize("23").x + (with_seconds?2:1)*CalcTextSize(":").x;
    const float best_time_width = (with_seconds?0.35:0.25)*CalcItemWidth();
    const float time_width = best_time_width > min_time_width ? best_time_width : min_time_width;
    const float date_width = CalcItemWidth() - GetStyle().ItemSpacing.x - time_width - g.Style.FramePadding.x;

    bool value_changed = false;
    PushID(label);

    PushItemWidth(date_width);
    PushID(0);
    value_changed |= InputDate("", date, dateformat, sunday_first, close_on_leave, left_button, right_button, today_button);
    PopID();
    PopItemWidth();

    SameLine();

    PushItemWidth(time_width);
    PushID(1);
    value_changed |= InputTime("", date, with_seconds, close_on_leave);
    PopID();
    PopItemWidth();

    PopID();
    return value_changed;
}

//-------------------------------------------------------------------------
bool ImGui::BufferingBar(const char* label, float value, const ImVec2& size_arg, const float circle_pos, const ImU32& bg_col, const ImU32& fg_col)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = size_arg;
    size.x -= style.FramePadding.x * 2;
    
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;
    
    // Render
    const float circleStart = size.x * circle_pos;
    const float circleEnd = size.x;
    const float circleWidth = circleEnd - circleStart;
    
    window->DrawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart, bb.Max.y), bg_col);
    window->DrawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart*value, bb.Max.y), fg_col);
    
    const float t = g.Time;
    const float r = size.y / 2;
    const float speed = 1.5f;
    
    const float a = speed*0;
    const float b = speed*0.333f;
    const float c = speed*0.666f;
    
    const float o1 = (circleWidth+r) * (t+a - speed * (int)((t+a) / speed)) / speed;
    const float o2 = (circleWidth+r) * (t+b - speed * (int)((t+b) / speed)) / speed;
    const float o3 = (circleWidth+r) * (t+c - speed * (int)((t+c) / speed)) / speed;
    
    window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o1, bb.Min.y + r), r, bg_col);
    window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o2, bb.Min.y + r), r, bg_col);
    window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o3, bb.Min.y + r), r, bg_col);
    return true;
}

// Slider 2D and Slider 3D
static float Dist2(ImVec2 start, ImVec2 end)
{
    return (start[0] - end[0]) * (start[0] - end[0]) + (start[1] - end[1]) * (start[1] - end[1]);
}

static float DistToSegmentSqr(ImVec2 pos, ImVec2 start, ImVec2 end)
{
    float l2 = Dist2(start, end);
    if (l2 == 0) 
        return Dist2(pos, start);
    float t = ((pos[0] - start[0]) * (end[0] - start[0]) + (pos[1] - start[1]) * (end[1] - start[1])) / l2;
    t = ImMax(0.f, ImMin(1.f, t));
    return Dist2(pos, ImVec2(start[0] + t * (end[0] - start[0]), start[1] + t * (end[1] - start[1])));
}

static float DistOnSegmentSqr(ImVec2 pos, ImVec2 start, ImVec2 end)
{
    float dist = Dist2(start, pos);
    float to_dist = DistToSegmentSqr(pos, start, end);
    return dist - to_dist;
}

bool ImGui::SliderScalar2D(char const* pLabel, float* fValueX, float* fValueY, const float fMinX, const float fMaxX, const float fMinY, const float fMaxY, float const fZoom /*= 1.0f*/, bool bInput /*=true*/, bool bHandle /*=true*/)
{
    IM_ASSERT(fMinX < fMaxX);
    IM_ASSERT(fMinY < fMaxY);
    static bool drag_mouse = false;
    ImGuiContext& g = *GImGui;
    bool was_disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImGuiID const iID = ImGui::GetID(pLabel);
    ImVec2 const vSizeSubstract = bHandle ? ImGui::CalcTextSize(std::to_string(1.0f).c_str()) * 1.1f : ImVec2(8, 8);
    float const w = ImGui::CalcItemWidth();
    float const vSizeFull = (w - vSizeSubstract.x)*fZoom;
    ImVec2 const vSize(vSizeFull, vSizeFull);

    float const fHeightOffset = bInput ? ImGui::GetTextLineHeight() : 0;
    ImVec2 const vHeightOffset(0.0f, fHeightOffset);
    ImVec2 const FrameSize = ImVec2(w, w) + vHeightOffset * 4;
    ImGui::BeginChild(iID, FrameSize, false, ImGuiWindowFlags_NoMove);
    ImVec2 vPos = ImGui::GetCursorScreenPos() + ImVec2(0, 4);
    ImRect oRect(vPos + vHeightOffset, vPos + vSize + vHeightOffset);

    const char* label_end = ImGui::FindRenderedTextEnd(pLabel);
    ImGui::TextEx(pLabel, label_end);
    //ImGui::TextUnformatted(pLabel);
    ImGui::Spacing();
    ImGui::PushID(iID);

    ImU32 const uFrameCol = ImGui::GetColorU32(ImGuiCol_FrameBg);

    ImVec2 const vOriginPos = ImGui::GetCursorScreenPos();
    ImGui::RenderFrame(oRect.Min, oRect.Max, uFrameCol, false, 0.0f);

    float const fDeltaX = fMaxX - fMinX;
    float const fDeltaY = fMaxY - fMinY;

    bool bModified = false;
    ImVec2 const vSecurity(15.0f, 15.0f);
    if (!was_disabled && ImGui::IsMouseHoveringRect(oRect.Min - vSecurity, oRect.Max + vSecurity) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImVec2 const vCursorPos = ImGui::GetMousePos() - oRect.Min;

        *fValueX = vCursorPos.x/(oRect.Max.x - oRect.Min.x)*fDeltaX + fMinX;
        *fValueY = fDeltaY - vCursorPos.y/(oRect.Max.y - oRect.Min.y)*fDeltaY + fMinY;

        bModified = true;
        drag_mouse = true;
    }

    *fValueX = ImMin(ImMax(*fValueX, fMinX), fMaxX);
    *fValueY = ImMin(ImMax(*fValueY, fMinY), fMaxY);

    float const fScaleX = (*fValueX - fMinX)/fDeltaX;
    float const fScaleY = 1.0f - (*fValueY - fMinY)/fDeltaY;

    constexpr float fCursorOff = 10.0f;
    float const fXLimit = fCursorOff/oRect.GetWidth();
    float const fYLimit = fCursorOff/oRect.GetHeight();

    ImVec2 const vCursorPos((oRect.Max.x - oRect.Min.x)*fScaleX + oRect.Min.x, (oRect.Max.y - oRect.Min.y)*fScaleY + oRect.Min.y);

    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    ImVec4 const vBlue	(  70.0f/255.0f, 102.0f/255.0f, 230.0f/255.0f, 1.0f ); // TODO: choose from style
    ImVec4 const vOrange( 255.0f/255.0f, 128.0f/255.0f,  64.0f/255.0f, 1.0f ); // TODO: choose from style

    ImS32 const uBlue	= ImGui::GetColorU32(vBlue);
    ImS32 const uOrange	= ImGui::GetColorU32(vOrange);

    constexpr float fBorderThickness	= 2.0f;
    constexpr float fLineThickness		= 3.0f;
    constexpr float fHandleRadius		= 7.0f;
    constexpr float fHandleOffsetCoef	= 2.0f;

    // Cursor
    pDrawList->AddCircleFilled(vCursorPos, 5.0f, uBlue, 16);

    // Vertical Line
    if (fScaleY > 2.0f*fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oRect.Min.y + fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y - fCursorOff), uOrange, fLineThickness);
    if (fScaleY < 1.0f - 2.0f*	fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oRect.Max.y - fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y + fCursorOff), uOrange, fLineThickness);

    // Horizontal Line
    if (fScaleX > 2.0f*fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x + fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x - fCursorOff, vCursorPos.y), uOrange, fLineThickness);
    if (fScaleX < 1.0f - 2.0f*fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x - fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x + fCursorOff, vCursorPos.y), uOrange, fLineThickness);

    ImVec2 vXSize = {0, 0};
    ImVec2 vYSize = {0, 0};
    if (bHandle && !was_disabled)
    {
        char pBufferX[16];
        char pBufferY[16];
        ImFormatString(pBufferX, IM_ARRAYSIZE(pBufferX), "%.5f", *(float const*)fValueX);
        ImFormatString(pBufferY, IM_ARRAYSIZE(pBufferY), "%.5f", *(float const*)fValueY);

        ImU32 const uTextCol = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

        ImGui::SetWindowFontScale(0.75f);

        vXSize = ImGui::CalcTextSize(pBufferX);
        vYSize = ImGui::CalcTextSize(pBufferY);

        ImVec2 const vHandlePosX = ImVec2(vCursorPos.x, oRect.Max.y + vYSize.x*0.5f);
        ImVec2 const vHandlePosY = ImVec2(oRect.Max.x + fHandleOffsetCoef * fCursorOff + vYSize.x, vCursorPos.y);

        if (ImGui::IsMouseHoveringRect(vHandlePosX - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosX + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ImVec2 const vCursorPos = ImGui::GetMousePos() - oRect.Min;

            *fValueX = vCursorPos.x/(oRect.Max.x - oRect.Min.x)*fDeltaX + fMinX;

            bModified = true;
            drag_mouse = true;
        }
        else if (ImGui::IsMouseHoveringRect(vHandlePosY - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosY + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ImVec2 const vCursorPos = ImGui::GetMousePos() - oRect.Min;

            *fValueY = fDeltaY - vCursorPos.y/(oRect.Max.y - oRect.Min.y)*fDeltaY + fMinY;

            bModified = true;
            drag_mouse = true;
        }

        pDrawList->AddText(
            ImVec2(ImMin(ImMax(vCursorPos.x - vXSize.x*0.5f, oRect.Min.x), oRect.Min.x + vSize.x - vXSize.x),
                            oRect.Max.y + fCursorOff),
            uTextCol,
            pBufferX);
        pDrawList->AddText(
            ImVec2(oRect.Max.x + fCursorOff, ImMin(ImMax(vCursorPos.y - vYSize.y*0.5f, oRect.Min.y),
                    oRect.Min.y + vSize.y - vYSize.y)),
            uTextCol,
            pBufferY);
        ImGui::SetWindowFontScale(1.0f);
    }

    // Borders::Right
    pDrawList->AddCircleFilled(ImVec2(oRect.Max.x, vCursorPos.y), 2.0f, uOrange, 3);
    // Handle Right::Y
    if (bHandle)
        pDrawList->AddNgonFilled(ImVec2(oRect.Max.x + fHandleOffsetCoef*fCursorOff + vYSize.x, vCursorPos.y), fHandleRadius, uBlue, 4);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(oRect.Max.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Max.y), ImVec2(oRect.Max.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Top
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oRect.Min.y), 2.0f, uOrange, 3);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(vCursorPos.x - fCursorOff, oRect.Min.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(vCursorPos.x + fCursorOff, oRect.Min.y), uBlue, fBorderThickness);
    // Borders::Left
    pDrawList->AddCircleFilled(ImVec2(oRect.Min.x, vCursorPos.y), 2.0f, uOrange, 3);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Min.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(oRect.Min.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Bottom
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oRect.Max.y), 2.0f, uOrange, 3);
    // Handle Bottom::X
    if (bHandle)
        pDrawList->AddNgonFilled(ImVec2(vCursorPos.x, oRect.Max.y + vXSize.y*2.0f), fHandleRadius, uBlue, 4);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(vCursorPos.x - fCursorOff, oRect.Max.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Max.y), ImVec2(vCursorPos.x + fCursorOff, oRect.Max.y), uBlue, fBorderThickness);

    ImGui::PopID();

    if (bInput)
    {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Dummy(vHeightOffset);
        ImGui::Dummy(vSize);

        char pBufferID[64];
        ImFormatString(pBufferID, IM_ARRAYSIZE(pBufferID), "Values##%d", *(ImS32 const*)&iID);
        float const fSpeedX = fDeltaX/128.0f;
        float const fSpeedY = fDeltaY/128.0f;

        char pBufferXID[64];
        ImFormatString(pBufferXID, IM_ARRAYSIZE(pBufferXID), "X##%d", *(ImS32 const*)&iID);
        char pBufferYID[64];
        ImFormatString(pBufferYID, IM_ARRAYSIZE(pBufferYID), "Y##%d", *(ImS32 const*)&iID);

        bModified |= ImGui::DragScalar(pBufferXID, ImGuiDataType_Float, fValueX, fSpeedX, &fMinX, &fMaxX);
        bModified |= ImGui::DragScalar(pBufferYID, ImGuiDataType_Float, fValueY, fSpeedY, &fMinY, &fMaxY);
    }

    if (drag_mouse)
        ImGui::SetNextFrameWantCaptureMouse();

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag_mouse)
    {
        drag_mouse = false;
        ImGui::SetNextFrameWantCaptureMouse(false);
    }

    ImGui::EndChild();
    return bModified;
}

bool ImGui::SliderScalar3D(char const* pLabel, float* pValueX, float* pValueY, float* pValueZ, float const fMinX, float const fMaxX, float const fMinY, float const fMaxY, float const fMinZ, float const fMaxZ, float const fScale /*= 1.0f*/)
{
    IM_ASSERT(fMinX < fMaxX);
    IM_ASSERT(fMinY < fMaxY);
    IM_ASSERT(fMinZ < fMaxZ);

    ImGuiID const iID = ImGui::GetID(pLabel);
    static bool drag_mouse = false;
    ImVec2 const vSizeSubstract = ImGui::CalcTextSize(std::to_string(1.0f).c_str()) * 1.1f;
    float const w = ImGui::CalcItemWidth();
    float const vSizeFull = w;
    float const fMinSize = (vSizeFull - vSizeSubstract.x*0.5f)*fScale*0.75f;
    ImVec2 const vSize(fMinSize, fMinSize);

    float const fHeightOffset = ImGui::GetTextLineHeight();
    ImVec2 const vHeightOffset(0.0f, fHeightOffset);
    ImVec2 const FrameSize = ImVec2(w, w) + vHeightOffset * 4;
    ImGui::BeginChild(iID, FrameSize, false, ImGuiWindowFlags_NoMove);
    ImVec2 vPos = ImGui::GetCursorScreenPos();
    ImRect oRect(vPos + vHeightOffset, vPos + vSize + vHeightOffset);

    ImGui::TextUnformatted(pLabel);
    ImGui::Spacing();
    ImGui::PushID(iID);

    ImU32 const uFrameCol	= ImGui::GetColorU32(ImGuiCol_FrameBg) | 0xFF000000;
    ImU32 const uFrameCol2	= ImGui::GetColorU32(ImGuiCol_FrameBgActive);

    float& fX = *pValueX;
    float& fY = *pValueY;
    float& fZ = *pValueZ;

    float const fDeltaX = fMaxX - fMinX;
    float const fDeltaY = fMaxY - fMinY;
    float const fDeltaZ = fMaxZ - fMinZ;

    ImVec2 const vOriginPos = ImGui::GetCursorScreenPos();

    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    float const fX3 = vSize.x/3.0f;
    float const fY3 = vSize.y/3.0f;

    ImVec2 const vStart = oRect.Min;

    ImVec2 aPositions[] = {
        ImVec2(vStart.x,			vStart.y + fX3),
        ImVec2(vStart.x,			vStart.y + vSize.y),
        ImVec2(vStart.x + 2.0f*fX3,	vStart.y + vSize.y),
        ImVec2(vStart.x + vSize.x,	vStart.y + 2.0f*fY3),
        ImVec2(vStart.x + vSize.x,	vStart.y),
        ImVec2(vStart.x + fX3,		vStart.y)
    };

    pDrawList->AddPolyline(&aPositions[0], 6, uFrameCol2, true, 1.0f);

    // Cube Shape
    pDrawList->AddLine(
        oRect.Min + ImVec2(0.0f, vSize.y),
        oRect.Min + ImVec2(fX3, 2.0f*fY3), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(fX3, 2.0f*fY3),
        oRect.Min + ImVec2(vSize.x, 2.0f*fY3), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(fX3, 0.0f),
        oRect.Min + ImVec2(fX3, 2.0f*fY3), uFrameCol2, 1.0f);

    ImGui::PopID();

    constexpr float fDragZOffsetX = 40.0f;

    ImRect oZDragRect(ImVec2(vStart.x + 2.0f*fX3 + fDragZOffsetX, vStart.y + 2.0f*fY3), ImVec2(vStart.x + vSize.x + fDragZOffsetX, vStart.y + vSize.y));

    ImVec2 const vMousePos = ImGui::GetMousePos();
    ImVec2 const vSecurity(15.0f, 15.0f);
    ImVec2 const vDragStart	(oZDragRect.Min.x, oZDragRect.Max.y);
    ImVec2 const vDragEnd	(oZDragRect.Max.x, oZDragRect.Min.y);
    bool bModified = false;
    if (ImGui::IsMouseHoveringRect(oZDragRect.Min - vSecurity, oZDragRect.Max + vSecurity) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (DistToSegmentSqr(vMousePos, vDragStart, vDragEnd) < 100.0f) // 100 is arbitrary threshold
        {
            float const fMaxDist	= std::sqrt(Dist2(vDragStart, vDragEnd));
            float const fDist		= ImMax(ImMin(std::sqrt(DistOnSegmentSqr(vMousePos, vDragStart, vDragEnd))/fMaxDist, 1.0f), 0.0f);

            fZ = fDist*fDeltaZ*fDist + fMinZ;

            bModified = true;
            drag_mouse = true;
        }
    }

    float const fScaleZ = (fZ - fMinZ)/fDeltaZ;

    ImVec2 const vRectStart	(vStart.x, vStart.y + fX3);
    ImVec2 const vRectEnd	(vStart.x + fX3, vStart.y);
    ImRect const oXYDrag((vRectEnd - vRectStart)*fScaleZ + vRectStart,
                        (vRectEnd - vRectStart)*fScaleZ + vRectStart + ImVec2(2.0f*fX3, 2.0f*fY3));
    if (ImGui::IsMouseHoveringRect(oXYDrag.Min - vSecurity, oXYDrag.Max + vSecurity) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImVec2 const vLocalPos = ImGui::GetMousePos() - oXYDrag.Min;

        fX = vLocalPos.x/(oXYDrag.Max.x - oXYDrag.Min.x)*fDeltaX + fMinX;
        fY = fDeltaY - vLocalPos.y/(oXYDrag.Max.y - oXYDrag.Min.y)*fDeltaY + fMinY;

        bModified = true;
        drag_mouse = true;
    }

    fX = ImMin(ImMax(fX, fMinX), fMaxX);
    fY = ImMin(ImMax(fY, fMinY), fMaxY);
    fZ = ImMin(ImMax(fZ, fMinZ), fMaxZ);

    float const fScaleX = (fX - fMinX)/fDeltaX;
    float const fScaleY = 1.0f - (fY - fMinY)/fDeltaY;

    ImVec4 const vBlue	(  70.0f/255.0f, 102.0f/255.0f, 230.0f/255.0f, 1.0f );
    ImVec4 const vOrange( 255.0f/255.0f, 128.0f/255.0f,  64.0f/255.0f, 1.0f );

    ImS32 const uBlue	= ImGui::GetColorU32(vBlue);
    ImS32 const uOrange	= ImGui::GetColorU32(vOrange);

    constexpr float fBorderThickness	= 2.0f; // TODO: move as Style
    constexpr float fLineThickness		= 3.0f; // TODO: move as Style
    constexpr float fHandleRadius		= 7.0f; // TODO: move as Style
    constexpr float fHandleOffsetCoef	= 2.0f; // TODO: move as Style

    pDrawList->AddLine(
        vDragStart,
        vDragEnd, uFrameCol2, 1.0f);
    pDrawList->AddRectFilled(
        oXYDrag.Min, oXYDrag.Max, uFrameCol);

    constexpr float fCursorOff = 10.0f;
    float const fXLimit = fCursorOff/oXYDrag.GetWidth();
    float const fYLimit = fCursorOff/oXYDrag.GetHeight();
    float const fZLimit = fCursorOff/oXYDrag.GetWidth();

    char pBufferX[16];
    char pBufferY[16];
    char pBufferZ[16];
    ImFormatString(pBufferX, IM_ARRAYSIZE(pBufferX), "%.5f", *(float const*)&fX);
    ImFormatString(pBufferY, IM_ARRAYSIZE(pBufferY), "%.5f", *(float const*)&fY);
    ImFormatString(pBufferZ, IM_ARRAYSIZE(pBufferZ), "%.5f", *(float const*)&fZ);

    ImU32 const uTextCol = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

    ImVec2 const vCursorPos((oXYDrag.Max.x - oXYDrag.Min.x)*fScaleX + oXYDrag.Min.x, (oXYDrag.Max.y - oXYDrag.Min.y)*fScaleY + oXYDrag.Min.y);

    ImGui::SetWindowFontScale(0.75f);

    ImVec2 const vXSize = ImGui::CalcTextSize(pBufferX);
    ImVec2 const vYSize = ImGui::CalcTextSize(pBufferY);
    ImVec2 const vZSize = ImGui::CalcTextSize(pBufferZ);

    ImVec2 const vTextSlideXMin	= oRect.Min + ImVec2(0.0f, vSize.y);
    ImVec2 const vTextSlideXMax	= oRect.Min + ImVec2(2.0f*fX3, vSize.y);
    ImVec2 const vXTextPos((vTextSlideXMax - vTextSlideXMin)*fScaleX + vTextSlideXMin);

    ImVec2 const vTextSlideYMin	= oRect.Min + ImVec2(vSize.x, 2.0f*fY3);
    ImVec2 const vTextSlideYMax	= oRect.Min + ImVec2(vSize.x, 0.0f);
    ImVec2 const vYTextPos((vTextSlideYMax - vTextSlideYMin)*(1.0f - fScaleY) + vTextSlideYMin);

    ImVec2 const vTextSlideZMin = vDragStart + ImVec2(fCursorOff, fCursorOff);
    ImVec2 const vTextSlideZMax = vDragEnd   + ImVec2(fCursorOff, fCursorOff);
    ImVec2 const vZTextPos((vTextSlideZMax - vTextSlideZMin)*fScaleZ + vTextSlideZMin);

    ImVec2 const vHandlePosX = vXTextPos + ImVec2(0.0f, vXSize.y + fHandleOffsetCoef*fCursorOff);
    ImVec2 const vHandlePosY = vYTextPos + ImVec2(vYSize.x + (fHandleOffsetCoef + 1.0f)*fCursorOff, 0.0f);

    if (ImGui::IsMouseHoveringRect(vHandlePosX - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosX + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float const fCursorPosX = ImGui::GetMousePos().x - vStart.x;

        fX = fDeltaX*fCursorPosX/(2.0f*fX3) + fMinX;

        bModified = true;
        drag_mouse = true;
    }
    else if (ImGui::IsMouseHoveringRect(vHandlePosY - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosY + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float const fCursorPosY = ImGui::GetMousePos().y - vStart.y;

        fY = fDeltaY*(1.0f - fCursorPosY/(2.0f*fY3)) + fMinY;

        bModified = true;
        drag_mouse = true;
    }

    if (drag_mouse)
        ImGui::SetNextFrameWantCaptureMouse();

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag_mouse)
    {
        drag_mouse = false;
        ImGui::SetNextFrameWantCaptureMouse(false);
    }

    pDrawList->AddText(
        ImVec2(ImMin(ImMax(vXTextPos.x - vXSize.x*0.5f, vTextSlideXMin.x), vTextSlideXMax.x - vXSize.x),
                        vXTextPos.y + fCursorOff),
        uTextCol,
        pBufferX);
    pDrawList->AddText(
        ImVec2(vYTextPos.x + fCursorOff,
                ImMin(ImMax(vYTextPos.y - vYSize.y*0.5f, vTextSlideYMax.y), vTextSlideYMin.y - vYSize.y)),
        uTextCol,
        pBufferY);
    pDrawList->AddText(
        vZTextPos,
        uTextCol,
        pBufferZ);
    ImGui::SetWindowFontScale(1.0f);

    // Handles
    pDrawList->AddNgonFilled(vHandlePosX, fHandleRadius, uBlue, 4);
    pDrawList->AddNgonFilled(vHandlePosY, fHandleRadius, uBlue, 4);

    // Vertical Line
    if (fScaleY > 2.0f*fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oXYDrag.Min.y + fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y - fCursorOff), uOrange, fLineThickness);
    if (fScaleY < 1.0f - 2.0f*	fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oXYDrag.Max.y - fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y + fCursorOff), uOrange, fLineThickness);

    // Horizontal Line
    if (fScaleX > 2.0f*fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x + fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x - fCursorOff, vCursorPos.y), uOrange, fLineThickness);
    if (fScaleX < 1.0f - 2.0f*fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x - fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x + fCursorOff, vCursorPos.y), uOrange, fLineThickness);

    // Line To Text
    // X
    if (fScaleZ > 2.0f*fZLimit)
        pDrawList->AddLine(
            ImVec2(vCursorPos.x - 0.5f*fCursorOff, oXYDrag.Max.y + 0.5f*fCursorOff),
            ImVec2(vXTextPos.x + 0.5f*fCursorOff, vXTextPos.y - 0.5f*fCursorOff), uOrange, fLineThickness
        );
    else
        pDrawList->AddLine(
            ImVec2(vCursorPos.x, oXYDrag.Max.y),
            ImVec2(vXTextPos.x, vXTextPos.y), uOrange, 1.0f
        );
    pDrawList->AddCircleFilled(vXTextPos, 2.0f, uOrange, 3);
    // Y
    if (fScaleZ < 1.0f - 2.0f*fZLimit)
        pDrawList->AddLine(
            ImVec2(oXYDrag.Max.x + 0.5f*fCursorOff, vCursorPos.y - 0.5f*fCursorOff),
            ImVec2(vYTextPos.x - 0.5f*fCursorOff, vYTextPos.y + 0.5f*fCursorOff), uOrange, fLineThickness
        );
    else
        pDrawList->AddLine(
            ImVec2(oXYDrag.Max.x, vCursorPos.y),
            ImVec2(vYTextPos.x, vYTextPos.y), uOrange, 1.0f
        );
    pDrawList->AddCircleFilled(vYTextPos, 2.0f, uOrange, 3);

    // Borders::Right
    pDrawList->AddCircleFilled(ImVec2(oXYDrag.Max.x, vCursorPos.y), 2.0f, uOrange, 3);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Min.y), ImVec2(oXYDrag.Max.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Max.y), ImVec2(oXYDrag.Max.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Top
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oXYDrag.Min.y), 2.0f, uOrange, 3);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Min.y), ImVec2(vCursorPos.x - fCursorOff, oXYDrag.Min.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Min.y), ImVec2(vCursorPos.x + fCursorOff, oXYDrag.Min.y), uBlue, fBorderThickness);
    // Borders::Left
    pDrawList->AddCircleFilled(ImVec2(oXYDrag.Min.x, vCursorPos.y), 2.0f, uOrange, 3);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Min.y), ImVec2(oXYDrag.Min.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Max.y), ImVec2(oXYDrag.Min.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Bottom
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oXYDrag.Max.y), 2.0f, uOrange, 3);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Max.y), ImVec2(vCursorPos.x - fCursorOff, oXYDrag.Max.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Max.y), ImVec2(vCursorPos.x + fCursorOff, oXYDrag.Max.y), uBlue, fBorderThickness);

    pDrawList->AddLine(
        oRect.Min + ImVec2(0.0f, fY3),
        oRect.Min + ImVec2(2.0f*fX3, fY3), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(2.0f*fX3, fY3),
        oRect.Min + ImVec2(vSize.x, 0.0f), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(2.0f*fX3, fY3),
        oRect.Min + ImVec2(2.0f*fX3, vSize.y), uFrameCol2, 1.0f);

    // Cursor
    pDrawList->AddCircleFilled(vCursorPos, 5.0f, uBlue, 16);
    // CursorZ
    pDrawList->AddNgonFilled((vDragEnd - vDragStart)*fScaleZ + vDragStart, fHandleRadius, uBlue, 4);

    ImGui::Dummy(vHeightOffset);
    ImGui::Dummy(vHeightOffset*1.25f);
    ImGui::Dummy(vSize);

    char pBufferID[64];
    ImFormatString(pBufferID, IM_ARRAYSIZE(pBufferID), "Values##%d", *(ImS32 const*)&iID);

    float const fMoveDeltaX = fDeltaX/128.0f; // Arbitrary
    float const fMoveDeltaY = fDeltaY/128.0f; // Arbitrary
    float const fMoveDeltaZ = fDeltaZ/128.0f; // Arbitrary

    char pBufferXID[64];
    ImFormatString(pBufferXID, IM_ARRAYSIZE(pBufferXID), "X##%d", *(ImS32 const*)&iID);
    char pBufferYID[64];
    ImFormatString(pBufferYID, IM_ARRAYSIZE(pBufferYID), "Y##%d", *(ImS32 const*)&iID);
    char pBufferZID[64];
    ImFormatString(pBufferZID, IM_ARRAYSIZE(pBufferZID), "Z##%d", *(ImS32 const*)&iID);

    bModified |= ImGui::DragScalar(pBufferXID, ImGuiDataType_Float, &fX, fMoveDeltaX, &fMinX, &fMaxX);
    bModified |= ImGui::DragScalar(pBufferYID, ImGuiDataType_Float, &fY, fMoveDeltaY, &fMinY, &fMaxY);
    bModified |= ImGui::DragScalar(pBufferZID, ImGuiDataType_Float, &fZ, fMoveDeltaZ, &fMinZ, &fMaxZ);

    ImGui::EndChild();
    return bModified;
}

// drag for timestamp
bool ImGui::DragTimeMS(const char* label, float* p_data, float v_speed, float p_min, float p_max, const int decimals, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
        return false;

    const bool hovered = ItemHoverable(frame_bb, id, ImGuiItemFlags_None);
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        // Tabbing or CTRL-clicking on Drag turns it into an InputText
        const bool input_requested_by_tabbing = temp_input_allowed && (g.NavActivateFlags & ImGuiActivateFlags_FromTabbing) != 0;
        const bool clicked = (hovered && g.IO.MouseClicked[0]);
        const bool double_clicked = (hovered && g.IO.MouseClickedCount[0] == 2);
        const bool make_active = (input_requested_by_tabbing || clicked || double_clicked || g.NavActivateId == id);
        if (make_active && temp_input_allowed)
            if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || double_clicked || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                temp_input_is_active = true;

        // (Optional) simple click (without moving) turns Drag into an InputText
        if (g.IO.ConfigDragClickToInputText && temp_input_allowed && !temp_input_is_active)
            if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * 0.5f)) // DRAG_MOUSE_THRESHOLD_FACTOR
            {
                g.NavActivateId = id;
                g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
                temp_input_is_active = true;
            }

        if (make_active && !temp_input_is_active)
        {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

    // Drag behavior
    const bool value_changed = DragBehavior(id, ImGuiDataType_Float, p_data, v_speed, &p_min, &p_max, "%.0f", flags);
    if (value_changed)
        MarkItemEdited(id);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    // print time format string
    bool negative = (*p_data) < 0;
    uint64_t t = fabs(*p_data);
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    std::ostringstream ossTime;
    if (negative)
        ossTime << "-";
    if (hour > 0)
        ossTime << std::setw(2) << std::setfill('0') << hour << ':';
    ossTime << std::setw(2) << std::setfill('0') << min << ':' << std::setw(2) << std::setfill('0') << sec;
    if (decimals > 0 && decimals < 4)
    {
        if (decimals == 1)
            milli /= 100;
        else if (decimals == 2)
            milli /= 10;
        ossTime << "." << std::setw(decimals) << std::setfill('0') << milli;
    }
    std::string strTime = ossTime.str();
    const char* time_str_buf = strTime.c_str();
    const char* time_str_buf_end = time_str_buf + strTime.length();
    if (g.LogEnabled)
        LogSetNextTextDecoration("{", "}");
    RenderTextClipped(frame_bb.Min, frame_bb.Max, time_str_buf, time_str_buf_end, NULL, ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return value_changed;
}

// RangeSelect
bool ImGui::InputVec2(char const* pLabel, ImVec2* pValue, ImVec2 const vMinValue, ImVec2 const vMaxValue, float const fScale /*= 1.0f*/, bool bInput/* = true*/, bool bHandle/* = true*/)
{
    return ImGui::SliderScalar2D(pLabel, &pValue->x, &pValue->y, vMinValue.x, vMaxValue.x, vMinValue.y, vMaxValue.y, fScale, bInput, bHandle);
}

bool ImGui::InputVec3(char const* pLabel, ImVec4* pValue, ImVec4 const vMinValue, ImVec4 const vMaxValue, float const fScale /*= 1.0f*/)
{
    return ImGui::SliderScalar3D(pLabel, &pValue->x, &pValue->y, &pValue->z, vMinValue.x, vMaxValue.x, vMinValue.y, vMaxValue.y, vMinValue.z, vMaxValue.z, fScale);
}

static float Rescale01(float const x, float const min, float const max)
{
	return (x - min) / (max - min);
}

static float Rescale(float const x, float const min, float const max, float const newMin, float const newMax)
{
	return Rescale01(x, min, max) * (newMax - newMin) + newMin;
}

static ImVec2 Rescale01v(ImVec2 const v, ImVec2 const min, ImVec2 const max)
{
	return ImVec2(Rescale01(v.x, min.x, max.x), Rescale01(v.y, min.y, max.y));
}

static ImVec2 Rescalev(ImVec2 const x, ImVec2 const min, ImVec2 const max, ImVec2 const newMin, ImVec2 const newMax)
{
	ImVec2 const vNorm = Rescale01v(x, min, max);
	return ImVec2(vNorm.x * (newMax.x - newMin.x) + newMin.x, vNorm.y * (newMax.y - newMin.y) + newMin.y);
}

bool ImGui::RangeSelect2D(char const* pLabel, float* pCurMinX, float* pCurMinY, float* pCurMaxX, float* pCurMaxY, float const fBoundMinX, float const fBoundMinY, float const fBoundMaxX, float const fBoundMaxY, float const fScale /*= 1.0f*/)
{
	float& fCurMinX = *pCurMinX;
	float& fCurMinY = *pCurMinY;
	float& fCurMaxX = *pCurMaxX;
	float& fCurMaxY = *pCurMaxY;
	float const fDeltaBoundX = fBoundMaxX - fBoundMinX;
	float const fDeltaBoundY = fBoundMaxY - fBoundMinY;
	float const fDeltaX = fCurMaxX - fCurMinX;
	float const fDeltaY = fCurMaxY - fCurMinY;
	float const fScaleX = fDeltaX / fDeltaBoundX;
	float const fScaleY = fDeltaY / fDeltaBoundY;
	float const fScaleMinX = Rescale01(fCurMinX, fBoundMinX, fBoundMaxX);
	float const fScaleMinY = Rescale01(fCurMinY, fBoundMinY, fBoundMaxY);
	float const fScaleMaxX = Rescale01(fCurMaxX, fBoundMinX, fBoundMaxX);
	float const fScaleMaxY = Rescale01(fCurMaxY, fBoundMinY, fBoundMaxY);
	ImGuiID const iID = ImGui::GetID(pLabel);
	ImVec2 const vSizeSubstract = ImGui::CalcTextSize(std::to_string(1.0f).c_str()) * 1.1f;
    float const w = ImGui::CalcItemWidth();
	float const vSizeFull = (w - vSizeSubstract.x) * fScale;
	ImVec2 const vSize(vSizeFull, vSizeFull);
	float const fHeightOffset = ImGui::GetTextLineHeight();
	ImVec2 const vHeightOffset(0.0f, fHeightOffset);
    ImVec2 const FrameSize = ImVec2(w, w) + vHeightOffset * 4;
    ImGui::BeginChild(iID, FrameSize, false, ImGuiWindowFlags_NoMove);
	ImVec2 vPos = ImGui::GetCursorScreenPos() + ImVec2(0, 4);
	ImRect oRect(vPos + vHeightOffset, vPos + vSize + vHeightOffset);
	constexpr float fCursorOff = 10.0f;
	float const fXLimit = fCursorOff / oRect.GetWidth();
	float const fYLimit = fCursorOff / oRect.GetHeight();
	ImVec2 const vCursorPos((oRect.Max.x - oRect.Min.x) * fScaleX + oRect.Min.x, (oRect.Max.y - oRect.Min.y) * fScaleY + oRect.Min.y);
	
    ImGui::TextUnformatted(pLabel);
    ImGui::Spacing();
	ImGui::PushID(iID);

    ImGui::Dummy(vHeightOffset);
	ImGui::Dummy(vHeightOffset);
	ImU32 const uFrameCol = ImGui::GetColorU32(ImGuiCol_FrameBg);
	ImU32 const uFrameZoneCol = ImGui::GetColorU32(ImGuiCol_FrameBgActive);
	ImVec2 const vOriginPos = ImGui::GetCursorScreenPos();
	ImGui::RenderFrame(oRect.Min, oRect.Max, uFrameCol, false, 0.0f);
	bool bModified = false;
	ImVec2 const vSecurity(15.0f, 15.0f);
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	ImVec4 const vBlue(70.0f / 255.0f, 102.0f / 255.0f, 230.0f / 255.0f, 1.0f);
	ImVec4 const vOrange(255.0f / 255.0f, 128.0f / 255.0f, 64.0f / 255.0f, 1.0f);
	ImS32 const uBlue = ImGui::GetColorU32(vBlue);
	ImS32 const uOrange = ImGui::GetColorU32(vOrange);
	constexpr float fBorderThickness = 2.0f;
	constexpr float fLineThickness = 3.0f;
	constexpr float fHandleRadius = 7.0f;
	constexpr float fHandleOffsetCoef = 2.0f;
	float const fRegMinX = ImLerp(oRect.Min.x, oRect.Max.x, fScaleMinX);
	float const fRegMinY = ImLerp(oRect.Min.y, oRect.Max.y, fScaleMinY);
	float const fRegMaxX = ImLerp(oRect.Min.x, oRect.Max.x, fScaleMaxX);
	float const fRegMaxY = ImLerp(oRect.Min.y, oRect.Max.y, fScaleMaxY);
	ImRect oRegionRect(fRegMinX, fRegMinY, fRegMaxX, fRegMaxY);
	ImVec2 vMinCursorPos(fRegMinX, fRegMinY);
	ImVec2 vMaxCursorPos(fRegMaxX, fRegMaxY);
	float const fRegWidth = oRegionRect.GetWidth();
	ImRect oWidthHandle(ImVec2(vMinCursorPos.x + 0.25f * fRegWidth, oRect.Min.y - 0.5f * fCursorOff), ImVec2(vMaxCursorPos.x - 0.25f * fRegWidth, oRect.Min.y + 0.5f * fCursorOff));
	float const fRegHeight = oRegionRect.GetHeight();
	ImRect oHeightHandle(ImVec2(oRect.Min.x - 0.5f * fCursorOff, vMinCursorPos.y + 0.25f * fRegHeight), ImVec2(oRect.Min.x + 0.5f * fCursorOff, vMaxCursorPos.y - 0.25f * fRegHeight));
	ImGui::RenderFrame(oRegionRect.Min, oRegionRect.Max, uFrameZoneCol, false, 0.0f);
	pDrawList->AddNgonFilled(vMinCursorPos, 5.0f, uBlue, 4);
	pDrawList->AddNgonFilled(vMaxCursorPos, 5.0f, uBlue, 4);
	ImRect oDragZone(oRegionRect.Min + ImVec2(fCursorOff, fCursorOff) + vSecurity, oRegionRect.Max - ImVec2(fCursorOff, fCursorOff) - vSecurity);
	ImRect vDragBBMin(vMinCursorPos - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vMinCursorPos + ImVec2(fHandleRadius, fHandleRadius) + vSecurity);
	ImRect vDragBBMax(vMaxCursorPos - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vMaxCursorPos + ImVec2(fHandleRadius, fHandleRadius) + vSecurity);
	ImRect vDragHandleMin(oWidthHandle.Min - ImVec2(fCursorOff, fCursorOff) - vSecurity, oWidthHandle.Max + ImVec2(fCursorOff, fCursorOff) + vSecurity);
	ImRect vDragHandleHeight(oHeightHandle.Min - ImVec2(fCursorOff, fCursorOff) - vSecurity, oHeightHandle.Max + ImVec2(fCursorOff, fCursorOff) + vSecurity);
	ImRect vDragRect(oRegionRect.Min, oRegionRect.Max);
	float const fArbitrarySpeedScaleBar = 0.0125f;
    static bool drag_mouse = false;

    if (ImGui::IsMouseHoveringRect(vDragBBMin.Min, vDragBBMin.Max) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		ImVec2 const vLocalCursorPos = ImGui::GetMousePos();
		ImVec2 newVal = Rescalev(vLocalCursorPos, ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Max.x, oRect.Max.y), ImVec2(fBoundMinX, fBoundMinY), ImVec2(fBoundMaxX, fBoundMaxY));
		newVal.x = ImClamp(newVal.x, fBoundMinX, *pCurMaxX);
		newVal.y = ImClamp(newVal.y, fBoundMinY, *pCurMaxY);
		*pCurMinX = newVal.x;
		*pCurMinY = newVal.y;
		bModified = true;
        drag_mouse = true;
	}
	else
	{
        if (ImGui::IsMouseHoveringRect(vDragBBMax.Min, vDragBBMax.Max) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			ImVec2 const vLocalCursorPos = ImGui::GetMousePos();
					
			ImVec2 newVal = Rescalev(vLocalCursorPos, ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Max.x, oRect.Max.y), ImVec2(fBoundMinX, fBoundMinY), ImVec2(fBoundMaxX, fBoundMaxY));
			newVal.x = ImClamp(newVal.x, *pCurMinX, fBoundMaxX);
			newVal.y = ImClamp(newVal.y, *pCurMinY, fBoundMaxY);
			*pCurMaxX = newVal.x;
			*pCurMaxY = newVal.y;
			bModified = true;
            drag_mouse = true;
		}
		else
		{
            if (ImGui::IsMouseHoveringRect(vDragHandleMin.Min, vDragHandleMin.Max) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				constexpr float fSpeedHandleWidth = 0.125f;
				float fDeltaWidth = oWidthHandle.GetCenter().x - ImGui::GetMousePos().x;
				fDeltaWidth = fSpeedHandleWidth * ImClamp(fDeltaWidth, -0.5f * oRect.GetWidth(), 0.5f * oRect.GetWidth());
				float fDeltaWidthValue = Rescale(fDeltaWidth, -0.5f * oRect.GetWidth(), 0.5f * oRect.GetWidth(), -0.5f * (fBoundMaxX - fBoundMinX), 0.5f * (fBoundMaxX - fBoundMinX));
				if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
				{
					*pCurMinX = ImClamp(*pCurMinX + fArbitrarySpeedScaleBar * fDeltaWidthValue, fBoundMinX, *pCurMaxX);
					*pCurMaxX = ImClamp(*pCurMaxX - fArbitrarySpeedScaleBar * fDeltaWidthValue, *pCurMinX, fBoundMaxX);
				}
				else
				{
					if (*pCurMinX <= fBoundMinX && fDeltaWidthValue > 0.0f)
					{
						fDeltaWidthValue = 0.0f;
					}
					else if (*pCurMaxX >= fBoundMaxX && fDeltaWidthValue < 0.0f)
					{
						fDeltaWidthValue = 0.0f;
					}
					*pCurMinX = ImClamp(*pCurMinX - fDeltaWidthValue, fBoundMinX, *pCurMaxX);
					*pCurMaxX = ImClamp(*pCurMaxX - fDeltaWidthValue, *pCurMinX, fBoundMaxX);
				}
				bModified = true;
                drag_mouse = true;
			}
			else
			{
                if (ImGui::IsMouseHoveringRect(vDragHandleHeight.Min, vDragHandleHeight.Max) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					constexpr float fSpeedHandleHeight = 0.125f;
					float fDeltaHeight = oHeightHandle.GetCenter().y - ImGui::GetMousePos().y;
					// Apply Soft-Threshold
					//fDeltaHeight = fSpeedHandleHeight*Sign(fDeltaHeight)*std::max(std::abs(fDeltaHeight) - 0.5f*fCursorOff, 0.0f);
					fDeltaHeight = fSpeedHandleHeight * ImClamp(fDeltaHeight, -0.5f * oRect.GetHeight(), 0.5f * oRect.GetHeight());
					float fDeltaHeightValue = Rescale(fDeltaHeight, -0.5f * oRect.GetHeight(), 0.5f * oRect.GetHeight(), -0.5f * (fBoundMaxY - fBoundMinY), 0.5f * (fBoundMaxY - fBoundMinY));
					if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
					{
						*pCurMinY = ImClamp(*pCurMinY + fArbitrarySpeedScaleBar * fDeltaHeightValue, fBoundMinY, *pCurMaxY);
						*pCurMaxY = ImClamp(*pCurMaxY - fArbitrarySpeedScaleBar * fDeltaHeightValue, *pCurMinY, fBoundMaxY);
					}
					else
					{
						if (*pCurMinY <= fBoundMinY && fDeltaHeightValue > 0.0f)
						{
							fDeltaHeightValue = 0.0f;
						}
						else if (*pCurMaxY >= fBoundMaxY && fDeltaHeightValue < 0.0f)
						{
							fDeltaHeightValue = 0.0f;
						}
						*pCurMinY = ImClamp(*pCurMinY - fDeltaHeightValue, fBoundMinY, *pCurMaxY);
						*pCurMaxY = ImClamp(*pCurMaxY - fDeltaHeightValue, *pCurMinY, fBoundMaxY);
					}
					bModified = true;
                    drag_mouse = true;
				}
				else
				{
                    if (ImGui::IsMouseHoveringRect(vDragRect.Min, vDragRect.Max) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						// Top Left
						pDrawList->AddLine(oDragZone.Min, oDragZone.Min + ImVec2(oRegionRect.GetWidth() * 0.2f, 0.0f), uFrameCol, 1.0f);
						pDrawList->AddLine(oDragZone.Min, oDragZone.Min + ImVec2(0.0f, oRegionRect.GetHeight() * 0.2f), uFrameCol, 1.0f);
						// Bottom Right
						pDrawList->AddLine(oDragZone.Max, oDragZone.Max - ImVec2(oRegionRect.GetWidth() * 0.2f, 0.0f), uFrameCol, 1.0f);
						pDrawList->AddLine(oDragZone.Max, oDragZone.Max - ImVec2(0.0f, oRegionRect.GetHeight() * 0.2f), uFrameCol, 1.0f);
						if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(oDragZone.Min, oDragZone.Max))
						{
                            ImGui::SetNextFrameWantCaptureMouse();
							ImVec2 vDragDelta = ImGui::GetMousePos() - oDragZone.GetCenter();
							if (*pCurMinX <= fBoundMinX && vDragDelta.x < 0.0f)
							{
								vDragDelta.x = 0.0f;
							}
							else if (*pCurMaxX >= fBoundMaxX && vDragDelta.x > 0.0f)
							{
								vDragDelta.x = 0.0f;
							}
							if (*pCurMinY <= fBoundMinY && vDragDelta.y < 0.0f)
							{
								vDragDelta.y = 0.0f;
							}
							else if (*pCurMaxY >= fBoundMaxY && vDragDelta.y > 0.0f)
							{
								vDragDelta.y = 0.0f;
							}
							float fLocalDeltaX = Rescale(vDragDelta.x, -0.5f * oRect.GetWidth(), 0.5f * oRect.GetWidth(), -0.5f * (fBoundMaxX - fBoundMinX), 0.5f * (fBoundMaxX - fBoundMinX));
							float fLocalDeltaY = Rescale(vDragDelta.y, -0.5f * oRect.GetHeight(), 0.5f * oRect.GetHeight(), -0.5f * (fBoundMaxY - fBoundMinY), 0.5f * (fBoundMaxY - fBoundMinY));
							*pCurMinX = ImClamp(*pCurMinX + fLocalDeltaX, fBoundMinX, *pCurMaxX);
							*pCurMaxX = ImClamp(*pCurMaxX + fLocalDeltaX, *pCurMinX, fBoundMaxX);
							*pCurMinY = ImClamp(*pCurMinY + fLocalDeltaY, fBoundMinY, *pCurMaxY);
							*pCurMaxY = ImClamp(*pCurMaxY + fLocalDeltaY, *pCurMinY, fBoundMaxY);
						}
					}
                    drag_mouse = true;
				}
			}
		}
	}

    if (drag_mouse)
        ImGui::SetNextFrameWantCaptureMouse();

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag_mouse)
    {
        drag_mouse = false;
        ImGui::SetNextFrameWantCaptureMouse(false);
    }

	char pBufferMinX[16];
	char pBufferMaxX[16];
	char pBufferMinY[16];
	char pBufferMaxY[16];
	ImFormatString(pBufferMinX, IM_ARRAYSIZE(pBufferMinX), "%.5f", *(float const*)pCurMinX);
	ImFormatString(pBufferMaxX, IM_ARRAYSIZE(pBufferMaxX), "%.5f", *(float const*)pCurMaxX);
	ImFormatString(pBufferMinY, IM_ARRAYSIZE(pBufferMinY), "%.5f", *(float const*)pCurMinY);
	ImFormatString(pBufferMaxY, IM_ARRAYSIZE(pBufferMaxY), "%.5f", *(float const*)pCurMaxY);
	ImU32 const uTextCol = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);
	ImGui::SetWindowFontScale(0.75f);
	ImVec2 const vMinXSize = ImGui::CalcTextSize(pBufferMinX);
	ImVec2 const vMaxXSize = ImGui::CalcTextSize(pBufferMaxX);
	ImVec2 const vMinYSize = ImGui::CalcTextSize(pBufferMinY);
	ImVec2 const vMaxYSize = ImGui::CalcTextSize(pBufferMaxY);
	pDrawList->AddText(ImVec2(vMinCursorPos.x - 0.5f * vMinXSize.x, oRect.Max.y + fCursorOff), uTextCol, pBufferMinX);
	pDrawList->AddText(ImVec2(vMaxCursorPos.x - 0.5f * vMaxXSize.x, oRect.Max.y + fCursorOff), uTextCol, pBufferMaxX);
	pDrawList->AddText(ImVec2(oRect.Max.x + fCursorOff, vMinCursorPos.y - 0.5f * vMinYSize.y), uTextCol, pBufferMinY);
	pDrawList->AddText(ImVec2(oRect.Max.x + fCursorOff, vMaxCursorPos.y - 0.5f * vMaxYSize.y), uTextCol, pBufferMaxY);
	ImGui::SetWindowFontScale(1.0f);
	if (!oRegionRect.IsInverted())
	{
		pDrawList->AddLine(vMinCursorPos + ImVec2(fCursorOff, 0.0f), vMinCursorPos + ImVec2(2.0f * fCursorOff, 0.0f), uOrange, fLineThickness);
		pDrawList->AddLine(vMinCursorPos + ImVec2(0.0f, fCursorOff), vMinCursorPos + ImVec2(0.0f, 2.0f * fCursorOff), uOrange, fLineThickness);
		pDrawList->AddLine(vMaxCursorPos - ImVec2(fCursorOff, 0.0f), vMaxCursorPos - ImVec2(2.0f * fCursorOff, 0.0f), uOrange, fLineThickness);
		pDrawList->AddLine(vMaxCursorPos - ImVec2(0.0f, fCursorOff), vMaxCursorPos - ImVec2(0.0f, 2.0f * fCursorOff), uOrange, fLineThickness);
	}
	// Cross Center
	pDrawList->AddLine(oDragZone.GetCenter() - ImVec2(fCursorOff, 0.0f), oDragZone.GetCenter() + ImVec2(fCursorOff, 0.0f), uFrameCol, 1.0f);
	pDrawList->AddLine(oDragZone.GetCenter() - ImVec2(0.0f, fCursorOff), oDragZone.GetCenter() + ImVec2(0.0f, fCursorOff), uFrameCol, 1.0f);
	//////////////////////////////////////////////////////////////////////////
	// Top Left
	pDrawList->AddLine(oRect.Min, ImVec2(vMinCursorPos.x, oRect.Min.y), uOrange, fLineThickness);
	pDrawList->AddLine(oRect.Min, ImVec2(oRect.Min.x, vMinCursorPos.y), uOrange, fLineThickness);
	// Bottom Left
	pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(vMinCursorPos.x - fCursorOff, oRect.Max.y), uOrange, fLineThickness);
	pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(oRect.Min.x, vMaxCursorPos.y), uOrange, fLineThickness);
	// 
	pDrawList->AddLine(ImVec2(vMinCursorPos.x + fCursorOff, oRect.Max.y), ImVec2(vMaxCursorPos.x - fCursorOff, oRect.Max.y), uOrange, fLineThickness);
	pDrawList->AddLine(ImVec2(vMaxCursorPos.x + fCursorOff, oRect.Max.y), ImVec2(oRect.Max.x, oRect.Max.y), uOrange, fLineThickness);
	// Right
	pDrawList->AddLine(oRect.Max, ImVec2(oRect.Max.x, vMaxCursorPos.y + fCursorOff), uOrange, fLineThickness);
	pDrawList->AddLine(ImVec2(oRect.Max.x, vMaxCursorPos.y - fCursorOff), ImVec2(oRect.Max.x, vMinCursorPos.y + fCursorOff), uOrange, fLineThickness);
	pDrawList->AddLine(ImVec2(oRect.Max.x, vMinCursorPos.y - fCursorOff), ImVec2(oRect.Max.x, oRect.Min.y), uOrange, fLineThickness);
	// Top Right
	pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(vMaxCursorPos.x, oRect.Min.y), uOrange, fLineThickness);
	// Top Handle
	pDrawList->AddLine(ImVec2(vMinCursorPos.x, oRect.Min.y - fCursorOff), ImVec2(vMinCursorPos.x, oRect.Min.y + fCursorOff), uBlue, fLineThickness);
	pDrawList->AddLine(ImVec2(vMaxCursorPos.x, oRect.Min.y - fCursorOff), ImVec2(vMaxCursorPos.x, oRect.Min.y + fCursorOff), uBlue, fLineThickness);
	pDrawList->AddRectFilled(oWidthHandle.Min, oWidthHandle.Max, uBlue);
	// Left Handle
	pDrawList->AddLine(ImVec2(oRect.Min.x - fCursorOff, vMinCursorPos.y), ImVec2(oRect.Min.x + fCursorOff, vMinCursorPos.y), uBlue, fLineThickness);
	pDrawList->AddLine(ImVec2(oRect.Min.x - fCursorOff, vMaxCursorPos.y), ImVec2(oRect.Min.x + fCursorOff, vMaxCursorPos.y), uBlue, fLineThickness);
	pDrawList->AddRectFilled(oHeightHandle.Min, oHeightHandle.Max, uBlue);
	// Dots
	pDrawList->AddCircleFilled(ImVec2(oRect.Max.x, vMinCursorPos.y), 2.0f, uBlue, 3);
	pDrawList->AddCircleFilled(ImVec2(oRect.Max.x, vMaxCursorPos.y), 2.0f, uBlue, 3);
	pDrawList->AddCircleFilled(ImVec2(vMinCursorPos.x, oRect.Max.y), 2.0f, uBlue, 3);
	pDrawList->AddCircleFilled(ImVec2(vMaxCursorPos.x, oRect.Max.y), 2.0f, uBlue, 3);
	//////////////////////////////////////////////////////////////////////////
	ImGui::PopID();
	ImGui::Dummy(vSize);
    auto string_width = ImGui::CalcTextSize("MinX");
    ImGui::PushMultiItemsWidths(2, w - string_width.x * 3);
    float const fSpeedX = fDeltaX/128.0f;
    float const fSpeedY = fDeltaY/128.0f;

    char pBufferMinXID[64];
    ImFormatString(pBufferMinXID, IM_ARRAYSIZE(pBufferMinXID), "MinX##%d", *(ImS32 const*)&iID);
    char pBufferMinYID[64];
    ImFormatString(pBufferMinYID, IM_ARRAYSIZE(pBufferMinYID), "MinY##%d", *(ImS32 const*)&iID);
    char pBufferMaxXID[64];
    ImFormatString(pBufferMaxXID, IM_ARRAYSIZE(pBufferMaxXID), "MaxX##%d", *(ImS32 const*)&iID);
    char pBufferMaxYID[64];
    ImFormatString(pBufferMaxYID, IM_ARRAYSIZE(pBufferMaxYID), "MaxY##%d", *(ImS32 const*)&iID);

    bModified |= ImGui::DragScalar(pBufferMinXID, ImGuiDataType_Float, pCurMinX , fSpeedX, &fBoundMinX, &fBoundMaxX);
    ImGui::SameLine();
    bModified |= ImGui::DragScalar(pBufferMinYID, ImGuiDataType_Float, pCurMinY , fSpeedY, &fBoundMinY, &fBoundMaxY);
    bModified |= ImGui::DragScalar(pBufferMaxXID, ImGuiDataType_Float, pCurMaxX , fSpeedX, &fBoundMinX, &fBoundMaxX);
    ImGui::SameLine();
    bModified |= ImGui::DragScalar(pBufferMaxYID, ImGuiDataType_Float, pCurMaxY , fSpeedY, &fBoundMinY, &fBoundMaxY);
    ImGui::EndChild();
	return bModified;
}

bool ImGui::RangeSelectVec2(const char* pLabel, ImVec2* pCurMin, ImVec2* pCurMax, ImVec2 const vBoundMin, ImVec2 const vBoundMax, float const fScale /*= 1.0f*/)
{
	return ImGui::RangeSelect2D(pLabel, &pCurMin->x, &pCurMin->y, &pCurMax->x, &pCurMax->y, vBoundMin.x, vBoundMin.y, vBoundMax.x, vBoundMax.y, fScale);
}

// Bezier Widget
template<int steps>
static void bezier_table(ImVec2 P[4], ImVec2 results[steps + 1]) {
    static float C[(steps + 1) * 4], *K = 0;
    if (!K) {
        K = C;
        for (unsigned step = 0; step <= steps; ++step) {
            float t = (float) step / (float) steps;
            C[step * 4 + 0] = (1 - t)*(1 - t)*(1 - t);   // * P0
            C[step * 4 + 1] = 3 * (1 - t)*(1 - t) * t; // * P1
            C[step * 4 + 2] = 3 * (1 - t) * t*t;     // * P2
            C[step * 4 + 3] = t * t*t;               // * P3
        }
    }
    for (unsigned step = 0; step <= steps; ++step) {
        ImVec2 point = {
            K[step * 4 + 0] * P[0].x + K[step * 4 + 1] * P[1].x + K[step * 4 + 2] * P[2].x + K[step * 4 + 3] * P[3].x,
            K[step * 4 + 0] * P[0].y + K[step * 4 + 1] * P[1].y + K[step * 4 + 2] * P[2].y + K[step * 4 + 3] * P[3].y
        };
        results[step] = point;
    }
}

float ImGui::BezierValue(float dt01, float P[4], int step) 
{
    enum { STEPS = 256 };
    ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
    int _step = step <= 0 ? STEPS : step;
    ImVec2 results[_step + 1];
    bezier_table<STEPS>(Q, results);
    return results[(int) ((dt01 < 0 ? 0 : dt01 > 1 ? 1 : dt01) * _step)].y;
}

bool ImGui::BezierSelect(const char *label, const ImVec2 size, float P[5]) 
{
    // visuals
    enum { SMOOTHNESS = 64 }; // curve smoothness: the higher number of segments, the smoother curve
    enum { CURVE_WIDTH = 4 }; // main curved line width
    enum { LINE_WIDTH = 1 }; // handlers: small lines width
    enum { GRAB_RADIUS = 8 }; // handlers: circle radius
    enum { GRAB_BORDER = 2 }; // handlers: circle border width
    enum { AREA_CONSTRAINED = false }; // should grabbers be constrained to grid area?
    // curve presets
    static struct { const char *name; float points[4]; } presets [] = 
    {
        { "Linear", 0.000f, 0.000f, 1.000f, 1.000f },
        { "In Sine", 0.470f, 0.000f, 0.745f, 0.715f },
        { "In Quad", 0.550f, 0.085f, 0.680f, 0.530f },
        { "In Cubic", 0.550f, 0.055f, 0.675f, 0.190f },
        { "In Quart", 0.895f, 0.030f, 0.685f, 0.220f },
        { "In Quint", 0.755f, 0.050f, 0.855f, 0.060f },
        { "In Expo", 0.950f, 0.050f, 0.795f, 0.035f },
        { "In Circ", 0.600f, 0.040f, 0.980f, 0.335f },
        { "In Back", 0.600f, -0.28f, 0.735f, 0.045f },
        { "Out Sine", 0.390f, 0.575f, 0.565f, 1.000f },
        { "Out Quad", 0.250f, 0.460f, 0.450f, 0.940f },
        { "Out Cubic", 0.215f, 0.610f, 0.355f, 1.000f },
        { "Out Quart", 0.165f, 0.840f, 0.440f, 1.000f },
        { "Out Quint", 0.230f, 1.000f, 0.320f, 1.000f },
        { "Out Expo", 0.190f, 1.000f, 0.220f, 1.000f },
        { "Out Circ", 0.075f, 0.820f, 0.165f, 1.000f },
        { "Out Back", 0.175f, 0.885f, 0.320f, 1.275f },
        { "InOut Sine", 0.445f, 0.050f, 0.550f, 0.950f },
        { "InOut Quad", 0.455f, 0.030f, 0.515f, 0.955f },
        { "InOut Cubic", 0.645f, 0.045f, 0.355f, 1.000f },
        { "InOut Quart", 0.770f, 0.000f, 0.175f, 1.000f },
        { "InOut Quint", 0.860f, 0.000f, 0.070f, 1.000f },
        { "InOut Expo", 1.000f, 0.000f, 0.000f, 1.000f },
        { "InOut Circ", 0.785f, 0.135f, 0.150f, 0.860f },
        { "InOut Back", 0.680f, -0.55f, 0.265f, 1.550f },
    };
    // preset selector
    bool reload = 0;
    ImGuiID const iID = ImGui::GetID(label);
    ImGui::PushID(label);
    ImGui::BeginGroup();
    if (ImGui::ArrowButton("##lt", ImGuiDir_Left))
    {
        if (--P[4] >= 0) reload = 1; else ++P[4];
    }
    ImGui::SameLine();
    if (ImGui::Button("Presets")) {
        ImGui::OpenPopup("!Presets");
    }
    if (ImGui::BeginPopup("!Presets")) {
        for (int i = 0; i < IM_ARRAYSIZE(presets); ++i)
        {
            if( i == 1 || i == 9 || i == 17 ) ImGui::Separator();
            if (ImGui::MenuItem(presets[i].name, NULL, P[4] == i))
            {
                P[4] = i;
                reload = 1;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##rt", ImGuiDir_Right))
    {
        if (++P[4] < IM_ARRAYSIZE(presets)) reload = 1; else --P[4];
    }

    ImGui::PopID();
    if (reload) {
        memcpy(P, presets[(int) P[4]].points, sizeof(float) * 4);
    }
    // bezier widget
    const ImGuiStyle& Style = ImGui::GetStyle();
    const ImGuiIO& IO = ImGui::GetIO();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImGuiWindow* Window = ImGui::GetCurrentWindow();
    if (Window->SkipItems)
    {
        ImGui::EndGroup();
        return false;
    }
    // header and spacing
    const float dim = size.x;
    ImGui::PushMultiItemsWidths(2, dim);
    bool changed = false;
    changed |= ImGui::SliderFloat("##P0X", &P[0], -2.f, 2.f, "%.2f", 1.f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##P0Y", &P[1], -2.f, 2.f, "%.2f", 1.f);
    changed |= ImGui::SliderFloat("##P1X", &P[2], -2.f, 2.f, "%.2f", 1.f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##P1Y", &P[3], -2.f, 2.f, "%.2f", 1.f);
    int hovered = ImGui::IsItemActive() || ImGui::IsItemHovered();
    ImGui::Dummy(ImVec2(0, 3));
    // prepare canvas
    ImVec2 Canvas(dim, dim);
    ImRect bb(Window->DC.CursorPos, Window->DC.CursorPos + Canvas);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0))
    {
        ImGui::EndGroup();
        return changed;
    }
    const ImGuiID id = Window->GetID(label);
    hovered |= 0 != ImGui::ItemHoverable(ImRect(bb.Min, bb.Min + ImVec2(dim, dim)), id, ImGuiItemFlags_None);
    ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg, 1), true, Style.FrameRounding);
    // background grid
    for (int i = 0; i <= Canvas.x; i += (Canvas.x / 4)) {
        DrawList->AddLine(
            ImVec2(bb.Min.x + i, bb.Min.y),
            ImVec2(bb.Min.x + i, bb.Max.y),
            GetColorU32(ImGuiCol_TextDisabled));
    }
    for (int i = 0; i <= Canvas.y; i += (Canvas.y / 4)) {
        DrawList->AddLine(
            ImVec2(bb.Min.x, bb.Min.y + i),
            ImVec2(bb.Max.x, bb.Min.y + i),
            GetColorU32(ImGuiCol_TextDisabled));
    }
    // eval curve
    ImVec2 Q[4] = { { 0, 0 }, { P[0], P[1] }, { P[2], P[3] }, { 1, 1 } };
    ImVec2 results[SMOOTHNESS + 1];
    bezier_table<SMOOTHNESS>(Q, results);
    // control points: 2 lines and 2 circles
    {
        // handle grabbers
        ImVec2 mouse = ImGui::GetIO().MousePos, pos[2];
        float distance[2];
        static int selected = 0;
        for (int i = 0; i < 2; ++i) {
            pos[i] = ImVec2(P[i * 2 + 0], 1 - P[i * 2 + 1]) * (bb.Max - bb.Min) + bb.Min;
            distance[i] = (pos[i].x - mouse.x) * (pos[i].x - mouse.x) + (pos[i].y - mouse.y) * (pos[i].y - mouse.y);
        }
        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            selected = distance[0] < distance[1] ? 0 : 1;
        if( distance[selected] < (4 * GRAB_RADIUS * 4 * GRAB_RADIUS) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImGui::SetTooltip("(%4.3f, %4.3f)", P[selected * 2 + 0], P[selected * 2 + 1]);
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                ImGui::SetNextFrameWantCaptureMouse();
                float &px = (P[selected * 2 + 0] += ImGui::GetIO().MouseDelta.x / Canvas.x);
                float &py = (P[selected * 2 + 1] -= ImGui::GetIO().MouseDelta.y / Canvas.y);
                if (AREA_CONSTRAINED) {
                    px = (px < 0 ? 0 : (px > 1 ? 1 : px));
                    py = (py < 0 ? 0 : (py > 1 ? 1 : py));
                }
                changed = true;
            }
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        ImGui::SetNextFrameWantCaptureMouse(false);
    // draw curve
    {
        ImColor color(ImGui::GetStyle().Colors[ImGuiCol_PlotLines]);
        for (int i = 0; i < SMOOTHNESS; ++i)
        {
            ImVec2 p = { results[i + 0].x, 1 - results[i + 0].y };
            ImVec2 q = { results[i + 1].x, 1 - results[i + 1].y };
            ImVec2 r(p.x * (bb.Max.x - bb.Min.x) + bb.Min.x, p.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
            ImVec2 s(q.x * (bb.Max.x - bb.Min.x) + bb.Min.x, q.y * (bb.Max.y - bb.Min.y) + bb.Min.y);
            DrawList->AddLine(r, s, color, CURVE_WIDTH);
        }
    }
    // draw preview (cycles every 1s)
    static clock_t epoch = clock();
    ImVec4 white(ImGui::GetStyle().Colors[ImGuiCol_Text]);
    for (int i = 0; i < 3; ++i)
    {
        double now = ((clock() - epoch) / (double)CLOCKS_PER_SEC);
        float delta = ((int) (now * 1000) % 1000) / 1000.f; delta += i / 3.f; if (delta > 1) delta -= 1;
        int idx = (int) (delta * SMOOTHNESS);
        float evalx = results[idx].x; // 
        float evaly = results[idx].y; //
        ImVec2 p0 = ImVec2(evalx, 1 - 0) * (bb.Max - bb.Min) + bb.Min;
        ImVec2 p1 = ImVec2(0, 1 - evaly) * (bb.Max - bb.Min) + bb.Min;
        ImVec2 p2 = ImVec2(evalx, 1 - evaly) * (bb.Max - bb.Min) + bb.Min;
        DrawList->AddCircleFilled(p0, GRAB_RADIUS / 2, ImColor(white));
        DrawList->AddCircleFilled(p1, GRAB_RADIUS / 2, ImColor(white));
        DrawList->AddCircleFilled(p2, GRAB_RADIUS / 2, ImColor(white));
    }
    // draw lines and grabbers
    float luma = ImGui::IsItemActive() || ImGui::IsItemHovered() ? 0.5f : 1.0f;
    ImVec4 pink(1.00f, 0.00f, 0.75f, luma), cyan(0.00f, 0.75f, 1.00f, luma);
    ImVec2 p1 = ImVec2(P[0], 1 - P[1]) * (bb.Max - bb.Min) + bb.Min;
    ImVec2 p2 = ImVec2(P[2], 1 - P[3]) * (bb.Max - bb.Min) + bb.Min;
    DrawList->AddLine(ImVec2(bb.Min.x, bb.Max.y), p1, ImColor(white), LINE_WIDTH);
    DrawList->AddLine(ImVec2(bb.Max.x, bb.Min.y), p2, ImColor(white), LINE_WIDTH);
    DrawList->AddCircleFilled(p1, GRAB_RADIUS, ImColor(white));
    DrawList->AddCircleFilled(p1, GRAB_RADIUS - GRAB_BORDER, ImColor(pink));
    DrawList->AddCircleFilled(p2, GRAB_RADIUS, ImColor(white));
    DrawList->AddCircleFilled(p2, GRAB_RADIUS - GRAB_BORDER, ImColor(cyan));
    ImGui::EndGroup();
    if (hovered && ImGui::BeginTooltip())
    {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float delta = (mouse.x - bb.Min.x) / bb.GetWidth();
        auto evaly = ImGui::BezierValue(delta, P);
        ImGui::Text("%f", evaly);
        ImGui::EndTooltip();
    }
    return changed;
}

// Color bar and ring
void ImGui::DrawHueBand(ImVec2 const vpos, ImVec2 const size, int division, float alpha, float gamma, float offset)
{
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	auto HueFunc = [alpha, offset](float const tt) -> ImU32
	{
		float t;
		if (tt - offset < 0.0f)
			t = ImFmod(1.0f + (tt - offset), 1.0f);
		else
			t = ImFmod(tt - offset, 1.0f);
		float r, g, b;
		ImGui::ColorConvertHSVtoRGB(t, 1.0f, 1.0f, r, g, b);
		int const ur = static_cast<int>(255.0f * r);
		int const ug = static_cast<int>(255.0f * g);
		int const ub = static_cast<int>(255.0f * b);
		int const ua = static_cast<int>(255.0f * alpha);
		return IM_COL32(ur, ug, ub, ua);
	};
	ImGui::DrawColorBandEx< true >(pDrawList, vpos, size, HueFunc, division, gamma);
}

void ImGui::DrawHueBand(ImVec2 const vpos, ImVec2 const size, int division, float colorStartRGB[3], float alpha, float gamma)
{
	float h, s, v;
	ImGui::ColorConvertRGBtoHSV(colorStartRGB[0], colorStartRGB[1], colorStartRGB[2], h, s, v);
	ImGui::DrawHueBand(vpos, size, division, alpha, gamma, h);
}

void ImGui::DrawLumianceBand(ImVec2 const vpos, ImVec2 const size, int division, ImVec4 const& color, float gamma)
{
	float h, s, v;
	ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	auto LumianceFunc = [h, s, v](float const t) -> ImU32
	{
		float r, g, b;
		ImGui::ColorConvertHSVtoRGB(h, s, ImLerp(0.0f, v, t), r, g, b);
		int const ur = static_cast<int>(255.0f * r);
		int const ug = static_cast<int>(255.0f * g);
		int const ub = static_cast<int>(255.0f * b);
		return IM_COL32(ur, ug, ub, 255);
	};
	ImGui::DrawColorBandEx< true >(pDrawList, vpos, size, LumianceFunc, division, gamma);
}

void ImGui::DrawSaturationBand(ImVec2 const vpos, ImVec2 const size, int division, ImVec4 const& color, float gamma)
{
	float h, s, v;
	ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	auto SaturationFunc = [h, s, v](float const t) -> ImU32
	{
		float r, g, b;
		ImGui::ColorConvertHSVtoRGB(h, ImLerp(0.0f, 1.0f, t) * s, ImLerp(0.5f, 1.0f, t) * v, r, g, b);
		int const ur = static_cast<int>(255.0f * r);
		int const ug = static_cast<int>(255.0f * g);
		int const ub = static_cast<int>(255.0f * b);
		return IM_COL32(ur, ug, ub, 255);
	};
	ImGui::DrawColorBandEx< true >(pDrawList, vpos, size, SaturationFunc, division, gamma);
}

void ImGui::DrawContrastBand(ImVec2 const vpos, ImVec2 const size, ImVec4 const& color)
{
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    ImGui::DrawColorBandEx< true >(pDrawList, vpos, size,
	[size, color](float t)
	{
		int seg = t * size.x;
        float v = ((seg & 1) == 0) ? 1.0 : (1 - t);
		return IM_COL32(v * color.x * 255.f, v * color.y * 255.f, v * color.z * 255.f, color.w * 255.f);
	}, size.x * 1.5, 1.f);
}

bool ImGui::ColorRing(const char* label, float thickness, int split)
{
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
	ImVec2 curPos = ImGui::GetCursorScreenPos();
	float const width = ImGui::GetContentRegionAvail().x;
	float const height = width;
	ImGui::InvisibleButton("##Zone", ImVec2(width, height), 0);
	float radius = width * 0.5f;
	const float dAngle = 2.0f * IM_PI / ((float)split);
	float angle = 2.0f * IM_PI / 3.0f;
	ImVec2 offset = curPos + ImVec2(radius, radius);
	ImVec2 const uv = ImGui::GetFontTexUvWhitePixel();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	pDrawList->PrimReserve(split * 6, split * 4);
	for (int i = 0; i < split; ++i)
	{
		float x0 = radius * ImCos(angle);
		float y0 = radius * ImSin(angle);
		float x1 = radius * ImCos(angle + dAngle);
		float y1 = radius * ImSin(angle + dAngle);
		float x2 = (radius - thickness) * ImCos(angle + dAngle);
		float y2 = (radius - thickness) * ImSin(angle + dAngle);
		float x3 = (radius - thickness) * ImCos(angle);
		float y3 = (radius - thickness) * ImSin(angle);
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 1));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 2));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 2));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 3));
		float r0, g0, b0;
		float r1, g1, b1;
		ImGui::ColorConvertHSVtoRGB(((float)i) / ((float)(split - 1)), 1.0f, 1.0f, r0, g0, b0);
		ImGui::ColorConvertHSVtoRGB(((float)((i + 1)%split)) / ((float)(split - 1)), 1.0f, 1.0f, r1, g1, b1);
		pDrawList->PrimWriteVtx(offset + ImVec2(x0, y0), uv, IM_COL32(r0 * 255, g0 * 255, b0 * 255, 255));
		pDrawList->PrimWriteVtx(offset + ImVec2(x1, y1), uv, IM_COL32(r1 * 255, g1 * 255, b1 * 255, 255));
		pDrawList->PrimWriteVtx(offset + ImVec2(x2, y2), uv, IM_COL32(r1 * 255, g1 * 255, b1 * 255, 255));
		pDrawList->PrimWriteVtx(offset + ImVec2(x3, y3), uv, IM_COL32(r0 * 255, g0 * 255, b0 * 255, 255));
		angle += dAngle;
	}
	ImGui::PopID();
	return false;
}

static void HueSelectorEx(char const* label, ImVec2 const size, float* hueCenter, float* hueWidth, float* featherLeft, float* featherRight, float defaultVal, float ui_zoom, ImU32 triangleColor, int division, float alpha, float hideHueAlpha, float offset)
{
    ImGuiIO &io = ImGui::GetIO();
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
	ImVec2 curPos = ImGui::GetCursorScreenPos();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float arrowWidth = pDrawList->_Data->FontSize;
    ImRect selectorRect = ImRect(curPos, curPos + size + ImVec2(0, arrowWidth));
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##ZoneHueLineSlider", selectorRect.GetSize());
	ImGui::DrawHueBand(curPos, size, division, alpha, 1.0f, offset);
	float center = ImClamp(ImFmod(*hueCenter + offset, 1.0f), 0.0f, 1.0f - 1e-4f);
	float width = ImClamp(*hueWidth, 0.0f, 0.5f - 1e-4f);
	float featherL = ImClamp(*featherLeft, 0.0f, 0.5f - 1e-4f);
	float featherR = ImClamp(*featherRight, 0.0f, 0.5f - 1e-4f);
	float xCenter = curPos.x + center * size.x;
    ImU32 text_color = ImGui::IsItemDisabled() ? IM_COL32(255,255,0,128) : IM_COL32(255,255,0,255);
	if (width == 0.0f)
	{
		pDrawList->AddLine(ImVec2(xCenter, curPos.y), ImVec2(xCenter, curPos.y + size.y), IM_COL32(0, 0, 0, 255));
	}
	else
    {
        ImGui::DrawColorDensityPlotEx< true >(pDrawList,
            [hueAlpha = hideHueAlpha, center, width, left = featherL, right = featherR](float const xx, float const) -> ImU32
            {
                float x = ImFmod(xx, 1.0f);
                float val;
                if (x < center - width && x > center - width - left)
                {
                	val = ImClamp((center * (-1 + hueAlpha) + left + width + x - hueAlpha * (width + x)) / left, hueAlpha, 1.0f);
                }
                else if (x < center + width + right && x > center + width)
                {
                	val = ImClamp((center - center * hueAlpha + right + width - hueAlpha * width + (-1 + hueAlpha) * x) / right, hueAlpha, 1.0f);
                }
                else if (x > center - width - left && x < center + width + right)
                {
                    val = 1.0f;
                }
                else if (center + width + right > 1.0f)
                {
                	val = ImClamp((center - center * hueAlpha + right + width - hueAlpha * width + (-1 + hueAlpha) * (x + 1.0f)) / right, hueAlpha, 1.0f);
                }
                else if (center - width - left < 0.0f)
                {
                	val = ImClamp((center * (-1 + hueAlpha) + left + width + x - 1.0f - hueAlpha * (width + x - 1.0f)) / left, hueAlpha, 1.0f);
                }
                else
                {
                    val = hueAlpha;
                }
            	return IM_COL32(0, 0, 0, ImPow(1.0f - val, 1.0f / 2.2f) * 255);
            }, 0.0f, 1.0f, 0.0f, 0.0f, curPos, size, division, 1);
	}
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto diff = io.MouseDelta.x * ui_zoom / size.x;
            *hueCenter += diff;
            *hueCenter = ImClamp(*hueCenter, 0.f, 1.f);
        }
        if (io.MouseWheel < -FLT_EPSILON)
        {
            *hueWidth *= 0.9;
        }
        if (io.MouseWheel > FLT_EPSILON)
        {
            *hueWidth *= 1.1;
            if (*hueWidth > 0.5)
                *hueWidth = 0.5;
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *hueCenter = defaultVal;
        }
    }

    float arrowOffset = curPos.x + *hueCenter * size.x;
    ImGui::Dummy(ImVec2(0, arrowWidth / 2));
    ImGui::RenderArrow(pDrawList, ImVec2(arrowOffset - arrowWidth / 2, curPos.y + size.y), text_color, ImGuiDir_Up);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *hueCenter;
    std::string value_str = oss.str();
    ImVec2 str_size = ImGui::CalcTextSize(value_str.c_str(), nullptr, true);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
    pDrawList->AddText(ImVec2(curPos.x + size.x / 2 - str_size.x * 0.5f, curPos.y + size.y / 2 - arrowWidth / 2), text_color, value_str.c_str());
    ImGui::PopStyleVar();
    ImGui::EndGroup();
	ImGui::PopID();
}

void ImGui::HueSelector(char const* label, ImVec2 const size, float* hueCenter, float* hueWidth, float* featherLeft, float* featherRight, float defaultVal, float ui_zoom, int division, float alpha, float hideHueAlpha, float offset)
{
	HueSelectorEx(label, size, hueCenter, hueWidth, featherLeft, featherRight, defaultVal, ui_zoom, IM_COL32(255, 128, 0, 255), division, alpha, hideHueAlpha, offset);
}

void ImGui::LumianceSelector(char const* label, ImVec2 const size, float* lumCenter, float defaultVal, float vmin, float vmax, float ui_zoom, int division, float gamma, bool rgb_color, ImVec4 const color)
{
    ImGuiIO &io = ImGui::GetIO();
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float arrowWidth = pDrawList->_Data->FontSize;
    ImRect selectorRect = ImRect(curPos, curPos + size + ImVec2(0, arrowWidth));
    IM_ASSERT(vmax > vmin);
    float range = vmax - vmin;
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##ZoneLumianceSlider", selectorRect.GetSize());
    ImU32 text_color = ImGui::IsItemDisabled() ? IM_COL32(255,255,0,128) : IM_COL32(255,255,0,255);
    if (!rgb_color)
    {
        ImGui::DrawLumianceBand(curPos, size, division, color, gamma);
    }
    else
    {
        ImVec2 bar_size = ImVec2(size.x, size.y / 4);
        ImVec2 r_pos = curPos;
        ImVec2 g_pos = ImVec2(curPos.x, curPos.y + size.y / 4);
        ImVec2 b_pos = ImVec2(curPos.x, curPos.y + size.y * 2 / 4);
        ImVec2 w_pos = ImVec2(curPos.x, curPos.y + size.y * 3 / 4);
        ImGui::DrawLumianceBand(r_pos, bar_size, division, ImVec4(1, 0, 0, 1), gamma);
        ImGui::DrawLumianceBand(g_pos, bar_size, division, ImVec4(0, 1, 0, 1), gamma);
        ImGui::DrawLumianceBand(b_pos, bar_size, division, ImVec4(0, 0, 1, 1), gamma);
        ImGui::DrawLumianceBand(w_pos, bar_size, division, ImVec4(1, 1, 1, 1), gamma);
    }
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto diff = io.MouseDelta.x * ui_zoom / size.x;
            *lumCenter += diff * range;
            *lumCenter = ImClamp(*lumCenter, vmin, vmax);
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *lumCenter = defaultVal;
        }
    }
    float arrowOffset = curPos.x + (*lumCenter / range + 0.5) * size.x;
    ImGui::Dummy(ImVec2(0, arrowWidth / 2));
    ImGui::RenderArrow(pDrawList, ImVec2(arrowOffset - arrowWidth / 2, curPos.y + size.y), text_color, ImGuiDir_Up);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *lumCenter;
    std::string value_str = oss.str();
    ImVec2 str_size = ImGui::CalcTextSize(value_str.c_str(), nullptr, true);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
    pDrawList->AddText(ImVec2(curPos.x + size.x / 2 - str_size.x * 0.5f, curPos.y + size.y / 2 - arrowWidth / 2), text_color, value_str.c_str());
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
}

void ImGui::GammaSelector(char const* label, ImVec2 const size, float* gammaCenter, float defaultVal, float vmin, float vmax, float ui_zoom, int division)
{
    if (vmax <= vmin)
        return;
    float v_range = fabs(vmax - vmin);
    ImGuiIO &io = ImGui::GetIO();
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float arrowWidth = pDrawList->_Data->FontSize;
    ImRect selectorRect = ImRect(curPos, curPos + size + ImVec2(0, arrowWidth));
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##ZoneGammaSlider", selectorRect.GetSize());
    ImU32 text_color = ImGui::IsItemDisabled() ? IM_COL32(255,255,0,128) : IM_COL32(255,255,0,255);
	auto GammaFunc = [v_range](float const t) -> ImU32
	{
        float gamma = t * v_range;
        int color = ImPow((1 - t), gamma) * 255;
		return IM_COL32(color, color, color, 255);
	};
	ImGui::DrawColorBandEx< true >(pDrawList, curPos, size, GammaFunc, division, 1.f);
    
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto diff = io.MouseDelta.x * v_range * ui_zoom / size.x;
            *gammaCenter += diff;
            *gammaCenter = ImClamp(*gammaCenter, vmin, vmax);
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *gammaCenter = defaultVal;
        }
    }
    float arrowOffset = curPos.x + ((*gammaCenter - vmin) / v_range) * size.x;
    ImGui::Dummy(ImVec2(0, arrowWidth / 2));
    ImGui::RenderArrow(pDrawList, ImVec2(arrowOffset - arrowWidth / 2, curPos.y + size.y), text_color, ImGuiDir_Up);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *gammaCenter;
    std::string value_str = oss.str();
    ImVec2 str_size = ImGui::CalcTextSize(value_str.c_str(), nullptr, true);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
    pDrawList->AddText(ImVec2(curPos.x + size.x / 2 - str_size.x * 0.5f, curPos.y + size.y / 2 - arrowWidth / 2), text_color, value_str.c_str());
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
}

void ImGui::TemperatureSelector(char const* label, ImVec2 const size, float* tempCenter, float defaultVal, float vmin, float vmax, float ui_zoom, int division)
{
    if (vmax <= vmin)
        return;
    float v_range = fabs(vmax - vmin);
    ImGuiIO &io = ImGui::GetIO();
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float arrowWidth = pDrawList->_Data->FontSize;
    ImRect selectorRect = ImRect(curPos, curPos + size + ImVec2(0, arrowWidth));
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##ZoneTemperatureSlider", selectorRect.GetSize());
    ImU32 text_color = ImGui::IsItemDisabled() ? IM_COL32(255,255,0,128) : IM_COL32(255,255,0,255);
	auto TemperatureFunc = [](float const t) -> ImU32
	{
		return IM_COL32(t * 255, 0, (1 - t) * 255, 255);
	};
	ImGui::DrawColorBandEx< true >(pDrawList, curPos, size, TemperatureFunc, division, 1.f);
    
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto diff = io.MouseDelta.x * v_range * ui_zoom / size.x;
            *tempCenter += diff;
            *tempCenter = ImClamp(*tempCenter, vmin, vmax);
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *tempCenter = defaultVal;
        }
    }
    float arrowOffset = curPos.x + ((*tempCenter - vmin) / v_range) * size.x;
    ImGui::Dummy(ImVec2(0, arrowWidth / 2));
    ImGui::RenderArrow(pDrawList, ImVec2(arrowOffset - arrowWidth / 2, curPos.y + size.y), text_color, ImGuiDir_Up);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *tempCenter;
    std::string value_str = oss.str();
    ImVec2 str_size = ImGui::CalcTextSize(value_str.c_str(), nullptr, true);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
    pDrawList->AddText(ImVec2(curPos.x + size.x / 2 - str_size.x * 0.5f, curPos.y + size.y / 2 - arrowWidth / 2), text_color, value_str.c_str());
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
}

void ImGui::SaturationSelector(char const* label, ImVec2 const size, float* satCenter, float defaultVal, float vmin, float vmax, float ui_zoom, int division, float gamma, bool rgb_color, ImVec4 const color)
{
    ImGuiIO &io = ImGui::GetIO();
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float arrowWidth = pDrawList->_Data->FontSize;
    ImRect selectorRect = ImRect(curPos, curPos + size + ImVec2(0, arrowWidth));
    IM_ASSERT(vmax > vmin);
    float range = vmax - vmin;
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##ZoneSaturationSlider", selectorRect.GetSize());
    ImU32 text_color = ImGui::IsItemDisabled() ? IM_COL32(255,255,0,128) : IM_COL32(255,255,0,255);
    if (!rgb_color)
    {
        ImGui::DrawSaturationBand(curPos, size, division, color, gamma);
    }
    else
    {
        ImVec2 bar_size = ImVec2(size.x, size.y / 4);
        ImVec2 r_pos = curPos;
        ImVec2 g_pos = ImVec2(curPos.x, curPos.y + size.y / 4);
        ImVec2 b_pos = ImVec2(curPos.x, curPos.y + size.y * 2 / 4);
        ImVec2 w_pos = ImVec2(curPos.x, curPos.y + size.y * 3 / 4);
        ImGui::DrawSaturationBand(r_pos, bar_size, division, ImVec4(1, 0, 0, 1), gamma);
        ImGui::DrawSaturationBand(g_pos, bar_size, division, ImVec4(0, 1, 0, 1), gamma);
        ImGui::DrawSaturationBand(b_pos, bar_size, division, ImVec4(0, 0, 1, 1), gamma);
        ImGui::DrawSaturationBand(w_pos, bar_size, division, ImVec4(1, 1, 1, 1), gamma);
    }
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto diff = io.MouseDelta.x * ui_zoom / size.x;
            *satCenter += diff * range;
            *satCenter = ImClamp(*satCenter, vmin, vmax);
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *satCenter = defaultVal;
        }
    }
    float arrowOffset = curPos.x + (*satCenter / range + 0.5) * size.x;
    ImGui::Dummy(ImVec2(0, arrowWidth / 2));
    ImGui::RenderArrow(pDrawList, ImVec2(arrowOffset - arrowWidth / 2, curPos.y + size.y), text_color, ImGuiDir_Up);
	std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *satCenter;
    std::string value_str = oss.str();
    ImVec2 str_size = ImGui::CalcTextSize(value_str.c_str(), nullptr, true);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
    pDrawList->AddText(ImVec2(curPos.x + size.x / 2 - str_size.x * 0.5f, curPos.y + size.y / 2 - arrowWidth / 2), text_color, value_str.c_str());
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
}

void ImGui::ContrastSelector(char const* label, ImVec2 const size, float* conCenter, float defaultVal, float ui_zoom, bool rgb_color, ImVec4 const color)
{
    ImGuiIO &io = ImGui::GetIO();
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float arrowWidth = pDrawList->_Data->FontSize;
    ImRect selectorRect = ImRect(curPos, curPos + size + ImVec2(0, arrowWidth));
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##ZoneContrastSlider", selectorRect.GetSize());
    ImU32 text_color = ImGui::IsItemDisabled() ? IM_COL32(0,255,0,128) : IM_COL32(0,255,0,255);
    if (!rgb_color)
    {
        ImGui::DrawContrastBand(curPos, size, color);
    }
    else
    {
        ImVec2 bar_size = ImVec2(size.x, size.y / 4);
        ImVec2 r_pos = curPos;
        ImVec2 g_pos = ImVec2(curPos.x, curPos.y + size.y / 4);
        ImVec2 b_pos = ImVec2(curPos.x, curPos.y + size.y * 2 / 4);
        ImVec2 w_pos = ImVec2(curPos.x, curPos.y + size.y * 3 / 4);
        ImGui::DrawContrastBand(r_pos, bar_size, ImVec4(1, 0, 0, 1));
        ImGui::DrawContrastBand(g_pos, bar_size, ImVec4(0, 1, 0, 1));
        ImGui::DrawContrastBand(b_pos, bar_size, ImVec4(0, 0, 1, 1));
        ImGui::DrawContrastBand(w_pos, bar_size, ImVec4(1, 1, 1, 1));
    }
    if (ImGui::IsItemHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto diff = io.MouseDelta.x * 4 * ui_zoom / size.x;
            *conCenter += diff;
            *conCenter = ImClamp(*conCenter, 0.f, 4.f);
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *conCenter = defaultVal;
        }
    }
    float arrowOffset = curPos.x + (*conCenter / 4) * size.x;
    ImGui::Dummy(ImVec2(0, arrowWidth / 2));
    ImGui::RenderArrow(pDrawList, ImVec2(arrowOffset - arrowWidth / 2, curPos.y + size.y), text_color, ImGuiDir_Up);
	std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *conCenter;
    std::string value_str = oss.str();
    ImVec2 str_size = ImGui::CalcTextSize(value_str.c_str(), nullptr, true);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
    ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImVec4(0, 0, 0, 1.0));
    pDrawList->AddText(ImVec2(curPos.x + size.x / 2 - str_size.x * 0.5f, curPos.y + size.y / 2 - arrowWidth / 2), text_color, value_str.c_str());
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
}

static bool isCircleContainsPoint(ImVec2 point, float radius, ImVec2 center)
{
    float dist = (point.x - center.x) * (point.x - center.x) + (point.y - center.y) * (point.y - center.y);
    return dist <= radius * radius;
}

static float PointToAngle(float dx, float dy)
{
    float hAngle = 0;
    if (dy == 0 && dx > 0)
        hAngle = 0.f;           // positive x axis
    else if (dy == 0 && dx < 0)
        hAngle = 180.f;         // negative x axis
    else if (dx == 0 && dy < 0)
        hAngle = 270.f;         // negative y axis
    else if (dx == 0 && dy > 0)
        hAngle = 90.f;          // positive y axis
    else if (dx > 0 && dy > 0)
        hAngle = 90.f - atan2(dx, dy) * 180.f / M_PI;   // first quadrant
    else if (dx < 0 && dy > 0)
        hAngle = 90.f + atan2(-dx, dy) * 180.f / M_PI;  // second quadrant
    else if (dx < 0 && dy < 0)
        hAngle = 270.f - atan2(-dx, -dy) * 180.f / M_PI;// third quadrant
    else if (dx > 0 && dy < 0)
        hAngle = 270.f + atan2(dx, -dy) * 180.f / M_PI; // fourth quadrant
    return hAngle;
}

static ImVec2 AngleToPoint(float angle, float length)
{
    ImVec2 point(0, 0);
    float hAngle = angle * M_PI / 180.f;
    if (angle == 0.f)
        point = ImVec2(length, 0);  // positive x axis
    else if (angle == 180.f)
        point = ImVec2(-length, 0); // negative x axis
    else if (angle == 90.f)
        point = ImVec2(0, length); // positive y axis
    else if (angle == 270.f)
        point = ImVec2(0, -length);  // negative y axis
    else
        point = ImVec2(length * cos(hAngle), length * sin(hAngle));
    return point;
}

bool ImGui::BalanceSelector(char const* label, ImVec2 const size, ImVec4 * rgba, ImVec4 defaultVal, ImVec2* offset, float ui_zoom, float speed, int division, float thickness, float colorOffset)
{
    bool reset = false;
    ImGuiIO &io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
	ImGuiID const iID = ImGui::GetID(label);
	ImGui::PushID(iID);
    auto curPos = ImGui::GetCursorScreenPos();
    const float ringDiameter = size.x > size.y ? size.x : size.y;
    const float ringRadius = ringDiameter / 2;
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const float sliderHeight = pDrawList->_Data->FontSize;
    const bool is_Enabled = !ImGui::IsItemDisabled();
    ImGui::BeginGroup();
    auto str_size = ImGui::CalcTextSize(label);
    auto text_pos = ImVec2(curPos.x + size.x / 2 - str_size.x / 2, curPos.y);
    pDrawList->AddText(text_pos, IM_COL32_WHITE, label);
    auto ringPos = curPos + ImVec2(0, sliderHeight + 5);
    auto center_point = ringPos + ImVec2(ringRadius, ringRadius);
    auto ringSize = ImVec2(size.x, ringDiameter + 10);
    ImGui::SetCursorScreenPos(ringPos);
    ImGuiID const iIDr = ImGui::GetID("##ZoneBalanceSlider");
    ImGui::InvisibleButton("##ZoneBalanceSlider", ringSize);
    pDrawList->AddCircle(center_point, ringRadius + 2, IM_COL32_WHITE, 0, 2);
    ImGui::DrawColorRingEx< true >(pDrawList, ringPos, ImVec2(ringDiameter, ringDiameter), thickness, [rgba, is_Enabled](float t)
	{
		float r, g, b;
		ImGui::ColorConvertHSVtoRGB(t, 1.0f, 1.0f, r, g, b);
		return IM_COL32(r * 255, g * 255, b * 255, is_Enabled ? 255 : 144);
	}, division, colorOffset);
    
    auto RGBToPoint = [](ImVec4 rgba)
    {
        float l = 0, r = rgba.x, g = rgba.y, b = rgba.z;;
        if (std::min(r, std::min(g, b)) == r)
        { l = -r; r = 1 - l; g = g + 1 - l; b = b + 1 - l; }
        if (std::min(r, std::min(g, b)) == b)
        { l = -b; b = 1 - l; g = g + 1 - l; r = r + 1 - l; }
        if (std::min(r, std::min(g, b)) == g)
        { l = -g; g = 1 - l; b = b + 1 - l; r = r + 1 - l; }
        if (rgba.x == 0 && rgba.y == 0 && rgba.z == 0)
        {
            r = 1.f; b = 1.f; g = 1.f; l = 0.f;
        }
        float angle, length, v;
        ImGui::ColorConvertRGBtoHSV(r, g, b, angle, length, v);
        angle = angle * 360;
        return AngleToPoint(angle, length);
    };
    auto pointToRGB = [](float x, float y)
    {
        float a = PointToAngle(x, y);
        float l = sqrt(x * x + y * y);
        l = ImClamp(l, 0.f, 1.f);
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(a / 360.0, l, 1.0f, r, g, b);
        if (std::min(r, std::min(g, b)) == r)
        { r = -l; g = g + l - 1.f; b = b + l - 1.0f; }
        if (std::min(r, std::min(g, b)) == g)
        { g = -l; r = r + l - 1.f; b = b + l - 1.0f; }
        if (std::min(r, std::min(g, b)) == b)
        { b = -l; r = r + l - 1.f; g = g + l - 1.0f; }
        if (x == 0 && y == 0)
        {
            r = 0; g = 0; b = 0;
        }
        return ImVec4(r, g, b, 1.0);
    };

    auto point = RGBToPoint(*rgba);
    point = ImVec2(point.x * ringRadius, -point.y * ringRadius);
    pDrawList->AddCircle(center_point + point, 3, IM_COL32_BLACK);
    pDrawList->AddCircle(center_point + point, 2, IM_COL32_WHITE);

    if (is_Enabled)
    {
        if (isCircleContainsPoint(io.MousePos, ringRadius, center_point))
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                *rgba = defaultVal;
                if (offset)
                {
                    offset->x = 0;
                    offset->y = 0;
                }
                reset = true;
            }
            else if (g.ActiveId == iIDr && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float x_offset = ( io.MouseDelta.x * speed + point.x) / ringRadius;
                float y_offset = (-io.MouseDelta.y * speed - point.y) / ringRadius;
                if (offset)
                {
                    offset->x = io.MouseDelta.x;
                    offset->y = io.MouseDelta.y;
                }
                *rgba = pointToRGB(x_offset, y_offset);
            }
        }
        else if (offset)
        {
            float x_offset = ( offset->x * speed + point.x) / ringRadius;
            float y_offset = (-offset->y * speed - point.y) / ringRadius;
            *rgba = pointToRGB(x_offset, y_offset);
        }
        if (offset && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            offset->x = 0;
            offset->y = 0;
        }
    }

    ImGui::SetCursorScreenPos(ringPos + ImVec2(0, ringSize.y));
    ImGui::SetWindowFontScale(0.7);
    ImGui::PushItemWidth(size.x / 3);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 0, 0, 128));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(255, 64, 64, 128));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(255, 128, 128, 128));
	ImGui::DragFloat("##R##BalanceSelector", &rgba->x, 0.005f, -1.0f, 1.0f, "%.2f"); ImGui::SameLine();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 255, 0, 128));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(64, 255, 64, 128));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(128, 255, 128, 128));
	ImGui::DragFloat("##G##BalanceSelector", &rgba->y, 0.005f, -1.0f, 1.0f, "%.2f"); ImGui::SameLine();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 255, 128));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(64, 64, 255, 128));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(128, 128, 255, 128));
    ImGui::DragFloat("##B##BalanceSelector", &rgba->z, 0.005f, -1.0f, 1.0f, "%.2f");
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::SetWindowFontScale(1.0);
    ImGui::EndGroup();
    ImGui::PopID();
    return reset;
}

//////////////////////////////////////////////////////////////////////////
// Interactions
//////////////////////////////////////////////////////////////////////////
enum ImTriangulatorNodeType0
{
	ImTriangulatorNodeType_Convex,
	ImTriangulatorNodeType_Ear,
	ImTriangulatorNodeType_Reflex
};

struct ImTriangulatorNode0
{
	ImTriangulatorNodeType0  Type;
	int                     Index;
	ImVec2                  Pos;
	ImTriangulatorNode0* Next;
	ImTriangulatorNode0* Prev;
	void    Unlink()
	{
		Next->Prev = Prev; Prev->Next = Next;
	}
};

struct ImTriangulatorNodeSpan0
{
	ImTriangulatorNode0** Data = NULL;
	int                     Size = 0;
	void    push_back( ImTriangulatorNode0* node )
	{
		Data[ Size++ ] = node;
	}
	void    find_erase_unsorted( int idx )
	{
		for ( int i = Size - 1; i >= 0; i-- ) if ( Data[ i ]->Index == idx )
		{
			Data[ i ] = Data[ Size - 1 ]; Size--; return;
		}
	}
};

struct ImTriangulator0
{
	static int EstimateTriangleCount( int points_count )
	{
		return ( points_count < 3 ) ? 0 : points_count - 2;
	}
	static int EstimateScratchBufferSize( int points_count )
	{
		return sizeof( ImTriangulatorNode0 ) * points_count + sizeof( ImTriangulatorNode0* ) * points_count * 2;
	}
	void    Init( const std::vector<ImVec2>& points, void* scratch_buffer )
    {
		IM_ASSERT( scratch_buffer != NULL && points.size() >= 3 );
		_TrianglesLeft = EstimateTriangleCount( points.size() );
		_Nodes = ( ImTriangulatorNode0* )scratch_buffer;                          // points_count x Node
		_Ears.Data = ( ImTriangulatorNode0** )( _Nodes + points.size() );                // points_count x Node*
		_Reflexes.Data = ( ImTriangulatorNode0** )( _Nodes + points.size() ) + points.size(); // points_count x Node*
		BuildNodes( points );
		BuildReflexes();
		BuildEars();
	}
	void    GetNextTriangle( unsigned int out_triangle[ 3 ] )     // Return relative indexes for next triangle
    {
		if ( _Ears.Size == 0 )
		{
			FlipNodeList();

			ImTriangulatorNode0* node = _Nodes;
			for ( int i = _TrianglesLeft; i >= 0; i--, node = node->Next )
				node->Type = ImTriangulatorNodeType_Convex;
			_Reflexes.Size = 0;
			BuildReflexes();
			BuildEars();

			// If we still don't have ears, it means geometry is degenerated.
			if ( _Ears.Size == 0 )
			{
				// Return first triangle available, mimicking the behavior of convex fill.
				IM_ASSERT( _TrianglesLeft > 0 ); // Geometry is degenerated
				_Ears.Data[ 0 ] = _Nodes;
				_Ears.Size = 1;
			}
		}

		ImTriangulatorNode0* ear = _Ears.Data[ --_Ears.Size ];
		out_triangle[ 0 ] = ear->Prev->Index;
		out_triangle[ 1 ] = ear->Index;
		out_triangle[ 2 ] = ear->Next->Index;

		ear->Unlink();
		if ( ear == _Nodes )
			_Nodes = ear->Next;

		ReclassifyNode( ear->Prev );
		ReclassifyNode( ear->Next );
		_TrianglesLeft--;
	}
	// Internal functions
	void    BuildNodes( const std::vector<ImVec2>& points )
    {
		for ( int i = 0; i < points.size(); i++ )
		{
			_Nodes[ i ].Type = ImTriangulatorNodeType_Convex;
			_Nodes[ i ].Index = i;
			_Nodes[ i ].Pos = points[ i ];
			_Nodes[ i ].Next = _Nodes + i + 1;
			_Nodes[ i ].Prev = _Nodes + i - 1;
		}
		_Nodes[ 0 ].Prev = _Nodes + points.size() - 1;
		_Nodes[ points.size() - 1 ].Next = _Nodes;
	}
	void    BuildReflexes()
    {
		ImTriangulatorNode0* n1 = _Nodes;
		for ( int i = _TrianglesLeft; i >= 0; i--, n1 = n1->Next )
		{
			if ( ImTriangleIsClockwise( n1->Prev->Pos, n1->Pos, n1->Next->Pos ) )
				continue;
			n1->Type = ImTriangulatorNodeType_Reflex;
			_Reflexes.push_back( n1 );
		}
	}
	void    BuildEars()
    {
		ImTriangulatorNode0* n1 = _Nodes;
		for ( int i = _TrianglesLeft; i >= 0; i--, n1 = n1->Next )
		{
			if ( n1->Type != ImTriangulatorNodeType_Convex )
				continue;
			if ( !IsEar( n1->Prev->Index, n1->Index, n1->Next->Index, n1->Prev->Pos, n1->Pos, n1->Next->Pos ) )
				continue;
			n1->Type = ImTriangulatorNodeType_Ear;
			_Ears.push_back( n1 );
		}
	}
	void    FlipNodeList()
    {
		ImTriangulatorNode0* prev = _Nodes;
		ImTriangulatorNode0* temp = _Nodes;
		ImTriangulatorNode0* current = _Nodes->Next;
		prev->Next = prev;
		prev->Prev = prev;
		while ( current != _Nodes )
		{
			temp = current->Next;

			current->Next = prev;
			prev->Prev = current;
			_Nodes->Next = current;
			current->Prev = _Nodes;

			prev = current;
			current = temp;
		}
		_Nodes = prev;
	}
	bool    IsEar( int i0, int i1, int i2, const ImVec2& v0, const ImVec2& v1, const ImVec2& v2 ) const
    {
		ImTriangulatorNode0** p_end = _Reflexes.Data + _Reflexes.Size;
		for ( ImTriangulatorNode0** p = _Reflexes.Data; p < p_end; p++ )
		{
			ImTriangulatorNode0* reflex = *p;
			if ( reflex->Index != i0 && reflex->Index != i1 && reflex->Index != i2 )
				if ( ImTriangleContainsPoint( v0, v1, v2, reflex->Pos ) )
					return false;
		}
		return true;
	}
	void    ReclassifyNode( ImTriangulatorNode0* node )
    {
		// Classify node
		ImTriangulatorNodeType0 type;
		const ImTriangulatorNode0* n0 = node->Prev;
		const ImTriangulatorNode0* n2 = node->Next;
		if ( !ImTriangleIsClockwise( n0->Pos, node->Pos, n2->Pos ) )
			type = ImTriangulatorNodeType_Reflex;
		else if ( IsEar( n0->Index, node->Index, n2->Index, n0->Pos, node->Pos, n2->Pos ) )
			type = ImTriangulatorNodeType_Ear;
		else
			type = ImTriangulatorNodeType_Convex;

		// Update lists when a type changes
		if ( type == node->Type )
			return;
		if ( node->Type == ImTriangulatorNodeType_Reflex )
			_Reflexes.find_erase_unsorted( node->Index );
		else if ( node->Type == ImTriangulatorNodeType_Ear )
			_Ears.find_erase_unsorted( node->Index );
		if ( type == ImTriangulatorNodeType_Reflex )
			_Reflexes.push_back( node );
		else if ( type == ImTriangulatorNodeType_Ear )
			_Ears.push_back( node );
		node->Type = type;
	}

	// Internal members
	int                     _TrianglesLeft = 0;
	ImTriangulatorNode0* _Nodes = NULL;
	ImTriangulatorNodeSpan0  _Ears;
	ImTriangulatorNodeSpan0  _Reflexes;
};

static bool IsTriangleContains( ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 p )
{
	// Compute vectors
	ImVec2 v0 = { c.x - a.x, c.y - a.y };
	ImVec2 v1 = { b.x - a.x, b.y - a.y };
	ImVec2 v2 = { p.x - a.x, p.y - a.y };
	// Compute dot products
	float dot00 = v0.x * v0.y + v0.y * v0.y;
	float dot01 = v0.x * v1.x + v0.y * v1.y;
	float dot02 = v0.x * v2.x + v0.y * v2.y;
	float dot11 = v1.x * v1.x + v1.y * v1.y;
	float dot12 = v1.x * v2.x + v1.y * v2.y;
	// Compute barycentric coordinates
	float invDenom = 1.0f / ( dot00 * dot11 - dot01 * dot01 );
	float u = ( dot11 * dot02 - dot01 * dot12 ) * invDenom;
	float v = ( dot00 * dot12 - dot01 * dot02 ) * invDenom;
	// Check if point is in triangle
	return ( u >= 0.0f ) && ( v >= 0.0f ) && ( u + v < 1.0f );
}

bool ImGui::IsPolyConvexContains(const std::vector<ImVec2>& pts, ImVec2 p )
{
	IM_ASSERT( pts.size() >= 3 );
	for ( int k = 2; k < pts.size(); ++k )
	{
		ImVec2 a = pts[ 0 ];
		ImVec2 b = pts[ k - 1 ];
		ImVec2 c = pts[ k ];
		if ( ImTriangleContainsPoint( a, b, c, p ) )
			return true;
	}
	return false;
}

bool ImGui::IsPolyConcaveContains(const std::vector<ImVec2>& pts, ImVec2 p )
{
	IM_ASSERT( pts.size() >= 3 );
	ImRect bb;
	bb.Min = ImVec2( FLT_MAX, FLT_MAX );
	bb.Max = ImVec2( -FLT_MAX, -FLT_MAX );
	for ( int k = 0; k < pts.size(); ++k )
	{
		bb.Min.x = ImMin( bb.Min.x, pts[ k ].x );
		bb.Min.y = ImMin( bb.Min.y, pts[ k ].y );
		bb.Max.x = ImMax( bb.Max.x, pts[ k ].x );
		bb.Max.y = ImMax( bb.Max.y, pts[ k ].y );
	}
	if ( !bb.ContainsWithPad( p, ImVec2( 1.0f, 1.0f ) ) )
		return false;
	ImDrawListSharedData* _Data = ImGui::GetCurrentWindow()->DrawList->_Data;
	ImTriangulator0 triangulator;
	unsigned int triangle[ 3 ];
	{
		// Non Anti-aliased Fill
		const int idx_count = ( pts.size() - 2 ) * 3;
		triangulator.Init( pts, _Data->TempBuffer.Data );
		while ( triangulator._TrianglesLeft > 0 )
		{
			triangulator.GetNextTriangle( triangle );
			if ( ImTriangleContainsPoint( pts[ triangle[ 0 ] ], pts[ triangle[ 1 ] ], pts[ triangle[ 2 ] ], p ) )
				return true;
		}
	}
	return false;
}

bool ImGui::IsPolyWithHoleContains( const std::vector<ImVec2>& pts, ImVec2 p, ImRect* p_bb, int gap, int strokeWidth )
{
	ImVector<ImVec2> scanHits;
	ImVec2 min, max; // polygon min/max points
	float y;
	bool isMinMaxDone = false;
	// find the orthagonal bounding box
	// probably can put this as a predefined
	if ( !isMinMaxDone )
	{
		min.x = min.y = FLT_MAX;
		max.x = max.y = FLT_MIN;
		//for ( auto p : poly )
		for ( int i = 0; i < pts.size(); ++i )
		{
			ImVec2 p = pts[ i ];
			if ( p.x < min.x )
				min.x = p.x;
			if ( p.y < min.y )
				min.y = p.y;
			if ( p.x > max.x )
				max.x = p.x;
			if ( p.y > max.y )
				max.y = p.y;
		}
		isMinMaxDone = true;
	}
	ImRect bb;
	if ( p_bb == NULL )
	{
		bb.Min = min;
		bb.Max = max;
	}
	else
	{
		bb = *p_bb;
	}
	if ( !bb.ContainsWithPad( p, ImVec2( 1.0f, 1.0f ) ) )
		return false;
	// Bounds check
	if ( ( max.x < 0 ) || ( max.y < 0 ) )
		return false;
	// Vertically clip
	if ( min.y <= 0 )
		min.y = 0;
	// so we know we start on the outside of the object we step out by 1.
	min.x -= 1;
	max.x += 1;
	// Initialise our starting conditions
	y = min.y;
	struct ImVec2CompX
	{
		static int IMGUI_CDECL Comp( const void* lhs, const void* rhs )
		{
			if ( ( ( const ImVec2* )lhs )->x > ( ( const ImVec2* )rhs )->x )
				return +1;
			if ( ( ( const ImVec2* )lhs )->x < ( ( const ImVec2* )rhs )->x )
				return -1;
			return 0;
		}
	};
	while ( y < max.y )
	{
		scanHits.clear();
		{
			int jump = 1;
			ImVec2 fp = pts[ 0 ];
			for ( size_t i = 0; i < pts.size() - 1; i++ )
			{
				ImVec2 pa = pts[ i ];
				ImVec2 pb = pts[ i + 1 ];
				// jump double/dud points
				if ( pa.x == pb.x && pa.y == pb.y ) continue;
				// if we encounter our hull/poly start point, then we've now created the
				// closed
				// hull, jump the next segment and reset the first-point
				if ( ( !jump ) && ( fp.x == pb.x ) && ( fp.y == pb.y ) )
				{
					if ( i < pts.size() - 2 )
					{
						fp = pts[ i + 2 ];
						jump = 1;
						i++;
					}
				}
				else
				{
					jump = 0;
				}
				// test to see if this segment makes the scan-cut.
				if ( ( pa.y > pb.y && y < pa.y && y > pb.y ) || ( pa.y < pb.y && y > pa.y && y < pb.y ) )
				{
					ImVec2 intersect;
					intersect.y = y;
					if ( pa.x == pb.x )
					{
						intersect.x = pa.x;
					}
					else
					{
						intersect.x = ( pb.x - pa.x ) / ( pb.y - pa.y ) * ( y - pa.y ) + pa.x;
					}
					scanHits.push_back( intersect );
				}
			}
			// Sort the scan hits by X, so we have a proper left->right ordering
			ImQsort( &scanHits[ 0 ], scanHits.size(), sizeof( ImVec2 ), &ImVec2CompX::Comp );
			// generate the line segments.
			int i = 0;
			int l = scanHits.size() - 1; // we need pairs of points, this prevents segfault.
			for ( i = 0; i < l; i += 2 )
			{
				ImVec2 a = scanHits[ i ];
				ImVec2 b = scanHits[ i + 1 ];
				if ( ImRect( a, b ).ContainsWithPad( p, ImVec2( 0.0f, ( float )strokeWidth ) ) )
				{
					scanHits.clear();
					return true;
				}
			}
		}
		y += gap;
	}
	return false;
}

bool ImGui::IsMouseHoveringPolyConvex( const ImVec2& r_min, const ImVec2& r_max, const std::vector<ImVec2>& pts, bool clip )
{
	bool well_form = true;
	for ( int k = 0; k < pts.size(); ++k )
	{
		well_form &= ImRect( r_min, r_max ).Contains( pts[ k ] );
	}
	IM_ASSERT( well_form );
	ImGuiContext& g = *ImGui::GetCurrentContext();
	// Clip
	ImRect rect_clipped( r_min, r_max );
	if ( clip )
		rect_clipped.ClipWith( g.CurrentWindow->ClipRect );
	// Hit testing, expanded for touch input
	if ( !rect_clipped.ContainsWithPad( g.IO.MousePos, g.Style.TouchExtraPadding ) )
		return false;
	if ( !IsPolyConvexContains( pts, g.IO.MousePos ) )
		return false;
	return true;
}

bool ImGui::ItemHoverablePolyConvex( const ImRect& bb, ImGuiID id, const std::vector<ImVec2>& pts, ImGuiItemFlags item_flags )
{
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	bool well_form = true;
	for ( int k = 0; k < pts.size(); ++k )
	{
		well_form &= bb.Contains( pts[ k ] );
	}
	IM_ASSERT( well_form );
	if ( g.HoveredWindow != window )
		return false;
	if ( !ImGui::IsMouseHoveringRect( bb.Min, bb.Max ) )
		return false;
	if ( !IsMouseHoveringPolyConvex( bb.Min, bb.Max, pts ) )
		return false;
	if ( g.HoveredId != 0 && g.HoveredId != id && !g.HoveredIdAllowOverlap )
		return false;
	if ( g.ActiveId != 0 && g.ActiveId != id && !g.ActiveIdAllowOverlap )
		if ( !g.ActiveIdFromShortcut )
			return false;
	// Done with rectangle culling so we can perform heavier checks now.
	if ( !( item_flags & ImGuiItemFlags_NoWindowHoverableCheck ) && !ImGui::IsWindowContentHoverable( window, ImGuiHoveredFlags_None ) )
	{
		g.HoveredIdIsDisabled = true;
		return false;
	}
	// We exceptionally allow this function to be called with id==0 to allow using it for easy high-level
	// hover test in widgets code. We could also decide to split this function is two.
	if ( id != 0 )
	{
		// Drag source doesn't report as hovered
		if ( g.DragDropActive && g.DragDropPayload.SourceId == id && !( g.DragDropSourceFlags & ImGuiDragDropFlags_SourceNoDisableHover ) )
			return false;
		ImGui::SetHoveredID( id );
		// AllowOverlap mode (rarely used) requires previous frame HoveredId to be null or to match.
		// This allows using patterns where a later submitted widget overlaps a previous one. Generally perceived as a front-to-back hit-test.
		if ( item_flags & ImGuiItemFlags_AllowOverlap )
		{
			g.HoveredIdAllowOverlap = true;
			if ( g.HoveredIdPreviousFrame != id )
				return false;
		}
		// Display shortcut (only works with mouse)
		if ( id == g.LastItemData.ID && ( g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HasShortcut ) )
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNormal ) )
				ImGui::SetTooltip( "%s", ImGui::GetKeyChordName( g.LastItemData.Shortcut ) );
	}
	// When disabled we'll return false but still set HoveredId
	if ( item_flags & ImGuiItemFlags_Disabled )
	{
		// Release active id if turning disabled
		if ( g.ActiveId == id && id != 0 )
			ImGui::ClearActiveID();
		g.HoveredIdIsDisabled = true;
		return false;
	}

#ifndef IMGUI_DISABLE_DEBUG_TOOLS
	if ( id != 0 )
	{
		// [DEBUG] Item Picker tool!
		// We perform the check here because reaching is path is rare (1~ time a frame),
		// making the cost of this tool near-zero! We could get better call-stack and support picking non-hovered
		// items if we performed the test in ItemAdd(), but that would incur a bigger runtime cost.
		if ( g.DebugItemPickerActive && g.HoveredIdPreviousFrame == id )
			ImGui::GetForegroundDrawList()->AddRect( bb.Min, bb.Max, IM_COL32( 255, 255, 0, 255 ) );
		if ( g.DebugItemPickerBreakId == id )
			IM_DEBUG_BREAK();
	}
#endif

	if ( g.NavDisableMouseHover )
		return false;
	return true;
}

bool ImGui::IsMouseHoveringPolyConcave( const ImVec2& r_min, const ImVec2& r_max, const std::vector<ImVec2>& pts, bool clip )
{
	bool well_form = true;
	for ( int k = 0; k < pts.size(); ++k )
	{
		well_form &= ImRect( r_min, r_max ).Contains( pts[ k ] );
	}
	IM_ASSERT( well_form );
	ImGuiContext& g = *ImGui::GetCurrentContext();
	// Clip
	ImRect rect_clipped( r_min, r_max );
	if ( clip )
		rect_clipped.ClipWith( g.CurrentWindow->ClipRect );
	// Hit testing, expanded for touch input
	if ( !rect_clipped.ContainsWithPad( g.IO.MousePos, g.Style.TouchExtraPadding ) )
		return false;
	if ( !IsPolyConcaveContains( pts, g.IO.MousePos ) )
		return false;
	return true;
}

bool ImGui::ItemHoverablePolyConcave( const ImRect& bb, ImGuiID id, const std::vector<ImVec2>& pts, ImGuiItemFlags item_flags )
{
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	bool well_form = true;
	for ( int k = 0; k < pts.size(); ++k )
	{
		well_form &= bb.Contains( pts[ k ] );
	}
	IM_ASSERT( well_form );
	if ( g.HoveredWindow != window )
		return false;
	if ( !ImGui::IsMouseHoveringRect( bb.Min, bb.Max ) )
		return false;
	if ( !IsMouseHoveringPolyConcave( bb.Min, bb.Max, pts ) )
		return false;
	if ( g.HoveredId != 0 && g.HoveredId != id && !g.HoveredIdAllowOverlap )
		return false;
	if ( g.ActiveId != 0 && g.ActiveId != id && !g.ActiveIdAllowOverlap )
		if ( !g.ActiveIdFromShortcut )
			return false;
	// Done with rectangle culling so we can perform heavier checks now.
	if ( !( item_flags & ImGuiItemFlags_NoWindowHoverableCheck ) && !ImGui::IsWindowContentHoverable( window, ImGuiHoveredFlags_None ) )
	{
		g.HoveredIdIsDisabled = true;
		return false;
	}
	// We exceptionally allow this function to be called with id==0 to allow using it for easy high-level
	// hover test in widgets code. We could also decide to split this function is two.
	if ( id != 0 )
	{
		// Drag source doesn't report as hovered
		if ( g.DragDropActive && g.DragDropPayload.SourceId == id && !( g.DragDropSourceFlags & ImGuiDragDropFlags_SourceNoDisableHover ) )
			return false;
		ImGui::SetHoveredID( id );
		// AllowOverlap mode (rarely used) requires previous frame HoveredId to be null or to match.
		// This allows using patterns where a later submitted widget overlaps a previous one. Generally perceived as a front-to-back hit-test.
		if ( item_flags & ImGuiItemFlags_AllowOverlap )
		{
			g.HoveredIdAllowOverlap = true;
			if ( g.HoveredIdPreviousFrame != id )
				return false;
		}
		// Display shortcut (only works with mouse)
		if ( id == g.LastItemData.ID && ( g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HasShortcut ) )
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNormal ) )
				ImGui::SetTooltip( "%s", ImGui::GetKeyChordName( g.LastItemData.Shortcut ) );
	}
	// When disabled we'll return false but still set HoveredId
	if ( item_flags & ImGuiItemFlags_Disabled )
	{
		// Release active id if turning disabled
		if ( g.ActiveId == id && id != 0 )
			ImGui::ClearActiveID();
		g.HoveredIdIsDisabled = true;
		return false;
	}

#ifndef IMGUI_DISABLE_DEBUG_TOOLS
	if ( id != 0 )
	{
		// [DEBUG] Item Picker tool!
		// We perform the check here because reaching is path is rare (1~ time a frame),
		// making the cost of this tool near-zero! We could get better call-stack and support picking non-hovered
		// items if we performed the test in ItemAdd(), but that would incur a bigger runtime cost.
		if ( g.DebugItemPickerActive && g.HoveredIdPreviousFrame == id )
			ImGui::GetForegroundDrawList()->AddRect( bb.Min, bb.Max, IM_COL32( 255, 255, 0, 255 ) );
		if ( g.DebugItemPickerBreakId == id )
			IM_DEBUG_BREAK();
	}
#endif

	if ( g.NavDisableMouseHover )
		return false;
	return true;
}

bool ImGui::IsMouseHoveringPolyWithHole( const ImVec2& r_min, const ImVec2& r_max, const std::vector<ImVec2>& pts, bool clip )
{
	bool well_form = true;
	for ( int k = 0; k < pts.size(); ++k )
	{
		well_form &= ImRect( r_min, r_max ).Contains( pts[ k ] );
	}
	IM_ASSERT( well_form );
	ImGuiContext& g = *ImGui::GetCurrentContext();
	// Clip
	ImRect rect_clipped( r_min, r_max );
	if ( clip )
		rect_clipped.ClipWith( g.CurrentWindow->ClipRect );
	// Hit testing, expanded for touch input
	if ( !rect_clipped.ContainsWithPad( g.IO.MousePos, g.Style.TouchExtraPadding ) )
		return false;
	if ( !IsPolyWithHoleContains( pts, g.IO.MousePos ) )
		return false;
	return true;
}

bool ImGui::ItemHoverablePolyWithHole( const ImRect& bb, ImGuiID id, const std::vector<ImVec2>& pts, ImGuiItemFlags item_flags )
{
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	bool well_form = true;
	for ( int k = 0; k < pts.size(); ++k )
	{
		well_form &= bb.Contains( pts[ k ] );
	}
	IM_ASSERT( well_form );
	if ( g.HoveredWindow != window )
		return false;
	if ( !ImGui::IsMouseHoveringRect( bb.Min, bb.Max ) )
		return false;
	if ( !IsMouseHoveringPolyWithHole( bb.Min, bb.Max, pts ) )
		return false;
	if ( g.HoveredId != 0 && g.HoveredId != id && !g.HoveredIdAllowOverlap )
		return false;
	if ( g.ActiveId != 0 && g.ActiveId != id && !g.ActiveIdAllowOverlap )
		if ( !g.ActiveIdFromShortcut )
			return false;
	// Done with rectangle culling so we can perform heavier checks now.
	if ( !( item_flags & ImGuiItemFlags_NoWindowHoverableCheck ) && !ImGui::IsWindowContentHoverable( window, ImGuiHoveredFlags_None ) )
	{
		g.HoveredIdIsDisabled = true;
		return false;
	}
	// We exceptionally allow this function to be called with id==0 to allow using it for easy high-level
	// hover test in widgets code. We could also decide to split this function is two.
	if ( id != 0 )
	{
		// Drag source doesn't report as hovered
		if ( g.DragDropActive && g.DragDropPayload.SourceId == id && !( g.DragDropSourceFlags & ImGuiDragDropFlags_SourceNoDisableHover ) )
			return false;
		ImGui::SetHoveredID( id );
		// AllowOverlap mode (rarely used) requires previous frame HoveredId to be null or to match.
		// This allows using patterns where a later submitted widget overlaps a previous one. Generally perceived as a front-to-back hit-test.
		if ( item_flags & ImGuiItemFlags_AllowOverlap )
		{
			g.HoveredIdAllowOverlap = true;
			if ( g.HoveredIdPreviousFrame != id )
				return false;
		}
		// Display shortcut (only works with mouse)
		if ( id == g.LastItemData.ID && ( g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HasShortcut ) )
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNormal ) )
				ImGui::SetTooltip( "%s", ImGui::GetKeyChordName( g.LastItemData.Shortcut ) );
	}
	// When disabled we'll return false but still set HoveredId
	if ( item_flags & ImGuiItemFlags_Disabled )
	{
		// Release active id if turning disabled
		if ( g.ActiveId == id && id != 0 )
			ImGui::ClearActiveID();
		g.HoveredIdIsDisabled = true;
		return false;
	}

#ifndef IMGUI_DISABLE_DEBUG_TOOLS
	if ( id != 0 )
	{
		// [DEBUG] Item Picker tool!
		// We perform the check here because reaching is path is rare (1~ time a frame),
		// making the cost of this tool near-zero! We could get better call-stack and support picking non-hovered
		// items if we performed the test in ItemAdd(), but that would incur a bigger runtime cost.
		if ( g.DebugItemPickerActive && g.HoveredIdPreviousFrame == id )
			ImGui::GetForegroundDrawList()->AddRect( bb.Min, bb.Max, IM_COL32( 255, 255, 0, 255 ) );
		if ( g.DebugItemPickerBreakId == id )
			IM_DEBUG_BREAK();
	}
#endif

	if ( g.NavDisableMouseHover )
		return false;
	return true;
}

void ImGui::DrawShapeWithHole( ImDrawList* draw, const std::vector<ImVec2>& poly, ImU32 color, ImRect* p_bb, int gap, int strokeWidth )
{
	ImVector<ImVec2> scanHits;
	ImVec2 min, max; // polygon min/max points
	ImGuiIO io = ImGui::GetIO();
	float y;
	bool isMinMaxDone = false;
	// find the orthagonal bounding box
	// probably can put this as a predefined
	if ( !isMinMaxDone )
	{
		min.x = min.y = FLT_MAX;
		max.x = max.y = FLT_MIN;
		//for ( auto p : poly )
		for ( int i = 0; i < poly.size(); ++i )
		{
			ImVec2 p = poly[ i ];
			if ( p.x < min.x )
				min.x = p.x;
			if ( p.y < min.y )
				min.y = p.y;
			if ( p.x > max.x )
				max.x = p.x;
			if ( p.y > max.y )
				max.y = p.y;
		}
		isMinMaxDone = true;
	}
	ImRect bb;
	if ( p_bb == NULL )
	{
		bb.Min = min;
		bb.Max = max;
	}
	else
	{
		bb = *p_bb;
	}
	// Bounds check
	if ( ( max.x < 0 ) || ( min.x > io.DisplaySize.x ) || ( max.y < 0 ) || ( min.y > io.DisplaySize.y ) )
		return;
	// Vertically clip
	if ( min.y <= 0 )
		min.y = 0;
	if ( max.y > io.DisplaySize.y )
		max.y = io.DisplaySize.y;
	// so we know we start on the outside of the object we step out by 1.
	min.x -= 1;
	max.x += 1;
	// Initialise our starting conditions
	y = min.y;
	struct ImVec2CompX
	{
		static int IMGUI_CDECL Comp( const void* lhs, const void* rhs )
		{
			if ( ( ( const ImVec2* )lhs )->x > ( ( const ImVec2* )rhs )->x )
				return +1;
			if ( ( ( const ImVec2* )lhs )->x < ( ( const ImVec2* )rhs )->x )
				return -1;
			return 0;
		}
	};
	draw->PushClipRect( bb.Min, bb.Max, true );
	// Go through each scan line iteratively, jumping by 'gap' pixels each time
	while ( y < max.y )
	{
		scanHits.clear();
		{
			int jump = 1;
			ImVec2 fp = poly[ 0 ];
			for ( size_t i = 0; i < poly.size() - 1; i++ )
			{
				ImVec2 pa = poly[ i ];
				ImVec2 pb = poly[ i + 1 ];
				// jump double/dud points
				if ( pa.x == pb.x && pa.y == pb.y ) continue;
				// if we encounter our hull/poly start point, then we've now created the
				// closed
				// hull, jump the next segment and reset the first-point
				if ( ( !jump ) && ( fp.x == pb.x ) && ( fp.y == pb.y ) )
				{
					if ( i < poly.size() - 2 )
					{
						fp = poly[ i + 2 ];
						jump = 1;
						i++;
					}
				}
				else
				{
					jump = 0;
				}
				// test to see if this segment makes the scan-cut.
				if ( ( pa.y > pb.y && y < pa.y && y > pb.y ) || ( pa.y < pb.y && y > pa.y && y < pb.y ) )
				{
					ImVec2 intersect;
					intersect.y = y;
					if ( pa.x == pb.x )
					{
						intersect.x = pa.x;
					}
					else
					{
						intersect.x = ( pb.x - pa.x ) / ( pb.y - pa.y ) * ( y - pa.y ) + pa.x;
					}
					scanHits.push_back( intersect );
				}
			}
			// Sort the scan hits by X, so we have a proper left->right ordering
			ImQsort( &scanHits[ 0 ], scanHits.size(), sizeof( ImVec2 ), &ImVec2CompX::Comp );
			// generate the line segments.
			{
				int i = 0;
				int l = scanHits.size() - 1; // we need pairs of points, this prevents segfault.
				for ( i = 0; i < l; i += 2 )
				{
					draw->AddLine( scanHits[ i ], scanHits[ i + 1 ], color, ( float )strokeWidth );
				}
			}
		}
		y += gap;
	} // for each scan line
	draw->PopClipRect();
	scanHits.clear();
}

static void GetTrianglePointer( ImVec2& a, ImVec2& b, ImVec2& c, ImVec2 targetPoint, float angle, float height )
{
	//// Simple Work around to have approximatively the tip on the target point
	//// TODO: Find better solution
	//if ( thickness > 0.0f )
	//	targetPoint += ImVec2( 0.0f, thickness * 0.5f );
	float cos2pi_3 = ImCos( ( 2.0f / 3.0f ) * IM_PI );
	float sin2pi_3 = ImSin( ( 2.0f / 3.0f ) * IM_PI );
	// minus to have the pointer turning in positive
	// PI/2 to have the triangle pointing up when angle == 0.0f
	float cos0 = ImCos( -angle - IM_PI * 0.5f );
	float sin0 = ImSin( -angle - IM_PI * 0.5f );
	ImVec2 center = targetPoint + ImVec2( -1.0f, 0.0f );
	b = ImVec2( cos2pi_3 - 1.0f, sin2pi_3 );
	c = ImVec2( cos2pi_3 - 1.0f, -sin2pi_3 );
	a = targetPoint;
	b = ImRotate( b, cos0, sin0 ) * 2.0f * height / 3.0f + targetPoint;
	c = ImRotate( c, cos0, sin0 ) * 2.0f * height / 3.0f + targetPoint;
}

void ImGui::DrawTriangleCursor( ImDrawList* pDrawList, ImVec2 targetPoint, float angle, float size, float thickness, ImU32 col )
{
	ImVec2 a, b, c;
	GetTrianglePointer( a, b, c, targetPoint, angle, size );
	pDrawList->AddTriangle( a, b, c, col, thickness );
}

void ImGui::DrawTriangleCursorFilled( ImDrawList* pDrawList, ImVec2 targetPoint, float angle, float size, ImU32 col )
{
	ImVec2 a, b, c;
	GetTrianglePointer( a, b, c, targetPoint, angle, size );
	pDrawList->AddTriangleFilled( a, b, c, col );
}

static void GetRotatePoints( ImVec2* pts, int pts_count, ImVec2 pivot, float angle )
{
	float cos0 = ImCos( angle );
	float sin0 = ImSin( angle );
	ImVec2 t;
	for ( int k = 0; k < pts_count; ++k )
	{
		t = pts[ k ] - pivot;
		ImVec2 r;
		r.x = t.x * cos0 - t.y * sin0;
		r.y = t.x * sin0 + t.y * cos0;
		pts[ k ] = r + pivot;
	}
}

static void ImInternalGetSignetVertices( ImVec2* pts, int pts_count, ImVec2 targetPoint, float width, float height, float height_ratio, float align01, float angle, float thickness )
{
	//// Simple Work around to have approximatively the tip on the target point
	//// TODO: Find better solution
	//if ( thickness > 0.0f )
	//	targetPoint += ImVec2( 0.0f, thickness * 0.5f );
	float left = ImLerp( targetPoint.x - width, targetPoint.x, align01 );
	float right = left + width;
	float top = ImLerp( targetPoint.y, targetPoint.y + height, height_ratio );
	float bottom = targetPoint.y + height;
	pts[ 0 ] = targetPoint;
	pts[ 1 ] = ImVec2( left, top );
	pts[ 2 ] = ImVec2( left, bottom );
	pts[ 3 ] = ImVec2( right, bottom );
	pts[ 4 ] = ImVec2( right, top );
	GetRotatePoints( &pts[ 0 ], pts_count, targetPoint, angle );
}

void ImGui::DrawSignetCursor( ImDrawList* pDrawList, ImVec2 targetPoint, float width, float height, float height_ratio, float align01, float angle, float thickness, ImU32 col )
{
	ImVec2 pts[ 5 ];
	ImInternalGetSignetVertices( &pts[ 0 ], 5, targetPoint, width, height, height_ratio, align01, angle, thickness );
	pDrawList->AddPolyline( &pts[ 0 ], 5, col, ImDrawFlags_Closed, thickness );
}

void ImGui::DrawSignetFilledCursor( ImDrawList* pDrawList, ImVec2 targetPoint, float width, float height, float height_ratio, float align01, float angle, ImU32 col )
{
	ImVec2 pts[ 5 ];
	ImInternalGetSignetVertices( &pts[ 0 ], 5, targetPoint, width, height, height_ratio, align01, angle, 0.0f );
	pDrawList->AddConvexPolyFilled( &pts[ 0 ], 5, col );
}

void ImGui::SetCurrentWindowBackgroundImage( ImTextureID id, ImVec2 imgSize, bool fixedSize, ImU32 col )
{
	float ar = imgSize.x / imgSize.y;
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	ImVec2 cur = window->InnerRect.Min;
	ImVec2 uv;
	ImVec2 winSize = ImGui::GetWindowSize();
	if ( fixedSize )
	{
		uv.x = winSize.x / imgSize.x;
		uv.y = winSize.y / imgSize.y;
	}
	else
	{
		if ( winSize.x > winSize.y )
		{
			if ( imgSize.x > imgSize.y )
			{
				uv.x = 1.0f;
				uv.y = winSize.y / winSize.x;
			}
			else
			{
				uv.x = winSize.y / winSize.x;
				uv.y = 1.0f;
			}
		}
		else
		{
			if ( imgSize.x > imgSize.y )
			{
				uv.x = winSize.x / winSize.y;
				uv.y = 1.0f;
			}
			else
			{
				uv.x = 1.0f;
				uv.y = winSize.y / winSize.x;
			}
		}
	}
	drawList->AddImageRounded( id, cur, cur + winSize, ImVec2( 0.0f, 0.0f ), uv, col, window->WindowRounding );
}

void ImGui::DrawLinearLineGraduation( ImDrawList* drawlist, ImVec2 start, ImVec2 end,
                                    float mainLineThickness, ImU32 mainCol,
                                    int division0, float height0, float thickness0, float angle0, ImU32 col0,
                                    int division1, float height1, float thickness1, float angle1, ImU32 col1,
                                    int division2, float height2, float thickness2, float angle2, ImU32 col2 )
{
	if ( mainLineThickness < 0.5f ||
		( thickness0 < 0.5f &&
		thickness1 < 0.5f &&
		thickness2 < 0.5f ) )
		return;
	if ( ( mainCol & IM_COL32_A_MASK ) == 0 &&
		( ( col0 | col1 | col2 ) & IM_COL32_A_MASK ) == 0 )
		return;
	ImVec2 delta = end - start;
	float length = delta.length(); // ImLength( delta );
	ImVec2 forward = delta / length; // ImNormalized( delta );
	ImVec2 up = ImVec2(forward.y, -forward.x); // ImAntiHalfTurn( forward );
	if ( division0 > 0 )
	{
		float dx0 = length / ( ( float )division0 );
		float inv_div0 = 1.0f / ( ( float )division0 );
		ImVec2 up0;
		if ( angle0 != 0.0f )
		{
			float cos0 = ImCos( -angle0 );
			float sin0 = ImSin( -angle0 );
			up0 = ImRotate( up, cos0, sin0 );
		}
		else
		{
			up0 = up;
		}
		for ( int k = 0; k <= division0; ++k )
		{
			float fk = ( float )k;
			float t = fk * inv_div0;
			ImVec2 v = ImLerp( start, end, t );
			drawlist->AddLine( v, v + up0 * height0, col0, thickness0 );
		}
	}
	if ( division0 > 0 && division1 > 0 )
	{
		float dx0 = length / ( ( float )division0 );
		float inv_div0 = 1.0f / ( ( float )division0 );
		float dx1 = dx0 / ( ( float )division1 );
		float inv_div1 = 1.0f / ( ( float )division1 );
		ImVec2 up1;
		if ( angle1 != 0.0f )
		{
			float cos0 = ImCos( -angle1 );
			float sin0 = ImSin( -angle1 );
			up1 = ImRotate( up, cos0, sin0 );
		}
		else
		{
			up1 = up;
		}
		for ( int p = 0; p < division0; ++p )
		{
			float fp = ( float )p;
			float t0 = fp * inv_div0;
			ImVec2 v0 = ImLerp( start, end, t0 );
			for ( int k = 1; k < division1; ++k )
			{
				float fk = ( float )k;
				float t1 = fk * inv_div1;
				ImVec2 v = v0 + forward * (fk * dx1);
				drawlist->AddLine( v, v + up1 * height1, col1, thickness1 );
			}
		}
	}
	if ( division0 > 0 && division1 > 0 && division2 > 0 )
	{
		float dx0 = length / ( ( float )division0 );
		float inv_div0 = 1.0f / ( ( float )division0 );
		float dx1 = dx0 / ( ( float )division1 );
		float inv_div1 = 1.0f / ( ( float )division1 );
		float dx2 = dx1 / ( ( float )division2 );
		float inv_div2 = 1.0f / ( ( float )division2 );
		ImVec2 up2;
		if ( angle2 != 0.0f )
		{
			float cos0 = ImCos( -angle2 );
			float sin0 = ImSin( -angle2 );
			up2 = ImRotate( up, cos0, sin0 );
		}
		else
		{
			up2 = up;
		}
		for ( int p = 0; p < division0; ++p )
		{
			float fp = ( float )p;
			float t0 = fp * inv_div0;
			ImVec2 v0 = ImLerp( start, end, t0 );
			for ( int q = 0; q < division1; ++q )
			{
				float fk = ( float )q;
				ImVec2 v1 = v0 + forward * (fk * dx1);
				for ( int k = 1; k < division2; ++k )
				{
					float fk = ( float )k;
					ImVec2 v = v1 + forward * ( fk * dx2 );
					drawlist->AddLine( v, v + up2 * height2, col2, thickness2 );
				}
			}
		}
	}
	drawlist->AddLine( start, end, mainCol, mainLineThickness );
}

void ImGui::DrawLogLineGraduation( ImDrawList* drawlist, ImVec2 start, ImVec2 end,
                                    float mainLineThickness, ImU32 mainCol,
                                    int division0, float height0, float thickness0, float angle0, ImU32 col0,
                                    int division1, float height1, float thickness1, float angle1, ImU32 col1 )
{
	if ( mainLineThickness < 0.5f ||
		( thickness0 < 0.5f &&
		thickness1 < 0.5f ) )
		return;
	if ( ( mainCol & IM_COL32_A_MASK ) == 0 &&
			( ( col0 | col1 ) & IM_COL32_A_MASK ) == 0 )
		return;
	ImVec2 delta = end - start;
	float length = delta.length(); // ImLength( delta );
	ImVec2 forward = delta / length; // ImNormalized( delta );
	ImVec2 up = ImVec2(forward.y, -forward.x); // ImAntiHalfTurn( forward );
	if ( division0 > 0 )
	{
		float dx0 = length / ( ( float )division0 );
		float inv_div0 = 1.0f / ( ( float )division0 );
		ImVec2 up0;
		if ( angle0 != 0.0f )
		{
			float cos0 = ImCos( -angle0 );
			float sin0 = ImSin( -angle0 );
			up0 = ImRotate( up, cos0, sin0 );
		}
		else
		{
			up0 = up;
		}
		for ( int k = 0; k <= division0; ++k )
		{
			float fk = ( float )k;
			float t = fk * inv_div0;
			ImVec2 v = ImLerp( start, end, t );
			drawlist->AddLine( v, v + up0 * height0, col0, thickness0 );
		}
	}
	if ( division0 > 0 && division1 > 0 )
	{
		float dx0 = length / ( ( float )division0 );
		float inv_div0 = 1.0f / ( ( float )division0 );
		float dx1 = dx0 / ( ( float )division1 );
		float inv_div1 = 1.0f / ( ( float )division1 );
		float scale = 1.0f / ImLog( ( float )( division1 + 1 ) );
		ImVec2 up1;
		if ( angle1 != 0.0f )
		{
			float cos0 = ImCos( -angle1 );
			float sin0 = ImSin( -angle1 );
			up1 = ImRotate( up, cos0, sin0 );
		}
		else
		{
			up1 = up;
		}
		for ( int p = 0; p < division0; ++p )
		{
			float fp = ( float )p;
			float t0 = fp * inv_div0;
			ImVec2 v0 = ImLerp( start, end, t0 );
			for ( int k = 2; k < division1 + 1; ++k )
			{
				float fk = ImLog( ( float )( k ) ) * scale;
				ImVec2 v = v0 + forward * ( fk * dx0 );
				drawlist->AddLine( v, v + up1 * height1, col1, thickness1 );
			}
		}
	}
	drawlist->AddLine( start, end, mainCol, mainLineThickness );
}

void ImGui::DrawLinearCircularGraduation( ImDrawList* drawlist, ImVec2 center, float radius, float start_angle, float end_angle, int num_segments,
                                        float mainLineThickness, ImU32 mainCol,
                                        int division0, float height0, float thickness0, float angle0, ImU32 col0,
                                        int division1, float height1, float thickness1, float angle1, ImU32 col1,
                                        int division2, float height2, float thickness2, float angle2, ImU32 col2 )
{
	if ( radius < 0.5f ||
		( thickness0 < 0.5f &&
		thickness1 < 0.5f &&
		thickness2 < 0.5f ) )
		return;
	if ( ( mainCol & IM_COL32_A_MASK ) == 0 &&
		( ( col0 | col1 | col2 ) & IM_COL32_A_MASK ) == 0 )
		return;
	if ( start_angle > end_angle )
	{
		while ( start_angle > end_angle )
		{
			start_angle += 2.0f * IM_PI;
		}
	}
	float angle_spread = end_angle - start_angle;
	ImVec2 up = ImRotate( ImVec2( 1.0f, 0.0f ), ImCos( start_angle ), ImSin( start_angle ) );
	if ( division0 > 0 )
	{
		float da0 = angle_spread / ( ( float )division0 );
		for ( int k = 0; k <= division0; ++k )
		{
			float fk = ( float )k;
			float a0 = start_angle + fk * da0;
			float a1 = a0 + angle0;
			float cos00 = ImCos( -a0 );
			float sin00 = ImSin( -a0 );
			float cos01 = ImCos( -a1 );
			float sin01 = ImSin( -a1 );
			ImVec2 up0 = ImRotate( ImVec2( 1.0f, 0.0f ), cos00, sin00 );
			ImVec2 up1 = ImRotate( ImVec2( 1.0f, 0.0f ), cos01, sin01 );
			ImVec2 v = center + up0 * radius;
			drawlist->AddLine( v, v + up1 * height0, col0, thickness0 );
		}
	}
	if ( division0 > 0 && division1 > 0 )
	{
		float da0 = angle_spread / ( ( float )division0 );
		float da1 = da0 / ( ( float )division1 );
		for ( int p = 0; p < division0; ++p )
		{
			float fp = ( float )p;
			float ap = start_angle + fp * da0;
			for ( int k = 1; k < division1; ++k )
			{
				float fk = ( float )k;
				float a0 = ap + fk * da1;
				float a1 = a0 + angle1;
				float cos00 = ImCos( -a0 );
				float sin00 = ImSin( -a0 );
				float cos01 = ImCos( -a1 );
				float sin01 = ImSin( -a1 );
				ImVec2 up0 = ImRotate( ImVec2( 1.0f, 0.0f ), cos00, sin00 );
				ImVec2 up1 = ImRotate( ImVec2( 1.0f, 0.0f ), cos01, sin01 );
				ImVec2 v = center + up0 * radius;
				drawlist->AddLine( v, v + up1 * height1, col1, thickness1 );
			}
		}
	}
	if ( division0 > 0 && division1 > 0 && division2 > 0 )
	{
		float da0 = angle_spread / ( ( float )division0 );
		float da1 = da0 / ( ( float )division1 );
		float da2 = da1 / ( ( float )division2 );
		for ( int p = 0; p < division0; ++p )
		{
			float fp = ( float )p;
			float ap = start_angle + fp * da0;
			for ( int q = 0; q < division1; ++q )
			{
				float fq = ( float )q;
				float aq = ap + fq * da1;
				for ( int k = 1; k < division2; ++k )
				{
					float fk = ( float )k;
					float a0 = aq + fk * da2;
					float a1 = a0 + angle2;
					float cos00 = ImCos( -a0 );
					float sin00 = ImSin( -a0 );
					float cos01 = ImCos( -a1 );
					float sin01 = ImSin( -a1 );
					ImVec2 up0 = ImRotate( ImVec2( 1.0f, 0.0f ), cos00, sin00 );
					ImVec2 up1 = ImRotate( ImVec2( 1.0f, 0.0f ), cos01, sin01 );
					ImVec2 v = center + up0 * radius;
					drawlist->AddLine( v, v + up1 * height2, col2, thickness2 );
				}
			}
		}
	}
	drawlist->PathArcTo( center, radius, -start_angle, -end_angle, num_segments );
	drawlist->PathStroke( mainCol, ImDrawFlags_None, mainLineThickness );
}

void ImGui::DrawLogCircularGraduation( ImDrawList* drawlist, ImVec2 center, float radius, float start_angle, float end_angle, int num_segments,
                                        float mainLineThickness, ImU32 mainCol,
                                        int division0, float height0, float thickness0, float angle0, ImU32 col0,
                                        int division1, float height1, float thickness1, float angle1, ImU32 col1 )
{
	if ( radius < 0.5f ||
		( thickness0 < 0.5f &&
		thickness1 < 0.5f ) )
		return;
	if ( ( mainCol & IM_COL32_A_MASK ) == 0 &&
			( ( col0 | col1 ) & IM_COL32_A_MASK ) == 0 )
		return;
	if ( start_angle > end_angle )
	{
		while ( start_angle > end_angle )
		{
			start_angle += 2.0f * IM_PI;
		}
	}
	float angle_spread = end_angle - start_angle;
	ImVec2 up = ImRotate( ImVec2( 1.0f, 0.0f ), ImCos( start_angle ), ImSin( start_angle ) );
	if ( division0 > 0 )
	{
		float da0 = angle_spread / ( ( float )division0 );
		for ( int k = 0; k <= division0; ++k )
		{
			float fk = ( float )k;
			float a0 = start_angle + fk * da0;
			float a1 = a0 + angle0;
			float cos00 = ImCos( -a0 );
			float sin00 = ImSin( -a0 );
			float cos01 = ImCos( -a1 );
			float sin01 = ImSin( -a1 );
			ImVec2 up0 = ImRotate( ImVec2( 1.0f, 0.0f ), cos00, sin00 );
			ImVec2 up1 = ImRotate( ImVec2( 1.0f, 0.0f ), cos01, sin01 );
			ImVec2 v = center + up0 * radius;
			drawlist->AddLine( v, v + up1 * height0, col0, thickness0 );
		}
	}
	if ( division0 > 0 && division1 > 0 )
	{
		float da0 = angle_spread / ( ( float )division0 );
		float da1 = da0 / ( ( float )division1 );
		float scale = 1.0f / ImLog( ( float )( division1 + 1 ) );
		for ( int p = 0; p < division0; ++p )
		{
			float fp = ( float )p;
			float ap = start_angle + fp * da0;
			for ( int k = 2; k < division1 + 1; ++k )
			{
				float fk = ImLog( ( float )( k ) ) * scale;
				float a0 = ap + fk * da0;
				float a1 = a0 + angle1;
				float cos00 = ImCos( -a0 );
				float sin00 = ImSin( -a0 );
				float cos01 = ImCos( -a1 );
				float sin01 = ImSin( -a1 );
				ImVec2 up0 = ImRotate( ImVec2( 1.0f, 0.0f ), cos00, sin00 );
				ImVec2 up1 = ImRotate( ImVec2( 1.0f, 0.0f ), cos01, sin01 );
				ImVec2 v = center + up0 * radius;
				drawlist->AddLine( v, v + up1 * height1, col1, thickness1 );
			}
		}
	}
	drawlist->PathArcTo( center, radius, -start_angle, -end_angle, num_segments );
	drawlist->PathStroke( mainCol, ImDrawFlags_None, mainLineThickness );
}

// imgInspect
inline void histogram(const int width, const int height, const unsigned char* const bits)
{
    unsigned int count[4][256] = {0};
    const unsigned char* ptrCols = bits;
    ImGui::InvisibleButton("histogram", ImVec2(256, 128));
    for (int l = 0; l < height * width; l++)
    {
        count[0][*ptrCols++]++;
        count[1][*ptrCols++]++;
        count[2][*ptrCols++]++;
        count[3][*ptrCols++]++;
    }
    unsigned int maxv = count[0][0];
    unsigned int* pCount = &count[0][0];
    for (int i = 0; i < 3 * 256; i++, pCount++)
    {
        maxv = (maxv > *pCount) ? maxv : *pCount;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 rmin = ImGui::GetItemRectMin();
    const ImVec2 rmax = ImGui::GetItemRectMax();
    const ImVec2 size = ImGui::GetItemRectSize();
    const float hFactor = size.y / float(maxv);
    for (int i = 0; i <= 10; i++)
    {
        float ax = rmin.x + (size.x / 10.f) * float(i);
        float ay = rmin.y + (size.y / 10.f) * float(i);
        drawList->AddLine(ImVec2(rmin.x, ay), ImVec2(rmax.x, ay), 0x80808080);
        drawList->AddLine(ImVec2(ax, rmin.y), ImVec2(ax, rmax.y), 0x80808080);
    }
    const float barWidth = (size.x / 256.f);
    for (int j = 0; j < 256; j++)
    {
        // pixel count << 2 + color index(on 2 bits)
        uint32_t cols[3] = {(count[0][j] << 2), (count[1][j] << 2) + 1, (count[2][j] << 2) + 2};
        if (cols[0] > cols[1])
            ImSwap(cols[0], cols[1]);
        if (cols[1] > cols[2])
            ImSwap(cols[1], cols[2]);
        if (cols[0] > cols[1])
            ImSwap(cols[0], cols[1]);
        float heights[3];
        uint32_t colors[3];
        uint32_t currentColor = 0xFFFFFFFF;
        for (int i = 0; i < 3; i++)
        {
            heights[i] = rmax.y - (cols[i] >> 2) * hFactor;
            colors[i] = currentColor;
            currentColor -= 0xFF << ((cols[i] & 3) * 8);
        }
        float currentHeight = rmax.y;
        const float left = rmin.x + barWidth * float(j);
        const float right = left + barWidth;
        for (int i = 0; i < 3; i++)
        {
            if (heights[i] >= currentHeight)
            {
                continue;
            }
            drawList->AddRectFilled(ImVec2(left, currentHeight), ImVec2(right, heights[i]), colors[i]);
            currentHeight = heights[i];
        }
    }
}

inline void drawNormal(ImDrawList* draw_list, const ImRect& rc, float x, float y)
{
    draw_list->AddCircle(rc.GetCenter(), rc.GetWidth() / 2.f, 0x20AAAAAA, 24, 1.f);
    draw_list->AddCircle(rc.GetCenter(), rc.GetWidth() / 4.f, 0x20AAAAAA, 24, 1.f);
    draw_list->AddLine(rc.GetCenter(), rc.GetCenter() + ImVec2(x, y) * rc.GetWidth() / 2.f, 0xFF0000FF, 2.f);
}

void ImGui::ImageInspect(const int width,
                        const int height,
                        const unsigned char* const bits,
                        ImVec2 mouseUVCoord,
                        ImVec2 displayedTextureSize,
                        bool histogram_full,
                        int zoom_size)
{
    if (ImGui::BeginTooltip())
    {
        ImGui::BeginGroup();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        static const float zoomRectangleWidth = 80.f;
        // bitmap zoom
        ImGui::InvisibleButton("AnotherInvisibleMan", ImVec2(zoomRectangleWidth, zoomRectangleWidth));
        const ImRect pickRc(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        draw_list->AddRectFilled(pickRc.Min, pickRc.Max, 0xFF000000);
        static int zoomSize = zoom_size / 2;
        uint32_t zoomData[(zoomSize * 2 + 1) * (zoomSize * 2 + 1)];
        const float quadWidth = zoomRectangleWidth / float(zoomSize * 2 + 1);
        const ImVec2 quadSize(quadWidth, quadWidth);
        const int basex = ImClamp(int(mouseUVCoord.x * width), zoomSize, width - zoomSize);
        const int basey = ImClamp(int(mouseUVCoord.y * height), zoomSize, height - zoomSize);
        for (int y = -zoomSize; y <= zoomSize; y++)
        {
            for (int x = -zoomSize; x <= zoomSize; x++)
            {
                uint32_t texel = ((uint32_t*)bits)[(basey - y) * width + x + basex];
                ImVec2 pos = pickRc.Min + ImVec2(float(x + zoomSize), float(zoomSize - y)) * quadSize;
                draw_list->AddRectFilled(pos, pos + quadSize, texel);
            }
        }
        //ImGui::SameLine();
        // center quad
        const ImVec2 pos = pickRc.Min + ImVec2(float(zoomSize), float(zoomSize)) * quadSize;
        draw_list->AddRect(pos, pos + quadSize, 0xFF0000FF, 0.f, 15, 2.f);
        // normal direction
        ImGui::InvisibleButton("AndOneMore", ImVec2(zoomRectangleWidth, zoomRectangleWidth));
        ImRect normRc(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        for (int y = -zoomSize; y <= zoomSize; y++)
        {
            for (int x = -zoomSize; x <= zoomSize; x++)
            {
                uint32_t texel = ((uint32_t*)bits)[(basey - y) * width + x + basex];
                const ImVec2 posQuad = normRc.Min + ImVec2(float(x + zoomSize), float(zoomSize - y)) * quadSize;
                //draw_list->AddRectFilled(pos, pos + quadSize, texel);
                const float nx = float(texel & 0xFF) / 128.f - 1.f;
                const float ny = float((texel & 0xFF00)>>8) / 128.f - 1.f;
                const ImRect rc(posQuad, posQuad + quadSize);
                drawNormal(draw_list, rc, nx, ny);
            }
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        uint32_t texel = ((uint32_t*)bits)[basey * width + basex];
        ImVec4 color = ImColor(texel);
        ImVec4 colHSV;
        ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, colHSV.x, colHSV.y, colHSV.z);
        ImGui::Text("U %1.3f V %1.3f", mouseUVCoord.x, mouseUVCoord.y);
        ImGui::Text("Coord %d %d", int(mouseUVCoord.x * width), int(mouseUVCoord.y * height));
        ImGui::Separator();
        ImGui::Text("R 0x%02x  G 0x%02x  B 0x%02x", int(color.x * 255.f), int(color.y * 255.f), int(color.z * 255.f));
        ImGui::Text("R %1.3f G %1.3f B %1.3f", color.x, color.y, color.z);
        ImGui::Separator();
        ImGui::Text(
            "H 0x%02x  S 0x%02x  V 0x%02x", int(colHSV.x * 255.f), int(colHSV.y * 255.f), int(colHSV.z * 255.f));
        ImGui::Text("H %1.3f S %1.3f V %1.3f", colHSV.x, colHSV.y, colHSV.z);
        ImGui::Separator();
        ImGui::Text("Alpha 0x%02x", int(color.w * 255.f));
        ImGui::Text("Alpha %1.3f", color.w);
        ImGui::Separator();
        ImGui::Text("Size %d, %d", int(displayedTextureSize.x), int(displayedTextureSize.y));
        ImGui::EndGroup();
        if (histogram_full)
        {
            histogram(width, height, bits);
        }
        else
        {
            for (int y = -zoomSize; y <= zoomSize; y++)
            {
                for (int x = -zoomSize; x <= zoomSize; x++)
                {
                    uint32_t texel = ((uint32_t*)bits)[(basey - y) * width + x + basex];
                    zoomData[(y + zoomSize) * zoomSize * 2 + x + zoomSize] = texel;
                }
            }
            histogram(zoomSize * 2, zoomSize * 2, (const unsigned char*)zoomData);
        }
        ImGui::EndTooltip();
    }
}

// Extensions to ImDrawList
inline static ImU32 GetVerticalGradient(const ImVec4& ct,const ImVec4& cb,float DH,float H)
{
    IM_ASSERT(H!=0);
    const float fa = DH/H;
    const float fc = (1.f-fa);
    return ImGui::ColorConvertFloat4ToU32
    (
        ImVec4
        (
            ct.x * fc + cb.x * fa,
            ct.y * fc + cb.y * fa,
            ct.z * fc + cb.z * fa,
            ct.w * fc + cb.w * fa
        )
    );
}

inline static void GetVerticalGradientTopAndBottomColors(ImU32 c, float fillColorGradientDeltaIn0_05, ImU32& tc, ImU32& bc)
{
    if (fillColorGradientDeltaIn0_05 == 0) { tc = bc = c; return; }

    static ImU32 cacheColorIn = 0;
    static float cacheGradientIn = 0.f;
    static ImU32 cacheTopColorOut = 0;
    static ImU32 cacheBottomColorOut = 0;
    if (cacheColorIn == c && cacheGradientIn == fillColorGradientDeltaIn0_05) { tc = cacheTopColorOut; bc = cacheBottomColorOut; return; }
    cacheColorIn = c;
    cacheGradientIn = fillColorGradientDeltaIn0_05;
    const bool negative = (fillColorGradientDeltaIn0_05 < 0);
    if (negative) fillColorGradientDeltaIn0_05 = -fillColorGradientDeltaIn0_05;
    if (fillColorGradientDeltaIn0_05 > 0.5f) fillColorGradientDeltaIn0_05 = 0.5f;

    // New code:
    //#define IM_COL32(R,G,B,A)    (((ImU32)(A)<<IM_COL32_A_SHIFT) | ((ImU32)(B)<<IM_COL32_B_SHIFT) | ((ImU32)(G)<<IM_COL32_G_SHIFT) | ((ImU32)(R)<<IM_COL32_R_SHIFT))
    const int fcgi = fillColorGradientDeltaIn0_05 * 255.0f;
    const int R = (unsigned char) (c >> IM_COL32_R_SHIFT);    // The cast should reset upper bits (as far as I hope)
    const int G = (unsigned char) (c >> IM_COL32_G_SHIFT);
    const int B = (unsigned char) (c >> IM_COL32_B_SHIFT);
    const int A = (unsigned char) (c >> IM_COL32_A_SHIFT);

    int r = R + fcgi, g = G + fcgi, b = B + fcgi;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (negative) bc = IM_COL32(r,g,b,A); else tc = IM_COL32(r,g,b,A);

    r = R - fcgi; g = G - fcgi; b = B - fcgi;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (negative) tc = IM_COL32(r,g,b,A); else bc = IM_COL32(r,g,b,A);

    cacheTopColorOut = tc;
    cacheBottomColorOut = bc;
}

void ImGui::AddConvexPolyFilledWithVerticalGradient(ImDrawList *dl, const ImVec2 *points, const int points_count, ImU32 colTop, ImU32 colBot,float miny,float maxy)
{
    if (!dl) return;
    if (colTop == colBot)  
    {
        dl->AddConvexPolyFilled(points, points_count, colTop);
        return;
    }
    const ImVec2 uv = GImGui->DrawListSharedData.TexUvWhitePixel;
    const bool anti_aliased = GImGui->Style.AntiAliasedFill;

    int height=0;
    if (miny <= 0 || maxy <= 0)
    {
        const float max_float = 999999999999999999.f;
        miny = max_float; maxy = -max_float;
        for (int i = 0; i < points_count; i++)
        {
            const float h = points[i].y;
            if (h < miny) miny = h;
            else if (h > maxy) maxy = h;
        }
    }
    height = maxy - miny;
    const ImVec4 colTopf = ColorConvertU32ToFloat4(colTop);
    const ImVec4 colBotf = ColorConvertU32ToFloat4(colBot);

    if (anti_aliased)
    {
        // Anti-aliased Fill
        const float AA_SIZE = 1.0f;

        const ImVec4 colTransTopf(colTopf.x,colTopf.y,colTopf.z,0.f);
        const ImVec4 colTransBotf(colBotf.x,colBotf.y,colBotf.z,0.f);
        const int idx_count = (points_count - 2) * 3 + points_count * 6;
        const int vtx_count = (points_count * 2);
        dl->PrimReserve(idx_count, vtx_count);

        // Add indexes for fill
        unsigned int vtx_inner_idx = dl->_VtxCurrentIdx;
        unsigned int vtx_outer_idx = dl->_VtxCurrentIdx + 1;
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx); 
            dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + ((i - 1) << 1)); 
            dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_inner_idx + (i << 1));
            dl->_IdxWritePtr += 3;
        }

        // Compute normals
        ImVec2* temp_normals = (ImVec2*)alloca(points_count * sizeof(ImVec2));
        for (int i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            const ImVec2& p0 = points[i0];
            const ImVec2& p1 = points[i1];
            ImVec2 diff = p1 - p0;
            diff *= ImInvLength(diff, 1.0f);
            temp_normals[i0].x = diff.y;
            temp_normals[i0].y = -diff.x;
        }

        for (int i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            // Average normals
            const ImVec2& n0 = temp_normals[i0];
            const ImVec2& n1 = temp_normals[i1];
            ImVec2 dm = (n0 + n1) * 0.5f;
            float dmr2 = dm.x * dm.x + dm.y * dm.y;
            if (dmr2 > 0.000001f)
            {
                float scale = 1.0f / dmr2;
                if (scale > 100.0f) scale = 100.0f;
                dm *= scale;
            }
            dm *= AA_SIZE * 0.5f;

            // Add vertices
            //_VtxWritePtr[0].pos = (points[i1] - dm); _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;        // Inner
            //_VtxWritePtr[1].pos = (points[i1] + dm); _VtxWritePtr[1].uv = uv; _VtxWritePtr[1].col = col_trans;  // Outer
            dl->_VtxWritePtr[0].pos = (points[i1] - dm); dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colTopf,colBotf,points[i1].y - miny,height);        // Inner
            dl->_VtxWritePtr[1].pos = (points[i1] + dm); dl->_VtxWritePtr[1].uv = uv; dl->_VtxWritePtr[1].col = GetVerticalGradient(colTransTopf,colTransBotf,points[i1].y - miny,height);  // Outer
            dl->_VtxWritePtr += 2;

            // Add indexes for fringes
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx+(i1<<1)); dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + (i0 << 1)); dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1));
            dl->_IdxWritePtr[3] = (ImDrawIdx)(vtx_outer_idx+(i0<<1)); dl->_IdxWritePtr[4] = (ImDrawIdx)(vtx_outer_idx + (i1 << 1)); dl->_IdxWritePtr[5] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1));
            dl->_IdxWritePtr += 6;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
    else
    {
        // Non Anti-aliased Fill
        const int idx_count = (points_count - 2) * 3;
        const int vtx_count = points_count;
        dl->PrimReserve(idx_count, vtx_count);
        for (int i = 0; i < vtx_count; i++)
        {
            //_VtxWritePtr[0].pos = points[i]; _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;
            dl->_VtxWritePtr[0].pos = points[i]; dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colTopf,colBotf,points[i].y - miny,height);
            dl->_VtxWritePtr++;
        }
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(dl->_VtxCurrentIdx);
            dl->_IdxWritePtr[1] = (ImDrawIdx)(dl->_VtxCurrentIdx + i - 1);
            dl->_IdxWritePtr[2] = (ImDrawIdx)(dl->_VtxCurrentIdx + i);
            dl->_IdxWritePtr += 3;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
}

void ImGui::PathFillWithVerticalGradientAndStroke(ImDrawList *dl, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness, float miny, float maxy)
{
    if (!dl) return;
    if (fillColorTop == fillColorBottom) dl->AddConvexPolyFilled(dl->_Path.Data,dl->_Path.Size, fillColorTop);
    else if ((fillColorTop & IM_COL32_A_MASK) != 0 || (fillColorBottom & IM_COL32_A_MASK) != 0) AddConvexPolyFilledWithVerticalGradient(dl, dl->_Path.Data, dl->_Path.Size, fillColorTop, fillColorBottom,miny,maxy);
    if ((strokeColor & IM_COL32_A_MASK) != 0 && strokeThickness > 0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness);
    dl->PathClear();
}

void ImGui::PathFillAndStroke(ImDrawList *dl, const ImU32 &fillColor, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness)
{
    if (!dl) return;
    if ((fillColor & IM_COL32_A_MASK) != 0) dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, fillColor);
    if ((strokeColor & IM_COL32_A_MASK) != 0 && strokeThickness>0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness);
    dl->PathClear();
}

void ImGui::AddRect(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness)
{
    if (!dl || (((fillColor & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    dl->PathRect(a, b, rounding, rounding_corners);
    PathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness);
}

void ImGui::AddRectWithVerticalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness)
{
    if (!dl || (((fillColorTop & IM_COL32_A_MASK) == 0) && ((fillColorBottom & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    if (rounding == 0.f || rounding_corners == ImDrawFlags_RoundCornersNone)
    {
        dl->AddRectFilledMultiColor(a,b,fillColorTop,fillColorTop,fillColorBottom,fillColorBottom); // Huge speedup!
        if ((strokeColor& IM_COL32_A_MASK) != 0 && strokeThickness > 0.f)
        {
            dl->PathRect(a, b, rounding, rounding_corners);
            dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, true, strokeThickness);
            dl->PathClear();
        }
    }
    else 
    {
        dl->PathRect(a, b, rounding, rounding_corners);
        PathFillWithVerticalGradientAndStroke(dl,fillColorTop,fillColorBottom,strokeColor,true,strokeThickness,a.y,b.y);
    }
}

void ImGui::PathArcTo(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, float amin, float amax, int num_segments)
{
    if (!dl) return;
    if (radii.x == 0.0f || radii.y == 0) dl->_Path.push_back(centre);
    dl->_Path.reserve(dl->_Path.Size + (num_segments + 1));
    for (int i = 0; i <= num_segments; i++)
    {
        const float a = amin + ((float)i / (float)num_segments) * (amax - amin);
        dl->_Path.push_back(ImVec2(centre.x + cosf(a) * radii.x, centre.y + sinf(a) * radii.y));
    }
}

void ImGui::AddEllipse(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColor, const ImU32 &strokeColor, int num_segments, float strokeThickness)
{
    if (!dl) return;
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    PathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness);
}

void ImGui::AddEllipseWithVerticalGradient(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, int num_segments, float strokeThickness)
{
    if (!dl) return;
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    PathFillWithVerticalGradientAndStroke(dl,fillColorTop,fillColorBottom,strokeColor,true,strokeThickness,centre.y-radii.y,centre.y+radii.y);
}

void ImGui::AddCircle(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColor, const ImU32 &strokeColor, int num_segments, float strokeThickness)
{
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcTo(dl,centre, radii, 0.0f, a_max, num_segments-1);
    PathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness);
}

void ImGui::AddCircleWithVerticalGradient(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, int num_segments, float strokeThickness)
{
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcTo(dl,centre, radii, 0.0f, a_max, num_segments-1);
    PathFillWithVerticalGradientAndStroke(dl,fillColorTop,fillColorBottom,strokeColor,true,strokeThickness,centre.y-radius,centre.y+radius);
}

void ImGui::AddRectWithVerticalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, float fillColorGradientDeltaIn0_05, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness)
{
    ImU32 fillColorTop,fillColorBottom;
    GetVerticalGradientTopAndBottomColors(fillColor,fillColorGradientDeltaIn0_05,fillColorTop,fillColorBottom);
    AddRectWithVerticalGradient(dl,a,b,fillColorTop,fillColorBottom,strokeColor,rounding,rounding_corners,strokeThickness);
}

void ImGui::AddPolyLine(ImDrawList *dl, const ImVec2* polyPoints, int numPolyPoints, ImU32 strokeColor, float strokeThickness, bool strokeClosed, const ImVec2 &offset, const ImVec2 &scale)
{
    if (polyPoints && numPolyPoints>0 && (strokeColor & IM_COL32_A_MASK) != 0)
    {
        static ImVector<ImVec2> points;
        points.resize(numPolyPoints);
	    for (int i = 0; i < numPolyPoints; i++)   points[i] = offset + polyPoints[i] * scale;
        dl->AddPolyline(&points[0],points.size(),strokeColor,strokeClosed,strokeThickness);
    }
}

void ImGui::AddConvexPolyFilledWithHorizontalGradient(ImDrawList *dl, const ImVec2 *points, const int points_count, ImU32 colLeft, ImU32 colRight, float minx, float maxx)
{
    if (!dl) return;
    if (colLeft == colRight) 
    {
        dl->AddConvexPolyFilled(points,points_count,colLeft);
        return;
    }
    const ImVec2 uv = GImGui->DrawListSharedData.TexUvWhitePixel;
    const bool anti_aliased = GImGui->Style.AntiAliasedFill;

    int width = 0;
    if (minx <= 0 || maxx <= 0) 
    {
        const float max_float = 999999999999999999.f;
        minx = max_float;
        maxx = -max_float;
        for (int i = 0; i < points_count; i++)
        {
            const float w = points[i].x;
            if (w < minx) minx = w;
            else if (w > maxx) maxx = w;
        }
    }
    width = maxx - minx;
    const ImVec4 colLeftf  = ColorConvertU32ToFloat4(colLeft);
    const ImVec4 colRightf = ColorConvertU32ToFloat4(colRight);

    if (anti_aliased)
    {
        // Anti-aliased Fill
        const float AA_SIZE = 1.0f;

        const ImVec4 colTransLeftf(colLeftf.x,colLeftf.y,colLeftf.z,0.f);
        const ImVec4 colTransRightf(colRightf.x,colRightf.y,colRightf.z,0.f);
        const int idx_count = (points_count - 2) * 3 + points_count * 6;
        const int vtx_count = (points_count * 2);
        dl->PrimReserve(idx_count, vtx_count);

        // Add indexes for fill
        unsigned int vtx_inner_idx = dl->_VtxCurrentIdx;
        unsigned int vtx_outer_idx = dl->_VtxCurrentIdx + 1;
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx);
            dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + ((i - 1) << 1));
            dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_inner_idx + (i << 1));
            dl->_IdxWritePtr += 3;
        }

        // Compute normals
        ImVec2* temp_normals = (ImVec2*)alloca(points_count * sizeof(ImVec2));
        for (int i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            const ImVec2& p0 = points[i0];
            const ImVec2& p1 = points[i1];
            ImVec2 diff = p1 - p0;
            diff *= ImInvLength(diff, 1.0f);
            temp_normals[i0].x = diff.y;
            temp_normals[i0].y = -diff.x;
        }

        for (int i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            // Average normals
            const ImVec2& n0 = temp_normals[i0];
            const ImVec2& n1 = temp_normals[i1];
            ImVec2 dm = (n0 + n1) * 0.5f;
            float dmr2 = dm.x * dm.x + dm.y * dm.y;
            if (dmr2 > 0.000001f)
            {
                float scale = 1.0f / dmr2;
                if (scale > 100.0f) scale = 100.0f;
                dm *= scale;
            }
            dm *= AA_SIZE * 0.5f;

            // Add vertices
            //_VtxWritePtr[0].pos = (points[i1] - dm); _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;        // Inner
            //_VtxWritePtr[1].pos = (points[i1] + dm); _VtxWritePtr[1].uv = uv; _VtxWritePtr[1].col = col_trans;  // Outer
            dl->_VtxWritePtr[0].pos = (points[i1] - dm); dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colLeftf,colRightf,points[i1].x - minx,width);        // Inner
            dl->_VtxWritePtr[1].pos = (points[i1] + dm); dl->_VtxWritePtr[1].uv = uv; dl->_VtxWritePtr[1].col = GetVerticalGradient(colTransLeftf,colTransRightf,points[i1].x - minx,width);  // Outer
            dl->_VtxWritePtr += 2;

            // Add indexes for fringes
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1)); dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + (i0 << 1)); dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1));
            dl->_IdxWritePtr[3] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1)); dl->_IdxWritePtr[4] = (ImDrawIdx)(vtx_outer_idx + (i1 << 1)); dl->_IdxWritePtr[5] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1));
            dl->_IdxWritePtr += 6;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
    else
    {
        // Non Anti-aliased Fill
        const int idx_count = (points_count - 2) * 3;
        const int vtx_count = points_count;
        dl->PrimReserve(idx_count, vtx_count);
        for (int i = 0; i < vtx_count; i++)
        {
            //_VtxWritePtr[0].pos = points[i]; _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;
            dl->_VtxWritePtr[0].pos = points[i]; dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colLeftf,colRightf,points[i].x - minx,width);
            dl->_VtxWritePtr++;
        }
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(dl->_VtxCurrentIdx); dl->_IdxWritePtr[1] = (ImDrawIdx)(dl->_VtxCurrentIdx + i - 1); dl->_IdxWritePtr[2] = (ImDrawIdx)(dl->_VtxCurrentIdx + i);
            dl->_IdxWritePtr += 3;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
}

void ImGui::PathFillWithHorizontalGradientAndStroke(ImDrawList *dl, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness, float minx, float maxx)
{
    if (!dl) return;
    if (fillColorLeft==fillColorRight) dl->AddConvexPolyFilled(dl->_Path.Data,dl->_Path.Size, fillColorLeft);
    else if ((fillColorLeft & IM_COL32_A_MASK) != 0 || (fillColorRight & IM_COL32_A_MASK) != 0) AddConvexPolyFilledWithHorizontalGradient(dl, dl->_Path.Data, dl->_Path.Size, fillColorLeft, fillColorRight,minx,maxx);
    if ((strokeColor & IM_COL32_A_MASK) != 0 && strokeThickness > 0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness);
    dl->PathClear();
}

void ImGui::AddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColorLeft, const ImU32 &fillColoRight, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness)
{
    if (!dl || (((fillColorLeft & IM_COL32_A_MASK) == 0) && ((fillColoRight & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    if (rounding == 0.f || rounding_corners == ImDrawFlags_RoundCornersNone)
    {
        dl->AddRectFilledMultiColor(a,b,fillColorLeft,fillColoRight,fillColoRight,fillColorLeft); // Huge speedup!
        if ((strokeColor& IM_COL32_A_MASK) != 0 && strokeThickness > 0.f)
        {
            dl->PathRect(a, b, rounding, rounding_corners);
            dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, true, strokeThickness);
            dl->PathClear();
        }
    }
    else
    {
        dl->PathRect(a, b, rounding, rounding_corners);
        PathFillWithHorizontalGradientAndStroke(dl,fillColorLeft,fillColoRight,strokeColor,true,strokeThickness,a.x,b.x);
    }
}

void ImGui::AddEllipseWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments, float strokeThickness)
{
    if (!dl) return;
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    PathFillWithHorizontalGradientAndStroke(dl,fillColorLeft,fillColorRight,strokeColor,true,strokeThickness,centre.y - radii.y,centre.y + radii.y);
}

void ImGui::AddCircleWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments, float strokeThickness)
{
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcTo(dl,centre, radii, 0.0f, a_max, num_segments - 1);
    PathFillWithHorizontalGradientAndStroke(dl,fillColorLeft,fillColorRight,strokeColor,true,strokeThickness,centre.y - radius,centre.y + radius);
}

void ImGui::AddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, float fillColorGradientDeltaIn0_05, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness)
{
    ImU32 fillColorTop,fillColorBottom;
    GetVerticalGradientTopAndBottomColors(fillColor,fillColorGradientDeltaIn0_05,fillColorTop,fillColorBottom);
    AddRectWithHorizontalGradient(dl,a,b,fillColorTop,fillColorBottom,strokeColor,rounding,rounding_corners,strokeThickness);
}

void ImGui::AddLineDashed(ImDrawList *dl, const ImVec2& a, const ImVec2& b, ImU32 col, float thickness, unsigned int num_segments, unsigned int on_segments, unsigned int off_segments)
{
    if ((col >> 24) == 0)
        return;
    int on = 0, off = 0;
    ImVec2 dir = (b - a) / num_segments;
    for (int i = 0; i <= num_segments; i++)
    {
        ImVec2 point(a + dir * i);
        if(on < on_segments) {
            dl->_Path.push_back(point);
            on++;
        } else if(on == on_segments && off == 0) {
            dl->_Path.push_back(point);
            dl->PathStroke(col, false, thickness);
            off++;
        } else if(on == on_segments && off < off_segments) {
            off++;
        } else {
            dl->_Path.resize(0);
            dl->_Path.push_back(point);
            on=1;
            off=0;
        }
    }
    dl->PathStroke(col, false, thickness);
}

void ImGui::PathArcToDashedAndStroke(ImDrawList *dl, const ImVec2& centre, float radius, float amin, float amax, ImU32 col, float thickness, int num_segments, int on_segments, int off_segments)
{
    if (radius == 0.0f)
        dl->_Path.push_back(centre);
    dl->_Path.reserve(dl->_Path.Size + (num_segments + 1));
    int on = 0, off = 0;
    for (int i = 0; i <= num_segments + 1; i++)
    {
        const float a = amin + ((float)i / (float)num_segments) * (amax - amin);
        ImVec2 point(centre.x + cosf(a) * radius, centre.y + sinf(a) * radius);
        if(on < on_segments) {
            dl->_Path.push_back(point);
            on++;
        } else if(on == on_segments && off == 0) {
            dl->_Path.push_back(point);
            dl->PathStroke(col, false, thickness);
            off++;
        } else if(on == on_segments && off < off_segments) {
            off++;
        } else {
            dl->_Path.resize(0);
            dl->_Path.push_back(point);
            on=1;
            off=0;
        }
    }
    dl->PathStroke(col, false, thickness);
}

void ImGui::AddCircleDashed(ImDrawList *dl, const ImVec2& centre, float radius, ImU32 col, int num_segments, float thickness, int on_segments, int off_segments)
{
    if ((col >> 24) == 0 || on_segments == 0)
        return;
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    PathArcToDashedAndStroke(dl, centre, radius-0.5f, 0.0f, a_max, col, thickness, num_segments, on_segments, off_segments);
}

void ImGui::AddImageRotate(ImDrawList *dl, ImTextureID tex_id, ImVec2 pos, ImVec2 size, float angle, ImU32 board_col)
{
    int rotation_start_index = dl->VtxBuffer.Size;
    dl->AddImage(tex_id, pos, pos + size);
    if (angle != 0)
    {
        float rad = M_PI / 180.0 * (90.0 - angle);
        ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX); // bounds
        auto& buf = dl->VtxBuffer;
        float s = sin(rad), c = cos(rad);
        for (int i = rotation_start_index; i < buf.Size; i++)
            l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);
        ImVec2 center = ImVec2((l.x + u.x) / 2, (l.y + u.y) / 2);
        center = ImRotate(center, s, c) - center;
        
        for (int i = rotation_start_index; i < buf.Size; i++)
            buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
    }
}

// Vertical Text
ImVec2 ImGui::CalcVerticalTextSize(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width)
{
    const ImVec2 rv = ImGui::CalcTextSize(text,text_end,hide_text_after_double_hash,wrap_width);
    return ImVec2(rv.y,rv.x);
}

void ImGui::RenderTextVertical(const ImFont* font,ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin,  const char* text_end, float wrap_width, bool cpu_fine_clip, bool rotateCCW, bool char_no_rotate) 
{
    if (!text_end) text_end = text_begin + strlen(text_begin);

    const float scale = size / font->FontSize;
    const float line_height = font->FontSize * scale;

    // Align to be pixel perfect
    //const ImVec2 text_size = CalcVerticalTextSize(text_begin, text_end, false, 0.0f);
    pos.x = (float)(int)pos.x;
    pos.y = (float)(int)pos.y;// + (rotateCCW ? text_size.y : 0);

    float x = pos.x;
    float y = pos.y;
    if (x > clip_rect.z)
        return;

    const bool word_wrap_enabled = (wrap_width > 0.0f);
    const char* word_wrap_eol = NULL;
    const float y_dir = rotateCCW ? -1.f : 1.f;

    // Skip non-visible lines
    const char* s = text_begin;
    if (!word_wrap_enabled && y + line_height < clip_rect.y)
        while (s < text_end && *s != '\n')  // Fast-forward to next line
            s++;

    // Reserve vertices for remaining worse case (over-reserving is useful and easily amortized)
    const int vtx_count_max = (int)(text_end - s) * 4;
    const int idx_count_max = (int)(text_end - s) * 6;
    const int idx_expected_size = draw_list->IdxBuffer.Size + idx_count_max;
    draw_list->PrimReserve(idx_count_max, vtx_count_max);

    ImDrawVert* vtx_write = draw_list->_VtxWritePtr;
    ImDrawIdx* idx_write = draw_list->_IdxWritePtr;
    unsigned int vtx_current_idx = draw_list->_VtxCurrentIdx;
    float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;

    while (s < text_end)
    {
        if (word_wrap_enabled)
        {
            // Calculate how far we can render. Requires two passes on the string data but keeps the code simple and not intrusive for what's essentially an uncommon feature.
            if (!word_wrap_eol)
            {
                word_wrap_eol = font->CalcWordWrapPositionA(scale, s, text_end, wrap_width - (y - pos.y));
                if (word_wrap_eol == s) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
                    word_wrap_eol++;    // +1 may not be a character start point in UTF-8 but it's ok because we use s >= word_wrap_eol below
            }

            if (s >= word_wrap_eol)
            {
                y = pos.y;
                x += line_height;
                word_wrap_eol = NULL;

                // Wrapping skips upcoming blanks
                while (s < text_end)
                {
                    const char c = *s;
                    if (ImCharIsBlankA(c)) { s++; } else if (c == '\n') { s++; break; } else { break; }
                }
                continue;
            }
        }

        // Decode and advance source
        unsigned int c = (unsigned int)*s;
        if (c < 0x80)
        {
            s += 1;
        }
        else
        {
            s += ImTextCharFromUtf8(&c, s, text_end);
            if (c == 0)
                break;
        }

        if (c < 32)
        {
            if (c == '\n')
            {
                y = pos.y;
                x += line_height;

                if (x > clip_rect.z)
                    break;
                if (!word_wrap_enabled && x + line_height < clip_rect.x)
                    while (s < text_end && *s != '\n')  // Fast-forward to next line
                        s++;
                continue;
            }
            if (c == '\r')
                continue;
        }

        float char_width = 0.0f;
        if (const ImFontGlyph* glyph = font->FindGlyph((unsigned short)c))
        {
            char_width = glyph->AdvanceX * scale;
            //fprintf(stderr,"%c [%1.4f]\n",(unsigned char) glyph->Codepoint,char_width);

            // Arbitrarily assume that both space and tabs are empty glyphs as an optimization
            if (c != ' ' && c != '\t')
            {
                // We don't do a second finer clipping test on the Y axis as we've already skipped anything before clip_rect.y and exit once we pass clip_rect.w
                if (!rotateCCW) 
                {
                    x1 = x + (font->FontSize-glyph->Y1) * scale;
                    x2 = x + (font->FontSize-glyph->Y0) * scale;
                    y1 = y + glyph->X0 * scale;
                    y2 = y + glyph->X1 * scale;
                }
                else
                {
                    x1 = x + glyph->Y0 * scale;
                    x2 = x + glyph->Y1 * scale;
                    y1 = y - glyph->X1 * scale;
                    y2 = y - glyph->X0 * scale;
                }
                if (y1 <= clip_rect.w && y2 >= clip_rect.y)
                {
                    // Render a character
                    float u1 = glyph->U0;
                    float v1 = glyph->V0;
                    float u2 = glyph->U1;
                    float v2 = glyph->V1;

                    // CPU side clipping used to fit text in their frame when the frame is too small. Only does clipping for axis aligned quads.
                    if (cpu_fine_clip)
                    {
                        if (x1 < clip_rect.x)
                        {
                            u1 = u1 + (1.0f - (x2 - clip_rect.x) / (x2 - x1)) * (u2 - u1);
                            x1 = clip_rect.x;
                        }
                        if (y1 < clip_rect.y)
                        {
                            v1 = v1 + (1.0f - (y2 - clip_rect.y) / (y2 - y1)) * (v2 - v1);
                            y1 = clip_rect.y;
                        }
                        if (x2 > clip_rect.z)
                        {
                            u2 = u1 + ((clip_rect.z - x1) / (x2 - x1)) * (u2 - u1);
                            x2 = clip_rect.z;
                        }
                        if (y2 > clip_rect.w)
                        {
                            v2 = v1 + ((clip_rect.w - y1) / (y2 - y1)) * (v2 - v1);
                            y2 = clip_rect.w;
                        }
                        if (x1 >= x2)
                        {
                            y += char_width*y_dir;
                            continue;
                        }
                    }

                    // We are NOT calling PrimRectUV() here because non-inlined causes too much overhead in a debug build.
                    // Inlined here:
                    if (char_no_rotate)
                    {
                        vtx_write[0].pos.x = x1; vtx_write[0].pos.y = y1; vtx_write[0].col = col; vtx_write[0].uv.x = u1; vtx_write[0].uv.y = v1;
                        vtx_write[1].pos.x = x2; vtx_write[1].pos.y = y1; vtx_write[1].col = col; vtx_write[1].uv.x = u2; vtx_write[1].uv.y = v1;
                        vtx_write[2].pos.x = x2; vtx_write[2].pos.y = y2; vtx_write[2].col = col; vtx_write[2].uv.x = u2; vtx_write[2].uv.y = v2;
                        vtx_write[3].pos.x = x1; vtx_write[3].pos.y = y2; vtx_write[3].col = col; vtx_write[3].uv.x = u1; vtx_write[3].uv.y = v2;
                        idx_write[0] = (ImDrawIdx)(vtx_current_idx); idx_write[1] = (ImDrawIdx)(vtx_current_idx + 1); idx_write[2] = (ImDrawIdx)(vtx_current_idx + 2);
                        idx_write[3] = (ImDrawIdx)(vtx_current_idx); idx_write[4] = (ImDrawIdx)(vtx_current_idx + 2); idx_write[5] = (ImDrawIdx)(vtx_current_idx + 3);
                        vtx_write += 4;
                        vtx_current_idx += 4;
                        idx_write += 6;
                    }
                    else
                    {
                        idx_write[0] = (ImDrawIdx)(vtx_current_idx); idx_write[1] = (ImDrawIdx)(vtx_current_idx+1); idx_write[2] = (ImDrawIdx)(vtx_current_idx+2);
                        idx_write[3] = (ImDrawIdx)(vtx_current_idx); idx_write[4] = (ImDrawIdx)(vtx_current_idx+2); idx_write[5] = (ImDrawIdx)(vtx_current_idx+3);
                        vtx_write[0].col = vtx_write[1].col = vtx_write[2].col = vtx_write[3].col = col;
                        vtx_write[0].pos.x = x1; vtx_write[0].pos.y = y1;
                        vtx_write[1].pos.x = x2; vtx_write[1].pos.y = y1;
                        vtx_write[2].pos.x = x2; vtx_write[2].pos.y = y2;
                        vtx_write[3].pos.x = x1; vtx_write[3].pos.y = y2;

                        if (rotateCCW)
                        {
                            vtx_write[0].uv.x = u2; vtx_write[0].uv.y = v1;
                            vtx_write[1].uv.x = u2; vtx_write[1].uv.y = v2;
                            vtx_write[2].uv.x = u1; vtx_write[2].uv.y = v2;
                            vtx_write[3].uv.x = u1; vtx_write[3].uv.y = v1;
                        }
                        else
                        {
                            vtx_write[0].uv.x = u1; vtx_write[0].uv.y = v2;
                            vtx_write[1].uv.x = u1; vtx_write[1].uv.y = v1;
                            vtx_write[2].uv.x = u2; vtx_write[2].uv.y = v1;
                            vtx_write[3].uv.x = u2; vtx_write[3].uv.y = v2;
                        }

                        vtx_write += 4;
                        vtx_current_idx += 4;
                        idx_write += 6;
                    }
                }
            }
        }

        y += char_width*y_dir;
    }

    // Give back unused vertices
    draw_list->VtxBuffer.resize((int)(vtx_write - draw_list->VtxBuffer.Data));
    draw_list->IdxBuffer.resize((int)(idx_write - draw_list->IdxBuffer.Data));
    draw_list->CmdBuffer[draw_list->CmdBuffer.Size-1].ElemCount -= (idx_expected_size - draw_list->IdxBuffer.Size);
    draw_list->_VtxWritePtr = vtx_write;
    draw_list->_IdxWritePtr = idx_write;
    draw_list->_VtxCurrentIdx = (unsigned int)draw_list->VtxBuffer.Size;
}

void ImGui::AddTextVertical(ImDrawList* drawList,const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect, bool rotateCCW, bool char_no_rotate)
{
    if ((col & IM_COL32_A_MASK) == 0)
        return;

    if (text_end == NULL)
        text_end = text_begin + strlen(text_begin);
    if (text_begin == text_end)
        return;

    // add by Dicky for multi-language support
    const char * _text_begin = text_begin;
    const char * _text_end = text_end;
    ImGuiContext& g = *GImGui;
    if (g.Style.TextInternationalize)
    {
        auto changed = ImGui::InternationalizedText(_text_begin, _text_end);
        if (changed > 0)
        {
            _text_begin = g.InternationalizedBuffer;
            _text_end = _text_begin + changed;
        }
    }
    // add by Dicky end

    // Note: This is one of the few instance of breaking the encapsulation of ImDrawList, as we pull this from ImGui state, but it is just SO useful.
    // Might just move Font/FontSize to ImDrawList?
    if (font == NULL)
        font = GImGui->Font;
    if (font_size == 0.0f)
        font_size = GImGui->FontSize;

    IM_ASSERT(drawList && font->ContainerAtlas->TexID == drawList->_TextureIdStack.back());  // Use high-level ImGui::PushFont() or low-level ImDrawList::PushTextureId() to change font.

    ImVec4 clip_rect = drawList->_ClipRectStack.back();
    if (cpu_fine_clip_rect)
    {
        clip_rect.x = ImMax(clip_rect.x, cpu_fine_clip_rect->x);
        clip_rect.y = ImMax(clip_rect.y, cpu_fine_clip_rect->y);
        clip_rect.z = ImMin(clip_rect.z, cpu_fine_clip_rect->z);
        clip_rect.w = ImMin(clip_rect.w, cpu_fine_clip_rect->w);
    }
    // modify by Dicky for multi-language 
    RenderTextVertical(font, drawList, font_size, pos, col, clip_rect, _text_begin, _text_end, wrap_width, cpu_fine_clip_rect != NULL, rotateCCW, char_no_rotate);
}

void ImGui::AddTextVertical(ImDrawList* drawList,const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end,bool rotateCCW, bool char_no_rotate)
{
    AddTextVertical(drawList,GImGui->Font, GImGui->FontSize, pos, col, text_begin, text_end, 0.0f, NULL, rotateCCW, char_no_rotate);
}

void ImGui::RenderTextVerticalClipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known, const ImVec2& align, const ImVec2* clip_min, const ImVec2* clip_max, bool rotateCCW, bool char_no_rotate)
{
    // Hide anything after a '##' string
    const char* text_display_end = FindRenderedTextEnd(text, text_end);
    const int text_len = (int)(text_display_end - text);
    if (text_len == 0)
        return;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();

    // Perform CPU side clipping for single clipped element to avoid using scissor state
    ImVec2 pos = pos_min;
    const ImVec2 text_size = text_size_if_known ? *text_size_if_known : CalcVerticalTextSize(text, text_display_end, false, 0.0f);

    if (!clip_max) clip_max = &pos_max;
    bool need_clipping = (pos.x + text_size.x >= clip_max->x) || (pos.y + text_size.y >= clip_max->y);
    if (!clip_min) clip_min = &pos_min; else need_clipping |= (pos.x < clip_min->x) || (pos.y < clip_min->y);

    // Align
    /*if (align & ImGuiAlign_Center) pos.x = ImMax(pos.x, (pos.x + pos_max.x - text_size.x) * 0.5f);
    else if (align & ImGuiAlign_Right) pos.x = ImMax(pos.x, pos_max.x - text_size.x);
    if (align & ImGuiAlign_VCenter) pos.y = ImMax(pos.y, (pos.y + pos_max.y - text_size.y) * 0.5f);*/

    //if (align & ImGuiAlign_Center) pos.y = ImMax(pos.y, (pos.y + pos_max.y - text_size.y) * 0.5f);
    //else if (align & ImGuiAlign_Right) pos.y = ImMax(pos.y, pos_max.y - text_size.y);
    //if (align & ImGuiAlign_VCenter) pos.x = ImMax(pos.x, (pos.x + pos_max.x - text_size.x) * 0.5f);

    // Align whole block (we should defer that to the better rendering function
    if (align.x > 0.0f) pos.x = ImMax(pos.x, pos.x + (pos_max.x - pos.x - text_size.x) * align.x);
    if (align.y > 0.0f) pos.y = ImMax(pos.y, pos.y + (pos_max.y - pos.y - text_size.y) * align.y);

    if (rotateCCW) pos.y+=text_size.y;
    // Render
    if (need_clipping)
    {
        ImVec4 fine_clip_rect(clip_min->x, clip_min->y, clip_max->x, clip_max->y);
        AddTextVertical(window->DrawList,g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Text), text, text_display_end, 0.0f, &fine_clip_rect, rotateCCW, char_no_rotate);
    }
    else
    {
        AddTextVertical(window->DrawList,g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Text), text, text_display_end, 0.0f, NULL, rotateCCW, char_no_rotate);
    }
}

void ImGui::AddTextRolling(ImDrawList* drawList, const ImFont* font, float font_size, const ImVec2& pos, const ImVec2& size, ImU32 col, const int speed, const char* text_begin, const char* text_end)
{
    static int start_offset = 0;
    if ((col & IM_COL32_A_MASK) == 0)
        return;

    if (text_end == NULL)
        text_end = text_begin + strlen(text_begin);
    if (text_begin == text_end)
        return;

    ImRect bb(pos, pos + size);
    if (!drawList)
    {
        ItemSize(size);
        if (!ItemAdd(bb, 0))
            return;
    }

    const char * _text_begin = text_begin;
    const char * _text_end = text_end;
    ImGuiContext& g = *GImGui;
    if (g.Style.TextInternationalize)
    {
        auto changed = ImGui::InternationalizedText(_text_begin, _text_end);
        if (changed > 0)
        {
            _text_begin = g.InternationalizedBuffer;
            _text_end = _text_begin + changed;
        }
    }

    
    // Note: This is one of the few instance of breaking the encapsulation of ImDrawList, as we pull this from ImGui state, but it is just SO useful.
    // Might just move Font/FontSize to ImDrawList?
    if (font == NULL)
        font = GImGui->Font;
    if (font_size == 0.0f)
        font_size = GImGui->FontSize;
    
    auto str_size = ImGui::CalcTextSize(_text_begin, _text_end);

    const float start = ImFmod((float)ImGui::GetTime() * speed, str_size.x);

    ImVec2 text_pos = pos;
    if (str_size.x > size.x)
    {
        text_pos.x -= start;
    }

    if (!drawList) drawList = ImGui::GetWindowDrawList();
    IM_ASSERT(drawList && font->ContainerAtlas->TexID == drawList->_TextureIdStack.back());  // Use high-level ImGui::PushFont() or low-level ImDrawList::PushTextureId() to change font.

    ImVec4 clip_rect = drawList->_ClipRectStack.back();
    clip_rect.x = ImMax(clip_rect.x, bb.Min.x);
    clip_rect.y = ImMax(clip_rect.y, bb.Min.y);
    clip_rect.z = ImMin(clip_rect.z, bb.Max.x);
    clip_rect.w = ImMin(clip_rect.w, bb.Max.y);
    font->RenderText(drawList, font_size, text_pos, col, clip_rect, _text_begin, _text_end, 0.0, true);
}

void ImGui::AddTextRolling(const char* text, const ImVec2& size, const ImVec2& pos, const int speed)
{
    ImGui::AddTextRolling(NULL, NULL, 0, pos, size, ImGui::GetColorU32(ImGuiCol_Text), speed, text);
}

void ImGui::AddTextRolling(const char* text, const ImVec2& size, const int speed)
{
    ImGui::AddTextRolling(NULL, NULL, 0, ImGui::GetCursorScreenPos(), size, ImGui::GetColorU32(ImGuiCol_Text), speed, text);
}

// add By Dicky
// Posted by @alexsr here: https://github.com/ocornut/imgui/issues/1901
// Sligthly modified to provide default behaviour with default args
void ImGui::LoadingIndicatorCircle(const char* label, float indicatorRadiusFactor,
                                    const ImVec4* pOptionalMainColor, const ImVec4* pOptionalBackdropColor,
                                    int circle_count,const float speed)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) {
        return;
    }

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const ImGuiStyle& style = GetStyle();

    if (circle_count<=0) circle_count = 12;
    if (indicatorRadiusFactor<=0.f) indicatorRadiusFactor = 1.f;
    if (!pOptionalMainColor)        pOptionalMainColor = &style.Colors[ImGuiCol_Button];
    if (!pOptionalBackdropColor)    pOptionalBackdropColor = &style.Colors[ImGuiCol_ButtonHovered];

    const float lineHeight = GetTextLineHeight(); // or GetTextLineHeight() or GetTextLineHeightWithSpacing() ?
    float indicatorRadiusPixels = indicatorRadiusFactor*lineHeight*0.5f;

    const ImVec2 pos = window->DC.CursorPos;
    const float circle_radius = indicatorRadiusPixels / 8.f;
    indicatorRadiusPixels-= 2.0f*circle_radius;
    const ImRect bb(pos, ImVec2(pos.x + indicatorRadiusPixels*2.f+4.f*circle_radius,
                                pos.y + indicatorRadiusPixels*2.f+4.f*circle_radius));
    ItemSize(bb, style.FramePadding.y);
    if (!ItemAdd(bb, id)) {
        return;
    }
    const float base_num_segments = circle_radius*1.f;
    const double t = g.Time;
    const float degree_offset = 2.0f * IM_PI / circle_count;
    for (int i = 0; i < circle_count; ++i) {
        const float sinx = -ImSin(degree_offset * i);
        const float cosx = ImCos(degree_offset * i);
        const float growth = ImMax(0.0f, ImSin((float)(t*(double)(speed*3.0f)-(double)(i*degree_offset))));
        ImVec4 color;
        color.x = pOptionalMainColor->x * growth + pOptionalBackdropColor->x * (1.0f - growth);
        color.y = pOptionalMainColor->y * growth + pOptionalBackdropColor->y * (1.0f - growth);
        color.z = pOptionalMainColor->z * growth + pOptionalBackdropColor->z * (1.0f - growth);
        color.w = 1.0f;
        float grown_circle_radius = circle_radius*(1.0f + growth);
        int num_segments = (int)(base_num_segments*grown_circle_radius);
        if (num_segments<4) num_segments=4;
        window->DrawList->AddCircleFilled(ImVec2(pos.x+2.f*circle_radius + indicatorRadiusPixels*(1.0f+sinx),
                                                pos.y+2.f*circle_radius + indicatorRadiusPixels*(1.0f+cosx)),
                                                grown_circle_radius,
                                                GetColorU32(color),num_segments);
    }
}

// Posted by @zfedoran here: https://github.com/ocornut/imgui/issues/1901
// Sligthly modified to provide default behaviour with default args
void ImGui::LoadingIndicatorCircle2(const char* label,float indicatorRadiusFactor, float indicatorRadiusThicknessFactor, const ImVec4* pOptionalColor)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    if (indicatorRadiusFactor<=0.f) indicatorRadiusFactor = 1.f;
    if (indicatorRadiusThicknessFactor<=0.f) indicatorRadiusThicknessFactor = 1.f;
    if (!pOptionalColor)    pOptionalColor = &style.Colors[ImGuiCol_Button];
    const ImU32 color = GetColorU32(*pOptionalColor);

    const float lineHeight = GetTextLineHeight(); // or GetTextLineHeight() or GetTextLineHeightWithSpacing() ?
    float indicatorRadiusPixels = indicatorRadiusFactor*lineHeight*0.5f;
    float indicatorThicknessPixels = indicatorRadiusThicknessFactor*indicatorRadiusPixels*0.6f;
    if (indicatorThicknessPixels>indicatorThicknessPixels*0.4f) indicatorThicknessPixels=indicatorThicknessPixels*0.4f;
    indicatorRadiusPixels-=indicatorThicknessPixels;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(indicatorRadiusPixels*2.f, (indicatorRadiusPixels + style.FramePadding.y)*2.f);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ItemSize(bb, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return;

    // Render
    window->DrawList->PathClear();

    //int num_segments = indicatorRadiusPixels/8.f;
    //if (num_segments<4) num_segments=4;

    int num_segments = 30;

    int start = abs(ImSin(g.Time*1.8f)*(num_segments-5));

    const float a_min = IM_PI*2.0f * ((float)start) / (float)num_segments;
    const float a_max = IM_PI*2.0f * ((float)num_segments-3) / (float)num_segments;

    const ImVec2 centre = ImVec2(pos.x+indicatorRadiusPixels, pos.y+indicatorRadiusPixels+style.FramePadding.y);

    for (int i = 0; i < num_segments; i++) {
        const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a+g.Time*8) * indicatorRadiusPixels,
                                            centre.y + ImSin(a+g.Time*8) * indicatorRadiusPixels));
    }

    window->DrawList->PathStroke(color, false, indicatorThicknessPixels);
}

int ImGui::PlotEx(ImGuiPlotType plot_type, const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 frame_size, bool b_tooltops, bool b_comband)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return -1;

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    if (frame_size.x == 0.0f)
        frame_size.x = CalcItemWidth();
    if (frame_size.y == 0.0f)
        frame_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0, &frame_bb))
        return -1;
    const bool hovered = b_tooltops /*&& ItemHoverable(frame_bb, id, ImGuiItemFlags_None)*/; // Modify By Dicky

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX)
    {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int i = 0; i < values_count; i++)
        {
            const float v = values_getter(data, i);
            if (v != v) // Ignore NaN values
                continue;
            v_min = ImMin(v_min, v);
            v_max = ImMax(v_max, v);
        }
        if (scale_min == FLT_MAX)
            scale_min = v_min;
        if (scale_max == FLT_MAX)
            scale_max = v_max;
    }

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    const int values_count_min = (plot_type == ImGuiPlotType_Lines) ? 2 : 1;
    int idx_hovered = -1;
    if (values_count >= values_count_min)
    {
        int res_w = ImMin((int)frame_size.x, values_count) + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);
        int item_count = values_count + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);

        const ImU32 col_base = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLines : ImGuiCol_PlotHistogram);
        const ImU32 col_hovered = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLinesHovered : ImGuiCol_PlotHistogramHovered);
        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

        // Tooltip on hover
        if (hovered && inner_bb.Contains(g.IO.MousePos))
        {
            const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
            const int v_idx = (int)(t * item_count);
            IM_ASSERT(v_idx >= 0 && v_idx < values_count);

            const float _v0 = values_getter(data, (v_idx + values_offset) % values_count);
            const float _v1 = values_getter(data, (v_idx + 1 + values_offset) % values_count);
            if (b_comband)
            {
                ImVec2 tp  = ImVec2(g.IO.MousePos.x, 1.0f - ImSaturate((_v0 - scale_min) * inv_scale)); 
                ImVec2 pos0 = ImVec2(g.IO.MousePos.x, inner_bb.Min.y);
                ImVec2 pos1 = ImVec2(g.IO.MousePos.x, inner_bb.Max.y);
                ImVec2 tpos = ImLerp(inner_bb.Min, inner_bb.Max, tp);
                tpos.x = g.IO.MousePos.x;
                window->DrawList->AddLine(pos0, pos1, col_hovered);
                window->DrawList->AddCircleFilled(tpos, 2, IM_COL32(0, 255, 0, 255));
            }
            if (plot_type == ImGuiPlotType_Lines && !b_comband)
                SetTooltip("%d: %8.4g\n%d: %8.4g", v_idx, _v0, v_idx + 1, _v1);
            else if (plot_type == ImGuiPlotType_Histogram || b_comband)
                SetTooltip("%d: %8.4g", v_idx, _v0);
            idx_hovered = v_idx;
        }

        float v0 = values_getter(data, (0 + values_offset) % values_count);
        float t0 = 0.0f;
        ImVec2 tp0 = ImVec2( t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale) );                       // Point in the normalized space of our target rectangle
        float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (1 + scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);   // Where does the zero line stands

        for (int n = 0; n < res_w; n++)
        {
            const float t1 = t0 + t_step;
            int v1_idx = (int)(t0 * item_count + 0.5f);
            if (v1_idx < 0) v1_idx = 0;
            if (v1_idx > values_count) v1_idx = values_count;
            const float v1 = values_getter(data, (v1_idx + values_offset + 1) % values_count);
            const ImVec2 tp1 = ImVec2( t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale) );

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, (plot_type == ImGuiPlotType_Lines) ? tp1 : ImVec2(tp1.x, histogram_zero_line_t));
            if (plot_type == ImGuiPlotType_Lines)
            {
                window->DrawList->AddLine(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
                if (b_comband)
                {
                    ImVec2 _pos1 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(tp1.x, histogram_zero_line_t));
                    if (_pos1.x >= pos0.x + 2.0f)
                        _pos1.x -= 1.0f;
                    window->DrawList->AddRectFilled(pos0, _pos1, idx_hovered == v1_idx ? GetColorU32(ImGuiCol_PlotHistogramHovered) : GetColorU32(ImGuiCol_PlotHistogram));
                }
            }
            else if (plot_type == ImGuiPlotType_Histogram)
            {
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                window->DrawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }

            t0 = t1;
            tp0 = tp1;
        }
    }

    // Text overlay
    if (overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f, 0.0f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    // Return hovered index or -1 if none are hovered.
    // This is currently not exposed in the public API because we need a larger redesign of the whole thing, but in the short-term we are making it available in PlotEx().
    return idx_hovered;
}

void ImGui::PlotMatEx(ImGui::ImMat& mat, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 frame_size, bool filled, bool b_fast, float thick, ImVec2 offset)
{
    if (values_count < 2) return;
    if (mat.empty() || mat.w < (int)frame_size.x || mat.h < (int)frame_size.y)
    {
        mat.create_type(frame_size.x, frame_size.y, 4);
        mat.elempack = 4;
    }

    auto line_color = ImGui::GetStyleColorVec4(ImGuiCol_PlotLines);
    auto histogram_color = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
    int res_w = ImMin((int)frame_size.x, values_count);
    int item_count = values_count - 1;
    const float t_step = 1.0f / (float)res_w;
    const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));
    float v0 = values_getter(data, (0 + values_offset) % values_count);
    float t0 = 0.0f;
    ImVec2 tp0 = ImVec2( t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale) );                       // Point in the normalized space of our target rectangle
    float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (1 + scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);   // Where does the zero line stands
    for (int n = 0; n < res_w; n++)
    {
        const float t1 = t0 + t_step;
        int v1_idx = (int)(t0 * item_count + 0.5f);
        if (v1_idx < 0) v1_idx = 0;
        if (v1_idx > values_count) v1_idx = values_count;
        const float v1 = values_getter(data, (v1_idx + values_offset + 1) % values_count);
        const ImVec2 tp1 = ImVec2( t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale) );
        ImVec2 pos0 = ImLerp(ImVec2(0, 0), frame_size, tp0);
        ImVec2 pos1 = ImLerp(ImVec2(0, 0), frame_size, tp1);
        if (b_fast)
            mat.draw_line(ImPoint(pos0.x + offset.x, pos0.y + offset.y), ImPoint(pos1.x + offset.x, pos1.y + offset.y), ImPixel(line_color.x, line_color.y, line_color.z, line_color.w));
        else
            mat.draw_line(ImPoint(pos0.x + offset.x, pos0.y + offset .y), ImPoint(pos1.x + offset.x, pos1.y + offset.y), thick, ImPixel(line_color.x, line_color.y, line_color.z, line_color.w));
        if (filled)
        {
            ImVec2 _pos1 = ImLerp(ImVec2(0, 0), frame_size, ImVec2(tp1.x, histogram_zero_line_t));
            if (b_fast)
                mat.draw_line(ImPoint(pos0.x + offset.x, pos0.y + offset.y), ImPoint(pos0.x + offset.x, pos0.y > _pos1.y ? _pos1.y + offset.y - 1 : _pos1.y + offset.y + 1), ImPixel(histogram_color.x, histogram_color.y, histogram_color.z, histogram_color.w));
            else
                mat.draw_line(ImPoint(pos0.x + offset.x, pos0.y + offset.y), ImPoint(pos0.x + offset.x, pos0.y > _pos1.y ? _pos1.y + offset.y - 1 : _pos1.y + offset.y + 1), thick, ImPixel(histogram_color.x, histogram_color.y, histogram_color.z, histogram_color.w));
        }
        t0 = t1;
        tp0 = tp1;
    }
}

struct ImGuiPlotArrayGetterData
{
    const float* Values;
    int Stride;

    ImGuiPlotArrayGetterData(const float* values, int stride) { Values = values; Stride = stride; }
};

static float Plot_ArrayGetter(void* data, int idx)
{
    ImGuiPlotArrayGetterData* plot_data = (ImGuiPlotArrayGetterData*)data;
    if (!plot_data || !plot_data->Values) return 0;
    const float v = *(const float*)(const void*)((const unsigned char*)plot_data->Values + (size_t)idx * plot_data->Stride);
    return v;
}

void ImGui::PlotLinesEx(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride, bool b_tooltips, bool b_comband)
{
    ImGuiPlotArrayGetterData data(values, stride);
    PlotEx(ImGuiPlotType_Lines, label, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size, b_tooltips, b_comband);
}

void ImGui::PlotLinesEx(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, bool b_tooltips, bool b_comband)
{
    PlotEx(ImGuiPlotType_Lines, label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size, b_tooltips, b_comband);
}

void ImGui::PlotMat(ImGui::ImMat& mat, const float* values, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, int stride, bool b_comband, bool b_fast, float thick)
{
    ImGuiPlotArrayGetterData data(values, stride);
    PlotMatEx(mat, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, scale_min, scale_max, graph_size, b_comband, b_fast, thick);
}

void ImGui::PlotMat(ImGui::ImMat& mat, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, bool b_comband, bool b_fast, float thick)
{
    PlotMatEx(mat, values_getter, data, values_count, values_offset, scale_min, scale_max, graph_size, b_comband, b_fast, thick);
}

void ImGui::PlotMat(ImGui::ImMat& mat, ImVec2 pos, const float* values, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, int stride, bool b_comband, bool b_fast, float thick)
{
    ImGuiPlotArrayGetterData data(values, stride);
    PlotMatEx(mat, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, scale_min, scale_max, graph_size, b_comband, b_fast, thick, pos);
}

void ImGui::PlotMat(ImGui::ImMat& mat, ImVec2 pos, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, bool b_comband, bool b_fast, float thick)
{
    PlotMatEx(mat, values_getter, data, values_count, values_offset, scale_min, scale_max, graph_size, b_comband, b_fast, thick, pos);
}

static bool IsRootOfOpenMenuSet()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if ((g.OpenPopupStack.Size <= g.BeginPopupStack.Size) || (window->Flags & ImGuiWindowFlags_ChildMenu))
        return false;

    // Initially we used 'OpenParentId' to differentiate multiple menu sets from each others (e.g. inside menu bar vs loose menu items) based on parent ID.
    // This would however prevent the use of e.g. PuhsID() user code submitting menus.
    // Previously this worked between popup and a first child menu because the first child menu always had the _ChildWindow flag,
    // making  hovering on parent popup possible while first child menu was focused - but this was generally a bug with other side effects.
    // Instead we don't treat Popup specifically (in order to consistently support menu features in them), maybe the first child menu of a Popup
    // doesn't have the _ChildWindow flag, and we rely on this IsRootOfOpenMenuSet() check to allow hovering between root window/popup and first chilld menu.
    const ImGuiPopupData* upper_popup = &g.OpenPopupStack[g.BeginPopupStack.Size];
    return (/*upper_popup->OpenParentId == window->IDStack.back() &&*/ upper_popup->Window && (upper_popup->Window->Flags & ImGuiWindowFlags_ChildMenu));
}

bool ImGui::MenuItemEx(const char* label, const char* icon, const char* shortcut, bool selected, bool enabled, const char* subscript)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 label_size = CalcTextSize(label, NULL, true);

    const bool menuset_is_open = IsRootOfOpenMenuSet();
    ImGuiWindow* backed_nav_window = g.NavWindow;
    if (menuset_is_open)
        g.NavWindow = window;

    // We've been using the equivalent of ImGuiSelectableFlags_SetNavIdOnHover on all Selectable() since early Nav system days (commit 43ee5d73),
    // but I am unsure whether this should be kept at all. For now moved it to be an opt-in feature used by menus only.
    bool pressed;
    PushID(label);
    if (!enabled)
        BeginDisabled();

    const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SelectOnRelease | ImGuiSelectableFlags_SetNavIdOnHover;
    const ImGuiMenuColumns* offsets = &window->DC.MenuColumns;
    if (window->DC.LayoutType == ImGuiLayoutType_Horizontal)
    {
        // Mimic the exact layout spacing of BeginMenu() to allow MenuItem() inside a menu bar, which is a little misleading but may be useful
        // Note that in this situation: we don't render the shortcut, we render a highlight instead of the selected tick mark.
        float w = label_size.x;
        window->DC.CursorPos.x += IM_FLOOR(style.ItemSpacing.x * 0.5f);
        ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * 2.0f, style.ItemSpacing.y));
        pressed = Selectable("", selected, selectable_flags, ImVec2(w, 0.0f));
        PopStyleVar();
        RenderText(text_pos, label);
        window->DC.CursorPos.x += IM_FLOOR(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
    }
    else
    {
        // Menu item inside a vertical menu
        // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
        //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.
        float icon_w = (icon && icon[0]) ? CalcTextSize(icon, NULL).x : 0.0f;
        float shortcut_w = (shortcut && shortcut[0]) ? CalcTextSize(shortcut, NULL).x : 0.0f;
        float checkmark_w = IM_FLOOR(g.FontSize * 1.20f);
        float min_w = window->DC.MenuColumns.DeclColumns(icon_w, label_size.x, shortcut_w, checkmark_w); // Feedback for next frame
        float stretch_w = ImMax(0.0f, GetContentRegionAvail().x - min_w);
        pressed = Selectable("", false, selectable_flags | ImGuiSelectableFlags_SpanAvailWidth, ImVec2(min_w, 0.0f));
        RenderText(pos + ImVec2(offsets->OffsetLabel, 0.0f), label);
        if (icon_w > 0.0f)
            RenderText(pos + ImVec2(offsets->OffsetIcon, 0.0f), icon);
        if (shortcut_w > 0.0f)
        {
            PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            RenderText(pos + ImVec2(offsets->OffsetShortcut + stretch_w, 0.0f), shortcut, NULL, false);
            PopStyleColor();
        }
        if (subscript)
            RenderText(pos + ImVec2(offsets->OffsetMark + stretch_w + g.FontSize * 0.40f, g.FontSize * 0.134f * 0.5f), subscript, NULL);
        else if (selected)
            RenderCheckMark(window->DrawList, pos + ImVec2(offsets->OffsetMark + stretch_w + g.FontSize * 0.40f, g.FontSize * 0.134f * 0.5f), GetColorU32(ImGuiCol_Text), g.FontSize  * 0.866f);
    }
    IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (selected ? ImGuiItemStatusFlags_Checked : 0));
    if (!enabled)
        EndDisabled();
    PopID();
    if (menuset_is_open)
        g.NavWindow = backed_nav_window;

    return pressed;
}

bool ImGui::MenuItem(const char* label, const char* shortcut, bool selected, bool enabled, const char* subscript)
{
    return MenuItemEx(label, NULL, shortcut, selected, enabled, subscript);
}

void ImGui::ProgressBarPanning(float fraction, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
    ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    
    // center line
    ImVec2 cpt1 = bb.Min + ImVec2(size.x / 2.f, 0);
    ImVec2 cpt2 = bb.Min + ImVec2(size.x / 2.f, size.y);
    window->DrawList->AddLine(cpt1, cpt2, IM_COL32_WHITE);

    if (fraction > 0)
    {
        ImVec2 pt1 = bb.Min + ImVec2(size.x / 2.f, 0); // center top
        ImVec2 pt2 = bb.Min + ImVec2(size.x * (0.5f + fraction), size.y);
        window->DrawList->AddRectFilled(pt1, pt2, GetColorU32(ImGuiCol_PlotHistogram), style.FrameRounding);
    }
    else if (fraction < 0)
    {
        ImVec2 pt1 = bb.Min + ImVec2(size.x * (0.5f + fraction), 0);
        ImVec2 pt2 = bb.Min + ImVec2(size.x / 2.f, size.y); // center bottom
        window->DrawList->AddRectFilled(pt1, pt2, GetColorU32(ImGuiCol_PlotHistogram), style.FrameRounding);
    }
}

#define KEY_NUM 58
static inline bool has_black(int key) 
{
    return (!((key - 1) % 7 == 0 || (key - 1) % 7 == 3) && key != KEY_NUM - 1);
}

void ImGui::Piano::up(int key)
{
    key_states[key] = 0;
}

void ImGui::Piano::down(int key, int velocity)
{
    key_states[key] = velocity;
}

void ImGui::Piano::draw_keyboard(ImVec2 size, bool input)
{
    static char key_name[] = { 'Q', '2', 'W', '3', 'E', 'R', '5', 'T', '6', 'Y', '7', 'U', 'Z', 'S', 'X', 'D', 'C', 'V', 'G', 'B', 'H', 'N', 'J', 'M'};
    char buf[4];
    ImGuiIO &io = ImGui::GetIO();
    ImU32 Black = IM_COL32(0, 0, 0, 255);
    ImU32 White = IM_COL32(255, 255, 255, 255);
    ImU32 White_Bark = IM_COL32(208, 208, 208, 255);
    ImU32 EventSustained = IM_COL32(192,192,192,255);
    ImU32 EventBlack = IM_COL32(255,0,0,255);
    ImGui::BeginGroup();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float key_width = size.x / KEY_NUM;
    float white_key_height = size.y;
    float black_key_height = size.y * 3.f / 5.f;
    int cur_key = 21;
    for (int key = 0; key < KEY_NUM; key++)
    {
        ImRect key_rect(ImVec2(p.x + key * key_width, p.y), ImVec2(p.x + key * key_width + key_width, p.y + white_key_height));
        ImU32 col = White;
        bool draw_text = false;
        char key_name_str = ' ';
        if (input)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && 
                key_rect.Contains(io.MousePos) && 
                (key_rect.Max.y - io.MousePos.y) < (white_key_height - black_key_height))
                key_states[cur_key] += 127;

            if (IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_RightAlt) && cur_key >= 24 && cur_key < 48)
            {
                col = White;
                draw_text = true;
                key_name_str = key_name[cur_key - 24];
            }
            else if (!IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_RightAlt) && cur_key >= 48 && cur_key < 72)
            {
                col = White;
                draw_text = true;
                key_name_str = key_name[cur_key - 48];
            }
            else if (IsKeyDown(ImGuiKey_RightAlt) && !IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && cur_key >= 72 && cur_key < 96)
            {
                col = White;
                draw_text = true;
                key_name_str = key_name[cur_key - 72];
            }
            else if (IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightAlt) && cur_key >= 96 && cur_key < 120)
            {
                col = White;
                draw_text = true;
                key_name_str = key_name[cur_key - 96];
            }
            else
                col = White_Bark;
        }

        if (key_states[cur_key])
        {
            if (key_states[cur_key] == -1)
                col = EventSustained;
            else if (key_states[cur_key] > 0)
            {
                int velocity = 255 - (key_states[cur_key] > 127 ? 127 : key_states[cur_key]) * 2;
                col = IM_COL32(velocity, 255, velocity, 255);
            }
        }
        draw_list->AddRectFilled(key_rect.Min, key_rect.Max, col, 2, ImDrawFlags_RoundCornersAll);
        draw_list->AddRect(key_rect.Min, key_rect.Max, Black, 2, ImDrawFlags_RoundCornersAll);
        if (draw_text && key_name_str != ' ')
        {
            ImFormatString(buf, 4, "%c\n", key_name_str);
            auto font_size = ImGui::CalcTextSize(buf);
            auto text_pos = ImVec2(key_rect.Min.x + (key_rect.GetWidth() - font_size.x) / 2, key_rect.Max.y - font_size.y - 4);
            draw_list->AddText(text_pos, IM_COL32_BLACK, buf);
        }
        cur_key++;
        if (has_black(key))
        {
            cur_key++;
        }
    }
    cur_key = 22;
    for (int key = 0; key < KEY_NUM; key++)
    {
        if (has_black(key))
        {
            ImRect key_rect(ImVec2(p.x + key * key_width + key_width * 3 / 4, p.y), ImVec2(p.x + key * key_width + key_width * 5 / 4 + 1, p.y + black_key_height));
            ImU32 col = Black;
            bool draw_text = false;
            char key_name_str = ' ';
            if (input)
            {
                if (key_rect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    key_states[cur_key] += 127;
                if (IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_RightAlt) && cur_key >= 24 && cur_key < 48)
                {
                    draw_text = true;
                    key_name_str = key_name[cur_key - 24];
                }
                else if (!IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_RightAlt) && cur_key >= 48 && cur_key < 72)
                {
                    draw_text = true;
                    key_name_str = key_name[cur_key - 48];
                }
                else if (IsKeyDown(ImGuiKey_RightAlt) && !IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && cur_key >= 72 && cur_key < 96)
                {
                    draw_text = true;
                    key_name_str = key_name[cur_key - 72];
                }
                else if (IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightAlt) && cur_key >= 96 && cur_key < 120)
                {
                    draw_text = true;
                    key_name_str = key_name[cur_key - 96];
                }
            }
            if (key_states[cur_key])
            {
                if (key_states[cur_key] == -1)
                    col = EventSustained;
                else if (key_states[cur_key] > 0)
                {
                    int velocity = 255 - (key_states[cur_key] > 127 ? 127 : key_states[cur_key]) * 2;
                    col = IM_COL32(255, velocity, velocity, 255);
                }
            }
            draw_list->AddRectFilled(key_rect.Min, key_rect.Max, col, 2, ImDrawFlags_RoundCornersAll);
            draw_list->AddRect(key_rect.Min, key_rect.Max, Black, 2, ImDrawFlags_RoundCornersAll);
            if (draw_text && key_name_str != ' ')
            {
                ImFormatString(buf, 4, "%c\n", key_name_str);
                auto font_size = ImGui::CalcTextSize(buf);
                auto text_pos = ImVec2(key_rect.Min.x + (key_rect.GetWidth() - font_size.x) / 2, key_rect.Max.y - font_size.y - 4);
                draw_list->AddText(text_pos, IM_COL32_WHITE, buf);
            }
            cur_key += 2;
        } 
        else
        {
            cur_key++;
        }
    }
    if (input)
    {
        /*                 1#  2#      4#  5#  6#
         *      ┏━━━┯━━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━━┯━━┓
         *      ┃ ` │ 1  │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ 0 │ - │ = │BS┃
         *      ┠───┼───┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┬┴──┨
         *      ┃Tab│Q1f│W2f│E3f│R4f│T5f│Y6f│U7f│ I │ O │ P │ [ │ ] │ | ┃
         *      ┠───┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴───┨
         *      ┃ Cap │ A │S1#│D2#│ F │G4#│H5#│J6#│ K │ L │ ; │ ' │ Ret ┃
         *      ┠─────┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─────┨
         * f2-> ┃ Shift │Z 1│X 2│C 3│V 4│B 5│N 6│M 7│ , │ . │ / │ Shift ┃<-g2
         *      ┠──┬───┬┴──┬┴──┬┴───┴───┴───┴───┴───┼───┼───┼───┼───┬───┨
         *      ┃Fn│Ctl│Alt│Cmd│      Space         │Cmd│Alt│ < │ v │ > ┃
         *      ┗━━┷━━━┷━━━┷━━━┷━━━━━━━━━━━━━━━━━━━━┷━━━┷━━━┷━━━┷━━━┷━━━┛
         *                             clean             g3
        */
        auto check_key = [&](ImGuiKey key, int ckey)
        {
            if          (ImGui::IsKeyDown(key))        key_states[ckey] += 127;
            else if (ImGui::IsKeyReleased(key))  { if (key_states[ckey] > 0) key_states[ckey] = -2; }
        };

        if (IsKeyDown(ImGuiKey_Space) || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            for (int i = 0; i < 256; i++) if (key_states[i] > 0) key_states[i] = -2;
        }
        else if (IsKeyReleased(ImGuiKey_LeftShift))
        {
            for (int i = 0; i < 48; i++) if (key_states[i] > 0) key_states[i] = -2;
        }
        else if (IsKeyReleased(ImGuiKey_RightAlt))
        {
            for (int i = 72; i < 96; i++) if (key_states[i] > 0) key_states[i] = -2;
        }
        else if (IsKeyReleased(ImGuiKey_RightShift))
        {
            for (int i = 96; i < 120; i++) if (key_states[i] > 0) key_states[i] = -2;
        }
        else
        {
            int cur_key = 48;
            if (IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_RightAlt))
                cur_key -= 12 * 2;
            else if (IsKeyDown(ImGuiKey_RightAlt) && !IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightShift))
                cur_key += 12 * 2;
            else if (IsKeyDown(ImGuiKey_RightShift) && !IsKeyDown(ImGuiKey_LeftShift) && !IsKeyDown(ImGuiKey_RightAlt))
                cur_key += 12 * 4;
            // low key
            check_key(ImGuiKey_Q, cur_key++);   // F1
            check_key(ImGuiKey_2, cur_key++);   // F1#
            check_key(ImGuiKey_W, cur_key++);   // F2
            check_key(ImGuiKey_3, cur_key++);   // F2#
            check_key(ImGuiKey_E, cur_key++);   // F3
            check_key(ImGuiKey_R, cur_key++);   // F4
            check_key(ImGuiKey_5, cur_key++);   // F4#
            check_key(ImGuiKey_T, cur_key++);   // F5
            check_key(ImGuiKey_6, cur_key++);   // F5#
            check_key(ImGuiKey_Y, cur_key++);   // F6
            check_key(ImGuiKey_7, cur_key++);   // F6#
            check_key(ImGuiKey_U, cur_key++);   // F7
            // high key
            check_key(ImGuiKey_Z, cur_key++);   // C1
            check_key(ImGuiKey_S, cur_key++);   // C1#
            check_key(ImGuiKey_X, cur_key++);   // C2
            check_key(ImGuiKey_D, cur_key++);   // C2#
            check_key(ImGuiKey_C, cur_key++);   // C3
            check_key(ImGuiKey_V, cur_key++);   // C4
            check_key(ImGuiKey_G, cur_key++);   // C4#
            check_key(ImGuiKey_B, cur_key++);   // C5
            check_key(ImGuiKey_H, cur_key++);   // C5#
            check_key(ImGuiKey_N, cur_key++);   // C6
            check_key(ImGuiKey_J, cur_key++);   // C6#
            check_key(ImGuiKey_M, cur_key++);   // C7
        }
    }
    ImGui::InvisibleButton("##keyboard", size);
    ImGui::EndGroup();
}

void ImGui::Piano::reset()
{
    memset(key_states, 0, sizeof(key_states));
}

void ImGui::ImSpectrogram(const ImGui::ImMat& in_mat, ImGui::ImMat& out_mat, int window, bool bstft, int hope)
{
    assert(in_mat.c == 1 && in_mat.h == 1 && in_mat.type == IM_DT_FLOAT32);
    auto powoftwo = [&](int n) { return(n > 0 && !(n & (n - 1))); };
    if (window == 0 || !powoftwo(window)) window = 512;
    if (bstft) { if (hope == 0 || hope > window) hope = window / 2; }

    int blocks = in_mat.w / window;
    if (bstft) blocks = in_mat.w / hope - 1;
    int N_FRQ = (window >> 1) + 1;
    out_mat.create_type(blocks, N_FRQ, 4, IM_DT_INT8);
    out_mat.elempack = 4;
    float* pin = (float *)in_mat.data;
    float* pin_end =  pin + in_mat.w;
    if (bstft)
    {
        ImGui::ImSTFT stft(window, hope);
        ImGui::ImMat fft_data, db_data, zero_data;
        fft_data.create_type(window, IM_DT_FLOAT32);
        zero_data.create_type(window, IM_DT_FLOAT32);
        db_data.create_type(N_FRQ, IM_DT_FLOAT32);
        int current_block = 0;
        int length = 0;
        while (length < in_mat.w + window - hope)
        {
            float * in_data;
            if (length < in_mat.w - hope)
                in_data = pin + length;
            else
                in_data = (float *)zero_data.data;

            stft.stft(in_data, (float *)fft_data.data);
            ImGui::ImReComposeDB((float*)fft_data.data, (float *)db_data.data, fft_data.w, false);

            if (length >= window - hope)
            {
                for (int n = 0; n < out_mat.h; n++)
                {
                    float value = db_data.at<float>(n) * M_SQRT2 + 64;
                    value = ImClamp(value, -64.f, 63.f);
                    float light = (value + 64) / 127.f;
                    light = ImClamp(light, 0.f, 1.f);
                    value = (int)((value + 64) + 170) % 255; 
                    auto hue = value / 255.f;
                    auto color = ImColor::HSV(hue, 1.0, light);
                    out_mat.set_pixel(current_block, out_mat.h - n - 1, ImPixel(color.Value.x, color.Value.y, color.Value.z, color.Value.w));
                }
                current_block ++;
            }
            length += hope;
        }
    }
    else
    {
        ImGui::ImMat fft_data, db_data;
        fft_data.create_type(window, IM_DT_FLOAT32);
        db_data.create_type(N_FRQ, IM_DT_FLOAT32);
        int current_block = 0;
        while (pin < pin_end)
        {
            ImGui::ImRFFT(pin, (float*)fft_data.data, window, true);
            ImGui::ImReComposeDB((float*)fft_data.data, (float *)db_data.data, fft_data.w, false);
            for (int n = 0; n < out_mat.h; n++)
            {
                float value = db_data.at<float>(n) * M_SQRT2 + 64;
                value = ImClamp(value, -64.f, 63.f);
                float light = (value + 64) / 127.f;
                value = (int)((value + 64) + 170) % 255; 
                auto hue = value / 255.f;
                auto color = ImColor::HSV(hue, 1.0, light);
                out_mat.set_pixel(current_block, out_mat.h - n - 1, ImPixel(color.Value.x, color.Value.y, color.Value.z, color.Value.w));
            }
            pin += window;
            current_block ++;
        }
    }
}

// Start CheckboxFlags ==============================================================================================
unsigned int ImGui::CheckboxFlags(const char* label,unsigned int* flags,int numFlags,int numRows,int numColumns,unsigned int flagAnnotations,int* itemHoveredOut,const unsigned int* pFlagsValues)
{
    unsigned int itemPressed = 0;
    if (itemHoveredOut) *itemHoveredOut=-1;
    if (numRows*numColumns*numFlags==0) return itemPressed;

    IM_ASSERT(flags);
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return itemPressed;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    int numItemsPerRow = numFlags/numRows;if (numFlags%numRows) ++numItemsPerRow;
    int numItemsPerGroup = numItemsPerRow/numColumns;if (numItemsPerRow%numColumns) ++numItemsPerGroup;
    numItemsPerRow = numItemsPerGroup * numColumns;

    ImGui::BeginGroup();
    const ImVec2 startCurPos = window->DC.CursorPos;
    ImVec2 curPos = startCurPos, maxPos = startCurPos;
    const ImVec2 checkSize(label_size.y,label_size.y);
    const float pad = ImMax(1.0f, (float)(int)(checkSize.x / 6.0f));
    unsigned int j=0;int groupItemCnt = 0, groupCnt = 0;
    const bool buttonPressed = ImGui::IsMouseClicked(0);

    ImU32 annColor = GetColorU32(ImGuiCol_FrameBg);int annSegments = 3;
    float annThickness = checkSize.x,annCenter = checkSize.x,annRadius = checkSize.x;
    if (flagAnnotations)    {
        annColor = GetColorU32(ImGuiCol_Button);
        annSegments = (int) (checkSize.x * 0.4f);if (annSegments<3) annSegments=3;
        annThickness = checkSize.x / 6.0f;
        annCenter = (float)(int)(checkSize.x * 0.5f);
        annRadius = (float)(int)(checkSize.x / 4.0f);
    }

    for (int i=0;i<numFlags;i++) {
        j = pFlagsValues ? pFlagsValues[i] : (1<<i);
        ImGui::PushID(i);
        ImGuiID itemID = window->GetID(label);
        ImGui::PopID();

        bool v = ((*flags & j) == j);
        const bool vNotif =  ((flagAnnotations & j) == j);
        ImRect bb(curPos,curPos + checkSize);
        ItemSize(bb, style.FramePadding.y);

        if (!ItemAdd(bb, itemID)) break;

        bool hovered=false, held= false, pressed = false;
        // pressed = ButtonBehavior(bb, itemID, &hovered, &held);   // Too slow!
        hovered = ItemHoverable(bb, itemID, ImGuiItemFlags_None);pressed =  buttonPressed && hovered;   // Way faster
        if (pressed) {
            v = !v;
            if (!ImGui::GetIO().KeyShift) *flags=0;
            if (v)  *flags |= j;
            else    *flags &= ~j;
            itemPressed = j;
        }
        if (itemHoveredOut && hovered) *itemHoveredOut=i;

        RenderFrame(bb.Min, bb.Max, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);

        if (v)  {
            window->DrawList->AddRectFilled(bb.Min+ImVec2(pad,pad), bb.Max-ImVec2(pad,pad), GetColorU32(ImGuiCol_CheckMark), style.FrameRounding);
            if (vNotif) window->DrawList->AddCircle(bb.Min+ImVec2(annCenter,annCenter), annRadius, annColor,annSegments,annThickness);
        }
        else if (vNotif) window->DrawList->AddCircle(bb.Min+ImVec2(annCenter,annCenter), annRadius, annColor,annSegments,annThickness);

        // Nope: LogRenderedText is static inside imgui.cpp => we remove it, so that imguivariouscontrols.h/cpp can be used on their own too
        //if (g.LogEnabled) LogRenderedText(&bb.Min, v ? "[x]" : "[ ]");

        curPos.x+=checkSize.x;
        if (maxPos.x<curPos.x) maxPos.x=curPos.x;
        if (i<numFlags-1) {
            ++groupCnt;
            if (++groupItemCnt==numItemsPerGroup) {
                groupItemCnt=0;
                curPos.x+=style.FramePadding.y*2;
                if (groupCnt>=numItemsPerRow) {
                    groupCnt=0;
                    curPos.x = startCurPos.x;
                    curPos.y+=checkSize.y;
                    if (maxPos.y<curPos.y) maxPos.y=curPos.y;
                }
                else {
                    ImGui::SameLine(0,0);
                }
            }
            else ImGui::SameLine(0,0);
        }

    }
    ImGui::EndGroup();


    if (label_size.x > 0) {
        const float spacing = style.ItemInnerSpacing.x;
        SameLine(0, spacing);
        const ImVec2 textStart(maxPos.x+spacing,startCurPos.y);
        const ImRect text_bb(textStart, textStart + label_size);


        ImGuiID itemID = window->GetID(label);
        ItemSize(text_bb, style.FramePadding.y*numRows);
        if (ItemAdd(text_bb, itemID)) RenderText(text_bb.Min, label);
    }

    return itemPressed;
}

unsigned int ImGui::CheckboxFlags(const char* label,int* flags,int numFlags,int numRows,int numColumns,unsigned int flagAnnotations,int* itemHoveredOut,const unsigned int* pFlagsValues)  {
    IM_ASSERT(flags);
    unsigned tmp = (unsigned) *flags;
    unsigned rv = CheckboxFlags(label,&tmp,numFlags,numRows,numColumns,flagAnnotations,itemHoveredOut,pFlagsValues);
    *flags = (int) tmp;
    return rv;
}
// End CheckboxFlags =================================================================================================

// draw leader
static void draw_badge(ImDrawList* drawList, ImRect bb, int type, bool filled, bool arrow, ImU32 color)
{
    const ImGuiStyle& style = GImGui->Style;
    auto rect           = bb;
    auto rect_x         = rect.Min.x;
    auto rect_y         = rect.Min.y;
    auto rect_w         = rect.Max.x - rect.Min.x;
    auto rect_h         = rect.Max.y - rect.Min.y;
    auto rect_center_x  = (rect.Min.x + rect.Max.x) * 0.5f;
    auto rect_center_y  = (rect.Min.y + rect.Max.y + style.FramePadding.y) * 0.5f;
    auto rect_center    = ImVec2(rect_center_x, rect_center_y);
    auto rect_center_l  = ImVec2(rect_center_x - 2.0f, rect_center_y);
    const   auto outline_scale  = ImMin(rect_w, rect_h) / 24.0f;
    const   auto extra_segments = static_cast<int>(2 * outline_scale); // for full circle
    auto triangleStart = rect_center_x + 0.32f * rect_w;
    auto rect_offset = -static_cast<int>(rect_w * 0.25f * 0.25f);

    rect.Min.x    += rect_offset;
    rect.Max.x    += rect_offset;
    rect_x        += rect_offset;
    rect_center_x += rect_offset * 0.5f;
    rect_center.x += rect_offset * 0.5f;

    switch (type)
    {
        case 0:
        {
            // Circle
            const auto c = rect_center;
            if (!filled)
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                drawList->AddCircle(c, r, color, 12 + extra_segments, 2.0f * outline_scale);
            }
            else
            {
                drawList->AddCircleFilled(c, 0.5f * rect_w / 2.0f, color, 12 + extra_segments);
            }
        }
        break;
        case 1:
        {
            // Square
            if (filled)
            {
                const auto r  = 0.5f * rect_w / 2.0f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);
                drawList->AddRectFilled(p0, p1, color, 0, ImDrawFlags_RoundCornersAll);
            }
            else
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);
                drawList->AddRect(p0, p1, color, 0, extra_segments, 2.0f * outline_scale);
            }
        }
        break;
        case 2:
        {
            // BracketSquare
            const auto r  = 0.5f * rect_w / 2.0f;
            const auto w = ceilf(r / 3.0f);
            const auto s = r / 1.5f;
            const auto p00 = rect_center - ImVec2(r, r);
            const auto p00w = p00 + ImVec2(s, 0);
            const auto p01 = p00 + ImVec2(0, 2 * r);
            const auto p01w = p01 + ImVec2(s, 0);
            const auto p10 = p00 + ImVec2(2 * r, 0);
            const auto p10w = p10 - ImVec2(s, 0);
            const auto p11 = rect_center + ImVec2(r, r);
            const auto p11w = p11 - ImVec2(s, 0);
            drawList->AddLine(p00, p01, color, 2.0f * outline_scale);
            drawList->AddLine(p10, p11, color, 2.0f * outline_scale);
            drawList->AddLine(p00, p00w, color, 2.0f * outline_scale);
            drawList->AddLine(p01, p01w, color, 2.0f * outline_scale);
            drawList->AddLine(p10, p10w, color, 2.0f * outline_scale);
            drawList->AddLine(p11, p11w, color, 2.0f * outline_scale);
            if (filled)
            {
                const auto pc0 = rect_center - ImVec2(1, 1);
                const auto pc1 = rect_center + ImVec2(2, 2);
                drawList->AddRectFilled(pc0, pc1, color, 0, ImDrawFlags_RoundCornersAll);
            }
            triangleStart = p11.x + w + 1.0f / 8.0f * rect_w;
        }
        break;
        case 3:
        {
            // RoundSquare
            if (filled)
            {
                const auto r  = 0.5f * rect_w / 2.0f;
                const auto cr = r * 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);
                drawList->AddRectFilled(p0, p1, color, cr, ImDrawFlags_RoundCornersAll);
            }
            else
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                const auto cr = r * 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);
                drawList->AddRect(p0, p1, color, cr, ImDrawFlags_RoundCornersAll, 2.0f * outline_scale);
            }
        }
        break;
        case 4:
        {
            // GridSquare
            const auto r = 0.5f * rect_w / 2.0f;
            const auto w = ceilf(r / 3.0f);
            const auto baseTl = ImVec2(floorf(rect_center_x - w * 2.5f), floorf(rect_center_y - w * 2.5f));
            const auto baseBr = ImVec2(floorf(baseTl.x + w), floorf(baseTl.y + w));
            auto tl = baseTl;
            auto br = baseBr;
            for (int i = 0; i < 3; ++i)
            {
                tl.x = baseTl.x;
                br.x = baseBr.x;
                drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                if (i != 1 || filled)
                    drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                drawList->AddRectFilled(tl, br, color);
                tl.y += w * 2;
                br.y += w * 2;
            }
            triangleStart = br.x + w + 1.0f / 8.0f * rect_w;
        }
        break;
        case 5:
        {
            // Diamond
            if (filled)
            {
                const auto r = 0.607f * rect_w / 2.0f;
                const auto c = rect_center;
                drawList->PathLineTo(c + ImVec2( 0, -r));
                drawList->PathLineTo(c + ImVec2( r,  0));
                drawList->PathLineTo(c + ImVec2( 0,  r));
                drawList->PathLineTo(c + ImVec2(-r,  0));
                drawList->PathFillConvex(color);
            }
            else
            {
                const auto r = 0.607f * rect_w / 2.0f - 0.5f;
                const auto c = rect_center;
                drawList->PathLineTo(c + ImVec2( 0, -r));
                drawList->PathLineTo(c + ImVec2( r,  0));
                drawList->PathLineTo(c + ImVec2( 0,  r));
                drawList->PathLineTo(c + ImVec2(-r,  0));
                drawList->PathStroke(color, true, 2.0f * outline_scale);
            }
        }
        break;
        default:
        break;
    }
    
    if (arrow)
    {
        const auto triangleTip = triangleStart + rect_w * (0.45f - 0.32f);
        drawList->AddTriangleFilled(
            ImVec2(ceilf(triangleTip), rect_y + (rect_h + style.FramePadding.y) * 0.5f),
            ImVec2(triangleStart, rect_center_y + 0.15f * (rect_h + style.FramePadding.y)),
            ImVec2(triangleStart, rect_center_y - 0.15f * (rect_h + style.FramePadding.y)),
            color);
    }
}

static void draw_leader(int type, bool filled, bool arrow)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;
    auto drawList = window->DrawList;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float line_height = ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), g.FontSize);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(g.FontSize, line_height));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0))
    {
        ImGui::SameLine(0, style.FramePadding.x * 2);
        return;
    }
    
    draw_badge(drawList, bb, type, filled, arrow, ImGui::GetColorU32(ImGuiCol_Text));

    ImGui::SameLine(0, style.FramePadding.x * 2.0f);
}

void ImGui::Circle(bool filled, bool arrow)
{
    draw_leader(0, filled, arrow);
}

void ImGui::Square(bool filled, bool arrow)
{
    draw_leader(1, filled, arrow);
}

void ImGui::BracketSquare(bool filled, bool arrow)
{
    draw_leader(2, filled, arrow);
}

void ImGui::RoundSquare(bool filled, bool arrow)
{
    draw_leader(3, filled, arrow);
}

void ImGui::GridSquare(bool filled, bool arrow)
{
    draw_leader(4, filled, arrow);
}

void ImGui::Diamond(bool filled, bool arrow)
{
    draw_leader(5, filled, arrow);
}

static bool draw_leader_button(const char* id_str, int type, bool filled, bool arrow, ImGuiButtonFlags flags)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    auto drawList = window->DrawList;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(id_str);
    const float line_height = ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), g.FontSize);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(g.FontSize, line_height));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;
    
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
    // Render
    const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImGui::RenderNavHighlight(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    draw_badge(drawList, bb, type, filled, arrow, color);
    
    return pressed;
}

bool ImGui::CircleButton(const char* id_str, bool filled, bool arrow)
{
    return draw_leader_button(id_str, 0, filled, arrow, ImGuiButtonFlags_None);
}

bool ImGui::SquareButton(const char* id_str, bool filled, bool arrow)
{
    return draw_leader_button(id_str, 1, filled, arrow, ImGuiButtonFlags_None);
}

bool ImGui::BracketSquareButton(const char* id_str, bool filled, bool arrow)
{
    return draw_leader_button(id_str, 2, filled, arrow, ImGuiButtonFlags_None);
}

bool ImGui::RoundSquareButton(const char* id_str, bool filled, bool arrow)
{
    return draw_leader_button(id_str, 3, filled, arrow, ImGuiButtonFlags_None);
}

bool ImGui::GridSquareButton(const char* id_str, bool filled, bool arrow)
{
    return draw_leader_button(id_str, 4, filled, arrow, ImGuiButtonFlags_None);
}

bool ImGui::DiamondButton(const char* id_str, bool filled, bool arrow)
{
    return draw_leader_button(id_str, 5, filled, arrow, ImGuiButtonFlags_None);
}

namespace ImGui {
// PieMenu code based code posted by @thennequin here: https://gist.github.com/thennequin/64b4b996ec990c6ddc13a48c6a0ba68c
// Hope we can use it...
    namespace Pie   {
    struct PieMenuContext
    {
        static const int	c_iMaxPieMenuStack = 8;
        static const int	c_iMaxPieItemCount = 12;
        static const int	c_iRadiusEmpty = 30;
        static const int	c_iRadiusMin = 30;
        static const int	c_iMinItemCount = 3;
        static const int	c_iMinItemCountPerLevel = 3;

        struct PieMenu
        {
            int				m_iCurrentIndex;
            float			m_fMaxItemSqrDiameter;
            float			m_fLastMaxItemSqrDiameter;
            int				m_iHoveredItem;
            int				m_iLastHoveredItem;
            int				m_iClickedItem;
            bool			m_oItemIsSubMenu[c_iMaxPieItemCount];
            ImVector<char>	m_oItemNames[c_iMaxPieItemCount];
            ImVec2			m_oItemSizes[c_iMaxPieItemCount];
        };

        PieMenuContext()
        {
            m_iCurrentIndex = -1;
            m_iLastFrame = 0;
        }

        PieMenu				m_oPieMenuStack[c_iMaxPieMenuStack];
        int					m_iCurrentIndex;
        int					m_iMaxIndex;
        int					m_iLastFrame;
        ImVec2				m_oCenter;
        int					m_iMouseButton;
        bool				m_bClose;
    };

    static PieMenuContext s_oPieMenuContext;

    static bool IsPopupOpen(const char* pName)
    {
        ImGuiID iID = ImGui::GetID(pName);
        ImGuiContext& g = *GImGui;
        return g.OpenPopupStack.Size > g.BeginPopupStack.Size && g.OpenPopupStack[g.BeginPopupStack.Size].PopupId == iID;
    }

    static void BeginPieMenuEx()
    {
        IM_ASSERT(s_oPieMenuContext.m_iCurrentIndex < PieMenuContext::c_iMaxPieMenuStack);

        ++s_oPieMenuContext.m_iCurrentIndex;
        ++s_oPieMenuContext.m_iMaxIndex;

        PieMenuContext::PieMenu& oPieMenu = s_oPieMenuContext.m_oPieMenuStack[s_oPieMenuContext.m_iCurrentIndex];
        oPieMenu.m_iCurrentIndex = 0;
        oPieMenu.m_fMaxItemSqrDiameter = 0.f;
        if( !ImGui::IsMouseReleased( s_oPieMenuContext.m_iMouseButton ) )
            oPieMenu.m_iHoveredItem = -1;
        if (s_oPieMenuContext.m_iCurrentIndex > 0)
            oPieMenu.m_fMaxItemSqrDiameter = s_oPieMenuContext.m_oPieMenuStack[s_oPieMenuContext.m_iCurrentIndex - 1].m_fMaxItemSqrDiameter;
    }

    static void EndPieMenuEx()
    {
        IM_ASSERT(s_oPieMenuContext.m_iCurrentIndex >= 0);
        //PieMenuContext::PieMenu& oPieMenu = s_oPieMenuContext.m_oPieMenuStack[s_oPieMenuContext.m_iCurrentIndex];

        --s_oPieMenuContext.m_iCurrentIndex;
    }

    /* Definition */
    bool BeginPopup(const char* pName, int iMouseButton)
    {
        if (IsPopupOpen(pName))
        {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);

            s_oPieMenuContext.m_iMouseButton = iMouseButton;
            s_oPieMenuContext.m_bClose = false;

            //ImGui::SetNextWindowPos( ImVec2( -100.f, -100.f ), ImGuiCond_Appearing );
            bool bOpened = ImGui::BeginPopup(pName, ImGuiWindowFlags_NoBackground);
            if (bOpened)
            {
                int iCurrentFrame = ImGui::GetFrameCount();
                if (s_oPieMenuContext.m_iLastFrame < (iCurrentFrame - 1))
                {
                    s_oPieMenuContext.m_oCenter = ImGui::GetIO().MousePos;
                }
                s_oPieMenuContext.m_iLastFrame = iCurrentFrame;

                s_oPieMenuContext.m_iMaxIndex = -1;
                BeginPieMenuEx();
                return true;
            }
            else
            {
                ImGui::End();
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);
            }
        }
        return false;
    }

    void EndPopup()
    {
        EndPieMenuEx();

        ImGuiStyle& oStyle = ImGui::GetStyle();
        const ImVec2& vTexUvWhitePixel = ImGui::GetDrawListSharedData()->TexUvWhitePixel;

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();
        pDrawList->PushClipRectFullScreen();

        const ImVec2 oMousePos = ImGui::GetIO().MousePos;
        const ImVec2 oDragDelta = ImVec2(oMousePos.x - s_oPieMenuContext.m_oCenter.x, oMousePos.y - s_oPieMenuContext.m_oCenter.y);
        const float fDragDistSqr = oDragDelta.x*oDragDelta.x + oDragDelta.y*oDragDelta.y;

        float fCurrentRadius = (float)PieMenuContext::c_iRadiusEmpty;

        ImRect oArea = ImRect( s_oPieMenuContext.m_oCenter, s_oPieMenuContext.m_oCenter );

        bool bItemHovered = false;

        const float c_fDefaultRotate = -IM_PI / 2.f;
        float fLastRotate = c_fDefaultRotate;
        for (int iIndex = 0; iIndex <= s_oPieMenuContext.m_iMaxIndex; ++iIndex)
        {
            PieMenuContext::PieMenu& oPieMenu = s_oPieMenuContext.m_oPieMenuStack[iIndex];

            float fMenuHeight = sqrt(oPieMenu.m_fMaxItemSqrDiameter);

            const float fMinRadius = fCurrentRadius;
            const float fMaxRadius = fMinRadius + (fMenuHeight * oPieMenu.m_iCurrentIndex) / ( 2.f );

            const float item_arc_span = 2 * IM_PI / ImMax(PieMenuContext::c_iMinItemCount + PieMenuContext::c_iMinItemCountPerLevel * iIndex, oPieMenu.m_iCurrentIndex);
            float drag_angle = atan2f(oDragDelta.y, oDragDelta.x);

            float fRotate = fLastRotate - item_arc_span * ( oPieMenu.m_iCurrentIndex - 1.f ) / 2.f;
            int item_hovered = -1;
            for( int item_n = 0; item_n < oPieMenu.m_iCurrentIndex; item_n++ )
            {
                const char* item_label = oPieMenu.m_oItemNames[ item_n ].Data;
                //const float inner_spacing = oStyle.ItemInnerSpacing.x / fMinRadius / 2;
                const float fMinInnerSpacing = oStyle.ItemInnerSpacing.x / ( fMinRadius * 2.f );
                const float fMaxInnerSpacing = oStyle.ItemInnerSpacing.x / ( fMaxRadius * 2.f );
                const float item_inner_ang_min = item_arc_span * ( item_n - 0.5f + fMinInnerSpacing ) + fRotate;
                const float item_inner_ang_max = item_arc_span * ( item_n + 0.5f - fMinInnerSpacing ) + fRotate;
                const float item_outer_ang_min = item_arc_span * ( item_n - 0.5f + fMaxInnerSpacing ) + fRotate;
                const float item_outer_ang_max = item_arc_span * ( item_n + 0.5f - fMaxInnerSpacing ) + fRotate;

                bool hovered = false;
                if( fDragDistSqr >= fMinRadius * fMinRadius && fDragDistSqr < fMaxRadius * fMaxRadius )
                {
                    while( ( drag_angle - item_inner_ang_min ) < 0.f )
                        drag_angle += 2.f * IM_PI;
                    while( ( drag_angle - item_inner_ang_min ) > 2.f * IM_PI )
                        drag_angle -= 2.f * IM_PI;

                    if( drag_angle >= item_inner_ang_min && drag_angle < item_inner_ang_max )
                    {
                        hovered = true;
                        bItemHovered = !oPieMenu.m_oItemIsSubMenu[ item_n ];
                    }
                }

                int arc_segments = (int)( 64 * item_arc_span / ( 2 * IM_PI ) ) + 1;

                ImU32 iColor = hovered ? ImColor( 100, 100, 150 ) : ImColor( 70, 70, 70 );
                iColor = ImGui::GetColorU32( hovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg );
                iColor = ImGui::GetColorU32( hovered ? ImGuiCol_Button : ImGuiCol_ButtonHovered );
                //iColor |= 0xFF000000;

                const float fAngleStepInner = (item_inner_ang_max - item_inner_ang_min) / arc_segments;
                const float fAngleStepOuter = ( item_outer_ang_max - item_outer_ang_min ) / arc_segments;
                pDrawList->PrimReserve(arc_segments * 6, (arc_segments + 1) * 2);
                for (int iSeg = 0; iSeg <= arc_segments; ++iSeg)
                {
                    float fCosInner = cosf(item_inner_ang_min + fAngleStepInner * iSeg);
                    float fSinInner = sinf(item_inner_ang_min + fAngleStepInner * iSeg);
                    float fCosOuter = cosf(item_outer_ang_min + fAngleStepOuter * iSeg);
                    float fSinOuter = sinf(item_outer_ang_min + fAngleStepOuter * iSeg);

                    if (iSeg < arc_segments)
                    {
                        pDrawList->PrimWriteIdx(pDrawList->_VtxCurrentIdx + 0);
                        pDrawList->PrimWriteIdx(pDrawList->_VtxCurrentIdx + 2);
                        pDrawList->PrimWriteIdx(pDrawList->_VtxCurrentIdx + 1);
                        pDrawList->PrimWriteIdx(pDrawList->_VtxCurrentIdx + 3);
                        pDrawList->PrimWriteIdx(pDrawList->_VtxCurrentIdx + 2);
                        pDrawList->PrimWriteIdx(pDrawList->_VtxCurrentIdx + 1);
                    }
                    pDrawList->PrimWriteVtx(ImVec2(s_oPieMenuContext.m_oCenter.x + fCosInner * (fMinRadius + oStyle.ItemInnerSpacing.x), s_oPieMenuContext.m_oCenter.y + fSinInner * (fMinRadius + oStyle.ItemInnerSpacing.x)), vTexUvWhitePixel, iColor);
                    pDrawList->PrimWriteVtx(ImVec2(s_oPieMenuContext.m_oCenter.x + fCosOuter * (fMaxRadius - oStyle.ItemInnerSpacing.x), s_oPieMenuContext.m_oCenter.y + fSinOuter * (fMaxRadius - oStyle.ItemInnerSpacing.x)), vTexUvWhitePixel, iColor);
                }

                float fRadCenter = ( item_arc_span * item_n ) + fRotate;
                ImVec2 oOuterCenter = ImVec2( s_oPieMenuContext.m_oCenter.x + cosf( fRadCenter ) * fMaxRadius, s_oPieMenuContext.m_oCenter.y + sinf( fRadCenter ) * fMaxRadius );
                oArea.Add( oOuterCenter );

                if (oPieMenu.m_oItemIsSubMenu[item_n])
                {
                    ImVec2 oTrianglePos[3];

                    float fRadLeft = fRadCenter - 5.f / fMaxRadius;
                    float fRadRight = fRadCenter + 5.f / fMaxRadius;

                    oTrianglePos[ 0 ].x = s_oPieMenuContext.m_oCenter.x + cosf( fRadCenter ) * ( fMaxRadius - 5.f );
                    oTrianglePos[ 0 ].y = s_oPieMenuContext.m_oCenter.y + sinf( fRadCenter ) * ( fMaxRadius - 5.f );
                    oTrianglePos[ 1 ].x = s_oPieMenuContext.m_oCenter.x + cosf( fRadLeft ) * ( fMaxRadius - 10.f );
                    oTrianglePos[ 1 ].y = s_oPieMenuContext.m_oCenter.y + sinf( fRadLeft ) * ( fMaxRadius - 10.f );
                    oTrianglePos[ 2 ].x = s_oPieMenuContext.m_oCenter.x + cosf( fRadRight ) * ( fMaxRadius - 10.f );
                    oTrianglePos[ 2 ].y = s_oPieMenuContext.m_oCenter.y + sinf( fRadRight ) * ( fMaxRadius - 10.f );

                    pDrawList->AddTriangleFilled(oTrianglePos[0], oTrianglePos[1], oTrianglePos[2], ImColor(255, 255, 255));
                }

                ImVec2 text_size = oPieMenu.m_oItemSizes[item_n];
                ImVec2 text_pos = ImVec2(
                    s_oPieMenuContext.m_oCenter.x + cosf((item_inner_ang_min + item_inner_ang_max) * 0.5f) * (fMinRadius + fMaxRadius) * 0.5f - text_size.x * 0.5f,
                    s_oPieMenuContext.m_oCenter.y + sinf((item_inner_ang_min + item_inner_ang_max) * 0.5f) * (fMinRadius + fMaxRadius) * 0.5f - text_size.y * 0.5f);
                pDrawList->AddText(text_pos, ImColor(255, 255, 255), item_label);

                if (hovered)
                    item_hovered = item_n;
            }

            fCurrentRadius = fMaxRadius;

            oPieMenu.m_fLastMaxItemSqrDiameter = oPieMenu.m_fMaxItemSqrDiameter;

            oPieMenu.m_iHoveredItem = item_hovered;

            if (fDragDistSqr >= fMaxRadius * fMaxRadius)
                item_hovered = oPieMenu.m_iLastHoveredItem;

            oPieMenu.m_iLastHoveredItem = item_hovered;

            fLastRotate = item_arc_span * oPieMenu.m_iLastHoveredItem + fRotate;
            if( item_hovered == -1 || !oPieMenu.m_oItemIsSubMenu[item_hovered] )
                break;
        }

        pDrawList->PopClipRect();

        if( oArea.Min.x < 0.f )
        {
            s_oPieMenuContext.m_oCenter.x = ( s_oPieMenuContext.m_oCenter.x - oArea.Min.x );
        }
        if( oArea.Min.y < 0.f )
        {
            s_oPieMenuContext.m_oCenter.y = ( s_oPieMenuContext.m_oCenter.y - oArea.Min.y );
        }

        ImVec2 oDisplaySize = ImGui::GetIO().DisplaySize;
        if( oArea.Max.x > oDisplaySize.x )
        {
            s_oPieMenuContext.m_oCenter.x = ( s_oPieMenuContext.m_oCenter.x - oArea.Max.x ) + oDisplaySize.x;
        }
        if( oArea.Max.y > oDisplaySize.y )
        {
            s_oPieMenuContext.m_oCenter.y = ( s_oPieMenuContext.m_oCenter.y - oArea.Max.y ) + oDisplaySize.y;
        }

        if( s_oPieMenuContext.m_bClose || ( !bItemHovered && ImGui::IsMouseReleased( s_oPieMenuContext.m_iMouseButton ) )
                )
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    bool BeginMenu(const char* pName)//, bool bEnabled)
    {
        IM_ASSERT(s_oPieMenuContext.m_iCurrentIndex >= 0 && s_oPieMenuContext.m_iCurrentIndex < PieMenuContext::c_iMaxPieItemCount);

        PieMenuContext::PieMenu& oPieMenu = s_oPieMenuContext.m_oPieMenuStack[s_oPieMenuContext.m_iCurrentIndex];

        ImVec2 oTextSize = ImGui::CalcTextSize(pName, NULL, true);
        oPieMenu.m_oItemSizes[oPieMenu.m_iCurrentIndex] = oTextSize;

        float fSqrDiameter = oTextSize.x * oTextSize.x + oTextSize.y * oTextSize.y;

        if (fSqrDiameter > oPieMenu.m_fMaxItemSqrDiameter)
        {
            oPieMenu.m_fMaxItemSqrDiameter = fSqrDiameter;
        }

        oPieMenu.m_oItemIsSubMenu[oPieMenu.m_iCurrentIndex] = true;

        int iLen = strlen(pName);
        ImVector<char>& oName = oPieMenu.m_oItemNames[oPieMenu.m_iCurrentIndex];
        oName.resize(iLen + 1);
        oName[iLen] = '\0';
        memcpy(oName.Data, pName, iLen);

        if (oPieMenu.m_iLastHoveredItem == oPieMenu.m_iCurrentIndex)
        {
            ++oPieMenu.m_iCurrentIndex;

            BeginPieMenuEx();
            return true;
        }
        ++oPieMenu.m_iCurrentIndex;

        return false;
    }

    void EndMenu()
    {
        IM_ASSERT(s_oPieMenuContext.m_iCurrentIndex >= 0 && s_oPieMenuContext.m_iCurrentIndex < PieMenuContext::c_iMaxPieItemCount);
        --s_oPieMenuContext.m_iCurrentIndex;
    }

    bool MenuItem(const char* pName) //, bool bEnabled)
    {
        IM_ASSERT(s_oPieMenuContext.m_iCurrentIndex >= 0 && s_oPieMenuContext.m_iCurrentIndex < PieMenuContext::c_iMaxPieItemCount);

        PieMenuContext::PieMenu& oPieMenu = s_oPieMenuContext.m_oPieMenuStack[s_oPieMenuContext.m_iCurrentIndex];

        ImVec2 oTextSize = ImGui::CalcTextSize(pName, NULL, true);
        oPieMenu.m_oItemSizes[oPieMenu.m_iCurrentIndex] = oTextSize;

        float fSqrDiameter = oTextSize.x * oTextSize.x + oTextSize.y * oTextSize.y;

        if (fSqrDiameter > oPieMenu.m_fMaxItemSqrDiameter)
        {
            oPieMenu.m_fMaxItemSqrDiameter = fSqrDiameter;
        }

        oPieMenu.m_oItemIsSubMenu[oPieMenu.m_iCurrentIndex] = false;

        int iLen = strlen(pName);
        ImVector<char>& oName = oPieMenu.m_oItemNames[oPieMenu.m_iCurrentIndex];
        oName.resize(iLen + 1);
        oName[iLen] = '\0';
        memcpy(oName.Data, pName, iLen);

        bool bActive = oPieMenu.m_iCurrentIndex == oPieMenu.m_iHoveredItem;
        ++oPieMenu.m_iCurrentIndex;

        if( bActive )   {
            s_oPieMenuContext.m_bClose = true;
        }

        return bActive;
    }

    }   // namespace Pie
} // namespace ImGui

bool ImGui::MsgBox::Init(const char *title, const char *icon, const char *text, const char **captions, bool show_checkbox)
{
    m_Title = title;
    m_Icon = icon;
    m_Text = text;
    m_Captions = captions;
    m_ShowCheckbox = show_checkbox;
    m_DontAskAgain = false;
    m_Selected = 0;
    return true;
}

int ImGui::MsgBox::Draw(float wrap_width)
{
    int index = 0;
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    if (ImGui::BeginPopupModal(m_Title, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        if (m_DontAskAgain && m_Selected != 0)
        {
            ImGui::CloseCurrentPopup();
            index = m_Selected;
        }
        else
        {
            if (m_Icon != NULL)
            {
                ImVec2 size = ImGui::CalcTextSize(m_Icon);
                ImVec2 pos = ImGui::GetCursorPos();
                float save_y = pos.y;
                pos.y += size.y / 2;
                ImGui::SetCursorPos(pos);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 0.0, 1.0));
                ImGui::Text("%s", m_Icon);
                ImGui::PopStyleColor();
                pos.x += size.x + pos.x;
                pos.y = save_y;
                ImGui::SetCursorPos(pos);
                ImGui::PushTextWrapPos(pos.x + wrap_width);
                ImGui::TextUnformatted(m_Text);
                ImGui::PopTextWrapPos();
            }
            else
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
                ImGui::TextUnformatted(m_Text);
                ImGui::PopTextWrapPos();
            }
            ImGui::Separator();
            if (m_ShowCheckbox)
            {
                ImGui::Checkbox("Don't ask me again", &m_DontAskAgain);
            }
            
            ImVec2 size = ImVec2(50.0f, 0.0f);
            int count;
            for (count = 0; m_Captions[count] != NULL; count++)
            {
                if (ImGui::Button(m_Captions[count], size))
                {
                    index = count + 1;
                    ImGui::CloseCurrentPopup();
                    break;
                }
                ImGui::SameLine();
            }
            size = ImVec2((4 - count) * 50.0f, 0.0f);
            ImGui::Dummy(size);
            if (m_DontAskAgain)
            {
                m_Selected = index;
            }
        }
        ImGui::EndPopup();
    }
    return index;
}

void ImGui::MsgBox::Open()
{
    ImGui::OpenPopup(m_Title, ImGuiPopupFlags_AnyPopup);
}

// VirtualKeyboard Implementation
const char** ImGui::GetKeyboardLogicalLayoutNames()    {
    static const char* names[] = {"QWERTY","QWERTZ","AZERTY"};
    IM_STATIC_ASSERT(IM_ARRAYSIZE(names)==KLL_COUNT);
    return names;
}
const char** ImGui::GetKeyboardPhysicalLayoutNames()    {
    static const char* names[] = {"ANSI","ISO","JIS"};
    IM_STATIC_ASSERT(IM_ARRAYSIZE(names)==KPL_COUNT);
    return names;
}

ImGuiKey ImGui::VirtualKeyboard(VirtualKeyboardFlags flags,KeyboardLogicalLayout logicalLayout,KeyboardPhysicalLayout physicalLayout)   {

    IM_ASSERT(logicalLayout!=KLL_COUNT && physicalLayout!=KPL_COUNT);
    const float sf = GetFontSize()/12.f; // scaling factor
    const float  key_rounding = 3.0f;
    const ImVec2 key_face_size = ImVec2(25.0f, 25.0f)*sf;
    const float key_border_width = 10.f*sf;
    const ImVec2 key_size_base = ImVec2(key_face_size.x+key_border_width, key_face_size.y+key_border_width);
    const ImVec2 key_face_pos = ImVec2(5.0f, 3.0f)*sf;
    const float  key_face_rounding = 2.0f;
    const ImVec2 key_label_pos = ImVec2(7.0f, 4.0f)*sf;
    const ImVec2 key_step = ImVec2(key_size_base.x - 1.0f*sf, key_size_base.y - 1.0f*sf);
    const float thickness = 1.f*sf; // used by AddRect(...) and AddLine(...) calls
    const float thicknessDouble = 2.f*thickness;

    ImVec2 board_min = GetCursorScreenPos();
    //ImVec2 board_max = ImVec2(board_min.x + 3 * key_step.x + 2 * key_row_offset + 10.0f, board_min.y + 3 * key_step.y + 10.0f);
    //ImVec2 start_pos = ImVec2(board_min.x + 5.0f - key_step.x, board_min.y);
    ImVec2 start_pos = ImVec2(board_min.x, board_min.y);

    // These should match the 'GKeyNames' variable in  'imgui.cpp', displaying what's written on the keys... most values are unused for now...
    static const char* KeyLabels[] =  {
        "|<-\n->|", "<", ">", "^", "v", "Pg^", "Pgv",
        "Home", "End", "Ins", "Canc", "<-", "", "Enter", "Esc",

        "Ctrl", "^", "Alt", "Super", "Ctrl", "^", "Alt", "Super", "Menu",

        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F", "G", "H",
        "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",

        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
        "'", ",", "-", ".", "/", ";", "=", "(",
        "\\", ")", "`", "CapsLock", "Scr\nLck", "Nm\nLck", "Print", // [...] graveaccent, capsL scrollL numL Print

        "Pause", "0", "1", "2", "3", "4", "5", "6",
        "7", "8", "9", ".", "/", "*",
        "-", "+", "Ent", "=",

        "Start", "Back", "FaceUp", "FaceDown", "FaceLeft", "FaceRight",
        "^", "v", "<", ">",                                     // Dpad
        "L1", "R1", "L2", "R2", "L3", "R3",
        "^", "v", "<", ">",  // LStick
        "^", "v", "<", ">",   // RStick
        "ModCtrl", "ModShift", "ModAlt", "ModSuper"
    };
    //IM_STATIC_ASSERT(ImGuiKey_NamedKey_COUNT == IM_ARRAYSIZE(KeyLabels));

    // USAGE:
//#           define IMIMPL_KEY_LABEL(key)  ((key==ImGuiKey_None)?"":KeyLabels[key-ImGuiKey_NamedKey_BEGIN]):
    //IM_ASSERT(key>=ImGuiKey_NamedKey_BEGIN && key<ImGuiKey_NamedKey_END);

    struct KeyLayoutName {ImGuiKey key;float width;};

    static KeyLayoutName keyNamesRows[6][26] ={
        // keyNamesRows[row in 0-6][column in 0-26]

        // rows are trivial: 0 -> F1, F2, etc.;     5 -> Ctrl, Super, Alt, Spacebar, etc.

        // columns:
        //
        //    |---------------------------------|---------------------|-------------------|
        //    0                               17 18                 21 22                 26
        //    |   Base                          |     Arrow           |    Keypad         |

        // ISO Layout
        {{ImGuiKey_Escape,1.75f/1.4f},{ImGuiKey_None,-0.9f/1.2f},{ImGuiKey_F1,1.f},{ImGuiKey_F2,1.f},{ImGuiKey_F3,1.f},{ImGuiKey_F4,1.f},{ImGuiKey_None,-0.52f/1.2f},{ImGuiKey_F5,1.f},{ImGuiKey_F6,1.f},{ImGuiKey_F7,1.f},{ImGuiKey_F8,1.f},{ImGuiKey_None,-0.52f/1.2f},{ImGuiKey_F9,1.f},{ImGuiKey_F10,1.f},{ImGuiKey_F11,1.f},{ImGuiKey_F12,1.f},{ImGuiKey_None,0.f},    {ImGuiKey_None,-0.52f},    {ImGuiKey_PrintScreen,1.f},{ImGuiKey_ScrollLock,1.f},{ImGuiKey_Pause,1.f},    {ImGuiKey_None,0.f},    {ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f}             },
        {{ImGuiKey_None,1.f},{ImGuiKey_1,1.f},{ImGuiKey_2,1.f},{ImGuiKey_3,1.f},{ImGuiKey_4,1.f},{ImGuiKey_5,1.f},{ImGuiKey_6,1.f},{ImGuiKey_7,1.f},{ImGuiKey_8,1.f},{ImGuiKey_9,1.f},{ImGuiKey_0,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,0.f},{ImGuiKey_Backspace,3.25f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},    {ImGuiKey_None,-0.52f},          {ImGuiKey_Insert,1.f},{ImGuiKey_Home,1.f},{ImGuiKey_PageUp,1.f},    {ImGuiKey_None,-0.52f/1.2f},    {ImGuiKey_NumLock,1.f},{ImGuiKey_KeypadDivide,1.f},{ImGuiKey_KeypadMultiply,1.f},{ImGuiKey_KeypadSubtract,1.f}      },
        {{ImGuiKey_Tab,2.15f/1.2f},{ImGuiKey_Q,1.f},{ImGuiKey_W,1.f},{ImGuiKey_E,1.f},{ImGuiKey_R,1.f},{ImGuiKey_T,1.f},{ImGuiKey_Y,1.f},{ImGuiKey_U,1.f},{ImGuiKey_I,1.f},{ImGuiKey_O,1.f},{ImGuiKey_P,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,1.f},{ImGuiKey_Enter,2.3f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},    {ImGuiKey_None,-0.52f},         {ImGuiKey_Delete,1.f},{ImGuiKey_End,1.f},{ImGuiKey_PageDown,1.f},    {ImGuiKey_None,-0.52f/1.2f},    {ImGuiKey_Keypad7,1.f},{ImGuiKey_Keypad8,1.f},{ImGuiKey_Keypad9,1.f},{ImGuiKey_KeypadAdd,1.f}      },
        {{ImGuiKey_CapsLock,2.7f/1.2f},{ImGuiKey_A,1.f},{ImGuiKey_S,1.f},{ImGuiKey_D,1.f},{ImGuiKey_F,1.f},{ImGuiKey_G,1.f},{ImGuiKey_H,1.f},{ImGuiKey_J,1.f},{ImGuiKey_K,1.f},{ImGuiKey_L,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,1.f},{ImGuiKey_Enter,1.74f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},    {ImGuiKey_None,-0.52f}, {ImGuiKey_None,-1.f},{ImGuiKey_None,-1.f},{ImGuiKey_None,-1.f},    {ImGuiKey_None,-0.52f/1.2f},    {ImGuiKey_Keypad4,1.f},{ImGuiKey_Keypad5,1.f},{ImGuiKey_Keypad6,1.f},{ImGuiKey_KeypadAdd,1.f}                    },
        {{ImGuiKey_LeftShift,1.74f/1.2f},{ImGuiKey_None,1.f},{ImGuiKey_Z,1.f},{ImGuiKey_X,1.f},{ImGuiKey_C,1.f},{ImGuiKey_V,1.f},{ImGuiKey_B,1.f},{ImGuiKey_N,1.f},{ImGuiKey_M,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,1.f},{ImGuiKey_None,0.f},{ImGuiKey_RightShift,4.4f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},    {ImGuiKey_None,-0.6f/1.2f},    {ImGuiKey_None,-1.f},{ImGuiKey_UpArrow,1.f},{ImGuiKey_None,-1.f},    {ImGuiKey_None,-0.52f/1.2f},    {ImGuiKey_Keypad1,1.f},{ImGuiKey_Keypad2,1.f},{ImGuiKey_Keypad3,1.f},{ImGuiKey_KeypadEnter,1.f}                    },
        {{ImGuiKey_LeftCtrl,1.74f/1.2f},{ImGuiKey_LeftSuper,1.74f/1.2f},{ImGuiKey_LeftAlt,1.74f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_Space,9.6f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_RightAlt,1.74f/1.2f},{ImGuiKey_RightSuper,1.74f/1.2f},{ImGuiKey_Menu,1.74f/1.2f},{ImGuiKey_RightCtrl,1.74f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},{ImGuiKey_None,0.f},    {ImGuiKey_None,-0.52f/1.2f},    {ImGuiKey_LeftArrow,1.f},{ImGuiKey_DownArrow,1.f},{ImGuiKey_RightArrow,1.f},    {ImGuiKey_None,-0.52f/1.2f},    {ImGuiKey_Keypad0,2.9f/1.2f},{ImGuiKey_None,0.f},{ImGuiKey_KeypadDecimal,1.f},{ImGuiKey_KeypadEnter,1.f}                    }
    };

    static KeyboardPhysicalLayout phyLayout = KPL_COUNT;  // KPL_ISO, but it's just for initializing stuff
    static KeyboardLogicalLayout logLayout = KLL_QWERTY;

    if (phyLayout==KPL_COUNT)    {
        phyLayout = KPL_ISO;

        // well, we should do this every frame (or store a reference of the last checked font),
        // but we just do it once at startup
        static const char* utf8[] = {
            "\xe2\x87\xa7",  // 0x21E7 ⇧
            "\xe2\xac\x86",  // 0x2B06 ⬆
            "\xe2\x86\x91",  // 0x2191 ↑
            "\xe2\x9f\xb5",  // 0x27F5 ⟵
            "\xe2\x86\x90",  // 0x2190 ←
            "\xe2\x86\x96",  // 0x2196 ↖
            "\xe2\x8c\x82",  // 0x2302 ⌂
            "\xe2\x8f\x8f",  // 0x23CF ⏏
            "\xe2\x86\xb9",  // 0x21B9 ↹
            "\xe2\x87\xa4\n\xe2\x87\xa5",  // 0x21E4,0x21E5 ⇤⇥
            "\xe2\x86\x92",  // 0x2191 →
            "\xe2\x86\x93",  // 0x2191 ↓
            "Pg\xe2\x86\x91",  // 0x2191 Pg↑
            "Pg\xe2\x86\x93",  // 0x2193 Pg↓
            "\xe2\x88\x97",     // 0x2217 ∗
            "\xe2\x88\x92",     // 0x2212 −
            " \xe2\x8f\xb8",     // 0x23f8 ⏸
            " \xe2\x96\xae\xe2\x96\xae",     // 0x25AE ▮▮
            "\xe2\x96\xae\xe2\x96\xaf",     // 0x25AF ▯▯
            "Enter\n\xe2\x86\xb5",    // 0x21B5 ↵
            "Enter\n\xe2\x86\xa9",    // 0x21A9 ↩
            "Enter\n\xe2\xa4\xb6"   // 0x2936 ⤶
        };

        const ImFont* font = ImGui::GetFont();
#       define IMIMPL_CHANGE_GLYPH(K,C,S)     if (font->FindGlyphNoFallback((C)))  KeyLabels[(K)-ImGuiKey_NamedKey_BEGIN]=S
#       define IMIMPL_CHANGE_GLYPH_FALLBACK1(K,C,S,C1,S1)     if (font->FindGlyphNoFallback((C)))  KeyLabels[(K)-ImGuiKey_NamedKey_BEGIN]=S;  \
                                                            else if (font->FindGlyphNoFallback((C1)))  KeyLabels[(K)-ImGuiKey_NamedKey_BEGIN]=S1
#       define IMIMPL_CHANGE_GLYPH_FALLBACK2(K,C,S,C1,S1,C2,S2)     if (font->FindGlyphNoFallback((C)))  KeyLabels[(K)-ImGuiKey_NamedKey_BEGIN]=S;  \
                                                                    else if (font->FindGlyphNoFallback((C1)))  KeyLabels[(K)-ImGuiKey_NamedKey_BEGIN]=S1;   \
                                                                    else if (font->FindGlyphNoFallback((C2)))  KeyLabels[(K)-ImGuiKey_NamedKey_BEGIN]=S2
#       define IMIMPL_CHANGE_GLYPH_ARROWS(KL,KU,KR,KD,CL,CU,CR,CD,SL,SU,SR,SD)     if (font->FindGlyphNoFallback((CL)) && font->FindGlyphNoFallback((CU)) && font->FindGlyphNoFallback((CR)) && font->FindGlyphNoFallback((CD))) \
                                                                                        {KeyLabels[(KL)-ImGuiKey_NamedKey_BEGIN]=SL;KeyLabels[(KU)-ImGuiKey_NamedKey_BEGIN]=SU;KeyLabels[(KR)-ImGuiKey_NamedKey_BEGIN]=SR;KeyLabels[(KD)-ImGuiKey_NamedKey_BEGIN]=SD;}

        IMIMPL_CHANGE_GLYPH_FALLBACK2(ImGuiKey_LeftShift, 0x21E7,utf8[0],0x2B06,utf8[1],0x2191,utf8[2]);
        IMIMPL_CHANGE_GLYPH_FALLBACK2(ImGuiKey_RightShift, 0x21E7,utf8[0],0x2B06,utf8[1],0x2191,utf8[2]);
        IMIMPL_CHANGE_GLYPH_FALLBACK1(ImGuiKey_Backspace,0x27F5,utf8[3],0x2190,utf8[4]);
        IMIMPL_CHANGE_GLYPH_FALLBACK1(ImGuiKey_Home, 0x2196,utf8[5],0x2302,utf8[6]);
        IMIMPL_CHANGE_GLYPH(ImGuiKey_CapsLock, 0x23CF,utf8[7]);
        IMIMPL_CHANGE_GLYPH(ImGuiKey_Tab, 0x21B9,utf8[8]);
        else if (font->FindGlyphNoFallback(0x21E4) && font->FindGlyphNoFallback(0x21E5)) KeyLabels[ImGuiKey_Tab-ImGuiKey_NamedKey_BEGIN]=utf8[9];
        IMIMPL_CHANGE_GLYPH_ARROWS(ImGuiKey_LeftArrow,ImGuiKey_UpArrow,ImGuiKey_RightArrow,ImGuiKey_DownArrow,
                                   0x2190,0x2191,0x2192,0x2193,
                                   utf8[4],utf8[5],utf8[10],utf8[11])        // no semicolon here
        IMIMPL_CHANGE_GLYPH_ARROWS(ImGuiKey_GamepadDpadLeft,ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadRight,ImGuiKey_GamepadDpadDown,
                                   0x2190,0x2191,0x2192,0x2193,
                                           utf8[4],utf8[5],utf8[10],utf8[11])        // no semicolon here
        IMIMPL_CHANGE_GLYPH_ARROWS(ImGuiKey_GamepadLStickLeft,ImGuiKey_GamepadLStickUp,ImGuiKey_GamepadLStickRight,ImGuiKey_GamepadLStickDown,
                                   0x2190,0x2191,0x2192,0x2193,
                                   utf8[4],utf8[5],utf8[10],utf8[11])        // no semicolon here
        IMIMPL_CHANGE_GLYPH_ARROWS(ImGuiKey_GamepadRStickLeft,ImGuiKey_GamepadRStickUp,ImGuiKey_GamepadRStickRight,ImGuiKey_GamepadRStickDown,
                                   0x2190,0x2191,0x2192,0x2193,
                                   utf8[4],utf8[5],utf8[10],utf8[11])        // no semicolon here
        IMIMPL_CHANGE_GLYPH_ARROWS(ImGuiKey_GamepadFaceLeft,ImGuiKey_GamepadFaceUp,ImGuiKey_GamepadFaceRight,ImGuiKey_GamepadFaceDown,
                                   0x2190,0x2191,0x2192,0x2193,
                                    utf8[4],utf8[5],utf8[10],utf8[11])        // no semicolon here
        IMIMPL_CHANGE_GLYPH(ImGuiKey_PageUp, 0x2191,utf8[12]);
        IMIMPL_CHANGE_GLYPH(ImGuiKey_PageDown, 0x2193,utf8[13]);
        IMIMPL_CHANGE_GLYPH(ImGuiKey_KeypadMultiply, 0x2217,utf8[14]);
        IMIMPL_CHANGE_GLYPH(ImGuiKey_KeypadSubtract, 0x2212,utf8[15]);
        IMIMPL_CHANGE_GLYPH_FALLBACK2(ImGuiKey_Pause, 0x23f8,utf8[16],0x25AE,utf8[17],0x25AF,utf8[18]);
        IMIMPL_CHANGE_GLYPH_FALLBACK2(ImGuiKey_Enter, 0x21B5,utf8[19],0x21A9,utf8[20],0x2936,utf8[21]);


#       undef IMIMPL_CHANGE_GLYPH
#       undef IMIMPL_CHANGE_GLYPH_FALLBACK1
#       undef IMIMPL_CHANGE_GLYPH_FALLBACK2
#       undef IMIMPL_CHANGE_GLYPH_ARROWS
    }

    IM_ASSERT(phyLayout!=KPL_COUNT && logLayout!=KLL_COUNT);

    if (phyLayout!=physicalLayout)  {
        // we must update the physical layout
        phyLayout = physicalLayout;
        KeyLayoutName *R1 = keyNamesRows[1], *R2 = keyNamesRows[2], *R3 = keyNamesRows[3], *R4 = keyNamesRows[4], *R5 = keyNamesRows[5];
        IM_ASSERT(R1[14].key==ImGuiKey_Backspace);
        IM_ASSERT(R2[13].key==ImGuiKey_Enter || R2[13].key==ImGuiKey_Backslash);
        IM_ASSERT(R3[13].key==ImGuiKey_Enter);
        IM_ASSERT(R4[13].key==ImGuiKey_RightShift);
        IM_ASSERT(R5[4].key==ImGuiKey_Space && R5[7].key==ImGuiKey_RightAlt && R5[10].key==ImGuiKey_RightCtrl);
        switch (phyLayout)  {
        case KPL_ANSI:   {
            R1[13].width=0.f;R1[14].width=3.25f/1.2f;
            R2[13].key=ImGuiKey_Backslash;
            R3[12].width=0.f;R3[13].width=3.425f/1.2f;
            R4[0].width=3.45f/1.2f; R4[1].width=0.f;R4[12].width=0.f;R4[13].width=4.4f/1.2f;//R4[12].width=1.f;R4[13].width=2.725f/1.2f;
            R5[3].width=0.f;R5[4].width=9.6f/1.2f;R5[5].width=0.f;R5[6].width=0.f;R5[7].width=1.74f/1.2f;R5[8].width=1.74f/1.2f;R5[9].width=1.74f/1.2f;R5[10].width=1.74f/1.2f;
        }
            break;
        case KPL_ISO:    {
            R1[13].width=0.f;R1[14].width=3.25f/1.2f;
            R2[13].key=ImGuiKey_Enter;
            R3[12].width=1.f;R3[13].width=1.74f/1.2f;
            R4[0].width=1.74f/1.2f; R4[1].width=1.f;R4[12].width=0.f;R4[13].width=4.4f/1.2f;
            R5[3].width=0.f;R5[4].width=9.6f/1.2f;R5[5].width=0.f;R5[6].width=0.f;R5[7].width=1.74f/1.2f;R5[8].width=1.74f/1.2f;R5[9].width=1.74f/1.2f;R5[10].width=1.74f/1.2f;
        }
            break;
        case KPL_JIS:    {
            R1[13].width=1.f;R1[14].width=1.575f/1.2f;
            R2[13].key=ImGuiKey_Enter;
            R3[12].width=1.f;R3[13].width=1.74f/1.2f;
            R4[0].width=3.45f/1.2f; R4[1].width=0.f;R4[12].width=1.f;R4[13].width=2.725f/1.2f;
            R5[3].width=0.98f;R5[4].width=6.1f/1.2f;R5[5].width=0.98f;R5[6].width=0.98f;R5[7].width=1.38f/1.2f;R5[8].width=1.38f/1.2f;R5[9].width=1.38f/1.2f;R5[10].width=1.38f/1.2f;
        }
            break;
        default:
            IM_ASSERT(0);
            break;

        }
    }
    if (logLayout!=logicalLayout)    {
        // we must update the keyboard layout
        logLayout = logicalLayout;
        KeyLayoutName *R2 = keyNamesRows[2], *R3 = keyNamesRows[3], *R4 = keyNamesRows[4];
        switch (logLayout)  {
        case KLL_QWERTY:  {
            // QWERTY
            R2[1].key=ImGuiKey_Q;R2[2].key=ImGuiKey_W;R2[6].key=ImGuiKey_Y;
            R3[1].key=ImGuiKey_A;R3[10].key=ImGuiKey_None;
            R4[2].key=ImGuiKey_Z;R4[8].key=ImGuiKey_M;
        }
            break;
        case KLL_QWERTZ:  {
            // QWERTZ
            R2[1].key=ImGuiKey_Q;R2[2].key=ImGuiKey_W;R2[6].key=ImGuiKey_Z;
            R3[1].key=ImGuiKey_A;R3[10].key=ImGuiKey_None;
            R4[2].key=ImGuiKey_Y;R4[8].key=ImGuiKey_M;
        }
            break;
        case KLL_AZERTY: {
            // AZERTY
            R2[1].key=ImGuiKey_A;R2[2].key=ImGuiKey_Z;R2[6].key=ImGuiKey_Y;
            R3[1].key=ImGuiKey_Q;R3[10].key=ImGuiKey_M;
            R4[2].key=ImGuiKey_W;R4[8].key=ImGuiKey_None;
        }
            break;
        default:
            IM_ASSERT(0);
            break;
        }
    }

    ImVec2 cur_pos = start_pos;ImVec2 board_max = start_pos;
    ImDrawList* draw_list = GetWindowDrawList();

    int rStart = 0,rEnd = 6;
    int cStart = 0,cEnd = 26;

    if (flags&VirtualKeyboardFlags_ShowBaseBlock && flags&VirtualKeyboardFlags_ShowKeypadBlock) flags|=VirtualKeyboardFlags_ShowArrowBlock;
    if (!(flags&VirtualKeyboardFlags_ShowFunctionBlock)) rStart=1;
    if (flags&VirtualKeyboardFlags_ShowFunctionBlock && !(flags&(VirtualKeyboardFlags_ShowBaseBlock|VirtualKeyboardFlags_ShowArrowBlock|VirtualKeyboardFlags_ShowKeypadBlock))) rEnd=1;
    if (!(flags&(VirtualKeyboardFlags_ShowArrowBlock|VirtualKeyboardFlags_ShowKeypadBlock))) cEnd=17;
    else if (!(flags&(VirtualKeyboardFlags_ShowKeypadBlock))) cEnd=21;
    if ((flags&VirtualKeyboardFlags_ShowAllBlocks)==VirtualKeyboardFlags_ShowFunctionBlock)  {
       // user wants only the function block: we don't give him the part above the arrow block
       rStart=0;rEnd=1;cStart=0;cEnd=17;
    }
    else if (!(flags&VirtualKeyboardFlags_ShowBaseBlock))  {
        if (flags&VirtualKeyboardFlags_ShowArrowBlock) cStart=18;
        else {
            cStart=22;
            rStart=1;flags&=~VirtualKeyboardFlags_ShowFunctionBlock;   // optional line: removes empty space above Keypad when shown alone.
        }
    }

    const bool mouseEnabled = !(flags&VirtualKeyboardFlags_NoMouseInteraction);
    const bool keyboardEnabled = !(flags&VirtualKeyboardFlags_NoKeyboardInteraction);
    const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImGuiKey mouseClickedKey = ImGuiKey_COUNT;    
    ImGuiKey keyboardClickedKey = ImGuiKey_COUNT;

    ImVec2 key_min_upper_enter(0,0),key_max_upper_enter(0,0);
    ImVec2 key_min_lower_enter(0,0),key_max_lower_enter(0,0);

    //draw_list->PushClipRect(board_min, board_max, true);
    for (int r = rStart; r < rEnd; r++) {
        cur_pos.x = start_pos.x;
        if (flags&VirtualKeyboardFlags_ShowFunctionBlock) {
            cur_pos.y = start_pos.y+r*key_step.y+(r>0?(key_face_size.x*0.7f/1.2f):0.f);
        }
        else {
            IM_ASSERT(r!=0);
            cur_pos.y = start_pos.y+(r-1)*key_step.y;
        }
        const KeyLayoutName *keyDataRow = keyNamesRows[r];
        for (int c = cStart; c < cEnd; c++) {
            const KeyLayoutName* key_data = &keyDataRow[c];

            if (key_data->width==0.f) continue;
            if (key_data->width<0.f) {
                // spacing entry
                IM_ASSERT(key_data->key==ImGuiKey_None);
                cur_pos.x+=key_face_size.x*(-key_data->width)+key_border_width;
                continue;
            }
            IM_ASSERT(key_data->key==ImGuiKey_None || (key_data->key>=ImGuiKey_NamedKey_BEGIN && key_data->key<ImGuiKey_NamedKey_END));
            const char* key_label = (key_data->key==ImGuiKey_None)?"":KeyLabels[key_data->key-ImGuiKey_NamedKey_BEGIN];

            // Draw the key ----------------------------------------------------------------------------
            ImDrawFlags df = ImDrawFlags_RoundCornersAll;

            ImVec2 key_min = ImVec2(cur_pos.x, cur_pos.y);
            ImVec2 key_max = ImVec2(key_min.x + key_face_size.x*key_data->width+key_border_width, key_min.y + key_size_base.y);
            ImVec2 face_min = ImVec2(key_min.x + key_face_pos.x, key_min.y + key_face_pos.y);
            ImVec2 face_max = ImVec2(face_min.x + key_face_size.x*key_data->width, face_min.y + key_face_size.y);
            ImVec2 label_min = ImVec2(key_min.x + key_label_pos.x, key_min.y + key_label_pos.y);

            if (key_data->key==ImGuiKey_Enter && phyLayout!=KPL_ANSI)  {
                if (r==2)   {
                    df = ImDrawFlags_RoundCornersTop|ImDrawFlags_RoundCornersBottomLeft;
                    key_min_upper_enter = key_min;key_max_upper_enter = key_max;
                }
                else if (r==3)  {
                    df = ImDrawFlags_RoundCornersBottom;key_label="";
                    key_min_lower_enter = key_min; key_max_lower_enter = key_max;
                    key_min_lower_enter.y = key_max_upper_enter.y;
                    key_min.y = cur_pos.y-6.f*sf;  // The last number will be reused twice
                    face_min.y = key_min.y;
                }
                else {IM_ASSERT(0);}
            }
            else if (c==25) {//key_data->key==ImGuiKey_KeypadAdd || key_data->key==ImGuiKey_KeypadEnter)
                if (r==2 || r==4) continue;
                if (r==3 || r==5)   {
                    // This button requires two rows: draw the whole button at once
                    if (flags&VirtualKeyboardFlags_ShowFunctionBlock) key_min.y = start_pos.y+(r-1)*key_step.y+key_face_size.x*0.7f/1.2f;
                    else key_min.y = start_pos.y+(r-2)*key_step.y;
                    face_min.y = key_min.y + key_face_pos.y;
                    label_min.y = key_min.y + key_label_pos.y;
                }
            }

            draw_list->AddRectFilled(key_min, key_max, IM_COL32(204, 204, 204, 255), key_rounding, df);
            if (key_data->key!=ImGuiKey_Enter || r==2)  {
                draw_list->AddRect(key_min, key_max, IM_COL32(24, 24, 24, 255), key_rounding, df, thickness);
                draw_list->AddRect(face_min, face_max, IM_COL32(193, 193, 193, 255), key_face_rounding, ImDrawFlags_None, thicknessDouble);
            }
            else  {
                IM_ASSERT(r==3);
                draw_list->AddLine(ImVec2(key_min.x,key_min.y+6.f*sf), ImVec2(key_min.x,key_max.y), IM_COL32(24, 24, 24, 255), thickness); // left
                draw_list->AddLine(ImVec2(key_min.x,key_max.y), key_max, IM_COL32(24, 24, 24, 255), thickness); // bottom
                draw_list->AddLine(ImVec2(key_max.x,key_min.y-1.f), key_max, IM_COL32(24, 24, 24, 255), thickness); // right
                draw_list->AddLine(ImVec2(face_min.x,face_min.y+6.f*sf), ImVec2(face_min.x,face_max.y), IM_COL32(193, 193, 193, 255), thicknessDouble); // left
                draw_list->AddLine(ImVec2(face_min.x,face_max.y), face_max, IM_COL32(193, 193, 193, 255), thicknessDouble);   // bottom
                draw_list->AddLine(ImVec2(face_max.x,face_min.y), face_max, IM_COL32(193, 193, 193, 255), thicknessDouble);   // right
            }
            draw_list->AddRectFilled(face_min, face_max, IM_COL32(252, 252, 252, 255), key_face_rounding, df);
            draw_list->AddText(label_min, IM_COL32(64, 64, 64, 255), key_label);

            // Process input and draw the 'mouse hovered' and 'pressed' color----------------------------------
            if (key_data->key!=ImGuiKey_Enter || phyLayout==KPL_ANSI)  {
                if (keyboardEnabled && key_data->key!=ImGuiKey_None)    {
                    if (ImGui::IsKeyDown(key_data->key)) {
                        draw_list->AddRectFilled(key_min, key_max, IM_COL32(255, 0, 0, 128), key_rounding, df);
                    }
                    if ((keyboardClickedKey==ImGuiKey_COUNT || keyboardClickedKey==ImGuiKey_None) && ImGui::IsKeyReleased(key_data->key)) keyboardClickedKey = key_data->key;
                }
                if (mouseEnabled)   {
                    if (ImGui::IsMouseHoveringRect(key_min,key_max))    {
                        const ImU32 color = mouseDown ? IM_COL32(255, 0, 0, 128) : IM_COL32(0, 0, 0, 32);
                        if (mouseDown)  mouseClickedKey = key_data->key;
                        draw_list->AddRectFilled(key_min, key_max, color, key_rounding, df);
                    }
                }
            }
            else if (r==3)  {
                IM_ASSERT(key_data->key==ImGuiKey_Enter);
                if (keyboardEnabled && key_data->key!=ImGuiKey_None)    {
                    if (ImGui::IsKeyDown(key_data->key)) {
                        draw_list->AddRectFilled(key_min_upper_enter, key_max_upper_enter, IM_COL32(255, 0, 0, 128), key_rounding, ImDrawFlags_RoundCornersTop|ImDrawFlags_RoundCornersBottomLeft);
                        draw_list->AddRectFilled(key_min_lower_enter, key_max_lower_enter, IM_COL32(255, 0, 0, 128), key_rounding, ImDrawFlags_RoundCornersBottom);
                    }
                    if ((keyboardClickedKey==ImGuiKey_COUNT || keyboardClickedKey==ImGuiKey_None) && ImGui::IsKeyReleased(key_data->key)) keyboardClickedKey = key_data->key;
                }
                if (mouseEnabled)   {
                    if (ImGui::IsMouseHoveringRect(key_min_upper_enter,key_max_upper_enter) || ImGui::IsMouseHoveringRect(key_min_lower_enter,key_max_lower_enter))    {
                        const ImU32 color = mouseDown ? IM_COL32(255, 0, 0, 128) : IM_COL32(0, 0, 0, 32);
                        if (mouseDown)  mouseClickedKey = key_data->key;
                        draw_list->AddRectFilled(key_min_upper_enter, key_max_upper_enter, color, key_rounding, ImDrawFlags_RoundCornersTop|ImDrawFlags_RoundCornersBottomLeft);
                        draw_list->AddRectFilled(key_min_lower_enter, key_max_lower_enter, color, key_rounding, ImDrawFlags_RoundCornersBottom);
                    }
                }
            }
            //----------------------------------------------------------------------------------------------------------------
            cur_pos.x = key_max.x;
            board_max.y = key_max.y;
        }
        board_max.x = cur_pos.x;
    }
    //draw_list->PopClipRect();
    ImGui::Dummy(ImVec2(board_max.x - board_min.x, board_max.y - board_min.y));
    if (mouseEnabled)   {
        if (!ImGui::IsItemClicked(ImGuiMouseButton_Left)) mouseClickedKey = ImGuiKey_COUNT;
        //else {printf("keyClicked: %s\n",mouseClickedKey==ImGuiKey_None?"None":ImGui::GetKeyName(mouseClickedKey));fflush(stdout);}
    }
    if (keyboardClickedKey!=ImGuiKey_COUNT)    {
        if (mouseClickedKey==ImGuiKey_COUNT || mouseClickedKey==ImGuiKey_None) return keyboardClickedKey;
        else return mouseClickedKey;
    }
    else return mouseClickedKey;
}

#if IMGUI_ICONS
#define DIGITAL_0       ICON_FAD_DIGITAL0
#define DIGITAL_1       ICON_FAD_DIGITAL1
#define DIGITAL_2       ICON_FAD_DIGITAL2
#define DIGITAL_3       ICON_FAD_DIGITAL3
#define DIGITAL_4       ICON_FAD_DIGITAL4
#define DIGITAL_5       ICON_FAD_DIGITAL5
#define DIGITAL_6       ICON_FAD_DIGITAL6
#define DIGITAL_7       ICON_FAD_DIGITAL7
#define DIGITAL_8       ICON_FAD_DIGITAL8
#define DIGITAL_9       ICON_FAD_DIGITAL9
#define DIGITAL_DOT     ICON_FAD_DIGITAL_DOT
#define DIGITAL_COLON   ICON_FAD_DIGITAL_COLON
#else
#define DIGITAL_0       "0"
#define DIGITAL_1       "1"
#define DIGITAL_2       "2"
#define DIGITAL_3       "3"
#define DIGITAL_4       "4"
#define DIGITAL_5       "5"
#define DIGITAL_6       "6"
#define DIGITAL_7       "7"
#define DIGITAL_8       "8"
#define DIGITAL_9       "9"
#define DIGITAL_DOT     "."
#define DIGITAL_COLON   ":"
#endif

static std::vector<std::string> DIGITALS = {DIGITAL_0, DIGITAL_1, DIGITAL_2, DIGITAL_3, DIGITAL_4, DIGITAL_5, DIGITAL_6, DIGITAL_7, DIGITAL_8, DIGITAL_9};

void ImGui::ShowDigitalTime(ImDrawList *draw_list, int64_t millisec, int show_millisec, ImVec2 pos, ImU32 color)
{
    auto ReplaceDigital = [](std::string str)
    {
        if (str.empty()) return str;
        std::string result = "";
        for (auto c : str)
        {
            if (c >= 0x30 && c <= 0x39)
                result += DIGITALS[c - 0x30];
            else if (c == 0x3A)
                result += DIGITAL_COLON;
            else if (c == 0x2E)
                result += DIGITAL_DOT;
            else
                result += c;
        }
        return result;
    };
    auto time_str = ImGuiHelper::MillisecToString(millisec, show_millisec);
    auto show_text = ReplaceDigital(time_str);
    ImGui::PushStyleVar(ImGuiStyleVar_TextSpacing, 0.75f);
    draw_list->AddText(pos, color, show_text.c_str());
    ImGui::PopStyleVar();
}

void ImGui::ShowDigitalDateTime(ImDrawList *draw_list, int64_t millisec, int show_millisec, ImVec2 pos, ImU32 color)
{
    auto ReplaceDigital = [](std::string str)
    {
        if (str.empty()) return str;
        std::string result = "";
        for (auto c : str)
        {
            if (c >= 0x30 && c <= 0x39)
                result += DIGITALS[c - 0x30];
            else if (c == 0x3A)
                result += DIGITAL_COLON;
            else if (c == 0x2E)
                result += DIGITAL_DOT;
            else
                result += c;
        }
        return result;
    };

    char str_buf[1024] = {0};
    if (millisec != 0)
    {
        auto mTime = std::chrono::milliseconds(millisec);
        auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(mTime);
        auto tt = std::chrono::system_clock::to_time_t(tp);
        std::tm *now = localtime(&tt);
        snprintf(str_buf, sizeof(str_buf), "%4d-%02d-%02d %02d:%02d:%02d\n", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
    }
    else
    {
        snprintf(str_buf, sizeof(str_buf), "0000-00-00 00:00:00\n");
    }
    
    std::string time_str = std::string(str_buf);
    auto show_text = ReplaceDigital(time_str);
    ImGui::PushStyleVar(ImGuiStyleVar_TextSpacing, 0.75f);
    draw_list->AddText(pos, color, show_text.c_str());
    ImGui::PopStyleVar();
}

void ImGui::ShowDigitalTimeDuration(ImDrawList *draw_list, int64_t millisec, int64_t duration, int show_millisec, ImVec2 pos, ImU32 color)
{
    auto ReplaceDigital = [](std::string str)
    {
        if (str.empty()) return str;
        std::string result = "";
        for (auto c : str)
        {
            if (c >= 0x30 && c <= 0x39)
                result += DIGITALS[c - 0x30];
            else if (c == 0x3A)
                result += DIGITAL_COLON;
            else if (c == 0x2E)
                result += DIGITAL_DOT;
            else
                result += c;
        }
        return result;
    };
    auto time_str = ImGuiHelper::MillisecToString(millisec, show_millisec);
    auto dur_str = ImGuiHelper::MillisecToString(duration, show_millisec);
    auto show_time_text = ReplaceDigital(time_str);
    auto show_dur_text = ReplaceDigital(dur_str);
    auto show_text = show_time_text + " / " + show_dur_text;

    ImGui::PushStyleVar(ImGuiStyleVar_TextSpacing, 0.75f);
    draw_list->AddText(pos, color, show_text.c_str());
    ImGui::PopStyleVar();
}

// Rainbow Text
void ImGui::RainbowText(const char* text)
{
    ImVec4 rainbowColors[] = {
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),   
        ImVec4(1.0f, 0.5f, 0.0f, 1.0f),   
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f), 
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
        ImVec4(0.0f, 0.0f, 1.0f, 1.0f),  
        ImVec4(0.5f, 0.0f, 0.5f, 1.0f)  
    };

    static float time = 0.0f;
    time += 0.1f;
    int colorIndex = int(time) % 6;
    float lerpFactor = time - int(time);

    ImVec4 color1 = rainbowColors[colorIndex];
    ImVec4 color2 = rainbowColors[(colorIndex + 1) % 6];

    ImVec4 lerpedColor;
    lerpedColor.x = color1.x + lerpFactor * (color2.x - color1.x);
    lerpedColor.y = color1.y + lerpFactor * (color2.y - color1.y);
    lerpedColor.z = color1.z + lerpFactor * (color2.z - color1.z);
    lerpedColor.w = color1.w + lerpFactor * (color2.w - color1.w);

    ImGui::TextColored(lerpedColor, "%s", text);
}

void ImGui::TextVWithPadding(const ImVec2& padding, const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const char* text, *text_end;
    ImFormatStringToTempBufferV(&text, &text_end, fmt, args);
    TextExWithPadding(padding, text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
}

void ImGui::TextColoredWithPadding(const ImVec4& col, const ImVec2& padding, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextColoredVWithPadding(col, padding, fmt, args);
    va_end(args);
}

void ImGui::TextColoredVWithPadding(const ImVec4& col, const ImVec2& padding, const char* fmt, va_list args)
{
    PushStyleColor(ImGuiCol_Text, col);
    TextVWithPadding(padding, fmt, args);
    PopStyleColor();
}

void ImGui::TextExWithPadding(const ImVec2& padding, const char* text, const char* text_end, ImGuiTextFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    ImGuiContext& g = *GImGui;

    // Accept null ranges
    if (text == text_end)
        text = text_end = "";

    // Calculate length
    const char* text_begin = text;
    if (text_end == NULL)
        text_end = text + strlen(text); // FIXME-OPT

    const ImVec2 text_pos(window->DC.CursorPos.x+padding.x, window->DC.CursorPos.y+window->DC.CurrLineTextBaseOffset+padding.y);
    const float wrap_pos_x = window->DC.TextWrapPos;
    const bool wrap_enabled = (wrap_pos_x >= 0.0f);
    if (text_end - text <= 2000 || wrap_enabled)
    {
        // Common case
        const float wrap_width = wrap_enabled ? CalcWrapWidthForPos(window->DC.CursorPos, wrap_pos_x) : 0.0f;
        const ImVec2 text_size = CalcTextSize(text_begin, text_end, false, wrap_width);

        ImRect bb(text_pos-padding, text_pos+text_size+padding);
        ItemSize(text_size+padding*2.0f, 0.0f);
        if (!ItemAdd(bb, 0))
            return;

        // Render (we don't hide text after ## in this end-user function)
        RenderTextWrapped(bb.Min+padding, text_begin, text_end, wrap_width);
    }
    else
    {
        // Long text!
        // Perform manual coarse clipping to optimize for long multi-line text
        // - From this point we will only compute the width of lines that are visible. Optimization only available when word-wrapping is disabled.
        // - We also don't vertically center the text within the line full height, which is unlikely to matter because we are likely the biggest and only item on the line.
        // - We use memchr(), pay attention that well optimized versions of those str/mem functions are much faster than a casually written loop.
        const char* line = text;
        const float line_height = GetTextLineHeight();
        ImVec2 text_size(0, 0);

        // Lines to skip (can't skip when logging text)
        ImVec2 pos = text_pos;
        if (!g.LogEnabled)
        {
            int lines_skippable = (int)((window->ClipRect.Min.y - text_pos.y) / line_height);
            if (lines_skippable > 0)
            {
                int lines_skipped = 0;
                while (line < text_end && lines_skipped < lines_skippable)
                {
                    const char* line_end = (const char*)memchr(line, '\n', text_end - line);
                    if (!line_end)
                        line_end = text_end;
                    if ((flags & ImGuiTextFlags_NoWidthForLargeClippedText) == 0)
                        text_size.x = ImMax(text_size.x, CalcTextSize(line, line_end).x);
                    line = line_end + 1;
                    lines_skipped++;
                }
                pos.y += lines_skipped * line_height;
            }
        }

        // Lines to render
        if (line < text_end)
        {
            ImRect line_rect(pos, pos + ImVec2(FLT_MAX, line_height));
            while (line < text_end)
            {
                if (IsClippedEx(line_rect, 0))
                    break;

                const char* line_end = (const char*)memchr(line, '\n', text_end - line);
                if (!line_end)
                    line_end = text_end;
                text_size.x = ImMax(text_size.x, CalcTextSize(line, line_end).x);
                RenderText(pos, line, line_end, false);
                line = line_end + 1;
                line_rect.Min.y += line_height;
                line_rect.Max.y += line_height;
                pos.y += line_height;
            }

            // Count remaining lines
            int lines_skipped = 0;
            while (line < text_end)
            {
                const char* line_end = (const char*)memchr(line, '\n', text_end - line);
                if (!line_end)
                    line_end = text_end;
                if ((flags & ImGuiTextFlags_NoWidthForLargeClippedText) == 0)
                    text_size.x = ImMax(text_size.x, CalcTextSize(line, line_end).x);
                line = line_end + 1;
                lines_skipped++;
            }
            pos.y += lines_skipped * line_height;
        }
        text_size.y = (pos - text_pos).y;

        ImRect bb(text_pos-padding, text_pos+text_size+padding);
        ItemSize(text_size+padding*2.0f, 0.0f);
        ItemAdd(bb, 0);
    }
}

bool ImGui::HoverButton(ImDrawList *draw_list, const char * label, ImVec2 pos, ImVec2 size, std::string tooltips, ImVec4 hover_color)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect rect(pos, pos + size);
    bool overButton = rect.Contains(io.MousePos);
    if (overButton)
        draw_list->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(hover_color), 2);
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImGui::SetWindowFontScale(0.75);
    draw_list->AddText(pos, ImGui::GetColorU32(color), label);
    ImGui::SetWindowFontScale(1.0);
    if (overButton && !tooltips.empty() && ImGui::BeginTooltip())
    {
        ImGui::TextUnformatted(tooltips.c_str());
        ImGui::EndTooltip();
    }
    return overButton;
}

// Multi-Context Compositor
// USAGE:
/*
    // Store persistent state somewhere
    static ImGuiMultiContextCompositor g_mcc_instance;
    ...
    // Add your contexts
    ImGuiMultiContextCompositor_AddContext(mcc, ctx1); // Add update context
    ImGuiMultiContextCompositor_AddContext(mcc, ctx2); // Add rendering context
    ...
    // New Frame
    ImGuiMultiContextCompositor_PreNewFrameUpdateAll(mcc);
    ImGui::SetCurrentContext(ctx1);
    ImGui::NewFrame();
    ImGuiMultiContextCompositor_PostNewFrameUpdateOne(mcc);
    ...
    ImGui::SetCurrentContext(ctx2);
    ImGui::NewFrame();
    ImGuiMultiContextCompositor_PostNewFrameUpdateOne(mcc);
    ...
    // End of frame
    ImGui::Render()/ImGui::EndFrame();
    ImGuiMultiContextCompositor_PostEndFrameUpdateAll(mcc);
*/

static void ImGuiMultiContextCompositor_BringContextToFront(ImGui::ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx, ImGuiContext* ctx_to_keep_inputs_for)
{
    mcc->ContextsFrontToBack.find_erase(ctx);
    mcc->ContextsFrontToBack.push_front(ctx);

    for (ImGuiContext* other_ctx : mcc->ContextsFrontToBack)
        if (other_ctx != ctx && other_ctx != ctx_to_keep_inputs_for)
            other_ctx->IO.ClearInputKeys();
}

static bool ImGuiMultiContextCompositor_DragDropGetPayloadFromSourceContext(ImGui::ImGuiMultiContextCompositor* mcc)
{
    ImGuiContext* src_ctx = mcc->CtxDragDropSrc;
    ImGuiPayload* dst_payload = &mcc->DragDropPayload;

    if (!src_ctx->DragDropActive)
        return false;
    if (src_ctx->DragDropSourceFlags & ImGuiDragDropFlags_PayloadNoCrossContext)
        return false;
    ImGuiPayload* src_payload = &src_ctx->DragDropPayload;
    *dst_payload = *src_payload;
    dst_payload->Data = ImGui::MemAlloc(src_payload->DataSize);
    memcpy(dst_payload->Data, src_payload->Data, src_payload->DataSize);
    return true;
}

static void MultiContext_DragDropSetPayloadToDestContext(ImGui::ImGuiMultiContextCompositor* mcc, ImGuiContext* dst_ctx)
{
    IM_ASSERT(dst_ctx == ImGui::GetCurrentContext());
    ImGuiPayload* src_payload = &mcc->DragDropPayload;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern | ImGuiDragDropFlags_SourceNoPreviewTooltip))
    {
        ImGui::SetDragDropPayload(src_payload->DataType, src_payload->Data, src_payload->DataSize);
        ImGui::EndDragDropSource();
    }
}

static void MultiContext_DragDropFreePayload(ImGuiPayload* src_payload)
{
    ImGui::MemFree(src_payload->Data);
    src_payload->Data = NULL;
}

void ImGui::ImGuiMultiContextCompositor_AddContext(ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx)
{
    IM_ASSERT(mcc->Contexts.contains(ctx) == false);
    mcc->Contexts.push_back(ctx);
    mcc->ContextsFrontToBack.push_back(ctx);
}

void ImGui::ImGuiMultiContextCompositor_RemoveContext(ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx)
{
    mcc->Contexts.find_erase(ctx);
    mcc->ContextsFrontToBack.find_erase(ctx);
}

void ImGui::ImGuiMultiContextCompositor_PreNewFrameUpdateAll(ImGuiMultiContextCompositor* mcc)
{
    // Clear transient data
    mcc->CtxMouseFirst = NULL;
    mcc->CtxMouseExclusive = NULL;
    mcc->CtxMouseShape = NULL;
    mcc->CtxKeyboardExclusive = NULL;
    mcc->CtxDragDropSrc = NULL;
    mcc->CtxDragDropDst = NULL;
    mcc->DragDropPayload.Clear();

    // Sync point (before NewFrame calls)
    // PASS 1:
    // - Find out who will receive mouse position (one or multiple contexts)
    // - FInd out who will change mouse cursor (one context)
    // - Find out who has an active drag and drop
    for (ImGuiContext* ctx : mcc->ContextsFrontToBack)
    {
        const bool ctx_is_front = (ctx == mcc->ContextsFrontToBack.front());

#if 1 // IMGUI_HAS_DOCK
        // When hovering a secondary viewport, only enable mouse for the context owning it
        // We specifically use 'ctx->IO.MouseHoveredViewport' (current, submitted by backend) and not 'ctx->MouseLastHoveredViewport' (last valid one)
        if (mcc->CtxMouseExclusive == NULL && ctx->IO.MouseHoveredViewport != 0)
        {
            ImGuiViewport* hovered_viewport = NULL;
            for (ImGuiViewport* viewport : ctx->PlatformIO.Viewports)
                if (viewport->ID == ctx->IO.MouseHoveredViewport)
                    hovered_viewport = viewport;
            if (hovered_viewport != NULL && (hovered_viewport->Flags & ImGuiViewportFlags_CanHostOtherWindows) == 0)
                mcc->CtxMouseExclusive = ctx;
        }

        // When a secondary viewport is focused, only enable keyboard for the context owning it.
        if (mcc->CtxKeyboardExclusive == NULL && ctx->NavWindow != NULL)
            if (ImGuiViewport* viewport = ctx->NavWindow->Viewport)
                if ((viewport->Flags & ImGuiViewportFlags_IsFocused) && (viewport->Flags & ImGuiViewportFlags_CanHostOtherWindows) == 0)
                    mcc->CtxKeyboardExclusive = ctx;
#endif

        // When hovering a main/shared viewport,
        // - feed mouse front-to-back until reaching context that has io.WantCaptureMouse.
        // - track second context to pass drag and drop payload
        if (ctx->IO.WantCaptureMouse && mcc->CtxMouseFirst == NULL)
            mcc->CtxMouseFirst = ctx;
        if (ctx->HoveredWindowBeforeClear != NULL && mcc->CtxDragDropDst == NULL)
            mcc->CtxDragDropDst = ctx;

        // Who owns mouse shape?
        if (mcc->CtxMouseShape == NULL && ctx->MouseCursor != ImGuiMouseCursor_Arrow)
            mcc->CtxMouseShape = ctx;

        // Who owns drag and drop source?
        if (ctx->DragDropActive == true && (ctx->DragDropSourceFlags & ImGuiDragDropFlags_SourceExtern) == 0 && mcc->CtxDragDropSrc == NULL)
            mcc->CtxDragDropSrc = ctx;
        else if (ctx->DragDropActive == false && mcc->CtxDragDropSrc == ctx)
            mcc->CtxDragDropSrc = NULL;
    }

    // If no secondary viewport are focused, we'll keep keyboard to top-most context
    if (mcc->CtxKeyboardExclusive == NULL)
        mcc->CtxKeyboardExclusive = mcc->ContextsFrontToBack.front();

    // Deep copy payload for replication
    if (mcc->CtxDragDropSrc)
        ImGuiMultiContextCompositor_DragDropGetPayloadFromSourceContext(mcc);
    if (mcc->CtxDragDropDst && mcc->DragDropPayload.Data == NULL)
        mcc->CtxDragDropDst = NULL;

    // Bring drag target context to front when using DragDropHold press
    // FIXME-MULTICONTEXT: Works but change of order means source tooltip not visible anymore...
    // - Solution 1 ? if user code always submitted drag and drop tooltip derived from payload data
    //   instead of submitting at drag source location, this wouldn't be a problem at the front
    //   most context could always display the tooltip. But it's a constraint.
    // - Solution 2 ? would be a more elaborate composited rendering, where top layer (tooltip)
    //   of one ImDrawData would be moved to another ImDrawData.
    // - Solution 3 ? somehow find a way to enforce tooltip always on own viewport, always on top?
    // Ultimately this is not so important, it's already quite a fun luxury to have cross context DND.
#if 0
    if (mcc->CtxDragDropDst && mcc->CtxDragDropDst != mcc->ContextsFrontToBack.front())
        if (mcc->CtxDragDropDst->DragDropHoldJustPressedId != 0)
            ImGuiMultiContextCompositor_BringContextToFront(mcc, mcc->CtxDragDropDst, mcc->ContextsFrontToBack.front());
#endif

    // PASS 2:
    // - Enable/disable mouse interactions on selected contexts.
    // - Enable/disable mouse cursor change so only 1 context can do it.
    // - Bring a context to front whenever clicked any of its windows.
    bool is_above_ctx_with_mouse_first = true;
    for (ImGuiContext* ctx : mcc->ContextsFrontToBack)
    {
        ImGuiIO& io = ctx->IO;
        const bool ctx_is_front = (ctx == mcc->ContextsFrontToBack.front());

        // Focused secondary viewport or top-most context in shared viewport gets keyboard
        if (mcc->CtxKeyboardExclusive == ctx)
            io.ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard; // Allow keyboard interactions
        else
            io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard; // Disable keyboard interactions

        // Top-most context with MouseCursor shape request gets it
        if (mcc->CtxMouseShape == NULL || mcc->CtxMouseShape == ctx)
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange; // Allow mouse cursor changes
        else
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; // Disable mouse cursor changes

        if (mcc->CtxMouseExclusive != NULL)
        {
            // Single context gets mouse interactions
            if (mcc->CtxMouseExclusive == ctx)
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; // Allow mouse interactions
            else
                io.ConfigFlags |= ImGuiConfigFlags_NoMouse; // Disable mouse interactions
        }
        else
        {
            // Top-most io.WantCaptureMouse context & anything above it gets mouse interactions
            if (is_above_ctx_with_mouse_first || mcc->CtxDragDropDst == ctx)
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; // Allow mouse interactions
            else
                io.ConfigFlags |= ImGuiConfigFlags_NoMouse; // Disable mouse interactions
        }

        // Bring to front on click
        if ((mcc->CtxMouseExclusive == ctx || mcc->CtxMouseFirst == ctx) && !ctx_is_front)
        {
            bool any_mouse_clicked = false; // conceptually a ~ImGui::IsAnyMouseClicked(), not worth adding to API.
            for (bool clicked : io.MouseClicked)
                any_mouse_clicked |= clicked;
            if (any_mouse_clicked)
                ImGuiMultiContextCompositor_BringContextToFront(mcc, ctx, NULL);
        }

        if (mcc->CtxMouseFirst == ctx)
            is_above_ctx_with_mouse_first = false;
    }
}

// This could technically be registered as a hook, but it would make things too magical.
void ImGui::ImGuiMultiContextCompositor_PostNewFrameUpdateOne(ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx)
{
    // Propagate drag and drop
    // (against all odds since we are only READING from 'mcc' and writing to our target
    // context this should be parallel/threading friendly)
    if (mcc->CtxDragDropDst == ctx && mcc->CtxDragDropDst != mcc->CtxDragDropSrc)
        MultiContext_DragDropSetPayloadToDestContext(mcc, ctx);
}

void ImGui::ImGuiMultiContextCompositor_PostEndFrameUpdateAll(ImGuiMultiContextCompositor* mcc)
{
    // Clear drag and drop payload
    if (mcc->DragDropPayload.Data != NULL)
        MultiContext_DragDropFreePayload(&mcc->DragDropPayload);
}
