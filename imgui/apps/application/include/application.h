#pragma once
#include <imgui.h>
#include <string>
#include <vector>

typedef void (*APP_SETUPCONTEXT)(ImGuiContext* ctx, void* handle, bool in_splash);
typedef void (*APP_INITIALIZE)(void** handle);
typedef void (*APP_FINALIZE)(void** handle);
typedef bool (*APP_SPLASHSCREEN)(void* handle, bool& app_will_quit);
typedef void (*APP_SPLASHFINALIZE)(void** handle);
typedef bool (*APP_FRAME)(void* handle, bool app_will_quit);
typedef void (*APP_DROPFROMSYSTEM)(std::vector<std::string>& drops);

typedef struct ApplicationFrameworks
{
    APP_SETUPCONTEXT        Application_SetupContext    {nullptr};
    APP_INITIALIZE          Application_Initialize      {nullptr};
    APP_FINALIZE            Application_Finalize        {nullptr};
    APP_SPLASHSCREEN        Application_SplashScreen    {nullptr};
    APP_SPLASHFINALIZE      Application_SplashFinalize  {nullptr};
    APP_FRAME               Application_Frame           {nullptr};
    APP_DROPFROMSYSTEM      Application_DropFromSystem  {nullptr};
} ApplicationFrameworks;

typedef struct ApplicationWindowProperty
{
    ApplicationWindowProperty() {}
    ApplicationWindowProperty(int _argc, char** _argv) { argc = _argc; argv = _argv; }
    std::string name;
    int pos_x       {0};
    int pos_y       {0};
    int width       {1440};
    int height      {960};
    float font_scale{1.0};
    bool resizable  {true};
    bool docking    {true};
    bool viewport   {true};
    bool navigator  {true};
    bool auto_merge {true};
    bool center     {true};
    bool power_save {false};
    bool low_reflash {true};
    float max_fps   {30.f};
    float min_fps   {5.f};
    bool full_screen{false};
    bool full_size  {false};
    bool using_setting_path {true};
    bool internationalize {false};
    bool top_most   {false};
    bool window_border {true};
    std::string language_path;
    std::string icon_path;
    int splash_screen_width {0};
    int splash_screen_height {0};
    float splash_screen_alpha {1.0};
    void* handle    {nullptr};
    ApplicationFrameworks application;
    int argc    {0};
    char ** argv {nullptr};
} ApplicationWindowProperty;

void Application_Setup(ApplicationWindowProperty& property);
void Application_FullScreen(bool on);