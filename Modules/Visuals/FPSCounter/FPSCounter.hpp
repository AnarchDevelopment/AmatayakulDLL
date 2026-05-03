#pragma once

#include <windows.h>
#include <string>
#include "../../../ImGui/imgui.h"

// Forward declarations
class ImDrawList;
struct ImVec2;
class HudElement;

/// @brief FPS Counter module - Displays FPS in format "{fps} FPS"
class FPSCounter {
public:
    static bool g_showFpsCounter;
    static float g_fpsTextScale;
    static ImVec4 g_fpsTextColor;
    static float g_fpsCounterAnim;
    static ULONGLONG g_fpsCounterEnableTime;
    static ULONGLONG g_fpsCounterDisableTime;
    static int g_fpsCounterAlignment;
    static bool g_fpsCounterShadow;
    static float g_fpsCounterShadowOffset;
    static ImVec4 g_fpsCounterShadowColor;
    static float g_fpsX;
    static float g_fpsY;
    static bool g_fpsFirstRender;
    static HudElement* g_fpsHud;

    /// @brief Initialize FPS Counter with HudElement reference
    static void Initialize(HudElement* hud);

    /// @brief Update animation state
    static void UpdateAnimation(ULONGLONG now);

    /// @brief Render FPS counter in array list
    static void RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd);

    /// @brief Render FPS counter display
    static void RenderDisplay(float sw, float sh);

    /// @brief Render menu controls
    static void RenderMenu();

    /// @brief Get current FPS value
    static float GetFPS();
};
