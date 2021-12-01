#ifndef __IMGUI_TOOLKIT_H_
#define __IMGUI_TOOLKIT_H_

#include <string>
#include <list>
#include <vector>

namespace ImGuiToolkit
{
    // buttons
    bool ButtonToggle  (const char* label, bool* toggle);
    bool ButtonSwitch  (const char* label, bool* toggle , const char *help = nullptr);
    void ButtonOpenUrl (const char* label, const char* url, const ImVec2& size_arg = ImVec2(0,0));

    // tooltip and mouse over
    void ToolTip    (const char* desc, const char* shortcut = nullptr);
    void HelpMarker (const char* desc, const char* icon = ICON_FA5_QUESTION_CIRCLE, const char* shortcut = nullptr);

    std::string MillisecToString(int64_t millisec);
    void RenderTimeline (struct ImGuiWindow* window, struct ImRect timeline_bbox, uint64_t duration, uint64_t step, bool verticalflip = false);
    bool TimelineSlider (const char* label, uint64_t *pos, uint64_t duration, uint64_t step, const float width);

    // sliders
    bool SliderTiming (const char* label, int *ms, int v_min, int v_max, int v_step, const char* text_max = nullptr);
    bool InvisibleSliderInt(const char* label, int *index, int min, int max, const ImVec2 size);
    bool EditPlotLines(const char* label, float *array, int values_count, float values_min, float values_max, const ImVec2 size);
    void ShowPlotHistoLines(const char* label, float *histogram_array, float *lines_array, int values_count, float values_min, float values_max, const ImVec2 size);

    // fonts from ressources 'fonts/'
    typedef enum {
        FONT_DEFAULT = 0,
        FONT_BOLD,
        FONT_ITALIC,
        FONT_MONO,
        FONT_LARGE
    } font_style;
    void SetFont (font_style type, const std::string &ttf_font_name, int pointsize, int oversample = 2);
    void PushFont (font_style type);
    void ImageGlyph(font_style type, char c, float h = 60);
    void Spacing();

    // text input
    bool InputText(const char* label, std::string* str);
    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), int linesize = 0);

    // varia
    void WindowText(const char* window_name, ImVec2 window_pos, const char* text);
    bool WindowButton(const char* window_name, ImVec2 window_pos, const char* text);
    void WindowDragFloat(const char* window_name, ImVec2 window_pos, float* v, float v_speed, float v_min, float v_max, const char* format);

}

#endif // __IMGUI_TOOLKIT_H_
