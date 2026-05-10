#pragma once

#include <windows.h>
#include "../ImGui/imgui.h"

#include <map>
#include <string>

/// @brief GUI class - Handles all UI logic and rendering
class GUI {
public:
    // Menu state
    static bool g_showMenu;
    static float g_menuAnim;
    
    // Tab state
    static int g_currentTab;
    static int g_previousTab;
    static ULONGLONG g_tabChangeTime;
    static float g_tabAnim;

    // Icons map (name -> texture)
    static std::map<std::string, ImTextureID> g_icons;
    static std::string g_currentSettingsModule;
    
    // Style and theme
    static void ApplyTheme();
    static void LoadFont();
    static void LoadIcons(void* pDevice);
    
    // Menu animation update
    static void UpdateAnimation(ULONGLONG now, float dt);
    
    // Menu rendering
    static void RenderMenu(float screenWidth, float screenHeight);
    
    // Notification
    static void RenderNotification(float screenWidth, float screenHeight);

    // Custom rectangular toggle button
    static void ToggleButton(const char* label, bool* v);
    
    // Custom module card
    static void RenderModuleCard(const char* name, const char* iconName, bool* enabled, bool* showSettings);
};
