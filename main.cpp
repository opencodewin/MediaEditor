#include <application.h>

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "BlueprintSDK Test";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
}

void Application_Initialize(void** handle)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
}

void Application_Finalize(void** handle)
{
}

bool Application_Frame(void * handle)
{
    return false;
}