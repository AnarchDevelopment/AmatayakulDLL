#pragma once

#include <cstdint>
#include "../ImGui/imgui.h"

// Forward declarations
class HudElement;
class ImDrawList;

/// @brief Module manager - Initializes and manages all modules
class Module {
public:
    static void Initialize(uintptr_t gameBase, HudElement* renderInfoHud, HudElement* watermarkHud, HudElement* keystrokesHud, HudElement* cpsHud, HudElement* fpsHud);
    static void UpdateAnimation(unsigned long long now);
    static void RenderDisplay(float sw, float sh);
    static void RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd);
};
