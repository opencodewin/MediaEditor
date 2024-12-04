#include "ImGuiTabWindow.h"
#include <imgui_helper.h>
#include <string>
namespace ImGui
{
static ImTextureID textue = nullptr;
void ShowAddonsTabWindow()
{
    ImGui::Spacing();
    ImGui::Text("TabLabels (based on the code by krys-spectralpixel):");
    static const std::vector<std::string> tabNames = {"TabLabelStyle","Render","Layers","Scene","World","Object","Constraints","Modifiers","Data","Material","Texture","Particle"};
    static const std::vector<std::string> tabTooltips = {"Edit the style of these labels","Render Tab Tooltip","This tab cannot be closed","Scene Tab Tooltip","","Object Tab Tooltip","","","","","Tired to add tooltips..."};
    static int tabItemOrdering[] = {0,1,2,3,4,5,6,7,8,9,10,11};
    static int selectedTab = 0;
    static int optionalHoveredTab = 0;
    static bool allowTabLabelDragAndDrop=true;
    static bool tabLabelWrapMode = true;
    static bool allowClosingTabs = false;
    static bool tableAtBottom = false;
    static int doneBadgeNumber = 0;
    static int doingBadgeNumber = 0;

    int justClosedTabIndex=-1,justClosedTabIndexInsideTabItemOrdering = -1,oldSelectedTab = selectedTab;
    ImGui::Checkbox("Wrap Mode##TabLabelWrapMode",&tabLabelWrapMode);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","WrapMode is only available\nin horizontal TabLabels");
    ImGui::SameLine();ImGui::Checkbox("Drag And Drop##TabLabelDragAndDrop",&allowTabLabelDragAndDrop);
    ImGui::SameLine();ImGui::Checkbox("Closable##TabLabelClosing",&allowClosingTabs);ImGui::SameLine();
    ImGui::Checkbox("At bottom##TabLabelAtbottom",&tableAtBottom);
    ImGui::SliderInt("Done Badge", &doneBadgeNumber, 0, 100);
    ImGui::SliderInt("Doing Badge", &doingBadgeNumber, 0, 100);
    bool resetTabLabels = ImGui::SmallButton("Reset Tabs");if (resetTabLabels) {selectedTab=0;for (int i=0;i<tabNames.size();i++) tabItemOrdering[i] = i; doneBadgeNumber = 0; doingBadgeNumber = 0; }

    ImVec2 table_size;
    std::vector<std::pair<int, int>> BadgeNumber;
    for (int i = 0; i < tabNames.size(); i++)
    {
        if (i == 0 || (tabNames.size() > 1 && i == 1))
            BadgeNumber.push_back(std::pair<int, int>(doneBadgeNumber, doingBadgeNumber));
        else
            BadgeNumber.push_back(std::pair<int, int>(0, 0));
    }       

    if (!tableAtBottom)
    {
        ImGui::TabLabels(tabNames, selectedTab, table_size, tabTooltips, BadgeNumber, tabLabelWrapMode, false, &optionalHoveredTab, &tabItemOrdering[0], allowTabLabelDragAndDrop, allowClosingTabs, &justClosedTabIndex, &justClosedTabIndexInsideTabItemOrdering);
        // Optional stuff
        if (justClosedTabIndex==1) {
            tabItemOrdering[justClosedTabIndexInsideTabItemOrdering] = justClosedTabIndex;   // Prevent the user from closing Tab "Layers"
            selectedTab = oldSelectedTab;   // This is safer, in case we had closed the selected tab
        }
    }
    // Draw tab page
    ImGui::BeginChild("MyTabLabelsChild",ImVec2(0,150),true);
    ImGui::Text("Tab Page For Tab: \"%s\" here.", selectedTab >= 0 ? tabNames[selectedTab].c_str() : "None!");
    if (selectedTab==0) {
        static bool editTheme = false;
        ImGui::Spacing();
        ImGui::Checkbox("Edit tab label style",&editTheme);
        ImGui::Spacing();
        if (editTheme) ImGui::TabLabelStyle::Edit(ImGui::TabLabelStyle().Get());   // This is good if we want to edit the tab label style
        else {
            static int selectedIndex=0;
            ImGui::PushItemWidth(135);
            ImGui::SelectTabLabelStyleCombo("select tab label style",&selectedIndex); // Good for just selecting it
            ImGui::PopItemWidth();
        }
    }
    ImGui::EndChild();
    if (tableAtBottom)
    {
        ImGui::TabLabels(tabNames,selectedTab,table_size,tabTooltips,BadgeNumber,tabLabelWrapMode,false,&optionalHoveredTab,&tabItemOrdering[0],allowTabLabelDragAndDrop,allowClosingTabs,&justClosedTabIndex,&justClosedTabIndexInsideTabItemOrdering, true);
        // Optional stuff
        if (justClosedTabIndex==1) {
            tabItemOrdering[justClosedTabIndexInsideTabItemOrdering] = justClosedTabIndex;   // Prevent the user from closing Tab "Layers"
            selectedTab = oldSelectedTab;   // This is safer, in case we had closed the selected tab
        }
    }

    // ImGui::TabLabelsVertical() are similiar to ImGui::TabLabels(), but they do not support WrapMode.
    // ImGui::TabLabelsVertical() example usage
    static bool verticalTabLabelsAtLeft = true;ImGui::Checkbox("Vertical Tab Labels at the left side##VerticalTabLabelPosition",&verticalTabLabelsAtLeft);
    static const std::vector<std::string> verticalTabNames = {"Layers","Scene","World"};
    static const std::vector<std::string> verticalTabTooltips = {"Layers Tab Tooltip","Scene Tab Tooltip","World Tab Tooltip"};
    static int verticalTabItemOrdering[3] = {0,1,2};
    static int selectedVerticalTab = 0;
    static int optionalHoveredVerticalTab = 0;
    if (resetTabLabels) {selectedVerticalTab=0;for (int i=0;i<verticalTabNames.size();i++) verticalTabItemOrdering[i] = i;}

    std::vector<std::pair<int, int>> vBadgeNumber;
    for (int i = 0; i < verticalTabNames.size(); i++)
    {
        if (i == 0 || (verticalTabNames.size() > 1 && i == 1))
            vBadgeNumber.push_back(std::pair<int, int>(doneBadgeNumber, doingBadgeNumber));
        else
            vBadgeNumber.push_back(std::pair<int, int>(0, 0));
    }       

    const float verticalTabsWidth = ImGui::CalcVerticalTabLabelsWidth();
    if (verticalTabLabelsAtLeft)	{
        ImGui::TabLabelsVertical(verticalTabNames,selectedVerticalTab,verticalTabTooltips,vBadgeNumber,false,&optionalHoveredVerticalTab,&verticalTabItemOrdering[0],allowTabLabelDragAndDrop,allowClosingTabs,NULL,NULL,!verticalTabLabelsAtLeft,false);
        ImGui::SameLine(0,0);
    }
    // Draw tab page
    ImGui::BeginChild("MyVerticalTabLabelsChild",ImVec2(ImGui::GetWindowWidth()-verticalTabsWidth-2.f*ImGui::GetStyle().WindowPadding.x-ImGui::GetStyle().ScrollbarSize,150),true);
    ImGui::Text("Tab Page For Tab: \"%s\" here.",selectedVerticalTab >= 0 ? verticalTabNames[selectedVerticalTab].c_str() : "None!");
    ImGui::EndChild();
    if (!verticalTabLabelsAtLeft)	{
        ImGui::SameLine(0,0);
        ImGui::TabLabelsVertical(verticalTabNames,selectedVerticalTab,verticalTabTooltips,vBadgeNumber,false,&optionalHoveredVerticalTab,&verticalTabItemOrdering[0],allowTabLabelDragAndDrop,allowClosingTabs,NULL,NULL,!verticalTabLabelsAtLeft,false);
    }

    // ImGui::TabImageLabels() example usage
    if (!textue)
    {
        ImGui::ImMat test_image(128, 72, 4, 1u, 4);
        for (int y = 0; y < 72; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                float dx = x + .5f;
                float dy = y + .5f;
                float dv = sinf(x * 0.02f) + sinf(0.03f * (x + y)) + sinf(sqrtf(0.04f * (dx * dx + dy * dy) + 1.f));
                test_image.at<unsigned char>(x, y, 3) = UCHAR_MAX;
                test_image.at<unsigned char>(x, y, 2) = fabsf(sinf(dv * 3.141592f + 4.f * 3.141592f / 3.f)) * UCHAR_MAX;
                test_image.at<unsigned char>(x, y, 1) = fabsf(sinf(dv * 3.141592f + 2.f * 3.141592f / 3.f)) * UCHAR_MAX;
                test_image.at<unsigned char>(x, y, 0) = fabsf(sinf(dv * 3.141592f)) * UCHAR_MAX;
            }
        }
        ImGui::ImMatToTexture(test_image, textue);
    }

    static int selectedImageTab = 0;
    static int optionalHoveredImageTab = 0;
    static int tab_num = 3;
    static std::vector<std::string> imageTabNames = {"Image1 it is long text string test","Image2","Image3"};
    static std::vector<ImTextureID> imageTabTextures = { textue, textue, textue };
    static std::vector<ImVec4> imageTabTextureROIs = { ImVec4(0, 0, 1, 1), ImVec4(0, 0, 1, 1), ImVec4(0, 0, 1, 1) };
    static std::vector<int> imageTabItemOrdering = {0, 1, 2};
    if (ImGui::SliderInt("image number", &tab_num, 3, 20))
    {
        imageTabNames.resize(tab_num);
        imageTabTextures.resize(tab_num);
        imageTabItemOrdering.resize(tab_num);
        for (int i = 3; i < tab_num; i++)
        {
            imageTabNames[i] = "Image" + std::to_string(i + 1);
            imageTabTextures[i] = textue;
            imageTabItemOrdering[i] = i;
        }
    }
    // Draw tab page
    ImGui::BeginChild("MyImageTabLabelsChild",ImVec2(0,300),true);
    ImGui::Text("Tab Page For Image Tab: \"%s\" here.",selectedImageTab >= 0 ? imageTabNames[selectedImageTab].c_str() : "None!");
    ImGui::EndChild();
    ImGui::TabImageLabels(imageTabNames,selectedImageTab,table_size,std::vector<std::string>(),std::vector<std::pair<int, int>>(),imageTabTextures,ImVec2(64,36),tabLabelWrapMode,false,&optionalHoveredImageTab,&imageTabItemOrdering[0],allowTabLabelDragAndDrop,allowClosingTabs,NULL,NULL,true);
}
void  ReleaseTabWindow()
{
    ImGui::ImDestroyTexture(&textue);
}
} // namespace ImGui