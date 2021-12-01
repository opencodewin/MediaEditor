
#include <map>
#include <algorithm>
#include <string>
#include <cctype>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif

#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#define MILISECOND 1000000UL
#define SECOND 1000000000UL
#define MINUTE 60000000000UL

//#include <glad/glad.h>

//#include "Resource.h"
#include "ImGuiToolkit_NoGst.h"
#include "SystemToolkit.h"

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

std::map <ImGuiToolkit::font_style, ImFont*>fontmap;

void ImGuiToolkit::ButtonOpenUrl( const char* label, const char* url, const ImVec2& size_arg )
{
    char _label[512];
    sprintf( _label, "%s  %s", ICON_FA5_EXTERNAL_LINK_ALT, label );

    if ( ImGui::Button(_label, size_arg) )
        SystemToolkit::open(url);
}


bool ImGuiToolkit::ButtonToggle( const char* label, bool* toggle )
{
    ImVec4* colors = ImGui::GetStyle().Colors;
    const auto active = *toggle;
    if( active ) {
        ImGui::PushStyleColor( ImGuiCol_Button, colors[ImGuiCol_TabActive] );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colors[ImGuiCol_TabHovered] );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, colors[ImGuiCol_Tab] );
    }
    bool action = ImGui::Button( label );
    if( action ) *toggle = !*toggle;
    if( active ) ImGui::PopStyleColor( 3 );
    return action;
}


bool ImGuiToolkit::ButtonSwitch(const char* label, bool* toggle, const char* help)
{
    bool ret = false;

    // utility style
    ImVec4* colors = ImGui::GetStyle().Colors;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // draw position when entering
    ImVec2 draw_pos = ImGui::GetCursorScreenPos();

    // layout
    float frame_height = ImGui::GetFrameHeight();
    float frame_width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetFrameHeight() * 0.75f;
    float width = height * 1.6f;
    float radius = height * 0.50f;

    // toggle action : operate on the whole area
    ImGui::InvisibleButton(label, ImVec2(frame_width, frame_height));
    if (ImGui::IsItemClicked()) {
        *toggle = !*toggle;
        ret = true;
    }
    float t = *toggle ? 1.0f : 0.0f;

    // animation
    ImGuiContext& g = *GImGui;
    const float ANIM_SPEED = 0.1f;
    if (g.LastActiveId == g.CurrentWindow->GetID(label))// && g.LastActiveIdTimer < ANIM_SPEED)
    {
        float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
        t = *toggle ? (t_anim) : (1.0f - t_anim);
    }

    // hover
    ImU32 col_bg;
    if (ImGui::IsItemHovered()) //
        col_bg = ImGui::GetColorU32(ImLerp(colors[ImGuiCol_FrameBgHovered], colors[ImGuiCol_TabHovered], t));
    else
        col_bg = ImGui::GetColorU32(ImLerp(colors[ImGuiCol_FrameBg], colors[ImGuiCol_TabActive], t));

    // draw help text if present
    if (help) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
        ImGui::RenderText(draw_pos, help);
        ImGui::PopStyleColor(1);
    }

    // draw the label right aligned
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 text_pos = draw_pos + ImVec2(frame_width -3.5f * ImGui::GetTextLineHeightWithSpacing() -label_size.x, 0.f);
    ImGui::RenderText(text_pos, label);

    // draw switch after the text
    ImVec2 p = draw_pos + ImVec2(frame_width -3.1f * ImGui::GetTextLineHeightWithSpacing(), 0.f);
    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 250));

    return ret;
}

void ImGuiToolkit::ToolTip(const char* desc, const char* shortcut)
{
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();

    if (shortcut) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
        ImGui::TextUnformatted(shortcut);
        ImGui::PopStyleColor();
    }
    ImGui::EndTooltip();
    ImGui::PopFont();
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
void ImGuiToolkit::HelpMarker(const char* desc, const char* icon, const char* shortcut)
{
    ImGui::TextDisabled( "%s", icon );
    if (ImGui::IsItemHovered())
        ToolTip(desc, shortcut);
}

bool ImGuiToolkit::SliderTiming (const char* label, int* ms, int v_min, int v_max, int v_step, const char* text_max)
{
    char text_buf[256];
    if ( *ms < v_max || text_max == nullptr)
    {
        int milisec = (*ms)%1000;
        int sec = (*ms)/1000;
        int min = sec/60;

        if (min > 0) {
            if (milisec>0)
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d min %02d s %03d ms", min, sec%60, milisec);
            else
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d min %02d s", min, sec%60);
        }
        else if (sec > 0) {
            if (milisec>0)
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d s %03d ms", sec, milisec);
            else
                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%d s", sec);
        }
        else
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%03d ms", milisec);
    }
    else {
        ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", text_max);
    }

//    int val = *ms / v_step;
//    bool ret = ImGui::SliderInt(label, &val, v_min / v_step, v_max / v_step, text_buf);
//    *ms = val * v_step;

    // quadratic scale
    float val = *ms / v_step;
    bool ret = ImGui::SliderFloat(label, &val, v_min / v_step, v_max / v_step, text_buf, 2.f);
    *ms = int(floor(val)) * v_step;

    return ret;
}

// Draws a timeline showing
// 1) a cursor at position *time in the range [0 duration]
// 2) a line of tick marks indicating time, every step if possible
// 3) a slider handle below the cursor: user can move the slider
// Behavior
// a) Returns TRUE if the left mouse button LMB is pressed over the timeline
// b) the value of *time is changed to the position of the slider handle from user input (LMB)

#define NUM_MARKS 10
#define LARGE_TICK_INCREMENT 1
#define LABEL_TICK_INCREMENT 3

std::string ImGuiToolkit::MillisecToString(int64_t millisec)
{
    std::ostringstream oss;
    if (millisec < 0)
    {
        oss << "-";
        millisec = -millisec;
    }
    uint64_t t = (uint64_t) millisec;
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    oss << std::setfill('0') << std::setw(2) << hour
        << min << sec << std::setw(3) << milli;
    return oss.str();
}

void ImGuiToolkit::RenderTimeline (ImGuiWindow* window, ImRect timeline_bbox, uint64_t duration, uint64_t step, bool verticalflip)
{
    static uint64_t optimal_tick_marks[NUM_MARKS + LABEL_TICK_INCREMENT] = { 100 * MILISECOND, 500 * MILISECOND, 1 * SECOND, 2 * SECOND, 5 * SECOND, 10 * SECOND, 20 * SECOND, 1 * MINUTE, 2 * MINUTE, 5 * MINUTE, 10 * MINUTE, 60 * MINUTE, 60 * MINUTE };

    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float fontsize = g.FontSize;
    const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);

    // by default, put a tick mark at every frame step and a large mark every second
    uint64_t tick_step = step;
    uint64_t large_tick_step = optimal_tick_marks[1+LARGE_TICK_INCREMENT];
    uint64_t label_tick_step = optimal_tick_marks[1+LABEL_TICK_INCREMENT];
    uint64_t tick_delta = 0;

    // how many pixels to represent one frame step?
    const float step_ = static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
    float tick_step_pixels = timeline_bbox.GetWidth() * step_;

    // large space
    if (tick_step_pixels > 5.f && step > 0)
    {
        // try to put a label ticks every second
        label_tick_step = (SECOND / step) * step;
        large_tick_step = label_tick_step % 5 ? (label_tick_step % 2 ?  label_tick_step : label_tick_step / 2 ) : label_tick_step / 5;
        tick_delta = SECOND - label_tick_step;

        // round to nearest
        if (tick_delta > step / 2) {
            label_tick_step += step;
            large_tick_step += step;
            tick_delta = SECOND - label_tick_step;
        }
    }
    else {
        // while there is less than 5 pixels between two tick marks (or at last optimal tick mark)
        for ( int i=0; i<10 && tick_step_pixels < 5.f; ++i )
        {
            // try to use the optimal tick marks pre-defined
            tick_step = optimal_tick_marks[i];
            large_tick_step = optimal_tick_marks[i+LARGE_TICK_INCREMENT];
            label_tick_step = optimal_tick_marks[i+LABEL_TICK_INCREMENT];
            tick_step_pixels = timeline_bbox.GetWidth() * static_cast<float> ( static_cast<double>(tick_step) / static_cast<double>(duration) );
        }
    }

    // render tics and text
    char text_buf[24];

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);

    // render tick and text END
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", MillisecToString(duration).c_str());
    ImVec2 duration_label_size = ImGui::CalcTextSize(text_buf, NULL);
    ImVec2 duration_label_pos = timeline_bbox.GetTR() + ImVec2( -2.f -duration_label_size.x, fontsize);
    if (verticalflip)
        duration_label_pos.y -= fontsize;
    ImGui::RenderTextClipped( duration_label_pos, duration_label_pos + duration_label_size,
                              text_buf, NULL, &duration_label_size);
    window->DrawList->AddLine( timeline_bbox.GetTR(), timeline_bbox.GetBR(), text_color, 1.5f);

    // render tick and text START
    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", MillisecToString(0).c_str());
    ImVec2 beginning_label_size = ImGui::CalcTextSize(text_buf, NULL);
    ImVec2 beginning_label_pos = timeline_bbox.GetTL() + ImVec2(3.f, fontsize);
    if (verticalflip)
        beginning_label_pos.y -= fontsize;
    if ( beginning_label_pos.x + beginning_label_size.x < duration_label_pos . x) {
        ImGui::RenderTextClipped( beginning_label_pos, beginning_label_pos + beginning_label_size,
                                  text_buf, NULL, &beginning_label_size);
    }
    window->DrawList->AddLine( timeline_bbox.GetTL(), timeline_bbox.GetBL(), text_color, 1.5f);

    ImGui::PopFont();

    // render the tick marks along TIMELINE
    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text] -ImVec4(0.f,0.f,0.f,0.4f));
    ImVec2 pos = verticalflip ? timeline_bbox.GetBL() : timeline_bbox.GetTL();

    // loop ticks from begin to end
    uint64_t tick = 0;
    while ( tick < duration )
    {
        // large tick mark ?
        float tick_length = (tick%large_tick_step) ? style.FramePadding.y : fontsize - style.FramePadding.y;

        // label tick mark
        if ( (tick%label_tick_step) < 1 )
        {
            // larger tick mark for label
            tick_length = fontsize;

            // correct tick value for delta for approximation to rounded second marks
            uint64_t ticklabel = tick + ( tick_delta * tick / label_tick_step);
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", MillisecToString(ticklabel).c_str());
            ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
            ImVec2 mini = ImVec2( pos.x - label_size.x / 2.f, pos.y);
            ImVec2 maxi = ImVec2( pos.x + label_size.x / 2.f, pos.y);

            if (verticalflip)   {
                mini.y -= tick_length + label_size.y;
                maxi.y -= tick_length;
            }
            else {
                mini.y += tick_length;
                maxi.y += tick_length + label_size.y;
            }

            // do not overlap with labels for beginning and duration
            if (mini.x - style.ItemSpacing.x > (beginning_label_pos.x + beginning_label_size.x) &&  maxi.x + style.ItemSpacing.x < duration_label_pos.x)
                ImGui::RenderTextClipped(mini, maxi, text_buf, NULL, &label_size);
        }

        // draw the tick mark each step
        window->DrawList->AddLine( pos, pos + ImVec2(0.f, verticalflip ? -tick_length : tick_length), text_color);

        // next tick
        tick += tick_step;
        float tick_percent = static_cast<float> ( static_cast<double>(tick) / static_cast<double>(duration) );
        if (verticalflip)
            pos = ImLerp(timeline_bbox.GetBL(), timeline_bbox.GetBR(), tick_percent);
        else
            pos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), tick_percent);

    }

    ImGui::PopStyleColor(1);
}

bool ImGuiToolkit::TimelineSlider(const char* label, uint64_t *pos, uint64_t duration, uint64_t step, const float width)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // get style & id
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float fontsize = g.FontSize;
    const ImGuiID id = window->GetID(label);

    //
    // FIRST PREPARE ALL data structures
    //

    // widget bounding box
    const float height = 2.f * (fontsize + style.FramePadding.y);
    ImVec2 curpos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, height);
    ImRect bbox(curpos, curpos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // cursor size
    const float cursor_width = 0.5f * fontsize;

    // TIMELINE is inside the bbox, in a slightly smaller bounding box
    ImRect timeline_bbox(bbox);
    timeline_bbox.Expand( ImVec2() - style.FramePadding );

    // SLIDER is inside the timeline
    ImRect slider_bbox( timeline_bbox.GetTL() + ImVec2(-cursor_width + 2.f, cursor_width + 4.f ), timeline_bbox.GetBR() + ImVec2( cursor_width - 2.f, 0.f ) );

    // units conversion: from time to float (calculation made with higher precision first)
    float time_ = static_cast<float> ( static_cast<double>(*pos) / static_cast<double>(duration) );

    //
    // SECOND GET USER INPUT AND PERFORM CHANGES AND DECISIONS
    //

    // read user input from system
    bool left_mouse_press = false;
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        left_mouse_press = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (left_mouse_press || g.NavActivateId == id)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }

    // time Slider behavior
    ImRect grab_slider_bb;
    ImU32 grab_slider_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
    float time_slider = time_ * 10.f; // x 10 precision on grab
    float time_zero = 0.f;
    float time_end = 10.f;
    bool value_changed = ImGui::SliderBehavior(slider_bbox, id, ImGuiDataType_Float, &time_slider, &time_zero,
                                               &time_end, "%.2f", ImGuiSliderFlags_None, &grab_slider_bb);
    if (value_changed)
    {
        *pos = static_cast<int64_t> ( 0.1 * static_cast<double>(time_slider) * static_cast<double>(duration) );
        grab_slider_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    //
    // THIRD RENDER
    //

    // Render the bounding box
    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderFrame(bbox.Min, bbox.Max, frame_col, true, style.FrameRounding);

    // render the timeline
    RenderTimeline(window, timeline_bbox, duration, step);

    // draw slider grab handle
    if (grab_slider_bb.Max.x > grab_slider_bb.Min.x) {
        window->DrawList->AddRectFilled(grab_slider_bb.Min + ImVec2(2, 0), grab_slider_bb.Max - ImVec2(2, 0), grab_slider_color, style.GrabRounding);
    }

    // draw the cursor
    curpos = ImLerp(timeline_bbox.GetTL(), timeline_bbox.GetTR(), time_) - ImVec2(cursor_width, 2.f);
    ImGui::RenderArrow(window->DrawList, curpos, ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)), ImGuiDir_Down);

    return left_mouse_press;
}

bool ImGuiToolkit::InvisibleSliderInt (const char* label, int *index, int min, int max, ImVec2 size)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // get id
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // read user input from system
    const bool left_mouse_press = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        if (hovered && left_mouse_press)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }
    else
        return false;

    bool value_changed = false;

    if (ImGui::GetActiveID() == id) {
        // Slider behavior
        ImRect grab_slider_bb;
        int _zero = min;
        int _end = max;
        value_changed = ImGui::SliderBehavior(bbox, id, ImGuiDataType_U32, index, &_zero,
                                              &_end, "%ld", ImGuiSliderFlags_None, &grab_slider_bb);
    }

    return value_changed;
}

bool ImGuiToolkit::EditPlotLines (const char* label, float *array, int values_count, float values_min, float values_max, const ImVec2 size)
{
    bool array_changed = false;

    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // capture coordinates before any draw or action
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_pos.x, ImGui::GetIO().MousePos.y - canvas_pos.y);

    // get id
    const ImGuiID id = window->GetID(label);

    // add item
    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return false;

    // read user input and activate widget
    const bool left_mouse_press = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool hovered = ImGui::ItemHoverable(bbox, id);
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        if (hovered && left_mouse_press)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }
    }
    else
        return false;

    ImVec4* colors = ImGui::GetStyle().Colors;
    ImVec4 bg_color = hovered ? colors[ImGuiCol_FrameBgHovered] : colors[ImGuiCol_FrameBg];

    // enter edit if widget is active
    if (ImGui::GetActiveID() == id) {

        static int previous_index = UINT32_MAX;
        bg_color = colors[ImGuiCol_FrameBgActive];

        // keep active area while mouse is pressed
        if (left_mouse_press)
        {
            float x = (float) values_count * mouse_pos_in_canvas.x / bbox.GetWidth();
            int index = CLAMP( (int) floor(x), 0, values_count-1);

            float y = mouse_pos_in_canvas.y / bbox.GetHeight();
            y = CLAMP( (y * (values_max-values_min)) + values_min, values_min, values_max);


            if (previous_index == UINT32_MAX)
                previous_index = index;

            array[index] = values_max - y;
            for (int i = MIN(previous_index, index); i < MAX(previous_index, index); ++i)
                array[i] = values_max - y;

            previous_index = index;

            array_changed = true;
        }
        // release active widget on mouse release
        else {
            ImGui::ClearActiveID();
            previous_index = UINT32_MAX;
        }

    }

    // back to draw
    ImGui::SetCursorScreenPos(canvas_pos);

    // plot lines
    char buf[128];
    sprintf(buf, "##Lines%s", label);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::PlotLines(buf, array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(1);

    return array_changed;
}

void ImGuiToolkit::ShowPlotHistoLines (const char* label, float *histogram_array, float *lines_array, int values_count, float values_min, float values_max, const ImVec2 size)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    // capture coordinates before any draw or action
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

    // get id
    const ImGuiID id = window->GetID(label);

    // add item
    ImVec2 pos = window->DC.CursorPos;
    ImRect bbox(pos, pos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bbox, id))
        return;

    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImVec4 bg_color = style.Colors[ImGuiCol_FrameBg];

    // back to draw
    ImGui::SetCursorScreenPos(canvas_pos);

    // plot transparent histogram
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0, 0, 0, 250));
    char buf[128];
    sprintf(buf, "##Histo%s", label);
    ImGui::PlotHistogram(buf, histogram_array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(2);

    ImGui::SetCursorScreenPos(canvas_pos);

    // plot lines
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    sprintf(buf, "##Lines%s", label);
    ImGui::PlotLines(buf, lines_array, values_count, 0, NULL, values_min, values_max, size);
    ImGui::PopStyleColor(1);

}


void ImGuiToolkit::SetFont (ImGuiToolkit::font_style style, const std::string &ttf_font_name, int pointsize, int oversample)
{
    // Font Atlas ImGui Management
    ImGuiIO& io = ImGui::GetIO();
    fontmap[style] = io.Fonts->Fonts[style];
}

void ImGuiToolkit::PushFont (ImGuiToolkit::font_style style)
{
    if (fontmap.count(style) > 0)
        ImGui::PushFont( fontmap[style] );
    else
        ImGui::PushFont( NULL );    
}


void ImGuiToolkit::ImageGlyph(font_style type, char c, float h)
{
    ImGuiIO& io = ImGui::GetIO();
    const ImTextureID my_tex_id = io.Fonts->TexID;
    const ImFontGlyph* glyph =  fontmap[type]->FindGlyph(c);
    const ImVec2 size( h * (glyph->X1 - glyph->X0) /  (glyph->Y1 - glyph->Y0)  , h);
    const ImVec2 uv0( glyph->U0, glyph->V0);
    const ImVec2 uv1( glyph->U1, glyph->V1);
    ImGui::Image((void*)(intptr_t)my_tex_id, size, uv0, uv1);
}


void ImGuiToolkit::Spacing()
{
    ImGui::Dummy(ImVec2(0, 0.5 * ImGui::GetTextLineHeight()));
}

void ImGuiToolkit::WindowText(const char* window_name, ImVec2 window_pos, const char* text)
{
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    if (ImGui::Begin(window_name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::TextUnformatted(text);
        ImGui::PopFont();

        ImGui::End();
    }
}

bool ImGuiToolkit::WindowButton(const char* window_name, ImVec2 window_pos, const char* button_text)
{
    bool ret = false;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    if (ImGui::Begin(window_name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ret = ImGui::Button(button_text);
        ImGui::PopFont();

        ImGui::End();
    }
    return ret;
}

void ImGuiToolkit::WindowDragFloat(const char* window_name, ImVec2 window_pos, float* v, float v_speed, float v_min, float v_max, const char* format)
{
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    if (ImGui::Begin(window_name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetNextItemWidth(100.f);
        ImGui::DragFloat("##nolabel", v, v_speed, v_min, v_max, format);
        ImGui::PopFont();

        ImGui::End();
    }
}

void word_wrap(std::string *str, unsigned per_line)
{
    unsigned line_begin = 0;
    while (line_begin < str->size())
    {
        const unsigned ideal_end = line_begin + per_line ;
        unsigned line_end = ideal_end < str->size() ? ideal_end : str->size()-1;

        if (line_end == str->size() - 1)
            ++line_end;
        else if (std::isspace(str->at(line_end)))
        {
            str->replace(line_end, 1, 1, '\n' );
            ++line_end;
        }
        else    // backtrack
        {
            unsigned end = line_end;
            while ( end > line_begin && !std::isspace(str->at(end)))
                --end;

            if (end != line_begin)
            {
                line_end = end;
                str->replace(line_end++, 1,  1, '\n' );
            }
            else {
                str->insert(line_end++, 1, '\n' );
            }
        }

        line_begin = line_end;
    }
}


struct InputTextCallback_UserData
{
    std::string*      Str;
    int               WordWrap;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCallback_UserData* user_data = static_cast<InputTextCallback_UserData*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
//        if (user_data->WordWrap > 1)
//            word_wrap(user_data->Str, user_data->WordWrap );

        // Resize string callback
        std::string* str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }

    return 0;
}

bool ImGuiToolkit::InputText(const char* label, std::string* str)
{
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CharsNoBlank;
    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;

    return ImGui::InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

bool ImGuiToolkit::InputTextMultiline(const char* label, std::string* str, const ImVec2& size, int linesize)
{
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize;
    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.WordWrap = linesize;

    return ImGui::InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
}


