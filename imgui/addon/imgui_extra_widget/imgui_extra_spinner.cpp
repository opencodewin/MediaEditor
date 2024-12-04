#include "imgui_extra_widget.h"
#include <array>
// ImSpinner
// SpinnerBegin is a function that starts a spinner widget, used to display an animation indicating that
// a task is in progress. It returns true if the widget is visible and can be used, or false if it should be skipped.
static bool SpinnerBegin(const char *label, float radius, ImVec2 &pos, ImVec2 &size, ImVec2 &centre, int &num_segments)
{
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);

    pos = window->DC.CursorPos;
    // The size of the spinner is set to twice the radius, plus some padding based on the style
    size = ImVec2((radius)*2, (radius + style.FramePadding.y) * 2);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);

    num_segments = window->DrawList->_CalcCircleAutoSegmentCount(radius);

    centre = bb.GetCenter();
    // If the item cannot be added to the window, return false
    if (!ImGui::ItemAdd(bb, id))
        return false;

    return true;
}

#define SPINNER_HEADER(pos, size, centre, num_segments) \
    ImVec2 pos, size, centre; int num_segments; \
    if (!SpinnerBegin(label, radius, pos, size, centre, num_segments)) { return; }; \
    ImGuiWindow *window = ImGui::GetCurrentWindow(); \
    auto circle = [&] (const std::function<ImVec2 (int)>& point_func, ImU32 dbc, float dth) { \
        window->DrawList->PathClear(); \
        for (int i = 0; i < num_segments; i++) { \
            ImVec2 p = point_func(i); \
            window->DrawList->PathLineTo(ImVec2(centre.x + p.x, centre.y + p.y)); \
        } \
        window->DrawList->PathStroke(dbc, 0, dth); \
    }

inline ImColor color_alpha(ImColor c, float alpha) { c.Value.w *= alpha * ImGui::GetStyle().Alpha; return c; }
inline float damped_spring(float mass, float stiffness, float damping, float time, float a = ImGui::PI_DIV_2, float b = ImGui::PI_DIV_2) {
    float omega = ImSqrt(stiffness / mass);
    float alpha = damping / (2 * mass);
    float exponent = std::exp(-alpha * time);
    float cosTerm = ImCos(omega * ImSqrt(1 - alpha * alpha) * time);
    float result = exponent * cosTerm;
    return ((result *= a) + b);
};

float damped_gravity(float limtime) {
    float time = 0.0f, initialHeight = 10.f, height = initialHeight, velocity = 0.f, prtime = 0.0f;
    while (height >= 0.0) {
        if (prtime >= limtime) { return height / 10.f; }
        time += 0.01f; prtime += 0.01f;
        height = initialHeight - 0.5 * 9.81f * time * time;
        if (height < 0.0) { initialHeight = 0.0; time = 0.0; }
    }
    return height / 10.f; // add By Dicky
}

inline float damped_trifolium(float limtime, float a = 0.f, float b = 1.f) {
    return a * ImSin(limtime) - b * ImSin(3 * limtime);
}

inline float damped_inoutelastic(float t, float amplitude, float period) {
    if( t == 0 ) return 0;
    t *= 2;
    if( t == 2 ) return 1;
    float s;
    if( amplitude < 1 ) {
        amplitude = 1;
        s = period / 4;
    } else {
        s = period / (2 * IM_PI) * std::asin( 1 / amplitude );
    }
    if( t < 1 ) return -0.5f * ( amplitude * ImPow(2.0f, 10.f*(t-1.f) ) * ImSin( (t-1.f-s)*(2.f*IM_PI)/period ));
    return amplitude * ImPow( 2.0f, -10*(t-1) ) * ImSin( (t-1.f-s)*(2.f*IM_PI)/period ) * 0.5f + 1.f;
}

inline std::pair<float, float> damped_infinity(float t, float a) {
    return std::make_pair((a * ImCos(t)) / (1 + (powf(ImSin(t), 2.0f))),
                          (a * ImSin(t) * ImCos(t)) / (1 + (powf(ImSin(t), 2.0f))));
};

inline float ease_inquad(float time) { return time * time; }
inline float ease_outquad(float time) { return time * (2.f - time); }
inline float ease_inoutquad(float t) { if (t < 0.5f) { return 2 * t * t; } else { return -1 + (4 - 2 * t) * t; }}
inline float ease_inoutquad(float *p) { float tr = ImMax(ImSin(p[0]) - 0.5f, 0.f) * (p[1] * 0.5f); return ease_inoutquad(tr); }
inline float ease_outcubic(float t) { float ft = t - 1; return ft * ft * ft + 1; }
inline float ease_inexpo(float t) { return t == 0 ? 0 : pow(2, 10 * (t - 1)); }
inline float ease_inoutexpo(float t) { if (t == 0) return 0; if (t == 1) return 1; if (t < 0.5f) return 0.5f * pow(2, (20 * t) - 10); return 0.5f * (2 - pow(2, -20 * t + 10)); }
inline float ease_inoutexpo(float *p) { float tr = ImMax(ImSin(p[0]) - 0.5f, 0.f) * (p[1] * 0.4f); return ease_inoutexpo(tr) * (p[1] * 0.3f); }
inline float ease_spring(float *p) { return damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(p[0], p[1])), p[2], p[3]);}
inline float ease_gravity(float *p) { return damped_gravity(p[0]); }
inline float ease_infinity(float *p) { return damped_infinity(p[0], p[1]).second; }
inline float ease_inoutelastic(float *p) { return damped_inoutelastic(p[1], p[2], p[3]); }

enum ease_mode {
    e_ease_none = 0,
    e_ease_inoutquad = 1,
    e_ease_inoutexpo = 2,
    e_ease_spring = 3,
    e_ease_gravity = 4,
    e_ease_infinity = 5,
    e_ease_elastic = 6,
};

template<typename ... Args>
inline float ease(ease_mode mode, Args ... args) {
    //static_assert((std::is_same_v<Args, float> && ...), "All arguments should be of type float");
    float params[] = {args...};
    switch (mode) {
    case e_ease_inoutquad: return ease_inoutquad(params);
    case e_ease_inoutexpo: return ease_inoutexpo(params);
    case e_ease_spring: return ease_spring(params);
    case e_ease_gravity: return ease_gravity(params);
    case e_ease_infinity: return ease_infinity(params);
    case e_ease_elastic: return ease_inoutelastic(params);
    case e_ease_none: return (0.f);
    }
    return 0.f;
}
/*
    const char *label: A string label for the spinner, used to identify it in ImGui.
    float radius: The radius of the spinner.
    float thickness: The thickness of the spinner's border.
    const ImColor &color: The color of the spinner.
    float speed: The speed of the spinning animation.
    float ang_min: Minimum angle of spinning.
    float ang_max: Maximum angle of spinning.
    int arcs: Number of arcs of the spinner.
*/

void ImGui::SpinnerRainbow(const char *label, float radius, float thickness, const ImColor &color, float speed, float ang_min, float ang_max, int arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImAbs(ImSin((float)ImGui::GetTime()) * (num_segments - 5));
    for (int i = 0; i < arcs; ++i)
    {
        const float rb = (radius / arcs) * (i + 1);
         const float a_min = ImMax(ang_min, PI_2 * ((float)start) / (float)num_segments + (IM_PI / arcs) * i);
        const float a_max = ImMin(ang_max, PI_2 * ((float)num_segments + 3 * (i + 1)) / (float)num_segments);
        
        circle([&](int i) {
            const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
            const float rspeed = a + (float)ImGui::GetTime() * speed;
            float pulse_factor = 1.f;
            if (mode == 1) {
                pulse_factor = 0.8f + 0.2f * ImSin((float)ImGui::GetTime() * 1.5f); // Pulsate between 0.8 and 1.0
            }
            const float pulsating_radius = rb * pulse_factor;
            return ImVec2(ImCos(rspeed) * pulsating_radius, ImSin(rspeed) * pulsating_radius);
        }, color_alpha(color, 1.f), thickness);
    }
}

void ImGui::SpinnerRainbowMix(const char *label, float radius, float thickness, const ImColor &color, float speed, float ang_min, float ang_max, int arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i < arcs; ++i)
    {
        const float rb = (radius / arcs) * (i + 1);
        const float start = ImAbs(ImSin((float)ImGui::GetTime()) * (num_segments - 5));
        const float a_min = ImMax(ang_min, PI_2 * ((float)start) / (float)num_segments + (IM_PI / arcs) * i);
        const float a_max = ImMin(ang_max, PI_2 * ((float)num_segments + 3 * (i + 1)) / (float)num_segments);
        const float koeff = mode ? (1.1f - 1.f / (i+1)) : 1.f;
        ImColor c = ImColor::HSV(out_h + i * (1.f / arcs), out_s, out_v);
        circle([&] (int i) {
            const float a =  a_min + ((float)i / (float)num_segments) * (a_max - a_min);
            const float rspeed = a + (float)ImGui::GetTime() * speed * koeff;
            return ImVec2(ImCos(rspeed) * rb, ImSin(rspeed) * rb);
        }, color_alpha(c, 1.f), thickness);
    }
}

// This function draws a rotating heart spinner. 
void ImGui::SpinnerRotatingHeart(const char *label, float radius, float thickness, const ImColor &color, float speed, float ang_min)
{
    // Calculate the position and size of the spinner, as well as the number of segments it will be divided into.
    SPINNER_HEADER(pos, size, centre, num_segments);
    // Calculate the start angle of the spinner based on the current time and speed.
    const float start = (float)ImGui::GetTime() * speed;
    // Modify the number of segments to ensure the heart shape is complete.
    num_segments = (num_segments * 3) / 2;
    // Create a lambda function to rotate points.
    auto rotate = [] (const ImVec2 &point, float angle) {
        const float s = ImSin(angle), c = ImCos(angle);
        return ImVec2(point.x * c - point.y * s, point.x * s + point.y * c);
    };
    // Calculate the radius of the bottom of the heart.
    const float rb = radius * ImMax(0.8f, ImSin(start * 2));
    auto scale = [rb] (float v) { return v / 16.f * rb; };
    // Draw the heart spinner by calling the circle function, passing in a lambda function that defines the shape of the heart.
    circle([&] (int i) {
        const float a = PI_2 * i / num_segments;
        const float x = (scale(16) * ImPow(ImSin(a), 3));
        const float y = -1.f * (scale(13) * ImCos(a) - scale(5) * ImCos(2 * a) - scale(2) * ImCos(3 * a) - ImCos(4 * a));
        return rotate(ImVec2(x, y), ang_min);
    }, color_alpha(color, 1.f), thickness); 
}

// SpinnerAng is a function that draws a spinner widget with a given angle.
void ImGui::SpinnerAng(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, float angle, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);                            // Get the position, size, centre, and number of segments of the spinner using the SPINNER_HEADER macro.
    float start = (float)ImGui::GetTime() * speed;                        // The start angle of the spinner is calculated based on the current time and the specified speed.
    float b = 0.f;
    switch (mode) {
    case 1: b = damped_gravity(ImSin(start * 1.1f)) * angle; break;
    case 2: radius = (0.8f + ImCos(start) * 0.2f) * radius; break;
    case 3: b = damped_infinity(start * 1.1f, 1.f).second; break;
    }

    auto radiusmode = [radius, mode] (float a) { switch (mode) { case 4: return damped_trifolium(a) * radius; } return radius; };
    circle([&] (int i) {                                                         // Draw the background of the spinner using the `circle` function, with the specified background color and thickness.
        const float a = start + (i * (PI_2 / (num_segments - 1)));               // Calculate the angle for each segment based on the start angle and the number of segments.
        return ImVec2(ImCos(a) * radiusmode(a), ImSin(a) * radiusmode(a));
    }, color_alpha(bg, 1.f), thickness);

    circle([&] (int i) {                                                        // Draw the spinner itself using the `circle` function, with the specified color and thickness.
        const float a = start - b + (i * angle / num_segments);
        return ImVec2(ImCos(a) * radiusmode(a), ImSin(a) * radiusmode(a));
    }, color_alpha(color, 1.f), thickness);
}

void ImGui::SpinnerAng8(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, float angle, int mode, float rkoef)
{
    SPINNER_HEADER(pos, size, centre, num_segments);                            // Get the position, size, centre, and number of segments of the spinner using the SPINNER_HEADER macro.
    float start = (float)ImGui::GetTime() * speed;                        // The start angle of the spinner is calculated based on the current time and the specified speed.
    float b = 0.f, kb = 1.f;
    switch (mode) {
    case 1: b = damped_gravity(ImSin(start * 1.1f)) * angle; break;
    case 2: radius = (0.8f + ImCos(start) * 0.2f) * radius; break;
    case 3: b = damped_infinity(start * 1.1f, 1.f).second; break;
    case 4: b = ease_outquad(ImSin(start * 1.1f)) * angle; break;
    case 5: kb = 2.f; break;
    }
    auto radiusmode = [radius, mode, kb] (float a, float k) { switch (mode) { case 4: return damped_trifolium(a) * radius; } return radius * k * kb; };
    float centerx_save = centre.x;
    centre.x = centerx_save + radius * (1.f - rkoef);
    circle([&] (int i) {                                                        // Draw the spinner itself using the `circle` function, with the specified color and thickness.
        const float a = start - b + (i * angle / num_segments);
        return ImVec2(ImCos(a) * radiusmode(a, rkoef), ImSin(a) * radiusmode(a, rkoef));
    }, color_alpha(color, 1.f), thickness);
    centre.x = centerx_save - radius * rkoef;
    circle([&] (int i) {                                                        // Draw the spinner itself using the `circle` function, with the specified color and thickness.
        const float a = start - b + (i * angle / num_segments);
        return ImVec2(ImCos(-a) * radiusmode(a, 1.f - rkoef), ImSin(-a) * radiusmode(a, 1.f - rkoef));
    }, color_alpha(color, 1.f), thickness);
}

void ImGui::SpinnerAngMix(const char *label, float radius, float thickness, const ImColor &color, float speed, float angle, int arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);                            // Get the position, size, centre, and number of segments of the spinner using the SPINNER_HEADER macro.
    for (int i = 0; i < arcs; ++i)
    {
        const float koeff = (1.1f - 1.f / (i+1));
        float start = (float)ImGui::GetTime() * speed * koeff;                        // The start angle of the spinner is calculated based on the current time and the specified speed.
        radius = (mode == 2) ? (0.8f + ImCos(start) * 0.2f) * radius : radius;
        const float rb = (radius / arcs) * (i + 1);
        const float b = (mode == 1) ? damped_gravity(ImSin(start * 1.1f)) * angle : 0.f;
        circle([&] (int i) {                                                        // Draw the spinner itself using the `circle` function, with the specified color and thickness.
            const float a = start - b + (i * angle / num_segments);
            return ImVec2(ImCos(a) * rb, ImSin(a) * rb);
        }, color_alpha(color, 1.f), thickness);
    }
}

void ImGui::SpinnerLoadingRing(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, int segments)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI);                         // Calculate the starting angle based on the current time and speed
    const float bg_angle_offset = PI_2 / num_segments - 1;

    num_segments *= 2;                                                                          // Double the number of segments for the background ringxxxxxxx
    circle([&] (int i) { 
        return ImVec2(ImCos(i * bg_angle_offset) * radius, ImSin(i * bg_angle_offset) * radius); // Draw the background ring
    }, color_alpha(bg, 1.f), thickness);

    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v); // Convert the color to HSV for variation in segment colors
    
    const float start_ang = (start < PI_DIV_2) ? 0.f : (start - PI_DIV_2) * 4.f;                // Calculate the angles and delta angle for each segment
    const float angle_offset = ((start < PI_DIV_2) ? PI_2 : (PI_2 - start_ang)) / segments;
    const float delta_angle = (start < PI_DIV_2) ? ImSin(start) * angle_offset : angle_offset;
    for (int i = 0; i < segments; ++i)                                                          // Draw each segment of the loading ring

    {
        window->DrawList->PathClear();
        const float begin_ang = start_ang - PI_DIV_2 + delta_angle * i;
        ImColor c = ImColor::HSV(out_h + i * (1.f / segments * 2.f), out_s, out_v);
        window->DrawList->PathArcTo(centre, radius, begin_ang, begin_ang + delta_angle, num_segments);
        window->DrawList->PathStroke(color_alpha(c, 1.f), false, thickness);
    }
}

void ImGui::SpinnerClock(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / (num_segments - 1);

    circle([&] (int i) { return ImVec2(ImCos(i * bg_angle_offset) * radius, ImSin(i * bg_angle_offset) * radius); }, color_alpha(bg, 1.f), thickness);

    window->DrawList->AddLine(centre, ImVec2(centre.x + ImCos(start) * radius, centre.y + ImSin(start) * radius), color_alpha(color, 1.f), thickness * 2);
    window->DrawList->AddLine(centre, ImVec2(centre.x + ImCos(start * 0.5f) * radius / 2.f, centre.y + ImSin(start * 0.5f) * radius / 2.f), color_alpha(color, 1.f), thickness * 2);
}

void ImGui::SpinnerPulsar(const char *label, float radius, float thickness, const ImColor &bg, float speed, bool sequence, float angle, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImGuiStorage *storage = window->DC.StateStorage;
    const ImGuiID radiusbId = window->GetID("##radiusb");
    float radius_b = storage->GetFloat(radiusbId, 0.8f);

    const float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / (num_segments - 1);

    float start_r = ImFmod(start, PI_DIV_2);
    switch (mode) {
    case 1: start_r = damped_infinity(start_r, angle).second; break;
    }
    float radius_k = ImSin(start_r);
    float radius1 = radius_k * radius;

    circle([&] (int i) {
        return ImVec2(ImCos(i * bg_angle_offset) * radius1, ImSin(i * bg_angle_offset) * radius1);
    }, color_alpha(bg, 1.f), thickness);

    if (sequence) { radius_b -= (0.005f * speed); radius_b = ImMax(radius_k, ImMax(0.8f, radius_b)); } 
    else { radius_b = (1.f - radius_k); }
    storage->SetFloat(radiusbId, radius_b);
    
    float radius_tb = sequence ? ImMax(radius_k, radius_b) * radius : (radius_b * radius);

    circle([&] (int i) {
        return ImVec2(ImCos(i * bg_angle_offset) * radius_tb, ImSin(i * bg_angle_offset) * radius_tb);
    }, color_alpha(bg, 1.f), thickness);
}

void ImGui::SpinnerDoubleFadePulsar(const char *label, float radius, float /*thickness*/, const ImColor &bg, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImGuiStorage* storage = window->DC.StateStorage;
    const ImGuiID radiusbId = window->GetID("##radiusb");
    float radius_b = storage->GetFloat(radiusbId, 0.8f);

    const float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2_DIV(num_segments);

    float start_r = ImFmod(start, PI_DIV_2);
    float radius_k = ImSin(start_r);
    window->DrawList->AddCircleFilled(centre, radius_k * radius, color_alpha(bg, ImMin(0.1f, radius_k)), num_segments);

    radius_b = (1.f - radius_k);
    storage->SetFloat(radiusbId, radius_b);

    window->DrawList->AddCircleFilled(centre, radius_b * radius, color_alpha(bg, ImMin(0.3f, radius_b)), num_segments);
}

void ImGui::SpinnerTwinPulsar(const char *label, float radius, float thickness, const ImColor &color, float speed, int rings, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float bg_angle_offset = PI_2 / (num_segments - 1);
    const float koeff = PI_DIV(2 * rings);
    float start = (float)ImGui::GetTime() * speed;

    for (int num_ring = 0; num_ring < rings; ++num_ring) {
        float radius_k = ImSin(ImFmod(start + (num_ring * koeff), PI_DIV_2));
        float radius1 = radius_k * radius;
        radius1 += ease((ease_mode)mode, start, radius);

        circle([&] (int i) {
            const float a = start + (i * bg_angle_offset);
            return ImVec2(ImCos(a) * radius1, ImSin(a) * radius1);
        }, color_alpha(color, radius_k > 0.5f ? 2.f - (radius_k * 2.f) : color.Value.w), thickness);
    }
}

void ImGui::SpinnerFadePulsar(const char *label, float radius, const ImColor &color, float speed, int rings, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float bg_angle_offset = PI_2_DIV(num_segments);
    const float koeff = PI_DIV(2 * rings);
    float start = (float)ImGui::GetTime() * speed;

    for (int num_ring = 0; num_ring < rings; ++num_ring) {
        float radius_k = ImSin(ImFmod(start + (num_ring * koeff), PI_DIV_2));
        ImColor c = color_alpha(color, (radius_k > 0.5f) ? (2.f - (radius_k * 2.f)) : color.Value.w);
        c.Value.w -= ease((ease_mode)mode, start, c.Value.w);
        window->DrawList->AddCircleFilled(centre, radius_k * radius, c, num_segments);
    }
}

void ImGui::SpinnerCircularLines(const char *label, float radius, const ImColor &color, float speed, int lines, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    auto ghalf_pi = [] (float f) -> float { return ImMin(f, PI_DIV_2); };
    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI);
    const float bg_angle_offset = PI_2_DIV(lines);
    for (size_t j = 0; j < 3; ++j)
    {
        const float start_offset = j * PI_DIV(7.f);
        const float rmax = ImMax(ImSin(ghalf_pi(start - start_offset)), 0.3f) * radius;
        const float rmin = ImMax(ImSin(ghalf_pi(start - PI_DIV_4 - start_offset)), 0.3f) * radius;
        ImColor c = color_alpha(color, 1.f - j * 0.3f);
        for (size_t i = 0; i <= lines; i++)
        {
            float a = (i * bg_angle_offset);
            a += ease((ease_mode)mode, start_offset, radius);
            window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * rmin, centre.y + ImSin(a) * rmin),
                                    ImVec2(centre.x + ImCos(a) * rmax, centre.y + ImSin(a) * rmax),
                                    color_alpha(c, 1.f), 1.f);
        }
    }
}

void ImGui::SpinnerDots(const char *label, float *nextdot, float radius, float thickness, const ImColor &color, float speed, size_t dots, float minth, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / dots;
    dots = ImMin(dots, (size_t)32);
    const size_t mdots = dots / 2;

    float def_nextdot = 0;
    float &ref_nextdot = nextdot ? *nextdot : def_nextdot;
    if (ref_nextdot < 0.f)
        ref_nextdot = (float)dots;

    auto radiusmode = [radius, mode, dots] (float a, int i) { 
        switch (mode) { 
        case 2: return damped_trifolium(a) * radius;
        case 3: return (radius / dots) * i;
        } return radius; 
    };
    auto thcorrect = [&thickness, &ref_nextdot, &mdots, &minth] (size_t i) {
        const float nth = minth < 0.f ? thickness / 2.f : minth;
        return ImMax(nth, ImSin(((i - ref_nextdot) / mdots) * IM_PI) * thickness);
    };

    switch (mode) {
    case 1: start = damped_infinity(start * 1.1f, 1.f).second; break;
    case 4: start = ease_outquad(ImSin(ImFmod(start, IM_PI))); break;
    case 5: start = ease_inoutexpo(ImSin(ImFmod(start, IM_PI))); break;
    }

    for (size_t i = 0; i <= dots; i++)
    {
        float a = start + (i * bg_angle_offset);
        a = ImFmod(a, PI_2);
        float th = minth < 0 ? thickness / 2.f : minth;

        if (ref_nextdot + mdots < dots) {
            if (i > ref_nextdot && i < ref_nextdot + mdots)
                th = thcorrect(i);
        } else {
            if ((i > ref_nextdot && i < dots) || (i < ((int)(ref_nextdot + mdots)) % dots))
                th = thcorrect(i);
        }

        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(-a) * radiusmode(a, i), centre.y + ImSin(-a) * radiusmode(a, i)), th, color_alpha(color, 1.f), 8);
    }
}

void ImGui::SpinnerVDots(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bgcolor, float speed, size_t dots, size_t mdots, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    switch (mode) {
    case 1: start += ease_inoutquad(ImSin(ImFmod(start, IM_PI))); break;
    }
    const float bg_angle_offset = PI_2_DIV(dots);
    dots = ImMin(dots, (size_t)32);

    for (size_t i = 0; i <= dots; i++)
    {
        float a = ImFmod(start + (i * bg_angle_offset), PI_2);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(-a) * radius, centre.y + ImSin(-a) * radius), thickness / 2, color_alpha(bgcolor, 1.f), 8);
    }

    window->DrawList->PathClear();
    const float d_ang = (mdots / (float)dots) * PI_2;
    const float angle_offset = (d_ang) / dots;
    for (size_t i = 0; i < dots; i++)
    {
        const float a = start + (i * angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
    }
    window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
}

void ImGui::SpinnerBounceDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float nextItemKoeff = 2.5f;
    const float heightKoeff = 2.f;
    const float heightSpeed = 0.8f;
    const float hsize = dots * (thickness * nextItemKoeff) / 2.f - (thickness * nextItemKoeff) * 0.5f;

    // Render
    float start = (float)ImGui::GetTime() * speed;
    const float offset = PI_DIV(dots);
    for (size_t i = 0; i < dots; i++) {
        float a = start + (IM_PI - i * offset);
        switch (mode) {
        case 1: a = damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(start + i * PI_DIV(dots * 2), PI_2))); break;
        case 2: a = damped_infinity((float)(start + i * PI_DIV(dots * 2)), radius).second; break;
        }
        float y =  centre.y + ImSin(a * heightSpeed) * thickness * heightKoeff;
        window->DrawList->AddCircleFilled(ImVec2(centre.x - hsize + i * (thickness * nextItemKoeff), ImMin(y, centre.y)), thickness, color_alpha(color, 1.f), 8);
    }
}

void ImGui::SpinnerZipDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 3.5f;
    const float heightKoeff = 2.f;
    const float heightSpeed = 0.8f;
    const float hsize = dots * (thickness * nextItemKoeff) / 2.f - (thickness * nextItemKoeff) * 0.5f;
    const float start = (float)ImGui::GetTime() * speed;
    const float offset = PI_DIV(dots);
    for (size_t i = 0; i < dots; i++)
    {
        const float sina = ImSin((start + (IM_PI - i * offset)) * heightSpeed);
        const float y = ImMin(centre.y + sina * thickness * heightKoeff, centre.y);
        const float deltay = ImAbs(y - centre.y);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - hsize + i * (thickness * nextItemKoeff), y), thickness, color_alpha(color, 1.f), 8);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - hsize + i * (thickness * nextItemKoeff), y + 2 * deltay), thickness, color_alpha(color, 1.f), 8);
    }
}

void ImGui::SpinnerDotsToPoints(const char *label, float radius, float thickness, float offset_k, const ImColor &color, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 3.5f;
    const float hsize = dots * (thickness * nextItemKoeff) / 2.f - (thickness * nextItemKoeff) * 0.5f;
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float offset = PI_DIV(dots);
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    if (start < PI_DIV_2) {
        const float sina = ImSin(start);
        for (size_t i = 0; i < dots; i++) {
            const float xx = ImMax(sina * (i * (thickness * nextItemKoeff)), 0.f);
            ImColor c = color_alpha(ImColor::HSV(out_h + i * ((1.f / dots) * 2.f), out_s, out_v), 1.f);
            window->DrawList->AddCircleFilled(ImVec2(centre.x - hsize + xx, centre.y), thickness, c, 8);
        }
    } else {
        for (size_t i = 0; i < dots; i++) {
            const float sina = ImSin(ImMax(start - (IM_PI / dots) * i, PI_DIV_2));
            const float xx = ImMax(1.f * (i * (thickness * nextItemKoeff)), 0.f);
            const float th = sina * thickness;
            ImColor c = color_alpha(ImColor::HSV(out_h + i * ((1.f / dots) * 2.f), out_s, out_v), 1.f);
            window->DrawList->AddCircleFilled(ImVec2(centre.x - hsize + xx, centre.y), th, c, 8);
        }
    }
}

void ImGui::SpinnerDotsToBar(const char *label, float radius, float thickness, float offset_k, const ImColor &color, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 3.5f;
    const float heightSpeed = 0.8f;
    const float hsize = dots * (thickness * nextItemKoeff) / 2.f - (thickness * nextItemKoeff) * 0.5f;
    const float start = (float)ImGui::GetTime() * speed;
    const float offset = PI_DIV(dots);
    const float hradius = (radius);
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (size_t i = 0; i < dots; i++)
    {
        const float sina = ImSin((start + (IM_PI - i * offset)) * heightSpeed);
        const float sinb = ImSin((start + (IM_PI + IM_PI * offset_k - i * offset)) * heightSpeed);
        const float y = ImMin(centre.y + sina * hradius, centre.y);
        const float y2 = ImMin(sinb, 0.f) * (hradius * offset_k);
        const float y3 = (y + y2);
        const float deltay = ImAbs(y - centre.y);
        ImColor c = color_alpha(ImColor::HSV(out_h + i * ((1.f / dots) * 2.f), out_s, out_v), 1.f);
        ImVec2 p1(centre.x - hsize + i * (thickness * nextItemKoeff), y3);
        ImVec2 p2(centre.x - hsize + i * (thickness * nextItemKoeff), y3 + 2 * deltay);
        window->DrawList->AddCircleFilled(p1, thickness, c, 8);
        window->DrawList->AddCircleFilled(p2, thickness, c, 8);
        window->DrawList->AddLine(p1, p2, c, thickness * 2.f);
    }
}

void ImGui::SpinnerWaveDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 2.5f;
    const float dots = (size.x / (thickness * nextItemKoeff));
    const float offset = PI_DIV(dots);
    const float start = (float)ImGui::GetTime() * speed;

    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (size_t i = 0; i < dots; i++)
    {
        float a = start + (IM_PI - i * offset);
        float y = centre.y + ImSin(a) * (size.y / 2.f);
        ImColor c = ImColor::HSV(out_h + i * (1.f / dots * 2.f), out_s, out_v);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff, y), thickness, color_alpha(c, 1.f), lt);
    }
}

void ImGui::SpinnerFadeDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const float nextItemKoeff = 2.5f;
    const float dots = (size.x / (thickness * nextItemKoeff));
    const float heightSpeed = 0.8f;
    for (size_t i = 0; i < dots; i++)
    {
        float a = mode 
                    ? damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(start + (IM_PI - i * (IM_PI / dots)), PI_2)))
                    : ImSin(start + (IM_PI - i * (IM_PI / dots)) * heightSpeed);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff, centre.y), thickness, color_alpha(color, ImMax(0.1f, a)), lt);
    }
}

void ImGui::SpinnerThreeDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float nextItemKoeff = 2.5f;
    const float offset = size.x / 4.f;
    float ab = start;
    int msize = 2;
    if (start < IM_PI) { ab = 0; msize = 1; }
    for (size_t i = 0; i < msize; i++)
    {
        float a = ab + i * IM_PI - PI_DIV_2;
        window->DrawList->AddCircleFilled(ImVec2(centre.x - offset + ImSin(a) * offset, centre.y + ImCos(a) * offset), thickness, color_alpha(color, 1.f), lt);
    }
    float ba = start; msize = 2;
    if (start > IM_PI && start < PI_2) { ba = 0; msize = 1; }
    for (size_t i = 0; i < msize; i++)
    {
        float a = -ba + i * IM_PI + PI_DIV_2;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + offset + ImSin(a) * offset, centre.y + ImCos(a) * offset), thickness, color_alpha(color, 1.f), lt);
    }
}

void ImGui::SpinnerFiveDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2 * 2);
    const float nextItemKoeff = 2.5f;
    const float offset = size.x / 4.f;
    float ab = 0;
    int msize = 1;
    if (start < IM_PI) { ab = start; msize = 2; }
    for (size_t i = 0; i < msize; i++)
    {
        float a = -ab + i * IM_PI - PI_DIV_2;
        window->DrawList->AddCircleFilled(ImVec2(centre.x - offset + ImSin(a) * offset, centre.y + ImCos(a) * offset), thickness, color_alpha(color, 1.f), lt);
    }
    float ba = 0; msize = 1;
    if (start > IM_PI && start < PI_2) { ba = start; msize = 2; }
    for (size_t i = 0; i < msize; i++)
    {
        float a = -ba + i * IM_PI;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(a) * offset, centre.y + offset + ImCos(a) * offset), thickness, color_alpha(color, 1.f), lt);
    }
    float bc = 0; msize = 1;
    if (start > PI_2 && start < IM_PI * 3) { bc = start; msize = 2; }
    for (size_t i = 0; i < msize; i++)
    {
        float a = -bc + i * IM_PI - IM_PI;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(a) * offset, centre.y - offset + ImCos(a) * offset), thickness, color_alpha(color, 1.f), lt);
    }
    float bd = 0; msize = 1;
    if (start > IM_PI * 3 && start < IM_PI * 4) { bd = start; msize = 2; }
    for (size_t i = 0; i < msize; i++)
    {
        float a = -bd + i * IM_PI + PI_DIV_2;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + offset + ImSin(a) * offset, centre.y + ImCos(a) * offset), thickness, color_alpha(color, 1.f), lt);
    }
}

void ImGui::Spinner4Caleidospcope(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float nextItemKoeff = 2.5f;
    const float offset = size.x / 4.f;
    float ab = start;
    int msize = 2;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (size_t i = 0; i < msize; i++)
    {
        float a = ab - i * IM_PI;
        ImColor c = color_alpha(ImColor::HSV(out_h + (0.1f * i), out_s, out_v, 0.7f), 1.f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - offset + ImSin(a) * offset, centre.y + ImCos(a) * offset), thickness, c, lt);
    }
    for (size_t i = 0; i < msize; i++)
    {
        float a = ab + i * IM_PI + PI_DIV_2;
        ImColor c = color_alpha(ImColor::HSV(out_h + 0.2f + (0.1f * i), out_s, out_v, 0.7f), 1.f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(a) * offset, centre.y - offset + ImCos(a) * offset), thickness, c, lt);
    }
    float ba = start; msize = 2;
    for (size_t i = 0; i < msize; i++)
    {
        float a = -ba + i * IM_PI + PI_DIV_2;
        ImColor c = color_alpha(ImColor::HSV(out_h + 0.4f + (0.1f * i), out_s, out_v, 0.7f), 1.f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + offset + ImSin(a) * offset, centre.y + ImCos(a) * offset), thickness, c, lt);
    }
    for (size_t i = 0; i < msize; i++)
    {
        float a = ab - i * IM_PI + PI_DIV_4;
        ImColor c = color_alpha(ImColor::HSV(out_h + 0.6f + (0.1f * i), out_s, out_v, 0.7f), 1.f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(a) * offset, centre.y + offset + ImCos(a) * offset), thickness, c, lt);
    }
}

void ImGui::SpinnerMultiFadeDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const float nextItemKoeff = 2.5f;
    const float dots = (size.x / (thickness * nextItemKoeff));
    const float heightSpeed = 0.8f;
    for (size_t j = 0; j < dots; j++)
    {
        for (size_t i = 0; i < dots; i++)
        {
            float a = start - (IM_PI - i * j * PI_DIV(dots));
            window->DrawList->AddCircleFilled(ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff, centre.y - (size.y / 2.f) + j * thickness * nextItemKoeff), thickness, color_alpha(color, ImMax(0.1f, ImSin(a * heightSpeed))), lt);
        }
    }
}

void ImGui::SpinnerScaleDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 2.5f;
    const float heightSpeed = 0.8f;
    const float dots = (size.x / (thickness * nextItemKoeff));
    const float start = (float)ImGui::GetTime() * speed;
    for (size_t i = 0; i < dots; i++)
    {
        const float a = start + (IM_PI - i * PI_DIV(dots));
        const float th = thickness * ImSin(a * heightSpeed);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff, centre.y), thickness, color_alpha(color, 0.1f), lt);
        window->DrawList->AddCircleFilled(ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff, centre.y), th, color_alpha(color, 1.f), lt);
    }
}

void ImGui::SpinnerSquareSpins(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 2.5f;
    const float heightSpeed = 0.8f;
    const float dots = (size.x / (thickness * nextItemKoeff));
    const float start = (float)ImGui::GetTime() * speed;
    for (size_t i = 0; i < dots; i++)
    {
        const float a = ImFmod(start + i * ((PI_DIV_2 * 0.7f) / dots), PI_DIV_2);
        const float th = thickness * (ImCos(a * heightSpeed) * 2.f);
        ImVec2 pmin = ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff - thickness, centre.y - thickness);
        ImVec2 pmax = ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff + thickness, centre.y + thickness);
        window->DrawList->AddRect(pmin, pmax, color_alpha(color, 1.f), 0.f);
        ImVec2 lmin = ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff - thickness, centre.y - th + thickness);
        ImVec2 lmax = ImVec2(centre.x - (size.x / 2.f) + i * thickness * nextItemKoeff + thickness - 1, centre.y - th + thickness);
        window->DrawList->AddLine(lmin, lmax, color_alpha(color, 1.f), 1.f);
    }
}

void ImGui::SpinnerMovingDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float nextItemKoeff = 2.5f;
    const float heightKoeff = 2.f;
    const float heightSpeed = 0.8f;
    const float start = ImFmod((float)ImGui::GetTime() * speed, size.x);

    float offset = 0;
    for (size_t i = 0; i < dots; i++)
    {
        float th = thickness;
        offset = ImFmod(start + i * (size.x / dots), size.x);

        if (offset < thickness) { th = offset; }
        if (offset > size.x - thickness) { th = size.x - offset; }
        
        window->DrawList->AddCircleFilled(ImVec2(pos.x + offset - thickness, centre.y), th, color_alpha(color, 1.f), 8);
    }
}

void ImGui::SpinnerRotateDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int dots, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImGuiStorage *storage = window->DC.StateStorage;
    const ImGuiID velocityId = window->GetID("##velocity");
    const ImGuiID vtimeId = window->GetID("##velocitytime");

    float velocity = storage->GetFloat(velocityId, 0.f);
    float vtime = storage->GetFloat(vtimeId, 0.f);

    float dtime = ImFmod((float)vtime, IM_PI);
    float start = (vtime += velocity);
    if (dtime > 0.f && dtime < PI_DIV_2) { velocity += 0.001f * speed; }
    else if (dtime > IM_PI * 0.9f && dtime < IM_PI) { velocity -= 0.01f * speed; }

    if (velocity > 0.1f) velocity = 0.1f;
    if (velocity < 0.01f) velocity = 0.01f;

    storage->SetFloat(velocityId, velocity);
    storage->SetFloat(vtimeId, vtime);

    window->DrawList->AddCircleFilled(centre, thickness, color_alpha(color, 1.f), 8);

    for (int i = 0; i < dots; i++)
    {
        float a = 0.f;
        switch (mode) {
        case 1: a = start + i * PI_2_DIV(dots) + damped_spring(1, 10.f, 1.0f, ImSin(start + i * PI_2_DIV(dots)), PI_2_DIV(dots), 0); break;
        case 2: a = start + i * PI_2_DIV(dots) + damped_infinity(start + i * PI_DIV(dots * 2), 1.f).second; break;
        default:
            a = start + (i * PI_2_DIV(dots));
        }
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius), thickness, color_alpha(color, 1.f), 8);
    }

    if (mode == 3) {
        float sm_thickness = thickness * 0.5f;
        const float bstart = (float)ImGui::GetTime() * speed;
        for (size_t i = 0; i < dots; i++)
        {
            const float b = bstart + (IM_PI - i * PI_DIV(dots) * 2.f);
            window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(b) * radius, centre.y + ImSin(b) * radius), sm_thickness, color_alpha(color, 1.f), 8);
        }
    }
}

void ImGui::SpinnerOrionDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    ImGuiStorage* storage = window->DC.StateStorage;
    const ImGuiID velocityId = window->GetID("##velocity");
    const ImGuiID vtimeId = window->GetID("##velocitytime");
    float velocity = storage->GetFloat(velocityId, 0.f);
    float vtime = storage->GetFloat(vtimeId, 0.f);
    float dtime = ImFmod((float)vtime, IM_PI);
    float start = (vtime += velocity);
    if (dtime > 0.f && dtime < PI_DIV_2) { velocity += 0.001f * speed; }
    else if (dtime > IM_PI * 0.9f && dtime < IM_PI) { velocity -= 0.01f * speed; }

    if (velocity > 0.1f) velocity = 0.1f;
    if (velocity < 0.01f) velocity = 0.01f;
    storage->SetFloat(velocityId, velocity);
    storage->SetFloat(vtimeId, vtime);
    window->DrawList->AddCircleFilled(centre, thickness, color_alpha(color, 1.f), 8);
    for (int j = 1; j < arcs; ++j) {
        const float r = (radius / (arcs + 1)) * j;
        for (int i = 0; i < j + 1; i++)
        {
            const float a = start + (i * PI_2_DIV(j+1));
            window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r), thickness, color_alpha(color, 1.f), 8);
        }
    }
}

void ImGui::SpinnerGalaxyDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    ImGuiStorage* storage = window->DC.StateStorage;
    const ImGuiID velocityId = window->GetID("##velocity");
    const ImGuiID vtimeId = window->GetID("##velocitytime");
    float velocity = storage->GetFloat(velocityId, 0.f);
    float vtime = storage->GetFloat(vtimeId, 0.f);
    float dtime = ImFmod((float)vtime, IM_PI);
    float start = (vtime += (velocity * speed));
    if (dtime > 0.f && dtime < PI_DIV_2) { velocity += 0.001f; }
    else if (dtime > IM_PI * 0.9f && dtime < IM_PI) { velocity -= 0.01f; }

    if (velocity > 0.1f) velocity = 0.1f;
    if (velocity < 0.01f) velocity = 0.01f;
    storage->SetFloat(velocityId, velocity);
    storage->SetFloat(vtimeId, vtime);
    window->DrawList->AddCircleFilled(centre, thickness, color_alpha(color, 1.f), 8);
    for (int j = 1; j < arcs; ++j) {
        const float r = ((j / (float)arcs) * radius);
        for (int i = 0; i < arcs; i++)
        {
            const float a = start * (1.f + j * 0.1f) + (i * PI_2_DIV(arcs));
            window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r), thickness, color_alpha(color, 1.f), 8);
        }
    }
}

void ImGui::SpinnerTwinAng(const char *label, float radius1, float radius2, float thickness, const ImColor &color1, const ImColor &color2, float speed, float angle, int mode)
{
    const float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float aoffset = ImFmod((float)ImGui::GetTime(), 1.5f * IM_PI);
    const float bofsset = (aoffset > angle) ? angle : aoffset;
    const float angle_offset = angle * 2.f / num_segments;

    window->DrawList->PathClear();
    auto radiusmode = [mode] (float a, float r) { switch (mode) { case 1: return damped_trifolium(a) * r; } return r; };
    for (size_t i = 0; i <= 2 * num_segments; i++)
    {
        const float a = start + (i * angle_offset);
        if (i * angle_offset > 2 * bofsset)
            break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radiusmode(a, radius1), centre.y + ImSin(a) * radiusmode(a, radius1)));
    }
    window->DrawList->PathStroke(color_alpha(color1, 1.f), false, thickness);

    window->DrawList->PathClear();
    for (size_t i = 0; i < num_segments / 2; i++)
    {
        const float a = start + (i * angle_offset);
        if (i * angle_offset > bofsset)
            break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radiusmode(a, radius2), centre.y + ImSin(a) * radiusmode(a, radius2)));
    }
    window->DrawList->PathStroke(color_alpha(color2, 1.f), false, thickness);
}

void ImGui::SpinnerFilling(const char *label, float radius, float thickness, const ImColor &color1, const ImColor &color2, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float angle_offset = PI_2_DIV(num_segments - 1);

    circle([&] (int i) {
          const float a = (i * angle_offset);
          return ImVec2(ImCos(a) * radius, ImSin(a) * radius);
    }, color_alpha(color1, 1.f), thickness);

    window->DrawList->PathClear();
    for (size_t i = 0; i < 2 * num_segments / 2; i++)
    {
        const float a = (i * angle_offset);
        if (a > start)
            break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
    }
    window->DrawList->PathStroke(color_alpha(color2, 1.f), false, thickness);
}

void ImGui::SpinnerFillingMem(const char *label, float radius, float thickness, const ImColor &color, ImColor &colorbg, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float angle_offset = PI_2_DIV(num_segments - 1);
    num_segments *= 4;
    circle([&] (int i) {
        const float a = (i * angle_offset);
        return ImVec2(ImCos(a) * radius, ImSin(a) * radius);
    }, color_alpha(colorbg, 1.f), thickness);
    if (start < 0.02f) {
        colorbg = color;
    }
    window->DrawList->PathClear();
    for (size_t i = 0; i < 2 * num_segments / 2; i++) {
        const float a = (i * angle_offset);
        if (a > start)
            break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
    }
    window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
}

void ImGui::SpinnerTopup(const char *label, float radius1, float radius2, const ImColor &color, const ImColor &fg, const ImColor &bg, float speed)
{
    const float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI);
    window->DrawList->AddCircleFilled(centre, radius1, color_alpha(bg, 1.f), num_segments);

    const float abegin = (PI_DIV_2) - start;
    const float aend = (PI_DIV_2) + start;
    const float angle_offset = (aend - abegin) / num_segments;

    window->DrawList->PathClear();
    window->DrawList->PathArcTo(centre, radius1, abegin, aend, num_segments * 2);
    ImDrawListFlags save = window->DrawList->Flags;
    window->DrawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    window->DrawList->PathFillConvex(color_alpha(color, 1.f));
    window->DrawList->AddCircleFilled(centre, radius2, color_alpha(fg, 1.f), num_segments);
    window->DrawList->Flags = save;
}

void ImGui::SpinnerTwinAng180(const char *label, float radius1, float radius2, float thickness, const ImColor &color1, const ImColor &color2, float speed, float angle, int mode)
{
    const float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    num_segments *= 8;
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float aoffset = ImFmod((float)ImGui::GetTime(), PI_2);
    const float bofsset = (aoffset > IM_PI) ? IM_PI : aoffset;
    const float angle_offset = PI_2_DIV(num_segments);
    float ared_min = 0, ared = 0;
    if (aoffset > IM_PI)
        ared_min = aoffset - IM_PI;

    window->DrawList->PathClear();
    auto radiusmode = [mode] (float a, float r, float f) { switch (mode) { case 2: return damped_trifolium(a, 0.f, f) * r; } return r; };
    for (size_t i = 0; i <= num_segments / 2 + 1; i++)
    {
        ared = start + (i * angle_offset);
        switch (mode) {
        case 1: ared += damped_infinity(start, angle).second; break;
        }

        if (i * angle_offset < ared_min)
            continue;

        if (i * angle_offset > bofsset)
            break;

        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ared) * radiusmode(ared, radius2, -1.1f), centre.y + ImSin(ared) * radiusmode(ared, radius2, -1.1f)));
    }
    window->DrawList->PathStroke(color_alpha(color2, 1.f), false, thickness);

    window->DrawList->PathClear();
    for (size_t i = 0; i <= 2 * num_segments + 1; i++)
    {
        const float a = ared + ared_min + (i * angle_offset);
        if (i * angle_offset < ared_min)
            continue;

        if (i * angle_offset > bofsset)
            break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radiusmode(a, radius1, 1.f), centre.y + ImSin(a) * radiusmode(a, radius1, 1.f)));
    }
    window->DrawList->PathStroke(color_alpha(color1, 1.f), false, thickness);
}

void ImGui::SpinnerTwinAng360(const char *label, float radius1, float radius2, float thickness, const ImColor &color1, const ImColor &color2, float speed1, float speed2, int mode)
{
    const float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    num_segments *= 4;
    float start1 = ImFmod((float)ImGui::GetTime() * speed1, PI_2);
    float start2 = ImFmod((float)ImGui::GetTime() * speed2, PI_2);
    const float aoffset = ImFmod((float)ImGui::GetTime(), PI_2);
    const float bofsset = (aoffset > IM_PI) ? IM_PI : aoffset;
    const float angle_offset = PI_2 / num_segments;
    float ared_min = 0, ared = 0;
    if (aoffset > IM_PI)
        ared_min = aoffset - IM_PI;

    window->DrawList->PathClear();
    for (size_t i = 0; i <= num_segments + 1; i++) {
        ared = ( mode ? damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(start1 + 0 * PI_DIV(2), PI_2))) : start1) + (i * angle_offset);
        if (i * angle_offset < ared_min * 2) continue;
        if (i * angle_offset > bofsset * 2.f) break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ared) * radius2, centre.y + ImSin(ared) * radius2));
    }
    window->DrawList->PathStroke(color_alpha(color2, 1.f), false, thickness);

    window->DrawList->PathClear();
    for (size_t i = 0; i <= num_segments + 1; i++) {
        ared = (mode ? damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(start2 + 1 * PI_DIV(2), PI_2))) : start2) + (i * angle_offset);
        if (i * angle_offset < ared_min * 2) continue;
        if (i * angle_offset > bofsset * 2.f) break;
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(-ared) * radius1, centre.y + ImSin(-ared) * radius1));
    }
    window->DrawList->PathStroke(color_alpha(color1, 1.f), false, thickness);
}

void ImGui::SpinnerIncDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, PI_DIV(dots));
    start -= astart;
    dots = ImMin<size_t>(dots, 32);

    for (size_t i = 0; i <= dots; i++)
    {
        float a = start + (i * PI_DIV(dots - 1));
        ImColor c = color_alpha(color, ImMax(0.1f, i / (float)dots));
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius), thickness, c, 8);
    }
}

void ImGui::SpinnerIncFullDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    dots = ImMin<size_t>(dots, 32);
    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, IM_PI / dots);
    start -= astart;
    const float bg_angle_offset = IM_PI / dots;

    for (size_t i = 0; i < dots * 2; i++)
    {
        float a = start + (i * bg_angle_offset);
        ImColor c = color_alpha(color, 0.1f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius), thickness, c, 8);
    }

    for (size_t i = 0; i < dots; i++)
    {
        float a = start + (i * bg_angle_offset);
        ImColor c = color_alpha(color, ImMax(0.1f, i / (float)dots));
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius), thickness, c, 8);
    }
}

void ImGui::SpinnerFadeBars(const char *label, float w, const ImColor &color, float speed, size_t bars, bool scale)
{
    float radius = (w * 0.5f) * bars;
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const float nextItemKoeff = 1.5f;
    const float yOffsetKoeftt = 0.8f;
    const float heightSpeed = 0.8f;
    const float start = (float)ImGui::GetTime() * speed;

    const float offset = IM_PI / bars;
    for (size_t i = 0; i < bars; i++)
    {
        float a = start + (IM_PI - i * offset);
        ImColor c = color_alpha(color, ImMax(0.1f, ImSin(a * heightSpeed)));
        float h = (scale ? (0.6f + 0.4f * c.Value.w) : 1.f) * size.y / 2;
        window->DrawList->AddRectFilled(ImVec2(pos.x + style.FramePadding.x + i * (w * nextItemKoeff) - w / 2, centre.y - h * yOffsetKoeftt),
                                        ImVec2(pos.x + style.FramePadding.x + i * (w * nextItemKoeff) + w / 2, centre.y + h * yOffsetKoeftt), c);
    }
}

void ImGui::SpinnerFadeTris(const char *label, float radius, const ImColor &color, float speed, size_t dim, bool scale)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const float nextItemKoeff = 1.5f;
    const float yOffsetKoeftt = 0.8f;
    const float heightSpeed = 0.8f;
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);

    std::vector<ImVec2> points;
    auto pushPoints = [] (std::vector<ImVec2> &pp, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3) { pp.push_back(p1); pp.push_back(p2); pp.push_back(p3); };
    auto hsumPoints = [] (const ImVec2 &p1, const ImVec2 &p2) { return ImVec2((p1.x + p2.x) / 2.f, (p1.y + p2.y) / 2.f); };
    
    auto splitTriangle = [&] (const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, int numDivisions) {
        pushPoints(points, p1, p2, p3);
        for (int i = 0; i < numDivisions; i++) {
            std::vector<ImVec2> newPoints;
            for (int j = 0; j < points.size() - 2; j += 3) {
                ImVec2 p1 = points[j];
                ImVec2 p2 = points[j + 1];
                ImVec2 p3 = points[j + 2];
                ImVec2 p4((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
                ImVec2 p5((p2.x + p3.x) / 2, (p2.y + p3.y) / 2);
                ImVec2 p6((p3.x + p1.x) / 2, (p3.y + p1.y) / 2);
                pushPoints(newPoints, p1, p4, p6);
                pushPoints(newPoints, p4, p5, p6);
                pushPoints(newPoints, p4, p2, p5);
                pushPoints(newPoints, p6, p5, p3);
            }
            points = newPoints;
        }
        return points;
    };
    auto calculateAngle = [] (ImVec2 v1, ImVec2 v2, const ImVec2 c) {
        v1.x -= c.x; v1.y -= c.y; v2.x -= c.x; v2.y -= c.y;
        float dotProduct = v1.x * v2.x + v1.y * v2.y;
        float magnitudeV1 = ImSqrt(v1.x * v1.x + v1.y * v1.y);
        float magnitudeV2 = ImSqrt(v2.x * v2.x + v2.y * v2.y);
        float angleInRadians = ImAcos(dotProduct / (magnitudeV1 * magnitudeV2));
        float crossProduct = v1.x * v2.y - v2.x * v1.y;
        float signedAngle = std::copysign(angleInRadians, crossProduct);
        return fmod(signedAngle + PI_2, PI_2);
    };
    const float offset = IM_PI / dim;
    ImVec2 p1 = ImVec2(centre.x + ImSin(0) * radius, centre.y + ImCos(0) * radius);
    ImVec2 p2 = ImVec2(centre.x + ImSin(PI_DIV(3) * 2) * radius, centre.y + ImCos(PI_DIV(3) * 2) * radius);
    ImVec2 p3 = ImVec2(centre.x + ImSin(PI_DIV(3) * 4) * radius, centre.y + ImCos(PI_DIV(3) * 4) * radius);
    std::vector<ImVec2> subdividedPoints = splitTriangle(p1, p2, p3, dim);
    for (size_t i = 0; i < subdividedPoints.size(); i+=3) {
        ImVec2 trisCenter = hsumPoints(hsumPoints(subdividedPoints[i], subdividedPoints[i + 1]), subdividedPoints[i + 2]);
        const float angle = calculateAngle(p1, trisCenter, centre);
        ImColor c = color_alpha(color, 1.f - ImMax(0.1f, ImFmod(start + angle, PI_2) / PI_2));
        window->DrawList->AddTriangleFilled(subdividedPoints[i], subdividedPoints[i+1], subdividedPoints[i+2], c);
    }
}

void ImGui::SpinnerBarsRotateFade(const char *label, float rmin, float rmax , float thickness, const ImColor &color, float speed, size_t bars)
{
    float radius = rmax;
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, IM_PI / bars);
    start -= astart;
    const float bg_angle_offset = IM_PI / bars;
    bars = ImMin<size_t>(bars, 32);

    for (size_t i = 0; i <= bars; i++)
    {
        float a = start + (i * bg_angle_offset);
        ImColor c = color_alpha(color, ImMax(0.1f, i / (float)bars));
        window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * rmin, centre.y + ImSin(a) * rmin), ImVec2(centre.x + ImCos(a) * rmax, centre.y + ImSin(a) * rmax), c, thickness);
    }
}

void ImGui::SpinnerBarsScaleMiddle(const char *label, float w, const ImColor &color, float speed, size_t bars)
{
    float radius = (w)*bars;
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const float nextItemKoeff = 1.5f;
    const float yOffsetKoeftt = 0.8f;
    const float heightSpeed = 0.8f;
    float start = (float)ImGui::GetTime() * speed;
    const float offset = IM_PI / bars;

    for (size_t i = 0; i < bars; i++)
    {
        float a = start + (IM_PI - i * offset);
        float h = (0.4f + 0.6f * ImMax(0.1f, ImSin(a * heightSpeed))) * (size.y / 2);
        window->DrawList->AddRectFilled(ImVec2(centre.x + style.FramePadding.x + i * (w * nextItemKoeff) - w / 2, centre.y - h * yOffsetKoeftt),
                                        ImVec2(centre.x + style.FramePadding.x + i * (w * nextItemKoeff) + w / 2, centre.y + h * yOffsetKoeftt), color_alpha(color, 1.f));
        if (i == 0)
            continue;

        window->DrawList->AddRectFilled(ImVec2(centre.x + style.FramePadding.x - i * (w * nextItemKoeff) - w / 2, centre.y - h * yOffsetKoeftt),
                                        ImVec2(centre.x + style.FramePadding.x - i * (w * nextItemKoeff) + w / 2, centre.y + h * yOffsetKoeftt), color_alpha(color, 1.f));
    }
}

void ImGui::SpinnerAngTwin(const char *label, float radius1, float radius2, float thickness, const ImColor &color, const ImColor &bg, float speed, float angle, size_t arcs, int mode)
{
    float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / num_segments;

    window->DrawList->PathClear();
    for (size_t i = 0; i <= num_segments; i++)
    {
        const float a = start + (i * bg_angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1));
    }
    window->DrawList->PathStroke(color_alpha(bg, 1.f), false, thickness);

    const float angle_offset = angle / num_segments;
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        window->DrawList->PathClear();
        float arc_start = 2 * IM_PI / arcs;
        float b = ease((ease_mode)mode, start + arc_num * PI_DIV(2) / arcs, IM_PI, 1.0f, 0.0f);
        //switch (mode) {
        //case 1: b = start + damped_spring(1, 10.f, 1.0f, , )), 1, 0); break;
        //case 2: b = start + damped_infinity(PI_2 - angle, start).second; break;
        //default: b = start;
        //}
        
        for (size_t i = 0; i < num_segments; i++) {
            const float a = start + b + arc_start * arc_num + (i * angle_offset);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius2, centre.y + ImSin(a) * radius2));
        }
        window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
    }
}

void ImGui::SpinnerSolarBalls(const char *label, float radius, float thickness, const ImColor &ball, const ImColor &bg, float speed, size_t balls)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / num_segments;
    for (int i = 0; i < balls; ++i) {
        const float rb = (radius / balls) * 1.3f * (i + 1);
        window->DrawList->AddCircle(centre, rb, color_alpha(bg, 1.f), num_segments, thickness * 0.3f);
    }
    for (int i = 0; i < balls; ++i) {
        const float rb = (radius / balls) * 1.3f * (i + 1);
        const float a = start * (1.0f + 0.1f * i);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * rb, centre.y + ImSin(a) * rb), thickness, color_alpha(ball, 1.f));
    }
}

void ImGui::SpinnerSolarScaleBalls(const char *label, float radius, float thickness, const ImColor &ball, float speed, size_t balls)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI * 16.f);
    const float bg_angle_offset = PI_2 / num_segments;
    for (int i = 0; i < balls; ++i) {
        const float rb = (radius / balls) * 1.3f * (i + 1);
        const float a = start * (1.0f + 0.1f * i);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * rb, centre.y + ImSin(a) * rb), ((thickness * 2.f) / balls) * i, color_alpha(ball, 1.f));
    }
}

void ImGui::SpinnerSolarArcs(const char *label, float radius, float thickness, const ImColor &ball, const ImColor &bg, float speed, size_t balls)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime()* speed;
    const int half_segments = num_segments / 2;
    for (int i = 0; i < balls; ++i)
    {
        const float rb = (radius / balls) * 1.3f * (i + 1);
        const float bg_angle_offset = IM_PI / half_segments;
        const int mul = i % 2 ? -1 : 1;
        window->DrawList->PathClear();
        for (size_t ii = 0; ii <= half_segments; ii++)
        {
            const float a = (ii * bg_angle_offset);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * rb, centre.y + ImSin(a) * rb * mul));
        }
        window->DrawList->PathStroke(color_alpha(bg, 1.f), false, thickness * 0.8f);
    }

    for (int i = 0; i < balls; ++i)
    {
        const float rb = (radius / balls) * 1.3f * (i + 1);
        const float a = ImFmod(start * (1.0f + 0.1f * i), PI_2);
        const int mul = i % 2 ? -1 : 1;
        float y = ImSin(a) * rb;
        if ((y > 0 && mul < 0) || (y < 0 && mul > 0)) y = -y;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * rb, centre.y + y), thickness, color_alpha(ball, 1.f));
    }
}

void ImGui::SpinnerMovingArcs(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImFmod(ImGui::GetTime() * speed, IM_PI * 2);
    const int half_segments = num_segments / 2;

    for (int i = 0; i < arcs; ++i) {
        const float rb = (radius / arcs) * 1.3f * (i + 1);
        float a = damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(start + i * PI_DIV(arcs), PI_2)));
        const float angle = ImMax(PI_DIV_2, (1.f - i/(float)arcs) * IM_PI);
        circle([&] (int i) {
            const float b = a + (i * angle / num_segments);
            return ImVec2(ImCos(b) * rb, ImSin(b) * rb);
        }, color_alpha(color, 1.f), thickness);
    }
}

void ImGui::SpinnerRainbowCircle(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs, float mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    num_segments *= 2;
    const float bg_angle_offset = IM_PI / num_segments;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i < arcs; ++i)
    {
        float max_angle = IM_PI - ImFmod(start /* * (1.0 + 0.1f * i) */, IM_PI + PI_DIV_2 + PI_DIV(8));
        max_angle = ImMin(IM_PI, max_angle + (PI_DIV_2 / arcs) * i);
        const float rb = (radius / arcs) * 1.1f * (i + 1);
        ImColor c = ImColor::HSV(out_h + i * (0.8f / arcs), out_s, out_v);
        const int draw_segments = ImMax<int>(0, (int)(max_angle / bg_angle_offset));

        for (int j = 0; j < 2; j++)
        {
            const int mul = j % 2 ? -1 : 1;
            const float py = (j % 2 ? -0.5f : 0.5f) * thickness;
            const float alpha_start = (j % 2 ? 0 : IM_PI) * mode;
            window->DrawList->PathClear();
            for (size_t ii = 0; ii <= draw_segments + 1; ii++)
            {
                const float a = (ii * bg_angle_offset);
                window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(IM_PI - alpha_start - a) * rb, centre.y + ImSin(a) * rb * mul + py));
            }
            window->DrawList->PathStroke(color_alpha(c, 1.f), false, thickness * 0.8f);
        }
    }
}

void ImGui::SpinnerBounceBall(const char *label, float radius, float thickness, const ImColor &color, float speed, int dots, bool shadow)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImGuiStorage* storage = window->DC.StateStorage;
    const ImGuiID vtimeId = window->GetID("##vtime");
    const ImGuiID hmaxId = window->GetID("##hmax");

    float vtime = storage->GetFloat(vtimeId, 0.f);
    float hmax = storage->GetFloat(hmaxId, 1.f);

    vtime += 0.05f;
    hmax += 0.01f;

    storage->SetFloat(vtimeId, vtime);
    storage->SetFloat(hmaxId, hmax);

    constexpr float rkoeff[9] = {0.1f, 0.15f, 0.17f, 0.25f, 0.31f, 0.19f, 0.08f, 0.24f, 0.9f};
    const int iterations = shadow ? 4 : 1;
    for (int j = 0; j < iterations; j++) {
        ImColor c = color_alpha(color, 1.f - 0.15f * j);
        for (int i = 0; i < dots; i++) {
            float start = ImFmod((float)ImGui::GetTime() * speed * (1 + rkoeff[i % 9]) - (IM_PI / 12.f) * j, IM_PI);
            float sign = ((i % 2 == 0) ? 1.f : -1.f);
            float offset = (i == 0) ? 0.f : (floorf((i+1) / 2.f + 0.1f) * sign * 2.f * thickness);
            float maxht = damped_gravity(ImSin(ImFmod(hmax, IM_PI))) * radius;
            window->DrawList->AddCircleFilled(ImVec2(centre.x + offset, centre.y + radius - ImSin(start) * 2.f * maxht), thickness, c, 8);
        }
    }
}

void ImGui::SpinnerPulsarBall(const char *label, float radius, float thickness, const ImColor &color, float speed, bool shadow, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    ImGuiStorage* storage = window->DC.StateStorage;
    const int iterations = shadow ? 4 : 1;
    for (int j = 0; j < iterations; j++) {
        ImColor c = color_alpha(color, 1.f - 0.15f * j);
        float start = ImFmod((float)ImGui::GetTime() * speed - (IM_PI / 12.f) * j, IM_PI);
        float maxht = damped_gravity(ImSin(ImFmod(start, IM_PI))) * (radius * 0.6f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x, centre.y), maxht, c, num_segments);
    }
    const float angle_offset = PI_DIV_2 / num_segments;
    const int arcs = 2;
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num) {
        window->DrawList->PathClear();
        float arc_start = 2 * IM_PI / arcs;
        float start = ImFmod((float)ImGui::GetTime() * speed - (IM_PI * arc_num), IM_PI);
        float b = mode ? start + damped_spring(1, 10.f, 1.0f, ImSin(ImFmod(start + arc_num * PI_DIV(2) / arcs, IM_PI)), 1, 0) : start;
        float maxht = (damped_gravity(ImSin(ImFmod(start, IM_PI))) * 0.3f + 0.7f) * radius;
        for (size_t i = 0; i < num_segments; i++) {
            const float a = b + arc_start * arc_num + (i * angle_offset);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * maxht, centre.y + ImSin(a) * maxht));
        }
        window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
    }
}

void ImGui::SpinnerIncScaleDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots, float angle, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, IM_PI / dots);
    start -= astart;
    const float bg_angle_offset = IM_PI / dots;
    dots = ImMin(dots, (size_t)32);

    for (size_t i = 0; i <= dots; i++)
    {
        float a = start + (i * bg_angle_offset);
        a += ease((ease_mode)mode, a, angle);
        float th = thickness * ImMax(0.1f, i / (float)dots);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius), th, color_alpha(color, 1.f), 8);
    }
}

void ImGui::SpinnerArcRotation(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime()* speed;
    float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;

    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        window->DrawList->PathClear();
        ImColor c = color_alpha(color, ImMax(0.1f, arc_num / (float)arcs));
        float b = ease((ease_mode)mode, start + arc_num * PI_DIV(2) / arcs, IM_PI, 1.0f, 0.0f);
        for (size_t i = 0; i <= num_segments; i++) {
            const float a = start + b + arc_angle * arc_num + (i * angle_offset);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
        }
        window->DrawList->PathStroke(c, false, thickness);
    }
}

void ImGui::SpinnerArcFade(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI * 4.f);
    const float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;

    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        window->DrawList->PathClear();
        for (size_t i = 0; i <= num_segments + 1; i++)
        {
            const float a = arc_angle * arc_num + (i * angle_offset) - PI_DIV_2 - PI_DIV_4;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
        }
        const float a = arc_angle * arc_num;
        ImColor c = color;
        if (start < PI_2)
        {
            c.Value.w = 0.f;
            if (start > a && start < (a + arc_angle)) { c.Value.w = 1.f - (start - a) / (float)arc_angle; }
            else if (start < a) { c.Value.w = 1.f; }
            float woff = ease((ease_mode)mode, start - a, 4.f); 
            c.Value.w = ImMax(0.05f, 1.f - c.Value.w - woff);
        }
        else
        {
            const float startk = start - PI_2;
            c.Value.w = 0.f;
            if (startk > a && startk < (a + arc_angle)) { c.Value.w = 1.f - (startk - a) / (float)arc_angle; }
            else if (startk < a) { c.Value.w = 1.f; }
            float woff = ease((ease_mode)mode, start - a, 4.f); 
            c.Value.w = ImMax(0.05f, c.Value.w + woff);
        }

        window->DrawList->PathStroke(c, false, thickness);
    }
}

void ImGui::SpinnerSimpleArcFade(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI * 4.f);
    const float arc_angle = PI_2 / (float)4;
    const float angle_offset = arc_angle / num_segments;
    auto draw_segment = [&] (int arc_num, float delta, auto c, float k, float t) {
        window->DrawList->PathClear();
        for (size_t i = 0; i <= num_segments + 1; i++) {
            const float a = t * start + arc_angle * arc_num + (i * angle_offset) - PI_DIV_2 - PI_DIV_4 + delta;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius * k, centre.y + ImSin(a) * radius * k));
        }
        window->DrawList->PathStroke(color_alpha(c, 1.f), false, thickness);
    };
    for (size_t arc_num = 0; arc_num < 2; ++arc_num) {
        const float a = arc_angle * arc_num;
        ImColor c = color;
        if (start < PI_2) {
            c.Value.w = 0.f;
            if (start > a && start < (a + arc_angle)) { c.Value.w = 1.f - (start - a) / (float)arc_angle; }
            else if (start < a) { c.Value.w = 1.f; }
            c.Value.w = ImMax(0.05f, 1.f - c.Value.w);
        } else {
            const float startk = start - PI_2;
            c.Value.w = 0.f;
            if (startk > a && startk < (a + arc_angle)) { c.Value.w = 1.f - (startk - a) / (float)arc_angle; }
            else if (startk < a) { c.Value.w = 1.f; }
            c.Value.w = ImMax(0.05f, c.Value.w);
        }
        draw_segment(arc_num, 0.f, c, 1.f + arc_num * 0.3f, arc_num > 0 ? -1 : 1);
        draw_segment(arc_num, IM_PI, c, 1.f + arc_num * 0.3f, arc_num > 0 ? -1 : 1);
    }
}

void ImGui::SpinnerSquareStrokeFade(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI * 4.f);
    const float arc_angle = PI_DIV_2;
    const float ht = thickness / 2.f;
    for (size_t arc_num = 0; arc_num < 4; ++arc_num)
    {
        float a = arc_angle * arc_num;
        ImColor c = color_alpha(color, 1.f);
        if (start < PI_2) {
            c.Value.w = (start > a && start < (a + arc_angle))
                            ? 1.f - (start - a) / (float)arc_angle
                            : (start < a ? 1.f : 0.f);
            c.Value.w = ImMax(0.05f, 1.f - c.Value.w);
        } else {
            const float startk = start - PI_2;
            c.Value.w = (startk > a && startk < (a + arc_angle))
                            ? 1.f - (startk - a) / (float)arc_angle
                            : (startk < a ? 1.f : 0.f);
            c.Value.w = ImMax(0.05f, c.Value.w);
        }
        a -= PI_DIV_4;
        const float r = radius * 1.4f;
        const bool right = ImSin(a) > 0;
        const bool top = ImCos(a) < 0;
        ImVec2 p1(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r);
        ImVec2 p2(centre.x + ImCos(a - PI_DIV_2) * r, centre.y + ImSin(a - PI_DIV_2) * r);
        switch (arc_num) {
        case 0: p1.x -= ht; p2.x -= ht; break;
        case 1: p1.y -= ht; p2.y -= ht; break;
        case 2: p1.x += ht; p2.x += ht; break;
        case 3: p1.y += ht; p2.y += ht; break;
        }
        window->DrawList->AddLine(p1, p2, color_alpha(c, 1.f), thickness);
    }
}

void ImGui::SpinnerAsciiSymbolPoints(const char *label, const char* text, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    if (!text || !*text)
        return;
    const float start = ImFmod((float)ImGui::GetTime() * speed, (float)strlen(text));
    const ImFontGlyph* glyph = ImGui::GetCurrentContext()->Font->FindGlyph(text[(int)start]);
    ImVec2 pp(centre.x - radius, centre.y - radius);
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    unsigned char* bitmap;
    int out_width, out_height;
    atlas->GetTexDataAsAlpha8(&bitmap, &out_width, &out_height);
    const int U1 = (int)(glyph->U1 * out_width);
    const int U0 = (int)(glyph->U0 * out_width);
    const int V1 = (int)(glyph->V1 * out_height);
    const int V0 = (int)(glyph->V0 * out_height);
    const float px = size.x / (U1 - U0);
    const float py = size.y / (V1 - V0);
    
    for (int x = U0, ppx = 0; x < U1; x++, ppx++) {
        for (int y = V0, ppy = 0; y < V1; y++, ppy++) {
            ImVec2 point(pp.x + (ppx * px), pp.y + (ppy * py));
            const unsigned char alpha = bitmap[out_width * y + x];
            window->DrawList->AddCircleFilled(point, thickness * 1.5f, color_alpha(0x80808080, alpha / 255.f));
            window->DrawList->AddCircleFilled(point, thickness, color_alpha(color, alpha / 255.f));
        }
    }
}

void ImGui::SpinnerTextFading(const char *label, const char* text, float radius, float fsize, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    if (!text || !*text)
        return;
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const char *last_symbol = ImGui::FindRenderedTextEnd(text);
    const ImVec2 text_size = ImGui::CalcTextSize(text, last_symbol);
    const ImFont* font = ImGui::GetCurrentContext()->Font;
    ImVec2 pp(centre.x - text_size.x / 2.f, centre.y - text_size.y / 2.f);
    const int text_len = last_symbol - text;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; text != last_symbol; ++text, ++i) {
        const ImFontGlyph* glyph = ImGui::GetCurrentContext()->Font->FindGlyph(*text);
        const float alpha = ImClamp(ImSin(-start + (i / (float)text_len * PI_DIV_2)), 0.f, 1.f);
        ImColor c = ImColor::HSV(out_h + i * (1.f / text_len), out_s, out_v);
        font->RenderChar(window->DrawList, fsize, pp, color_alpha(c, alpha), (ImWchar)*text);
        pp.x += glyph->AdvanceX;
    }
}

void ImGui::SpinnerSevenSegments(const char *label, const char* text, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    if (!text || !*text)
        return;
    const float start = ImFmod((float)ImGui::GetTime() * speed, (float)strlen(text));
    struct Segment { ImVec2 b, e; };
    const float q = 1.f, hq = q * 0.5f, xq = thickness / radius;
    const Segment segments[] = {{ ImVec2{-hq, 0.0f}, ImVec2{hq, 0.0f} }, /*central*/
                                { ImVec2{-hq - xq, 0.0f}, ImVec2{-hq - xq, -q} }, /*left up*/
                                { ImVec2{-hq - xq, q}, ImVec2{-hq - xq, xq} }, /*left down*/
                                { ImVec2{-hq, q}, ImVec2{hq, q} }, /*center up*/
                                { ImVec2{hq + xq, q}, ImVec2{hq + xq, xq} }, /*right down*/
                                { ImVec2{hq + xq, 0.0f}, ImVec2{hq + xq, -q} }, /*right up*/
                                { ImVec2{-hq, -q}, ImVec2{hq, -q} } /*center down*/};
    const int symbols[][8]{
        //segments
        //g,f,e,d,c,b,a,sym
        // NUMBERS
        {0,1,1,1,1,1,1,0}, //ZERO
        {0,0,0,0,1,1,0,1}, //ONE
        {1,0,1,1,0,1,1,2}, //TWO
        {1,0,0,1,1,1,1,3}, //THREE
        {1,1,0,0,1,1,0,4}, //FOUR
        {1,1,0,1,1,0,1,5}, //FIVE
        {1,1,1,1,1,0,1,6}, //SIX
        {0,0,0,0,1,1,1,7}, //SEVEN
        {1,1,1,1,1,1,1,8}, //EIGHT
        {1,1,0,1,1,1,1,9}, //NINE
        // LETTERS
        {1,1,1,0,1,1,1,'A'}, //LETTER A
        {1,1,1,1,1,0,0,'B'}, //LETTER B
        {0,1,1,1,0,0,1,'C'}, //LETTER C
        {1,0,1,1,1,1,0,'D'}, //LETTER D
        {1,1,1,1,0,0,1,'E'}, //LETTER E
        {1,1,1,0,0,0,1,'F'}, //LETTER F
        {0,1,1,1,1,0,1,'G'}, //LETTER G
        {1,1,1,0,1,0,0,'H'}, //LETTER H
        {0,1,1,0,0,0,0,'I'}, //LETTER I
        {0,0,1,1,1,1,0,'J'}, //LETTER J
        {1,1,1,0,1,0,1,'K'}, //LETTER K
        {0,1,1,1,0,0,0,'L'}, //LETTER L
        {0,0,1,0,1,0,1,'M'}, //LETTER M
        {0,1,1,0,1,1,1,'N'}, //LETTER N
        {0,1,1,1,1,1,1,'O'}, //LETTER O
        {1,1,1,0,0,1,1,'P'}, //LETTER P
        {1,1,0,0,1,1,1,'Q'}, //LETTER Q
        {0,1,1,0,0,1,1,'R'}, //LETTER R
        {1,1,0,1,1,0,1,'S'}, //LETTER S
        {1,1,1,1,0,0,0,'T'}, //LETTER T
        {0,1,1,1,1,1,0,'U'}, //LETTER U
        {0,1,0,1,1,1,0,'V'}, //LETTER V
        {0,1,0,1,0,1,0,'W'}, //LETTER W
        {1,1,1,0,1,1,0,'X'}, //LETTER X
        {1,1,0,1,1,1,0,'Y'}, //LETTER Y
        {1,0,0,1,0,1,1,'Z'}, //LETTER Z
    };
    auto draw_segment = [&] (const Segment &s) {
        ImVec2 p1(centre.x + radius * s.b.x, centre.y + radius * s.b.y);
        ImVec2 p2(centre.x + radius * s.e.x, centre.y + radius * s.e.y);
        window->DrawList->AddLine(p1, p2, color_alpha(color, 1.f), thickness);
    };
    auto draw_symbol = [&] (const int* sq) {
        for (int i = 0; i < 7; ++i)
            sq[i] ? draw_segment(segments[i]) : void();
    };
    char current_char = text[(int)start];
    if (isalpha(current_char)) {
        draw_symbol(symbols[tolower(current_char) - 'a' + 10]);
    } else if (isdigit(current_char)) {
        draw_symbol(symbols[current_char - '0']);
    }
}

void ImGui::SpinnerSquareStrokeFill(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float overt = 3.f;
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2 + overt);
    const float arc_angle = PI_DIV_2;
    const float ht = thickness / 2.f;

    const float r = radius * 1.4f;
    ImVec2 pp(centre.x + ImCos(-IM_PI * 0.75f) * r, centre.y + ImSin(-IM_PI * 0.75f) * r);
    if (start > PI_2) {
        ImColor c = color;
        const float delta = (start - PI_2) / overt;
        if (delta < 0.5f) {
            c.Value.w = 1.f - delta * 2.f;
            window->DrawList->AddLine(ImVec2(pp.x - ht, pp.y), ImVec2(pp.x + ht, pp.y), color_alpha(c, 1.f), thickness);
        } else {
            c.Value.w = (delta - 0.5f) * 2.f;
            window->DrawList->AddLine(ImVec2(pp.x - ht, pp.y), ImVec2(pp.x + ht, pp.y), color_alpha(color, 1.f), thickness);
        }
    } else {
        window->DrawList->AddLine(ImVec2(pp.x - ht, pp.y), ImVec2(pp.x + ht, pp.y), color_alpha(color, 1.f), thickness);
    }

    if (start < PI_2) {
        for (size_t arc_num = 0; arc_num < 4; ++arc_num) {
            float a = arc_angle * arc_num;
            float segment_progress = (start > a && start < (a + arc_angle))
                ? 1.f - (start - a) / (float)arc_angle
                : (start < a ? 1.f : 0.f);
            a -= PI_DIV_4;
            segment_progress = 1.f - segment_progress;
            ImVec2 p1(centre.x + ImCos(a - PI_DIV_2) * r, centre.y + ImSin(a - PI_DIV_2) * r);
            ImVec2 p2(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r);
            switch (arc_num) {
            case 0: p1.x += ht; p2.x -= ht; p2.x = p1.x + (p2.x - p1.x) * segment_progress; break;
            case 1: p1.y -= ht; p2.y -= ht; p2.y = p1.y + (p2.y - p1.y) * segment_progress; break;
            case 2: p1.x += ht; p2.x += ht; p2.x = p1.x + (p2.x - p1.x) * segment_progress; break;
            case 3: p1.y += ht; p2.y -= ht; p2.y = p1.y + (p2.y - p1.y) * segment_progress; break;
            }
            window->DrawList->AddLine(p1, p2, color_alpha(color, 1.f), thickness);
        }
    }
}

void ImGui::SpinnerSquareStrokeLoading(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2 );
    const float arc_angle = PI_DIV_2;
    const float ht = thickness / 2.f;

    const float best_radius = radius * 1.4f;
    const float delta = (start - PI_2) / 2.f;
    for (size_t arc_num = 0; arc_num < 4; ++arc_num) {
        float a = arc_angle * arc_num;
        a -= PI_DIV_4;
        ImVec2 pp(centre.x + ImCos(a) * best_radius, centre.y + ImSin(a) * best_radius);
        window->DrawList->AddLine(ImVec2(pp.x - ht, pp.y), ImVec2(pp.x + ht, pp.y), color_alpha(color, 1.f), thickness);
    }

    const bool grow = start < IM_PI;
    const float segment_progress = grow ? (start / IM_PI) : (1.f - (start - IM_PI) / IM_PI);
    for (size_t arc_num = 0; arc_num < 4; ++arc_num)             {
        float a = arc_angle * arc_num;
        a -= PI_DIV_4;
        const bool right = ImSin(a) > 0;
        const bool top = ImCos(a) < 0;
        ImVec2 p1(centre.x + ImCos(a - PI_DIV_2) * best_radius, centre.y + ImSin(a - PI_DIV_2) * best_radius);
        ImVec2 p2(centre.x + ImCos(a) * best_radius, centre.y + ImSin(a) * best_radius);
        switch (arc_num) {
        case 0: p1.x -= ht; p2.x += ht;
            p1.x = grow ? p1.x : p1.x + (p2.x - p1.x) * (1.f - segment_progress); p2.x = grow ? p1.x + (p2.x - p1.x) * segment_progress : p2.x; break;
        case 1: p1.y -= ht; p2.y += ht;
            p1.y = grow ? p1.y : p1.y + (p2.y - p1.y) * (1.f - segment_progress); p2.y = grow ? p1.y + (p2.y - p1.y) * segment_progress : p2.y; break;
        case 2: p1.x += ht; p2.x -= ht;
            p1.x = grow ? p1.x : grow ? p1.x : p1.x + (p2.x - p1.x) * (1.f - segment_progress); p2.x = grow ? p1.x + (p2.x - p1.x) * segment_progress : p2.x; break;
        case 3: p1.y += ht; p2.y -= ht;
            p1.y = grow ? p1.y : p1.y + (p2.y - p1.y) * (1.f - segment_progress); p2.y = grow ? p1.y + (p2.y - p1.y) * segment_progress : p2.y; break;
        }
        window->DrawList->AddLine(p1, p2, color_alpha(color, 1.f), thickness);
    }
}

void ImGui::SpinnerSquareLoading(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2 + PI_DIV_2 );
    const float arc_angle = PI_DIV_2;
    const float ht = thickness / 2.f;
    const float best_radius = radius * 1.4f;
    float a = arc_angle * 3 - PI_DIV_4 + (start > PI_2 ? start * 2.f : 0);
    ImVec2 last_pos(centre.x + ImCos(a) * best_radius, centre.y + ImSin(a) * best_radius);
    ImVec2 ppMin, ppMax;
    for (size_t arc_num = 0; arc_num < 4; ++arc_num) {              
        a = arc_angle * arc_num - PI_DIV_4 + (start > PI_2 ? start * 2.f : 0);
        ImVec2 pp(centre.x + ImCos(a) * best_radius, centre.y + ImSin(a) * best_radius);
        window->DrawList->AddLine(last_pos, pp, color_alpha(color, 1.f), thickness);
        last_pos = pp;
        if (start < PI_2) {
            if (arc_num == 2) ppMin = ImVec2(centre.x + ImCos(a) * best_radius * 0.8f, centre.y + ImSin(a) * best_radius * 0.8f);
            else if (arc_num == 0) ppMax = ImVec2(centre.x + ImCos(a) * best_radius * 0.8f, centre.y + ImSin(a) * best_radius * 0.8f);
        }
    }
    if (start < PI_2) {
        ppMax.y = ppMin.y + (start / PI_2) * (ppMax.y - ppMin.y);
        window->DrawList->AddRectFilled(ppMin, ppMax, color_alpha(color, 1.f), 0.f);
    }
}

void ImGui::SpinnerFilledArcFade(const char *label, float radius, const ImColor &color, float speed, size_t arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI * 4.f);
    const float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;

    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        const float b = arc_angle * arc_num - PI_DIV_2 - PI_DIV_4;
        const float e = arc_angle * arc_num + arc_angle - PI_DIV_2 - PI_DIV_4;
        const float a = arc_angle * arc_num;
        ImColor c = color;
        float vradius = radius;
        if (start < PI_2)
        {
            c.Value.w = 0.f;
            if (start > a && start < (a + arc_angle)) { c.Value.w = 1.f - (start - a) / (float)arc_angle; }
            else if (start < a) { c.Value.w = 1.f; }
            c.Value.w = ImMax(0.f, 1.f - c.Value.w);
            if (mode == 1)
                vradius = radius * c.Value.w;

        }
        else
        {
            const float startk = start - PI_2;
            c.Value.w = 0.f;
            if (startk > a && startk < (a + arc_angle)) { c.Value.w = 1.f - (startk - a) / (float)arc_angle; }
            else if (startk < a) { c.Value.w = 1.f; }
            if (mode == 1)
                vradius = radius * c.Value.w;
        }

        window->DrawList->PathClear();
        window->DrawList->PathLineTo(centre);
        for (size_t i = 0; i <= num_segments + 1; i++)
        {
            const float ar = arc_angle * arc_num + (i * angle_offset) - PI_DIV_2 - PI_DIV_4;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar) * vradius, centre.y + ImSin(ar) * vradius));
        }

        window->DrawList->PathFillConvex(color_alpha(c, 1.f));
    }
}

void ImGui::SpinnerPointsArcBounce(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t points, int circles, float rspeed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime()* speed, IM_PI * 4.f);
    const float arc_angle = PI_2 / (float)points;
    const float angle_offset = arc_angle / num_segments;
    float dspeed = rspeed;
    for (int c_num = 0; c_num < circles; c_num++)
    {
        float mr = radius * (1.f - (1.f / (circles + 2.f) * c_num));
        float adv_angle = IM_PI * c_num;// *(1.f + (0.1f * circles) * c_num);
        for (size_t arc_num = 0; arc_num < points; ++arc_num)
        {
            const float b = arc_angle * arc_num - PI_DIV_2 - PI_DIV_4;
            const float e = arc_angle * arc_num + arc_angle - PI_DIV_2 - PI_DIV_4;
            const float a = arc_angle * arc_num;
            ImColor c = color;
            float vradius = mr;
            if (start < PI_2) {
                c.Value.w = 0.f;
                if (start > a && start < (a + arc_angle)) { c.Value.w = 1.f - (start - a) / (float)arc_angle; }
                else if (start < a) { c.Value.w = 1.f; }
                c.Value.w = ImMax(0.f, 1.f - c.Value.w);
                 vradius = mr * c.Value.w;
            }
            else
            {
                const float startk = start - PI_2;
                c.Value.w = 0.f;
                if (startk > a && startk < (a + arc_angle)) { c.Value.w = 1.f - (startk - a) / (float)arc_angle; }
                else if (startk < a) { c.Value.w = 1.f; }
                vradius = mr * c.Value.w;
            }
            const float ar = start * dspeed + adv_angle + arc_angle * arc_num - PI_DIV_2 - PI_DIV_4;
            window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(ar) * vradius, centre.y + ImSin(ar) * vradius), thickness, color_alpha(c, 1.f), 8);
        }
        dspeed += rspeed;
    }
}

void ImGui::SpinnerFilledArcColor(const char *label, float radius, const ImColor &color, const ImColor &bg, float speed, size_t arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;

    window->DrawList->AddCircleFilled(centre, radius, color_alpha(bg, 1.f), num_segments * 2);
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        const float b = arc_angle * arc_num - PI_DIV_2;
        const float e = arc_angle * arc_num + arc_angle - PI_DIV_2;
        const float a = arc_angle * arc_num;

        ImColor c = color;
        c.Value.w = 0.f;
        if (start > a && start < (a + arc_angle)) { c.Value.w = 1.f - (start - a) / (float)arc_angle; }
        else if (start < a) { c.Value.w = 1.f; }
        c.Value.w = ImMax(0.f, 1.f - c.Value.w);

        window->DrawList->PathClear();
        window->DrawList->PathLineTo(centre);
        for (size_t i = 0; i < num_segments + 1; i++)
        {
            const float ar = arc_angle * arc_num + (i * angle_offset) - PI_DIV_2;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar) * radius, centre.y + ImSin(ar) * radius));
        }
        window->DrawList->PathFillConvex(color_alpha(c, 1.f));
    }
}

void ImGui::SpinnerFilledArcRing(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, size_t arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float pi_div_2 = PI_DIV_2;
    const float pi_div_4 = PI_DIV_4;
    const float pi_mul_2 = PI_2;
    const float start = ImFmod((float)ImGui::GetTime() * speed, pi_mul_2 + pi_div_4);
    const float arc_angle = pi_mul_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;
    window->DrawList->PathClear();
    for (size_t i = 0; i < num_segments + 1; i++)
    {
        const float ar_b = (i * (pi_mul_2 / num_segments));
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar_b) * radius, centre.y + ImSin(ar_b) * radius));
    }
    window->DrawList->PathStroke(color_alpha(bg, 1.f), false, thickness);
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        const float b = arc_angle * arc_num - pi_div_2;
        const float e = arc_angle * arc_num + arc_angle - pi_div_2;
        const float a = arc_angle * arc_num;

        float alpha = 0.f;
        if (start > pi_mul_2) { alpha = (start - pi_mul_2) / pi_div_4; }
        else if (start > a && start < (a + arc_angle)) { alpha = 1.f - (start - a) / (float)arc_angle; }
        else if (start < a) { alpha = 1.f; }

        window->DrawList->PathClear();
        for (size_t i = 0; i < num_segments + 1; i++)
        {
            const float ar_b = arc_angle * arc_num + (i * angle_offset) - pi_div_2;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar_b) * radius, centre.y + ImSin(ar_b) * radius));
        }
        window->DrawList->PathStroke(color_alpha(color, ImMax(0.f, 1.f - alpha)), false, thickness);
    }
}

void ImGui::SpinnerArcWedges(const char *label, float radius, const ImColor &color, float speed, size_t arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        const float b = arc_angle * arc_num - PI_DIV_2;
        const float e = arc_angle * arc_num + arc_angle - PI_DIV_2;
        const float a = arc_angle * arc_num;
        window->DrawList->PathClear();
        window->DrawList->PathLineTo(centre);
        for (size_t i = 0; i < num_segments + 1; i++)
        {
            const float start_a = ImFmod(start * (1.05f * (arc_num + 1)), PI_2);
            const float ar = start_a + arc_angle * arc_num + (i * angle_offset) - PI_DIV_2;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar) * radius, centre.y + ImSin(ar) * radius));
        }
        window->DrawList->PathFillConvex(color_alpha(ImColor::HSV(out_h + (1.f / arcs) * arc_num, out_s, out_v, 0.7f), 1.f));
    }
}

void ImGui::SpinnerTwinBall(const char *label, float radius1, float radius2, float thickness, float b_thickness, const ImColor &ball, const ImColor &bg, float speed, size_t balls)
{
    float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / num_segments;

    window->DrawList->PathClear();
    for (size_t i = 0; i <= num_segments; i++)
    {
        const float a = start + (i * bg_angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1));
    }
    window->DrawList->PathStroke(color_alpha(bg, 1.f), false, thickness);

    for (size_t b_num = 0; b_num < balls; ++b_num)
    {
        float b_start = PI_2 / balls;
        const float a = b_start * b_num + start;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius2, centre.y + ImSin(a) * radius2), b_thickness, color_alpha(ball, 1.f));
    }
}

void ImGui::SpinnerSomeScaleDots(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, IM_PI / dots);
    start -= astart;
    const float bg_angle_offset = IM_PI / dots;
    dots = ImMin(dots, (size_t)32);
    for (size_t j = 0; j < 4; j++)
    {
        float r = radius * (1.f - (0.15f * j));
        for (size_t i = 0; i <= dots; i++)
        {
            float a = start * (mode ? (1.f + j * 0.05f) : 1.f) + (i * bg_angle_offset);
            float th = thickness * ImMax(0.1f, i / (float)dots);
            float thh = th * (1.f - (0.2f * j));
            window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r), thh, color_alpha(color, 1.f), 8);
        }
    }
}

void ImGui::SpinnerAngTriple(const char *label, float radius1, float radius2, float radius3, float thickness, const ImColor &c1, const ImColor &c2, const ImColor &c3, float speed, float angle)
{
    float radius = ImMax(ImMax(radius1, radius2), radius3);
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start1 = (float)ImGui::GetTime() * speed;
    const float angle_offset = angle / num_segments;

    window->DrawList->PathClear();
    for (size_t i = 0; i < num_segments; i++)
    {
        const float a = start1 + (i * angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1));
    }
    window->DrawList->PathStroke(color_alpha(c1, 1.f), false, thickness);

    float start2 = (float)ImGui::GetTime() * 1.2f * speed;
    window->DrawList->PathClear();
    for (size_t i = 0; i < num_segments; i++)
    {
        const float a = start2 + (i * angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(-a) * radius2, centre.y + ImSin(-a) * radius2));
    }
    window->DrawList->PathStroke(color_alpha(c2, 1.f), false, thickness);

    float start3 = (float)ImGui::GetTime() * 0.9f * speed;
    window->DrawList->PathClear();
    for (size_t i = 0; i < num_segments; i++)
    {
        const float a = start3 + (i * angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius3, centre.y + ImSin(a) * radius3));
    }
    window->DrawList->PathStroke(color_alpha(c3, 1.f), false, thickness);
}

void ImGui::SpinnerAngEclipse(const char *label, float radius, float thickness, const ImColor &color, float speed, float angle)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime()* speed;
    const float angle_offset = angle / num_segments;
    const float th = thickness / num_segments;

    for (size_t i = 0; i < num_segments; i++)
    {
        const float a = start + (i * angle_offset);
        const float a1 = start + ((i+1) * angle_offset);
        window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius),
                                ImVec2(centre.x + ImCos(a1) * radius, centre.y + ImSin(a1) * radius),
                                color_alpha(color, 1.f),
                                th * i);
    }
}

void ImGui::SpinnerIngYang(const char *label, float radius, float thickness, bool reverse, float yang_detlta_r, const ImColor &colorI, const ImColor &colorY, float speed, float angle)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float startI = (float)ImGui::GetTime() * speed;
    const float startY = (float)ImGui::GetTime() * (speed + (yang_detlta_r > 0.f ? ImClamp(yang_detlta_r * 0.5f, 0.5f, 2.f) : 0.f));
    const float angle_offset = angle / num_segments;
    const float th = thickness / num_segments;

    for (int i = 0; i < num_segments; i++)
    {
        const float a = startI + (i * angle_offset);
        const float a1 = startI + ((i + 1) * angle_offset);
        window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius),
                                ImVec2(centre.x + ImCos(a1) * radius, centre.y + ImSin(a1) * radius),
                                color_alpha(colorI, 1.f),
                                th * i);
    }
    const float ai_end = startI + (num_segments * angle_offset);
    ImVec2 circle_i_center{centre.x + ImCos(ai_end) * radius, centre.y + ImSin(ai_end) * radius};
    window->DrawList->AddCircleFilled(circle_i_center, thickness / 2.f, color_alpha(colorI, 1.f), num_segments);

    const float rv = reverse ? -1.f : 1.f;
    const float yang_radius = (radius - yang_detlta_r);
    for (int i = 0; i < num_segments; i++)
    {
        const float a = startY + IM_PI + (i * angle_offset);
        const float a1 = startY + IM_PI + ((i+1) * angle_offset);
        window->DrawList->AddLine(ImVec2(centre.x + ImCos(a * rv) * yang_radius, centre.y + ImSin(a * rv) * yang_radius),
                                ImVec2(centre.x + ImCos(a1 * rv) * yang_radius, centre.y + ImSin(a1 * rv) * yang_radius),
                                color_alpha(colorY, 1.f),
                                th * i);
    }
    const float ay_end = startY + IM_PI + (num_segments * angle_offset);
    ImVec2 circle_y_center{centre.x + ImCos(ay_end * rv) * yang_radius, centre.y + ImSin(ay_end * rv) * yang_radius};
    window->DrawList->AddCircleFilled(circle_y_center, thickness / 2.f, color_alpha(colorY, 1.f), num_segments);
}

void ImGui::SpinnerGooeyBalls(const char *label, float radius, const ImColor &color, float speed, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI);
    start = mode ? damped_spring(1, 10.f, 1.0f, ImSin(start), 1, 0) : start;
    const float radius1 = (0.4f + 0.3f * ImSin(start)) * radius;
    const float radius2 = radius - radius1;

    window->DrawList->AddCircleFilled(ImVec2(centre.x - radius + radius1, centre.y), radius1, color_alpha(color, 1.f), num_segments);
    window->DrawList->AddCircleFilled(ImVec2(centre.x - radius + radius1 * 1.2f + radius2, centre.y), radius2, color_alpha(color, 1.f), num_segments);
}

void ImGui::SpinnerDotsLoading(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, IM_PI);
    const float radius1 = (2.f * ImSin(start)) * radius;
    float startb = ImFmod(start, PI_DIV_2);
    float lenb = startb < PI_DIV_2 ? ImAbs((0.5f * ImSin(start * 2)) * radius) : radius * 0.5f;
    float radius2 = radius * 0.25f;
    float deltae = thickness - ImMin(thickness, ImMax<float>(0, (2.f * radius - radius1 + thickness + lenb) * 0.25f));
    float deltag = ImMin(thickness, ImAbs(centre.x - radius + radius1 + thickness + lenb - centre.x - radius) * 0.25f);
    window->DrawList->AddCircleFilled(ImVec2(centre.x - radius, centre.y), radius2 + deltag, color_alpha(bg, 1.f), num_segments);
    window->DrawList->AddCircleFilled(ImVec2(centre.x + radius + thickness, centre.y), radius2 + deltae, color_alpha(bg, 1.f), num_segments);
    window->DrawList->AddRectFilled(ImVec2(centre.x - radius + radius1 - thickness - lenb, centre.y - thickness), ImVec2(centre.x - radius + radius1 + thickness + lenb, centre.y + thickness), color_alpha(color, 1.f), thickness);
}

void ImGui::SpinnerRotateGooeyBalls(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime(), IM_PI);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    float radius1 = (0.2f + 0.3f * ImSin(start)) * radius;
    const float angle_offset = PI_2 / balls;

    float roff = ease((ease_mode)mode, start, radius);
    for (int i = 0; i <= balls; i++)
    {
        const float a = rstart + (i * angle_offset);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * (radius1 + roff), centre.y + ImSin(a) * (radius1 + roff)), thickness, color_alpha(color, 1.f), num_segments);
    }
}

void ImGui::SpinnerHerbertBalls(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime(), IM_PI);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float radius1 = 0.3f * radius;
    const float radius2 = 0.8f * radius;
    const float angle_offset = PI_2 / balls;
    for (int i = 0; i < balls; i++)
    {
        const float a = rstart + (i * angle_offset);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1), thickness, color_alpha(color, 1.f), num_segments);
    }
    for (int i = 0; i < balls * 2; i++)
    {
        const float a = -rstart + (i * angle_offset / 2.f);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius2, centre.y + ImSin(a) * radius2), thickness, color_alpha(color, 1.f), num_segments);
    }
}

void ImGui::SpinnerHerbertBalls3D(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime(), IM_PI);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float radius1 = 0.3f * radius;
    const float radius2 = 0.8f * radius;
    const int balls = 2;
    const float angle_offset = PI_2 / balls;

    ImVec2 frontpos, backpos;
    for (int i = 0; i < balls; i++)
    {
        const float a = rstart + (i * angle_offset);
        const float t = (i == 1 ? 0.7f : 1.f) * thickness;
        const ImVec2 pos = ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1);
        window->DrawList->AddCircleFilled(pos, t, color_alpha(color, 1.f), num_segments);
        if (i == 0) frontpos = pos; else backpos = pos;
    }

    ImVec2 lastpos;
    for (int i = 0; i <= balls * 2; i++)
    {
        const float a = -rstart + (i * angle_offset / 2.f);
        const ImVec2 pos = ImVec2(centre.x + ImCos(a) * radius2, centre.y + ImSin(a) * radius2);
        float t = sqrt(pow(pos.x - frontpos.x, 2) + pow(pos.y - frontpos.y, 2)) / (radius * 1.f) * thickness;
        window->DrawList->AddCircleFilled(pos, t, color_alpha(color, 1.f), num_segments);
        window->DrawList->AddLine(pos, backpos, color_alpha(color, 0.5f), ImMax(thickness / 2.f, 1.f));
        if (i > 0) {
            window->DrawList->AddLine(pos, lastpos, color_alpha(color, 1.f), ImMax(thickness / 2.f, 1.f));
        }
        window->DrawList->AddLine(pos, frontpos, color_alpha(color, 1.f), ImMax(thickness / 2.f, 1.f));
        lastpos = pos;
    }
}

void ImGui::SpinnerRotateTriangles(const char *label, float radius, float thickness, const ImColor &color, float speed, int tris)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime(), IM_PI);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float radius1 = radius / 2.5f + thickness;
    const float angle_offset = PI_2 / tris;

    for (int i = 0; i <= tris; i++)
    {
        const float a = rstart + (i * angle_offset);
        ImVec2 tri_centre(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1);
        ImVec2 p1(tri_centre.x + ImCos(-a) * radius1, tri_centre.y + ImSin(-a) * radius1);
        ImVec2 p2(tri_centre.x + ImCos(-a + PI_2 / 3.f) * radius1, tri_centre.y + ImSin(-a + PI_2 / 3.f) * radius1);
        ImVec2 p3(tri_centre.x + ImCos(-a - PI_2 / 3.f) * radius1, tri_centre.y + ImSin(-a - PI_2 / 3.f) * radius1);
        ImVec2 points[] = {p1, p2, p3};
        window->DrawList->AddConvexPolyFilled(points, 3, color);
    }
}

void ImGui::SpinnerRotateShapes(const char *label, float radius, float thickness, const ImColor &color, float speed, int shapes, int pnt)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime(), IM_PI);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float radius1 = radius / 2.5f + thickness;
    const float angle_offset = PI_2 / shapes;
    std::vector<ImVec2> points(pnt);
    const float begin_a = -IM_PI / ((pnt % 2 == 0) ? pnt : (pnt - 1));
    for (int i = 0; i <= shapes; i++)
    {
        const float a = rstart + (i * angle_offset);
        ImVec2 tri_centre(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1);
        for (int pi = 0; pi < pnt; ++pi) {
            points[pi] = {tri_centre.x + ImCos(begin_a + pi * PI_2 / pnt) * radius1, tri_centre.y + ImSin(begin_a + pi * PI_2 / pnt) * radius1};
        }
        window->DrawList->AddConvexPolyFilled(points.data(), pnt, color_alpha(color, 1.f));
    }
}

void ImGui::SpinnerSinSquares(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime(), IM_PI);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float radius1 = radius / 2.5f + thickness;
    const float angle_offset = PI_DIV_2;
    std::vector<ImVec2> points(4);
    for (int i = 0; i <= 4; i++)
    {
        const float a = rstart + (i * angle_offset);
        const float begin_a = a - PI_DIV_2;
        float roff = ImMax(ImSin(start) - 0.5f, 0.f) * (radius * 0.4f);
        switch (mode) {
        case 1: roff = ease_inoutquad(roff); break;
        case 2: roff = ease_inoutexpo(roff) * (radius * 0.3f); break;
        }
        ImVec2 tri_centre(centre.x + ImCos(a) * (radius1 + roff), centre.y + ImSin(a) * (radius1 + roff));
        for (int pi = 0; pi < 4; ++pi) {
            points[pi] = {tri_centre.x + ImCos(begin_a+ pi * PI_DIV_2) * radius1, tri_centre.y + ImSin(begin_a + pi * PI_DIV_2) * radius1};
        }
        window->DrawList->AddConvexPolyFilled(points.data(), 4, color_alpha(color, 1.f));
    }
}

void ImGui::SpinnerMoonLine(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, float angle)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime()* speed;
    const float angle_offset = (angle * 0.5f) / num_segments;
    const float th = thickness / num_segments;

    window->DrawList->AddCircleFilled(centre, radius, bg, num_segments);

    auto draw_gradient = [&] (const std::function<float (int)>& b, const std::function<float (int)>& e, const std::function<float (int)>& th) {
        for (int i = 0; i < num_segments; i++)
        {
            window->DrawList->AddLine(ImVec2(centre.x + ImCos(start + b(i)) * radius, centre.y + ImSin(start + b(i)) * radius),
                                    ImVec2(centre.x + ImCos(start + e(i)) * radius, centre.y + ImSin(start + e(i)) * radius),
                                    color_alpha(color, 1.f),
                                    th(i));
        }
    };

    draw_gradient([&] (int i) { return (num_segments + i) * angle_offset; },
                    [&] (int i) { return (num_segments + i + 1) * angle_offset; },
                    [&] (int i) { return thickness - th * i; });

    draw_gradient([&] (int i) { return (i) * angle_offset; },
                    [&] (int i) { return (i + 1) * angle_offset; },
                    [&] (int i) { return th * i; });

    draw_gradient([&] (int i) { return (num_segments + i) * angle_offset; },
                    [&] (int i) { return (num_segments + i + 1) * angle_offset; },
                    [&] (int i) { return thickness - th * i; });

    const float b_angle_offset = (PI_2 - angle) / num_segments; 
    draw_gradient([&] (int i) { return num_segments * angle_offset * 2.f + (i * b_angle_offset); },
                    [&] (int i) { return num_segments * angle_offset * 2.f + ((i + 1) * b_angle_offset); },
                    [] (int) { return 1.f; });
}

void ImGui::SpinnerCircleDrop(const char *label, float radius, float thickness, float thickness_drop, const ImColor &color, const ImColor &bg, float speed, float angle)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime() * speed;
    const float bg_angle_offset = PI_2 / num_segments;
    const float angle_offset = angle / num_segments;
    const float th = thickness_drop / num_segments;
    const float drop_radius_th = thickness_drop / num_segments;

    for (int i = 0; i < num_segments; i++)
    {
        const float a = start + (i * angle_offset);
        const float a1 = start + ((i + 1) * angle_offset);
        const float s_drop_radius = radius - thickness / 2.f - (drop_radius_th * i);
        window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * s_drop_radius, centre.y + ImSin(a) * s_drop_radius),
                                ImVec2(centre.x + ImCos(a1) * s_drop_radius, centre.y + ImSin(a1) * s_drop_radius),
                                color_alpha(color, 1.f),
                                th * 2.f * i);
    }
    const float ai_end = start + (num_segments * angle_offset);
    const float f_drop_radius = radius - thickness / 2.f - thickness_drop;
    ImVec2 circle_i_center{centre.x + ImCos(ai_end) * f_drop_radius, centre.y + ImSin(ai_end) * f_drop_radius};
    window->DrawList->AddCircleFilled(circle_i_center, thickness_drop, color_alpha(color, 1.f), num_segments);

    window->DrawList->PathClear();
    for (int i = 0; i <= num_segments; i++)
    {
        const float a = (i * bg_angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
    }
    window->DrawList->PathStroke(color_alpha(bg, 1.f), false, thickness);
}

void ImGui::SpinnerSurroundedIndicator(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float lerp_koeff = (ImSin((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
    window->DrawList->AddCircleFilled(centre, thickness, color_alpha(bg, 1.f), num_segments);
    window->DrawList->AddCircleFilled(centre, thickness, color_alpha(color, ImMax(0.1f, ImMin(lerp_koeff, 1.f))), num_segments);

    auto PathArc = [&] (const ImColor& c, float th) {
        window->DrawList->PathClear();
        const float bg_angle_offset = PI_2 / num_segments;
        for (int i = 0; i <= num_segments; i++)
          window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(i * bg_angle_offset) * radius, centre.y + ImSin(i * bg_angle_offset) * radius));
        window->DrawList->PathStroke(color_alpha(c, 1.f), false, th);
    };

    lerp_koeff = (ImSin((float)ImGui::GetTime() * speed * 1.6f) + 1.f) * 0.5f;
    PathArc(bg, thickness);
    PathArc(color_alpha(color, 1.f - ImMax(0.1f, ImMin(lerp_koeff, 1.f))), thickness);
}

void ImGui::SpinnerWifiIndicator(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, float cangle, int dots) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    float lerp_koeff = (ImSin((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
    float start_ang = -cangle - PI_DIV_4 - PI_DIV_2;
    ImVec2 pc(centre.x + ImSin(cangle) * radius, centre.y + ImCos(cangle) * radius);
    window->DrawList->AddCircleFilled(pc, thickness, bg, num_segments);
    window->DrawList->AddCircleFilled(pc, thickness, color_alpha(color, ImMax(0.1f, ImMin(lerp_koeff, 1.f))), num_segments);
    auto PathArc = [&] (float as, const ImColor& c, float th, float r) {
        window->DrawList->PathClear();
        const float bg_angle_offset = PI_DIV(2) / num_segments;
        for (int i = 0; i <= num_segments; i++)
            window->DrawList->PathLineTo(ImVec2(pc.x + ImCos(as + i * bg_angle_offset) * r, pc.y + ImSin(as + i * bg_angle_offset) * r));
        window->DrawList->PathStroke(c, false, th);
    };
    const float interval = (size.x * 0.7f) / dots;
    for (int i = 0; i < dots; ++i) {
        float r = 1.5f * (i + 1) * interval;
        lerp_koeff = (ImSin((float)ImGui::GetTime() * speed - (i+1) * (IM_PI / dots)) + 1.f) * 0.5f;
        PathArc(start_ang, bg, thickness, r);
        PathArc(start_ang, color_alpha(color, ImMax(0.1f, ImMin(lerp_koeff, 1.f))), thickness, r);
    }
}

void ImGui::SpinnerTrianglesSelector(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, size_t bars) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    
    float lerp_koeff = (ImSin((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
    ImColor c = color_alpha(color, ImMax(0.1f, ImMin(lerp_koeff, 1.f)));
    float dr = radius - thickness - 3;
    window->DrawList->AddCircleFilled(centre, dr, bg, num_segments);
    window->DrawList->AddCircleFilled(centre, dr, c, num_segments);
    // Render
    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, PI_2 / bars);
    start -= astart;
    const float angle_offset = PI_2 / bars;
    const float angle_offset_t = angle_offset * 0.3f;
    bars = ImMin<size_t>(bars, 32);
    const float rmin = radius - thickness;
    auto get_points = [&] (float left, float right) -> std::array<ImVec2, 4> {
        return {
            ImVec2(centre.x + ImCos(left) * rmin, centre.y + ImSin(left) * rmin),
            ImVec2(centre.x + ImCos(left) * radius, centre.y + ImSin(left) * radius),
            ImVec2(centre.x + ImCos(right) * radius, centre.y + ImSin(right) * radius),
            ImVec2(centre.x + ImCos(right) * rmin, centre.y + ImSin(right) * rmin)
        };
    };
    auto draw_sectors = [&] (float s, const std::function<ImU32 (size_t)>& color_func) {
        for (size_t i = 0; i <= bars; i++) {
            float left = s + (i * angle_offset) - angle_offset_t;
            float right = s + (i * angle_offset) + angle_offset_t;
            auto points = get_points(left, right);
            window->DrawList->AddConvexPolyFilled(points.data(), 4, color_func(i));
        }
    };

    draw_sectors(0, [&] (size_t) { return color_alpha(bg, 0.1f); });
    draw_sectors(start, [&] (size_t i) { return color_alpha(bg, (i / (float)bars) - 0.5f); });
}

void ImGui::SpinnerCamera(const char *label, float radius, float thickness, LeafColor *leaf_color, float speed, size_t bars, int mode) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    switch (mode) {
    case 1:
        start += ease_inoutquad(ImSin(ImFmod(start, IM_PI)));
        thickness += ease_inoutquad(ImSin(ImFmod(start, IM_PI))) * (thickness * 0.5f);
        break;
    }
    const float angle_offset = PI_2 / bars;
    const float angle_offset_t = angle_offset * 0.3f;
    bars = ImMin<size_t>(bars, 32);

    const float rmin = radius - thickness - 1;
    auto get_points = [&] (float left, float right) -> std::array<ImVec2, 4> {
        return {
            ImVec2(centre.x + ImCos(left - 0.1f) * radius, centre.y + ImSin(left - 0.1f) * radius),
            ImVec2(centre.x + ImCos(right + 0.15f) * radius, centre.y + ImSin(right + 0.15f) * radius),
            ImVec2(centre.x + ImCos(right - 0.91f) * rmin, centre.y + ImSin(right - 0.91f) * rmin)
        };
    };

    auto draw_sectors = [&] (float s, const std::function<ImU32 (int)>& color_func) {
        for (size_t i = 0; i <= bars; i++) {
            float left = s + (i * angle_offset) - angle_offset_t;
            float right = s + (i * angle_offset) + angle_offset_t;
            auto points = get_points(left, right);
            window->DrawList->AddConvexPolyFilled(points.data(), 3, color_alpha(color_func((int)i), 1.f));
        }
    };

    draw_sectors(start, leaf_color);
}

void ImGui::SpinnerFlowingGradient(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, float angle)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime()* speed;
    const float angle_offset = (angle * 0.5f) / num_segments;
    const float bg_angle_offset = (PI_2) / num_segments;
    const float th = thickness / num_segments;

    for (size_t i = 0; i <= num_segments; i++)
    {
        const float a = (i * bg_angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + ImSin(a) * radius));
    }
    window->DrawList->PathStroke(bg, false, thickness);

    auto draw_gradient = [&] (const std::function<float (size_t)>& b, const std::function<float (size_t)>& e, const std::function<ImU32 (size_t)>& c) {
        for (size_t i = 0; i < num_segments; i++)
        {
            window->DrawList->AddLine(ImVec2(centre.x + ImCos(start + b(i)) * radius, centre.y + ImSin(start + b(i)) * radius),
                                    ImVec2(centre.x + ImCos(start + e(i)) * radius, centre.y + ImSin(start + e(i)) * radius),
                                    c(i),
                                    thickness);
        }
    };

    draw_gradient([&] (size_t i) { return (i) * angle_offset; },
                    [&] (size_t i) { return (i + 1) * angle_offset; },
                    [&] (size_t i) { return color_alpha(color, (i / (float)num_segments)); });

    draw_gradient([&] (size_t i) { return (num_segments + i) * angle_offset; },
                    [&] (size_t i) { return (num_segments + i + 1) * angle_offset; },
                    [&] (size_t i) { return color_alpha(color, 1.f - (i / (float)num_segments)); });
}

void ImGui::SpinnerRotateSegments(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs, size_t layers, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime()* speed;
    switch (mode) {
    case 1: start += ease_inoutquad(ImSin(ImFmod(start, IM_PI))); break;
    }
    float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;
    float r = radius;
    float reverse = 1.f;
    for (size_t layer = 0; layer < layers; layer++)
    {
        for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
        {
            window->DrawList->PathClear();
            for (size_t i = 2; i <= num_segments - 2; i++)
            {
                const float a = start * (1 + 0.1f * layer) + arc_angle * arc_num + (i * angle_offset);
                window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a * reverse) * r, centre.y + ImSin(a * reverse) * r));
            }
            window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
        }

        r -= (thickness + 1);
        reverse *= -1.f;
    }
}

void ImGui::SpinnerLemniscate(const char* label, float radius, float thickness, const ImColor& color, float speed, float angle)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime() * speed;
    const float a = radius;
    const float t = start;
    const float step = angle / num_segments;
    const float th = thickness / num_segments;
    
    for (size_t i = 0; i < num_segments; i++)
    {
        const auto xy0 = damped_infinity(start + (i * step), a);
        const auto xy1 = damped_infinity(start + ((i + 1) * step), a);

        window->DrawList->AddLine(ImVec2(centre.x + xy0.first, centre.y + xy0.second),
                                ImVec2(centre.x + xy1.first, centre.y + xy1.second),
                                color_alpha(color, 1.f),
                                th * i);
    }
}

void ImGui::SpinnerRotateGear(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t pins)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime()* speed;
    const float bg_angle_offset = PI_2 / num_segments;
    const float bg_radius = radius - thickness;

    window->DrawList->PathClear();
    for (size_t i = 0; i <= num_segments; i++)
    {
        const float a = (i * bg_angle_offset);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * bg_radius, centre.y + ImSin(a) * bg_radius));
    }
    window->DrawList->PathStroke(color_alpha(color, 1.f), false, bg_radius / 2);

    const float rmin = bg_radius;
    const float rmax = radius;
    const float pin_angle_offset = PI_2 / pins;
    for (size_t i = 0; i <= pins; i++)
    {
        float a = start + (i * pin_angle_offset);
        window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * rmin, centre.y + ImSin(a) * rmin),
                                ImVec2(centre.x + ImCos(a) * rmax, centre.y + ImSin(a) * rmax),
                                color_alpha(color, 1.f), thickness);
    }
}

void ImGui::SpinnerRotateWheel(const char *label, float radius, float thickness, const ImColor &bg_color, const ImColor &color, float speed, size_t pins)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const float bg_radius = radius - thickness;
    const float line_th = ImMax(radius / 8.f, 3.f);
    auto draw_circle = [window, num_segments, centre] (float r, const ImColor &c, float th) {
        const float bg_angle_offset = PI_2 / num_segments;
        window->DrawList->PathClear();
        for (size_t i = 0; i <= num_segments; i++)
        {
            const float a = (i * bg_angle_offset);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r));
        }
        window->DrawList->PathStroke(color_alpha(c, 1.f), false, th);
    };
    auto draw_pins = [window, centre, pins, start] (float rmin, float rmax, const ImColor &c, float th) {
        const float pin_angle_offset = PI_2 / pins;
        for (size_t i = 0; i <= pins; i++)             {
            float a = start + (i * pin_angle_offset);
            window->DrawList->AddLine(ImVec2(centre.x + ImCos(a) * rmin, centre.y + ImSin(a) * rmin),
                                    ImVec2(centre.x + ImCos(a) * rmax, centre.y + ImSin(a) * rmax),
                                    color_alpha(c, 1.f), th);
        }
    };
    draw_circle(bg_radius, bg_color, line_th);
    draw_pins(bg_radius, radius, bg_color, line_th);
    draw_circle(radius, color, line_th);
}

void ImGui::SpinnerAtom(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    
    const float start = (float)ImGui::GetTime()* speed;
    elipses = std::min<int>(elipses, 3);

    auto draw_rotated_ellipse = [&] (float alpha, float start) {
        alpha = ImFmod(alpha, IM_PI);
        float a = radius;
        float b = radius / 2.f; 
        window->DrawList->PathClear();
        for (int i = 0; i < num_segments; ++i) {
            float anga = (i * (PI_2 / (num_segments - 1)));

            float xx = a * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + centre.x;
            float yy = b * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + centre.y;
            window->DrawList->PathLineTo({xx, yy});
        }
        window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
        float anga = ImFmod(start, PI_2);
        float x = a * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + centre.x;
        float y = b * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + centre.y;
        return ImVec2{x, y};
    };
    ImVec2 ppos[3];
    for (int i = 0; i < elipses; ++i) {
        ppos[i % 3] = draw_rotated_ellipse((IM_PI * (float)i/ elipses), start * (1.f + 0.1f * i));
    }
    ImColor pcolors[3] = {ImColor(255, 0, 0), ImColor(0, 255, 0), ImColor(0, 0, 255)};
    for (int i = 0; i < elipses; ++i) {
        window->DrawList->AddCircleFilled(ppos[i], thickness * 2, color_alpha(pcolors[i], 1.f), int(num_segments / 3.f));
    }
}

void ImGui::SpinnerPatternRings(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime()* speed;
    elipses = std::max<int>(elipses, 1);
    auto draw_rotated_ellipse = [&] (float alpha, float tr, float y) {
        alpha = ImFmod(alpha, IM_PI);
        float a = radius;
        float b = radius / 2.f; 
        const float bg_angle_offset = PI_2 / (num_segments - 1);
        ImColor c = color_alpha(color, tr);
        window->DrawList->PathClear();
        for (int i = 0; i < num_segments; ++i) {
            float anga = (i * bg_angle_offset);
            float xx = a * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + centre.x;
            float yy = b * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + centre.y + y;
            window->DrawList->PathLineTo({xx, yy});
        }
        window->DrawList->PathStroke(c, false, thickness);
    };

    for (int i = 0; i < elipses; ++i)
    {
        const float h = (0.5f * ImSin(start + (IM_PI / elipses) * i));
        draw_rotated_ellipse(0.f, 0.1f + (0.9f / elipses) * i, radius * h);
    }
}

void ImGui::SpinnerPatternEclipse(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses, float delta_a, float delta_y)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime()* speed;
    elipses = std::max<int>(elipses, 1);
    auto draw_rotated_ellipse = [&] (const ImVec2 &pp, float alpha, float tr, float r, float x, float y) {
        alpha = ImFmod(alpha, IM_PI);
        float a = r;
        float b = r / delta_a; 
        ImColor c = color_alpha(color, tr);
        window->DrawList->PathClear();
        for (int i = 0; i < num_segments; ++i) {
            float anga = (i *  PI_2 / (num_segments - 1));
            float xx = a * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + pp.x + x;
            float yy = b * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + pp.y + y;
            window->DrawList->PathLineTo({xx, yy});
        }
        window->DrawList->PathStroke(c, false, thickness);
    };
    for (int i = 0; i < elipses; ++i)
    {
        const float rkoeff = (0.5f + 0.5f * ImSin(start + (IM_PI / elipses) * i));
        const float h = ((1.f / elipses) * (i+1));
        const float anga = start + (i *  PI_DIV_2 / elipses);
        const float yoff = ((1.f / elipses) * i) * delta_y;
        float a = (radius * (1.f - h));
        float b = (radius * (1.f - h)) / delta_a; 
        float xx = a * ImCos(anga) * ImCos(0) + b * ImSin(anga) * ImSin(0) + centre.x;
        float yy = b * ImSin(anga) * ImCos(0) - a * ImCos(anga) * ImSin(0) + centre.y;
        draw_rotated_ellipse(ImVec2(xx, yy), 0.f, 0.3f + (0.7f / elipses) * i, radius * h, 0.f, yoff * radius);
    }
}

void ImGui::SpinnerPatternSphere(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed * 3.f, size.y);
    elipses = std::max<int>(elipses, 1);
    auto draw_rotated_ellipse = [&] (float alpha, float tr, float y, float r) {
        alpha = ImFmod(alpha, IM_PI);
        float a = r;
        float b = r / 4.f; 
        ImColor c = color_alpha(color, tr);
        window->DrawList->PathClear();
        for (int i = 0; i < num_segments; ++i) {
            float anga = (i * PI_2 / (num_segments - 1));
            float xx = a * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + centre.x;
            float yy = b * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + pos.y + y;
            window->DrawList->PathLineTo({xx, yy});
        }
        window->DrawList->PathStroke(c, false, thickness);
    };
    float offset = 0;
    float half_size = size.y * 0.5f;
    for (size_t i = 0; i < elipses; i++)
    {
        offset = ImFmod(start + i * (size.y / elipses), size.y);
        const float th = (offset > half_size)
                            ? 1.f - (offset - half_size) / half_size
                            : offset / half_size;
        draw_rotated_ellipse(0.f, th, offset, ImSin(offset / size.y * IM_PI) * radius);
    }
}

void ImGui::SpinnerRingSynchronous(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    num_segments *= 4;
    const float aoffset = ImFmod((float)ImGui::GetTime(), PI_2);
    const float bofsset = (aoffset > IM_PI) ? IM_PI : aoffset;
    const float angle_offset = PI_2 / num_segments;
    float ared_min = 0, ared = 0;
    if (aoffset > IM_PI)
        ared_min = aoffset - IM_PI;
    auto draw_ellipse = [&] (float alpha, float y, float r) {
        window->DrawList->PathClear();
        for (size_t i = 0; i <= num_segments + 1; i++)
        {
            ared = start + (i * angle_offset);
            if (i * angle_offset < ared_min * 2)
                continue;
            if (i * angle_offset > bofsset * 2.f)
                break;
            float a = r;
            float b = r * 0.25f;
            const float xx = a * ImCos(ared) * ImCos(alpha) + b * ImSin(ared) * ImSin(alpha) + centre.x;
            const float yy = b * ImSin(ared) * ImCos(alpha) - a * ImCos(ared) * ImSin(alpha) + pos.y + y;
            window->DrawList->PathLineTo(ImVec2(xx, yy));
        }
        window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
    };
    for (int i = 0; i < elipses; ++i)
    {
        float y = i * ((float)(size.y * 0.7f) / (float)elipses) + (size.y * 0.15f);
        draw_ellipse(0, y, radius * ImSin((i + 1) * (IM_PI / (elipses + 1))));
    }
}

void ImGui::SpinnerRingWatermarks(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    num_segments *= 4;
    const float angle_offset = PI_2 / num_segments;
    auto draw_ellipse = [&] (float s, float alpha, float x, float y, float r) {
        const float aoffset = ImFmod((float)s, PI_2);
        const float bofsset = (aoffset > IM_PI) ? IM_PI : aoffset;
        float ared_min = 0, ared = 0;
        if (aoffset > IM_PI)
            ared_min = aoffset - IM_PI;
        window->DrawList->PathClear();
        for (size_t i = 0; i <= num_segments + 1; i++)
        {
            ared = s + (i * angle_offset);
            if (i * angle_offset < ared_min * 2)
                continue;
            if (i * angle_offset > bofsset * 2.f)
                break;
            float a = r;
            float b = r * 0.25f;
            const float xx = a * ImCos(ared) * ImCos(alpha) + b * ImSin(ared) * ImSin(alpha) + centre.x + x;
            const float yy = b * ImSin(ared) * ImCos(alpha) - a * ImCos(ared) * ImSin(alpha) + pos.y + y;
            window->DrawList->PathLineTo(ImVec2(xx, yy));
        }
        window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
    };
    for (int i = 0; i < elipses; ++i)
    {
        float y = i * ((float)(size.y * 0.7f) / (float)elipses) + (size.y * 0.15f);
        float x = -i * (radius / elipses);
        draw_ellipse(start + (i * IM_PI / (elipses * 2)), -PI_DIV_4, x, y, radius);
    }
}

void ImGui::SpinnerRotatedAtom(const char *label, float radius, float thickness, const ImColor &color, float speed, int elipses, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime()* speed;
    auto draw_rotated_ellipse = [&] (float alpha) {
        std::array<ImVec2, 36> pts;

        alpha = ImFmod(alpha, IM_PI);
        float a = radius;
        float b = radius / 2.f; 

        const float bg_angle_offset = PI_2 / num_segments;
        for (size_t i = 0; i < num_segments; ++i) {
            float anga = (i * bg_angle_offset);
            float h = ease((ease_mode)mode, start, radius);

            pts[i].x = (a + h) * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + centre.x;
            pts[i].y = (b + h) * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + centre.y;
        }
        for (size_t i = 1; i < num_segments; ++i) {
            window->DrawList->AddLine(pts[i-1], pts[i], color_alpha(color, 1.f), thickness);
        }
        window->DrawList->AddLine(pts[num_segments-1], pts[0], color_alpha(color, 1.f), thickness);
    };

    for (int i = 0; i < elipses; ++i) {
        draw_rotated_ellipse(start + (IM_PI * (float)i/ elipses));
    }
}

void ImGui::SpinnerRainbowBalls(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = ImFmod((float)ImGui::GetTime() * speed * 3.f, IM_PI);
    const float colorback = 0.3f + 0.2f * ImSin((float)ImGui::GetTime() * speed);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float radius1 = (0.7f + 0.2f * ImSin(start)) * radius;
    const float angle_offset = PI_2 / balls;
    const bool rainbow = ((ImU32)color.Value.w) == 0;

    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i < balls; i++)
    {
        const float a = rstart + (i * angle_offset);
        ImColor c = rainbow ? ImColor::HSV(out_h + i * (1.f / balls) + colorback, out_s, out_v) : color;
        float rb = radius1 + ease((ease_mode)mode, start, radius1);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * rb, centre.y + ImSin(a) * rb), thickness, color_alpha(c, 1.f), num_segments);
    }
}

void ImGui::SpinnerRainbowShot(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed * 3.f, PI_2);
    const float colorback = 0.3f + 0.2f * ImSin((float)ImGui::GetTime() * speed);
    const float rstart = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float angle_offset = PI_2 / balls;
    const bool rainbow = ((ImU32)color.Value.w) == 0;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i <= balls; i++)
    {
        float centera = start - PI_DIV_2 + (i * angle_offset);
        float rmul = ImMax(0.2f, 1.f - ImSin(centera));
        const float radius1 = ImMin(radius * rmul, radius);
        const float a = (i * angle_offset);
        ImColor c = rainbow ? ImColor::HSV(out_h + i * (1.f / balls) + colorback, out_s, out_v) : color;
        window->DrawList->AddLine(centre, ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1), color_alpha(c, 1.f), thickness);
    }
}

void ImGui::SpinnerSpiral(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    float a = radius / num_segments;
    float b = a;
    ImVec2 last = centre;
    for (size_t arc_num = 0; arc_num < (num_segments * arcs); ++arc_num)
    {
        float angle = (PI_2 / num_segments) * arc_num;
        float x = centre.x + (a + b * angle) * ImCos(start + angle);
        float y = centre.y + (a + b * angle) * ImSin(start + angle);
        window->DrawList->AddLine(last, ImVec2(x, y), color_alpha(color, 1.f), thickness);
        last = ImVec2(x, y);
    }
}

void ImGui::SpinnerSpiralEye(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    float a = (radius * 3.f) / num_segments;
    float b = a;
    num_segments *= 4;
    auto half_eye = [&] (float side) {
        for (size_t arc_num = 0; arc_num < num_segments; ++arc_num)         {
            float angle = (PI_2 / num_segments) * arc_num;
            float x = centre.x + (a + b * angle) * ImCos((start + angle) * side);
            float y = centre.y + (a + b * angle) * ImSin((start + angle) * side);
            window->DrawList->AddCircleFilled(ImVec2(x, y), thickness, color_alpha(color, 1.f));
        }
    };
    half_eye(1.f);
    half_eye(-1.f);
}

void ImGui::SpinnerBarChartSine(const char *label, float radius, float thickness, const ImColor &color, float speed, int bars, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const ImGuiStyle &style = GImGui->Style;
    const float nextItemKoeff = 1.5f;
    const float yOffsetKoeftt = 0.8f;
    const float heightSpeed = 0.8f;

    const float start = (float)ImGui::GetTime() * speed;
    const float offset = IM_PI / bars;
    for (int i = 0; i < bars; i++)
    {
        const float angle = ImMax(PI_DIV_2, (1.f - i/(float)bars) * IM_PI);
        float a = start + (IM_PI - i * offset);
        ImColor c = color_alpha(color, ImMax(0.1f, ImSin(a * heightSpeed)));
        float h = mode ? ImSin(a) * size.y / 2.f
                       : (0.6f + 0.4f * c.Value.w) * size.y;
        float halfs = mode ? 0 : size.y / 2.f;
        window->DrawList->AddRectFilled(ImVec2(pos.x + style.FramePadding.x + i * (thickness * nextItemKoeff) - thickness / 2, centre.y + halfs),
                                        ImVec2(pos.x + style.FramePadding.x + i * (thickness * nextItemKoeff) + thickness / 2, centre.y + halfs - h * yOffsetKoeftt),
                                        c);
    }
}

void ImGui::SpinnerBarChartAdvSine(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float nextItemKoeff = 1.5f;
    const float start = (float)ImGui::GetTime() * speed;
    const int bars = radius * 2 / thickness;
    const float offset = PI_DIV_2 / bars;
    for (int i = 0; i < bars; i++)
    {
        float a = start + (PI_DIV_2 - i * offset);
        float halfsx = thickness * ImSin(a);
        float halfsy = (ImMax(0.1f, ImSin(a) + 1.f)) * radius * 0.5f;
        window->DrawList->AddRectFilled(ImVec2(pos.x + i * (thickness * nextItemKoeff) - thickness / 2 + halfsx, centre.y + halfsy),
                                        ImVec2(pos.x + i * (thickness * nextItemKoeff) + thickness / 2 + halfsx, centre.y - halfsy),
                                        color);
    }
}

void ImGui::SpinnerBarChartAdvSineFade(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const int bars = radius * 2 / thickness;
    const float offset = PI_DIV_2 / bars;
    for (int i = 0; i < bars; i++)
    {
        float a = start - i * offset;
        float halfsy = ImMax(0.1f, ImCos(a) + 1.f) * radius * 0.5f;
        window->DrawList->AddRectFilled(ImVec2(pos.x + i * thickness - thickness / 2, centre.y + halfsy),
                                        ImVec2(pos.x + i * thickness + thickness / 2, centre.y - halfsy),
                                        color_alpha(color, ImMax(0.1f, halfsy / radius)));
    }
}

void ImGui::SpinnerBarChartRainbow(const char *label, float radius, float thickness, const ImColor &color, float speed, int bars, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const ImGuiStyle &style = GImGui->Style;
    const float nextItemKoeff = 1.5f;
    const float yOffsetKoeftt = 0.8f;
    const float start = (float)ImGui::GetTime() * speed;

    const float hspeed = 0.1f + ImSin((float)ImGui::GetTime() * 0.1f) * 0.05f;
    constexpr float rkoeff[6] = {4.f, 13.f, 3.4f, 8.7f, 25.f, 11.f};
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i < bars; i++)
    {
        ImColor c = ImColor::HSV(out_h + i * 0.1f, out_s, out_v);
        float h = (0.6f + 0.4f * ImSin(start + (1.f + rkoeff[i % 6] * i * hspeed)) ) * size.y;
        h += ease((ease_mode)mode, start, radius);
        window->DrawList->AddRectFilled(ImVec2(pos.x + style.FramePadding.x + i * (thickness * nextItemKoeff) - thickness / 2, centre.y + size.y / 2.f),
                                        ImVec2(pos.x + style.FramePadding.x + i * (thickness * nextItemKoeff) + thickness / 2, centre.y + size.y / 2.f - h * yOffsetKoeftt),
                                        color_alpha(c, 1.f));
    }
}

void ImGui::SpinnerBlocks(const char *label, float radius, float thickness, const ImColor &bg, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImVec2 lt{centre.x - radius, centre.y - radius};
    const float offset_block = radius * 2.f / 3.f;
    int start = (int)ImFmod((float)ImGui::GetTime() * speed, 8.f);
    const ImVec2ih poses[] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}};
    int ti = 0;
    for (const auto &rpos: poses)
    {
        const ImColor &c = (ti == start) ? color : bg;
        window->DrawList->AddRectFilled(ImVec2(lt.x + rpos.x * (offset_block), lt.y + rpos.y * offset_block),
                                        ImVec2(lt.x + rpos.x * (offset_block) + thickness, lt.y + rpos.y * offset_block + thickness),
                                        color_alpha(c, 1.f));
        ti++;
    }
}

void ImGui::SpinnerTwinBlocks(const char *label, float radius, float thickness, const ImColor &bg, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float offset_block = radius * 2.f / 3.f;
    ImVec2 lt{centre.x - radius - offset_block / 2.f, centre.y - radius - offset_block / 2.f};
    int start = (int)ImFmod((float)ImGui::GetTime() * speed, 8.f);
    const ImVec2ih poses[] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}};
    int ti = 0;
    for (const auto &rpos: poses)
    {
        const ImColor &c = (ti == start) ? color : bg;
        window->DrawList->AddRectFilled(ImVec2(lt.x + rpos.x * (offset_block), lt.y + rpos.y * offset_block),
                                        ImVec2(lt.x + rpos.x * (offset_block) + thickness, lt.y + rpos.y * offset_block + thickness),
                                        color_alpha(c, 1.f));
        ti++;
    }
    lt = ImVec2{centre.x - radius + offset_block / 2.f, centre.y - radius + offset_block / 2.f};
    ti = 7;//std::size(poses) - 1;
    start = (int)ImFmod((float)ImGui::GetTime() * speed * 1.1f, 8.f);
    for (const auto &rpos: poses)
    {
        const ImColor &c = (ti == start) ? color : bg;
        window->DrawList->AddRectFilled(ImVec2(lt.x + rpos.x * (offset_block), lt.y + rpos.y * offset_block),
                                        ImVec2(lt.x + rpos.x * (offset_block) + thickness, lt.y + rpos.y * offset_block + thickness),
                                        color_alpha(c, 1.f));
        ti--;
    }
}

void ImGui::SpinnerSquareRandomDots(const char *label, float radius, float thickness, const ImColor &bg, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float offset_block = radius * 2.f / 3.f;
    ImVec2 lt{centre.x - offset_block, centre.y - offset_block};
    int start = (int)ImFmod((float)ImGui::GetTime() * speed, 9.f);
    ImGuiStorage* storage = window->DC.StateStorage;
    const ImGuiID vtimeId = window->GetID("##vtime");
    const ImGuiID vvald = window->GetID("##vval");
    int vtime = storage->GetInt(vtimeId, 0);
    int vval = storage->GetInt(vvald, 0);
    if (vtime != start) {
        vval = rand() % 9;
        storage->SetInt(vvald, vval);
        storage->SetInt(vtimeId, start);
    }
    const ImVec2ih poses[] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}, {1, 1}};
    int ti = 0;
    for (const auto &rpos: poses)
    {
        const ImColor &c = (ti == vval) ? color : bg;
        window->DrawList->AddCircleFilled(ImVec2(lt.x + rpos.x * (offset_block), lt.y + rpos.y * offset_block), thickness,
                                        color_alpha(c, 1.f));
        ti++;
    }
}

void ImGui::SpinnerScaleBlocks(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    ImVec2 lt{centre.x - radius, centre.y - radius};
    const float offset_block = radius * 2.f / 3.f;

    const ImVec2ih poses[] = {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
    constexpr float rkoeff[9] = {0.1f, 0.15f, 0.17f, 0.25f, 0.6f, 0.15f, 0.1f, 0.12f, 0.22f};

    int ti = 0;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (const auto &rpos: poses)
    {
        ImColor c = ImColor::HSV(out_h + ti * 0.1f, out_s, out_v);
        if (mode) {
            float h = (0.1f + 0.4f * ImSin((float)ImGui::GetTime() * (speed * rkoeff[ti % 9])));
            window->DrawList->AddCircleFilled(ImVec2(lt.x + rpos.x * (offset_block), lt.y + rpos.y * offset_block), std::max<float>(1.f, h * thickness),
                                            color_alpha(c, 1.f));
        } else {
            float h = (0.8f + 0.4f * ImSin((float)ImGui::GetTime() * (speed * rkoeff[ti % 9])));
            window->DrawList->AddRectFilled(ImVec2(lt.x + rpos.x * (offset_block), lt.y + rpos.y * offset_block),
                                            ImVec2(lt.x + rpos.x * (offset_block) + h * thickness, lt.y + rpos.y * offset_block + h * thickness),
                                            color_alpha(c, 1.f));
        }
        ti++;
    }
}

void ImGui::SpinnerScaleSquares(const char *label, float radius, float thikness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    ImVec2 lt{centre.x - radius, centre.y - radius};
    const float offset_block = radius * 2.f / 3.f;
    const float hside = (thikness / 2.f);
    const ImVec2ih poses[] = {{0, 0}, {1, 0}, {0, 1}, {2, 0}, {1, 1}, {0, 2}, {2, 1}, {1, 2}, {2, 2}};
    const float offsets[] =  {0.f,    0.8f,   0.8f,   1.6f,   1.6f,   1.6f,   2.4f,   2.4f,   3.2f};
    int ti = 0;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (const auto &rpos: poses)
    {
        const ImColor c = ImColor::HSV(out_h + offsets[ti], out_s, out_v);
        const float strict = (0.5f + 0.5f * ImSin((float)-ImGui::GetTime() * speed + offsets[ti % 9]));
        const float side = ImClamp<float>(strict + 0.1f, 0.1f, 1.f) * hside;
        window->DrawList->AddRectFilled(ImVec2(lt.x + hside + (rpos.x * offset_block) - side, lt.y + hside + (rpos.y * offset_block) - side),
                                        ImVec2(lt.x + hside + (rpos.x * offset_block) + side, lt.y + hside + (rpos.y * offset_block) + side),
                                        color_alpha(c, 1.f));
        ti++;
    }
}

void ImGui::SpinnerSquishSquare(const char *label, float radius, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    
    float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float side = ImSin((float)-start) * radius;
    bool type = (start > IM_PI) ? 1 : 0;
    if (type) {
        if (start > IM_PI && start < IM_PI + PI_DIV_2) {
            window->DrawList->AddRectFilled(ImVec2(centre.x - side, centre.y - radius), ImVec2(centre.x + side, centre.y + radius), color_alpha(color, 1.f));
        } else {
            window->DrawList->AddRectFilled(ImVec2(centre.x - radius, centre.y - side), ImVec2(centre.x + radius, centre.y + side), color_alpha(color, 1.f));
        }
    } else {
        if (start < PI_DIV_2) {
            window->DrawList->AddRectFilled(ImVec2(centre.x - radius, centre.y - side), ImVec2(centre.x + radius, centre.y + side), color_alpha(color, 1.f));
        } else {
            window->DrawList->AddRectFilled(ImVec2(centre.x - side, centre.y - radius), ImVec2(centre.x + side, centre.y + radius), color_alpha(color, 1.f));
        }
    }
}

void ImGui::SpinnerFluid(const char *label, float radius, const ImColor &color, float speed, int bars)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const ImGuiStyle &style = GImGui->Style;
    const float hspeed = 0.1f + ImSin((float)ImGui::GetTime() * 0.1f) * 0.05f;
    constexpr float rkoeff[6][3] = {{0.15f, 0.1f, 0.1f}, {0.033f, 0.15f, 0.8f}, {0.017f, 0.25f, 0.6f}, {0.037f, 0.1f, 0.4f}, {0.25f, 0.1f, 0.3f}, {0.11f, 0.1f, 0.2f}};
    const float j_k = radius * 2.f / num_segments;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i < bars; i++)
    {
        ImColor c = color_alpha(ImColor::HSV(out_h - i * 0.1f, out_s, out_v), rkoeff[i % 6][1]);
        for (int j = 0; j < num_segments; ++j) {
            float h = (0.6f + 0.3f * ImSin((float)ImGui::GetTime() * (speed * rkoeff[i % 6][2] * 2.f) + (2.f * rkoeff[i % 6][0] * j * j_k))) * (radius * 2.f * rkoeff[i % 6][2]);
            window->DrawList->AddRectFilled(ImVec2(pos.x + style.FramePadding.x + j * j_k, centre.y + size.y / 2.f),
                                            ImVec2(pos.x + style.FramePadding.x + (j + 1) * (j_k), centre.y + size.y / 2.f - h),
                                            c);
        }
    }
}

void ImGui::SpinnerFluidPoints(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots, float delta)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const ImGuiStyle &style = GImGui->Style;
    const float rkoeff[3] = {0.033f, 0.3f, 0.8f};
    const float hspeed = 0.1f + ImSin((float)ImGui::GetTime() * 0.1f) * 0.05f;
    const float j_k = radius * 2.f / num_segments;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int j = 0; j < num_segments; ++j) {
        float h = (0.6f + delta * ImSin((float)ImGui::GetTime() * (speed * rkoeff[2] * 2.f) + (2.f * rkoeff[0] * j * j_k))) * (radius * 2.f * rkoeff[2]);
        for (int i = 0; i < dots; i++) {
            ImColor c = color_alpha(ImColor::HSV(out_h - i * 0.1f, out_s, out_v), 1.f);
            window->DrawList->AddCircleFilled(ImVec2(pos.x + style.FramePadding.x + j * j_k, centre.y + size.y / 2.f - (h / dots) * i), thickness, c);
        }
    }
}

void ImGui::SpinnerArcPolarFade(const char *label, float radius, const ImColor &color, float speed, size_t arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;
    constexpr float rkoeff[6][3] = {{0.15f, 0.1f, 0.1f}, {0.033f, 0.15f, 0.8f}, {0.017f, 0.25f, 0.6f}, {0.037f, 0.1f, 0.4f}, {0.25f, 0.1f, 0.3f}, {0.11f, 0.1f, 0.2f}};
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        const float b = arc_angle * arc_num - PI_DIV_2 - PI_DIV_4;
        const float e = arc_angle * arc_num + arc_angle - PI_DIV_2 - PI_DIV_4;
        const float a = arc_angle * arc_num;
        float h = (0.6f + 0.3f * ImSin((float)ImGui::GetTime() * (speed * rkoeff[arc_num % 6][2] * 2.f) + (2 * rkoeff[arc_num % 6][0])));
        ImColor c = color_alpha(color, h);
        c.Value.w += ease((ease_mode)mode, h, arc_angle);

        window->DrawList->PathClear();
        window->DrawList->PathLineTo(centre);
        for (size_t i = 0; i <= num_segments + 1; i++)
        {
            const float ar = arc_angle * arc_num + (i * angle_offset) - PI_DIV_2 - PI_DIV_4;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar) * radius, centre.y + ImSin(ar) * radius));
        }
        window->DrawList->PathFillConvex(c);
    }
}

void ImGui::SpinnerArcPolarRadius(const char *label, float radius, const ImColor &color, float speed, size_t arcs)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;
    constexpr float rkoeff[6][3] = {{0.15f, 0.1f, 0.41f}, {0.033f, 0.15f, 0.8f}, {0.017f, 0.25f, 0.6f}, {0.037f, 0.1f, 0.4f}, {0.25f, 0.1f, 0.3f}, {0.11f, 0.1f, 0.2f}};
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
    {
        const float b = arc_angle * arc_num - PI_DIV_2 - PI_DIV_4;
        const float e = arc_angle * arc_num + arc_angle - PI_DIV_2 - PI_DIV_4;
        const float a = arc_angle * arc_num;
        float r = (0.6f + 0.3f * ImSin((float)ImGui::GetTime() * (speed * rkoeff[arc_num % 6][2] * 2.f) + (2.f * rkoeff[arc_num % 6][0])));

        window->DrawList->PathClear();
        window->DrawList->PathLineTo(centre);
        ImColor c = ImColor::HSV(out_h + arc_num * 0.31f, out_s, out_v);
        for (size_t i = 0; i <= num_segments + 1; i++)
        {
            const float ar = arc_angle * arc_num + (i * angle_offset) - PI_DIV_2 - PI_DIV_4;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ar) * (radius * r), centre.y + ImSin(ar) * (radius * r)));
        }
        window->DrawList->PathFillConvex(color_alpha(c, 1.f));
    }
}

void ImGui::SpinnerCaleidoscope(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    float start = (float)ImGui::GetTime() * speed;
    float astart = ImFmod(start, PI_2 / arcs);
    start -= astart;
    const float angle_offset = PI_2 / arcs;
    const float angle_offset_t = angle_offset * 0.3f;
    arcs = ImMin<size_t>(arcs, 32);

    auto get_points = [&] (float left, float right, float r) -> std::array<ImVec2, 4> {
        const float rmin = r - thickness;
        return {
            ImVec2(centre.x + ImCos(left) * rmin, centre.y + ImSin(left) * rmin),
            ImVec2(centre.x + ImCos(left) * r, centre.y + ImSin(left) * r),
            ImVec2(centre.x + ImCos(right) * r, centre.y + ImSin(right) * r),
            ImVec2(centre.x + ImCos(right) * rmin, centre.y + ImSin(right) * rmin)
        };
    };

    auto draw_sectors = [&] (float s, const std::function<ImColor (size_t)>& color_func, float r) {
        for (size_t i = 0; i <= arcs; i++) {
            float left = s + (i * angle_offset) - angle_offset_t;
            float right = s + (i * angle_offset) + angle_offset_t;
            auto points = get_points(left, right, r);
            window->DrawList->AddConvexPolyFilled(points.data(), 4, color_alpha(color_func(i), 1.f));
        }
    };

    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    draw_sectors(start, [&] (size_t i) { return ImColor::HSV(out_h + i * 0.31f, out_s, out_v); }, radius);
    switch (mode) {
        case 0: draw_sectors(-start * 0.78f, [&] (size_t i) { return ImColor::HSV(out_h + i * 0.31f, out_s, out_v); }, radius - thickness - 2); break;
        case 1:
        {
            ImColor c = color;
            float lerp_koeff = (ImSin((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
            c.Value.w = ImMax(0.1f, ImMin(lerp_koeff, 1.f));
            float dr = radius - thickness - 3;
            window->DrawList->AddCircleFilled(centre, dr, c, num_segments);
        } 
        break;
    }
}

// spinner idea by nitz 'Chris Dailey'
void ImGui::SpinnerHboDots(const char *label, float radius, float thickness, const ImColor &color, float minfade, float ryk, float speed, size_t dots)
{
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime() * speed;

    for (size_t i = 0; i < dots; i++)
    {
        const float astart = start + PI_2_DIV(dots) * i;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(astart) * radius, centre.y + ryk * ImCos(astart) * radius), thickness,
                                            color_alpha(color, ImMax(minfade, ImSin(astart + PI_DIV_2))),
                                            8);
    }
}

void ImGui::SpinnerMoonDots(const char *label, float radius, float thickness, const ImColor &first, const ImColor &second, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    const float astart = ImFmod(start, IM_PI * 2.f);
    const float bstart = astart + IM_PI;
    const float sina = ImSin(astart);
    const float sinb = ImSin(bstart);
    if (astart < PI_DIV_2 || astart > IM_PI + PI_DIV_2) {
        window->DrawList->AddCircleFilled(ImVec2(centre.x + sina * thickness, centre.y), thickness, color_alpha(first, 1.f), 16);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + sinb * thickness, centre.y), thickness, color_alpha(second, 1.f), 16);
        window->DrawList->AddCircle(ImVec2(centre.x + sinb * thickness, centre.y), thickness, color_alpha(first, 1.f), 16);
    } else {
        window->DrawList->AddCircleFilled(ImVec2(centre.x + sinb * thickness, centre.y), thickness, color_alpha(second, 1.f), 16);
        window->DrawList->AddCircle(ImVec2(centre.x + sinb * thickness, centre.y), thickness, color_alpha(first, 1.f), 16);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + sina * thickness, centre.y), thickness, color_alpha(first, 1.f), 16);
    }
}

void ImGui::SpinnerTwinHboDots(const char *label, float radius, float thickness, const ImColor &color, float minfade, float ryk, float speed, size_t dots, float delta)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    for (size_t i = 0; i < dots; i++)
    {
        const float astart = start + PI_2_DIV(dots) * i;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(astart) * radius, centre.y + ryk * ImCos(astart) * radius + radius * delta), thickness,
                                          color_alpha(color, ImMax(minfade, ImSin(astart + PI_DIV_2))),
                                          8);
    }
    for (size_t i = 0; i < dots; i++)
    {
        const float astart = start + PI_2_DIV(dots) * i;
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(astart) * radius, centre.y - ryk * ImCos(astart) * radius - radius * delta), thickness,
                                          color_alpha(color, ImMax(minfade, ImSin(astart + PI_DIV_2))),
                                          8);
    }
}

void ImGui::SpinnerThreeDotsStar(const char *label, float radius, float thickness, const ImColor &color, float minfade, float ryk, float speed, float delta)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(-start) * radius, centre.y - ryk * ImCos(-start) * radius + radius * delta), thickness, color_alpha(color, ImMax(minfade, ImSin(-start + PI_DIV_2))), 8);
    window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(start) * radius, centre.y - ryk * ImCos(start) * radius - radius * delta), thickness, color_alpha(color, ImMax(minfade, ImSin(start + PI_DIV_2))), 8);
    window->DrawList->AddCircleFilled(ImVec2(centre.x + ImSin(start + PI_DIV_4) * radius, centre.y - ryk * ImCos(start + PI_DIV_4) * radius - radius * delta), thickness, color_alpha(color, ImMax(minfade, ImSin(start + PI_DIV_4 + PI_DIV_2))), 8);
}

void ImGui::SpinnerSineArcs(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    float length = ImFmod(start, IM_PI);
    const float dangle = ImSin(length) * IM_PI * 0.35f;
    const float angle_offset = IM_PI / num_segments;
    auto draw_spring = [&] (float k) {
        float arc = 0.f;
        window->DrawList->PathClear();
        for (size_t i = 0; i < num_segments; i++) {
            float a = start + (i * angle_offset);
            if (ImSin(a) < 0.f)
                a *= -1;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius, centre.y + k * ImSin(a) * radius));
            arc += angle_offset;
            if (arc > dangle)
                break;
        }
        window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
    };
    draw_spring(1);
    draw_spring(-1);
}

void ImGui::SpinnerTrianglesShift(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, size_t bars) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    //ImColor c = color;
    //float lerp_koeff = (ImSin((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
    //c.Value.w = ImMax(0.1f, ImMin(lerp_koeff, 1.f));
    const float angle_offset = PI_2 / bars;
    float start = (float)ImGui::GetTime() * speed;
    const float astart = ImFmod(start, angle_offset);
    const float save_start = start;
    start -= astart;
    const float angle_offset_t = angle_offset * 0.3f;
    bars = ImMin<size_t>(bars, 32);
    const float rmin = radius - thickness;
    auto get_points = [&] (float left, float right, float r1, float r2) -> std::array<ImVec2, 4> {
        return {
            ImVec2(centre.x + ImCos(left) * r1, centre.y + ImSin(left) * r1),
            ImVec2(centre.x + ImCos(left) * r2, centre.y + ImSin(left) * r2),
            ImVec2(centre.x + ImCos(right) * r2, centre.y + ImSin(right) * r2),
            ImVec2(centre.x + ImCos(right) * r1, centre.y + ImSin(right) * r1)
        };
    };
    ImColor rc = bg;
    for (size_t i = 0; i < bars; i++) {
        float left = start + (i * angle_offset) - angle_offset_t;
        float right = start + (i * angle_offset) + angle_offset_t;
        float centera = start - PI_DIV_2 + (i * angle_offset);
        float rmul = 1.f - ImClamp(ImAbs(centera - save_start), 0.f, PI_DIV_2) / PI_DIV_2;
        rc.Value.w = ImMax(rmul, 0.1f);
        rmul *= 1.5f;
        rmul = ImMax(0.5f, rmul);
        const float r1 = ImMax(rmin * rmul, rmin);
        const float r2 = ImMax(radius * rmul, radius);
        auto points = get_points(left, right, r1, r2);
        window->DrawList->AddConvexPolyFilled(points.data(), 4, rc);
    }
}

void ImGui::SpinnerPointsShift(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, size_t bars) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    //ImColor c = color;
    //float lerp_koeff = (ImSin((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
    //c.Value.w = ImMax(0.1f, ImMin(lerp_koeff, 1.f));
    const float angle_offset = PI_2 / bars;
    float start = (float)ImGui::GetTime() * speed;
    const float astart = ImFmod(start, angle_offset);
    const float save_start = start;
    start -= astart;
    const float angle_offset_t = angle_offset * 0.3f;
    bars = ImMin<size_t>(bars, 32);
    const float rmin = radius - thickness;
    ImColor rc = bg;
    for (size_t i = 0; i < bars; i++) {
        float left = start + (i * angle_offset) - angle_offset_t;
        float centera = start - PI_DIV_2 + (i * angle_offset);
        float rmul = 1.f - ImClamp(ImAbs(centera - save_start), 0.f, PI_DIV_2) / PI_DIV_2;
        rc.Value.w = ImMax(rmul, 0.1f);
        rmul *= 1.f + ImSin(rmul * IM_PI);
        const float r = ImMax(radius * rmul, radius);
        window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(left) * r, centre.y + ImSin(left) * r), thickness, rc, num_segments);
    }
}

void ImGui::SpinnerSwingDots(const char *label, float radius, float thickness, const ImColor &color, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = (float)ImGui::GetTime() * speed;
    constexpr int elipses = 2;
    auto get_rotated_ellipse_pos = [&] (float alpha, float start) {
        std::array<ImVec2, 36> pts;
        alpha = ImFmod(alpha, IM_PI);
        float a = radius;
        float b = radius / 10.f; 
        float anga = ImFmod(start, PI_2);
        float x = a * ImCos(anga) * ImCos(alpha) + b * ImSin(anga) * ImSin(alpha) + centre.x;
        float y = b * ImSin(anga) * ImCos(alpha) - a * ImCos(anga) * ImSin(alpha) + centre.y;
        return ImVec2{x, y};
    };
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int i = 0; i < elipses; ++i) {
        ImVec2 ppos = get_rotated_ellipse_pos((IM_PI * (float)i/ elipses) + PI_DIV_4, start + PI_DIV_2 * i);
        const float y_delta = ImAbs(centre.y - ppos.y);
        float th_koeff = ImMax((y_delta / size.y) * 4.f, 0.5f);
        window->DrawList->AddCircleFilled(ppos, th_koeff * thickness, color_alpha(ImColor::HSV(out_h + i * 0.5f, out_s, out_v), 1.f), num_segments);
    }
}

void ImGui::SpinnerCircularPoints(const char *label, float radius, float thickness, const ImColor &color, float speed, int lines)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, radius);
    const float bg_angle_offset = (PI_2) / lines;
    for (size_t j = 0; j < 3; ++j)
    {
        const float start_offset = j * radius / 3.f;
        const float rmax = ImFmod((start + start_offset), radius);
        ImColor c = color_alpha(color, ImSin((radius - rmax) / radius * IM_PI));
        for (size_t i = 0; i < lines; i++)
        {
            float a = (i * bg_angle_offset);
            window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * rmax, centre.y + ImSin(a) * rmax), thickness, c, num_segments);
        }
    }
}

void ImGui::SpinnerCurvedCircle(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t circles) 
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    const float bg_angle_offset = PI_2 / num_segments;
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    for (int j = 0; j < circles; j++)
    {
        window->DrawList->PathClear();
        const float rr = radius - ((radius * 0.5f) / circles) * j;
        const float start_a = start * (1.1f * (j+1));
        for (size_t i = 0; i <= num_segments; i++)
        {
            const float a = start_a + (i * bg_angle_offset);
            const float r = rr - (0.2f * (i % 2)) * rr;
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * r, centre.y + ImSin(a) * r));
        }
        window->DrawList->PathStroke(color_alpha(ImColor::HSV(out_h + (j * 1.f / circles), out_s, out_v), 1.f), false, thickness);
    }
}

void ImGui::SpinnerModCircle(const char *label, float radius, float thickness, const ImColor &color, float ang_min, float ang_max, float speed)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    window->DrawList->PathClear();
    for (size_t i = 0; i <= 90; i++)
    {
        const float ax = ((i / 90.f) * PI_2 * ang_min);
        const float ay = ((i / 90.f) * PI_2 * ang_max);
        window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(ax) * radius, centre.y + ImSin(ay) * radius));
    }
    window->DrawList->PathStroke(color_alpha(color, 1.f), false, thickness);
    start = (start < IM_PI) ? (start * 2.f) : (PI_2 - start) * 2.f;
    window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(start * ang_min) * radius, centre.y + ImSin(start * ang_max) * radius), thickness * 4.f, color_alpha(color, 1.f), num_segments);
}

void ImGui::SpinnerDnaDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt, float delta, bool mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 2.5f;
    const float dots = (size.x / (thickness * nextItemKoeff));
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    float out_h, out_s, out_v;
    ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, out_h, out_s, out_v);
    auto draw_point = [&] (float angle, int i) {
        float a = angle + start + (IM_PI - i * PI_DIV(dots));
        float th_koeff = 1.f + ImSin(a + PI_DIV_2) * 0.5f;
        float pp = mode ? centre.x + ImSin(a) * size.x * delta
                        : centre.y + ImSin(a) * size.y * delta;
        ImColor c = ImColor::HSV(out_h + i * (1.f / dots * 2.f), out_s, out_v);
        ImVec2 p = mode ? ImVec2(pp, centre.y - (size.y * 0.5f) + i * thickness * nextItemKoeff)
                        : ImVec2(centre.x - (size.x * 0.5f) + i * thickness * nextItemKoeff, pp);
        window->DrawList->AddCircleFilled(p, thickness * th_koeff, color_alpha(c, 1.f), lt);
        return p;
    };
    for (int i = 0; i < dots; i++) {
        ImVec2 p1 = draw_point(0, i);
        ImVec2 p2 = draw_point(IM_PI, i);
        window->DrawList->AddLine(p1, p2, color_alpha(color, 1.f), thickness * 0.5f);
    }
}

void ImGui::Spinner3SmuggleDots(const char *label, float radius, float thickness, const ImColor &color, float speed, int lt, float delta, bool mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float nextItemKoeff = 2.5f;
    const float dots = 2;// (size.x / (thickness * nextItemKoeff));
    const float start = ImFmod((float)ImGui::GetTime() * speed, PI_2);
    auto draw_point = [&] (float angle, int i, float k) {
        float a = angle + k * start + k * (IM_PI - i * PI_DIV(dots));
        float th_koeff = 1.f + ImSin(a + PI_DIV_2) * 0.3f;
        float pp = mode ? centre.x + ImSin(a) * size.x * delta
                        : centre.y + ImSin(a) * size.y * delta;
        ImVec2 p = mode ? ImVec2(pp, centre.y - (size.y * 0.5f) + i * thickness * nextItemKoeff)
                        : ImVec2(centre.x - (size.x * 0.5f) + i * thickness * nextItemKoeff, pp);
        window->DrawList->AddCircleFilled(p, thickness * th_koeff, color_alpha(color, 1.f), lt);
        return p;
    };
    {
        ImVec2 p1 = draw_point(0, 1, -1);
        ImVec2 p2 = draw_point(IM_PI, 2, 1);
        //window->DrawList->AddLine(p1, p2, color_alpha(color, 1.f), thickness * 0.5f);
        ImVec2 p3 = draw_point(PI_DIV_2, 3, -1);
        //window->DrawList->AddLine(p2, p3, color_alpha(color, 1.f), thickness * 0.5f);
    }
}

void ImGui::SpinnerRotateSegmentsPulsar(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t arcs, size_t layers)
{
    SPINNER_HEADER(pos, size, centre, num_segments);
    const float arc_angle = PI_2 / (float)arcs;
    const float angle_offset = arc_angle / num_segments;
    float r = radius;
    float reverse = 1.f;
    const float bg_angle_offset = PI_2_DIV(num_segments);
    const float koeff = PI_DIV(2 * layers);
    float start = (float)ImGui::GetTime() * speed;
    for (int num_ring = 0; num_ring < layers; ++num_ring) {
        float radius_k = ImSin(ImFmod(start + (num_ring * koeff), PI_DIV_2));
        ImColor c = color_alpha(color, (radius_k > 0.5f) ? (2.f - (radius_k * 2.f)) : color.Value.w);
        for (size_t arc_num = 0; arc_num < arcs; ++arc_num)
        {
            window->DrawList->PathClear();
            for (size_t i = 2; i <= num_segments - 2; i++)
            {
                const float a = start * (1.f + 0.1f * num_ring) + arc_angle * arc_num + (i * angle_offset);
                window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a * reverse) * (r * radius_k), centre.y + ImSin(a * reverse) * (r * radius_k)));
            }
            window->DrawList->PathStroke(c, false, thickness);
        }
    }
}

void ImGui::SpinnerSplineAng(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed, float angle, int mode)
{
    SPINNER_HEADER(pos, size, centre, num_segments);                            // Get the position, size, centre, and number of segments of the spinner using the SPINNER_HEADER macro.
    float start = (float)ImGui::GetTime() * speed;                        // The start angle of the spinner is calculated based on the current time and the specified speed.
    radius = (mode == 1) ? (0.8f + ImCos(start) * 0.2f) * radius : radius;
    //circle([&] (int i) {                                                         // Draw the background of the spinner using the `circle` function, with the specified background color and thickness.
    //    const float a = start + (i * (PI_2 / (num_segments - 1)));               // Calculate the angle for each segment based on the start angle and the number of segments.
    //    return ImVec2(ImCos(a) * (radius + thickness), damped_infinity(1.3f, (float)a).second * radius);
    //}, color_alpha(bg, 1.f), thickness);
    const float b = damped_gravity(ImSin(start * 1.1f)) * angle;
    circle([&] (int i) {                                                        // Draw the spinner itself using the `circle` function, with the specified color and thickness.
        const float a = start - b + (i * angle / num_segments);
        return ImVec2(ImSin(a) * radius, ImCos(a) * radius);
    }, color_alpha(color, 1.f), thickness);
}
