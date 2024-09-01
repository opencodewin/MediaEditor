#pragma once

#ifndef NO_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBIDEF inline
#define STBIWDEF inline
#define STBIRDEF inline
#ifdef _WIN32
#define STBI_WINDOWS_UTF8
#define STBIW_WINDOWS_UTF8
#endif
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"
#endif

inline float ImDegToRad(float degree) { return degree * (float(M_PI) / 180.0f); }
inline float ImRadToDeg(float radian) { return radian * (180.0f / float(M_PI)); }

namespace ImGui
{
IMGUI_API void GetVersion(int& major, int& minor, int& patch, int& build);
} // namespace ImGui

namespace ImGui
{
// Power saving mode
// Disabled by default; enabled by setting ImGuiConfigFlags_EnablePowerSavingMode or ImGuiConfigFlags_EnableLowRefreshMode in ImGuiIO.ConfigFlags.
// Requires platform binding support.
// When enabled and supported, ImGui will wait for events before starting new frames, instead of continuously polling, thereby helping to reduce power consumption.
IMGUI_API double    GetEventWaitingTime();                      // in seconds; note that it can be zero (in which case you might want to peek/poll) or infinity (in which case you may have to use a non-timeout event waiting method).
} // namespace ImGui

namespace ImGui
{
IMGUI_API double    get_current_time();
IMGUI_API uint64_t  get_current_time_msec();
IMGUI_API uint64_t  get_current_time_usec();
IMGUI_API void      sleep(float seconds);
IMGUI_API void      sleep(int ms_seconds);
} // namespace ImGui

#include <imgui_texture.h>

#if IMGUI_ICONS
#include "icons/icons.h"
#endif