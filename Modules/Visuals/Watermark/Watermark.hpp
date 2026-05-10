#pragma once

#include <windows.h>
#include "../../../ImGui/imgui.h"

// Forward declarations
class ImDrawList;
struct ImVec2;
struct ImVec4;
class HudElement;

/// @brief Watermark module - Displays "Amatayakul" branding with chroma effect
class Watermark {
public:
    // Static member variables
    static bool g_showWatermark;
    static ULONGLONG g_watermarkEnableTime;
    static ULONGLONG g_watermarkDisableTime;
    static float g_watermarkAnim;
    static HudElement* g_watermarkHud;

    // Watermark settings
    static bool g_fancyMode;      // true = logo image, false = text
    static bool g_chromaEnabled;   // chroma color cycling on text
    static ImVec4 g_textColor;     // custom text color (when chroma off)
    static float g_logoScale;      // scale for the logo image

    /// @brief Initialize Watermark with HudElement reference
    static void Initialize(HudElement* hud);

    /// @brief Update animation state (call from main render loop)
    static void UpdateAnimation(ULONGLONG now);

    /// @brief Render watermark in array list (for HUD display)
    static void RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd);

    /// @brief Render watermark display
    static void RenderDisplay();

    /// @brief Render menu controls
    static void RenderMenu();
};
