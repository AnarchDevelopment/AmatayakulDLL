#include "FPSCounter.hpp"
#include "../../../Animations/Animations.hpp"
#include "../../../Utils/HudElement.hpp"
#include "../../../ImGui/imgui.h"
#include "../../../GUI/GUI.hpp"
#include <windows.h>
#include <cmath>

// Static member initialization
bool FPSCounter::g_showFpsCounter = true;
float FPSCounter::g_fpsTextScale = 1.0f;
ImVec4 FPSCounter::g_fpsTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
float FPSCounter::g_fpsCounterAnim = 1.0f;
ULONGLONG FPSCounter::g_fpsCounterEnableTime = 0;
ULONGLONG FPSCounter::g_fpsCounterDisableTime = 0;
int FPSCounter::g_fpsCounterAlignment = 0;
bool FPSCounter::g_fpsCounterShadow = true;
float FPSCounter::g_fpsCounterShadowOffset = 2.0f;
ImVec4 FPSCounter::g_fpsCounterShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
float FPSCounter::g_fpsX = 0.5f;
float FPSCounter::g_fpsY = 0.05f;
bool FPSCounter::g_fpsFirstRender = false;
HudElement* FPSCounter::g_fpsHud = nullptr;

// Forward declarations
extern bool g_showMenu;

void FPSCounter::Initialize(HudElement* hudElement) {
    g_fpsHud = hudElement;
}

float FPSCounter::GetFPS() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.DeltaTime > 0.0f) {
        return 1.0f / io.DeltaTime;
    }
    return 0.0f;
}

void FPSCounter::UpdateAnimation(ULONGLONG now) {
    if (g_showFpsCounter && g_fpsCounterEnableTime == 0) {
        g_fpsCounterEnableTime = now;
        g_fpsCounterDisableTime = 0;
    }
    if (!g_showFpsCounter && g_fpsCounterDisableTime == 0 && g_fpsCounterEnableTime > 0) {
        g_fpsCounterDisableTime = now;
        g_fpsCounterEnableTime = 0;
    }

    if (g_fpsCounterEnableTime > 0) {
        float enableElapsed = (float)(now - g_fpsCounterEnableTime) / 1000.0f;
        g_fpsCounterAnim = fminf(1.0f, enableElapsed / 0.4f);
    }
    else if (g_fpsCounterDisableTime > 0) {
        float disableElapsed = (float)(now - g_fpsCounterDisableTime) / 1000.0f;
        float disableAnim = fminf(1.0f, disableElapsed / 0.3f);
        g_fpsCounterAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_fpsCounterEnableTime = 0;
            g_fpsCounterDisableTime = 0;
            g_fpsCounterAnim = 0.0f;
        }
    }
}

void FPSCounter::RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd) {
    if (g_showFpsCounter || (g_fpsCounterDisableTime > 0 && g_fpsCounterAnim > 0.01f)) {
        float fpsAlpha = g_fpsCounterAnim * 255.0f;
        float slideOffset = -60.0f + (Animations::EaseOutExpo(g_fpsCounterAnim) * 60.0f);

        if (fpsAlpha > 1.0f && draw) {
            char fBuf[64];
            sprintf_s(fBuf, sizeof(fBuf), "FPS Counter");
            ImVec2 textSize = ImGui::CalcTextSize(fBuf);
            float xPosF = arrayListStart.x + 300.0f - textSize.x - 10.0f;

            draw->AddText(ImVec2(xPosF + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), fBuf);
            draw->AddText(ImVec2(xPosF + slideOffset, yPos), IM_COL32(100, 200, 255, (int)fpsAlpha), fBuf);
            yPos += 18.0f;
            arrayListEnd.y = yPos;
        }
    }
}

void FPSCounter::RenderDisplay(float screenWidth, float screenHeight) {
    if (!g_fpsHud) return;

    if (g_showFpsCounter || g_fpsCounterAnim > 0.01f) {
        // Initialize position on first draw
        if (g_fpsHud->pos.x == 0 && g_fpsHud->pos.y == 0) {
            g_fpsHud->pos = ImVec2(screenWidth / 2.0f - 50, 50);
        }

        // Calculate FPS
        float fps = GetFPS();
        char fpsText[64];
        sprintf_s(fpsText, sizeof(fpsText), "%.0f FPS", fps);

        ImFont* fpsFont = ImGui::GetFont();
        if (fpsFont) {
            float fontSize = 18.0f * g_fpsTextScale;
            ImVec2 textSize = ImGui::CalcTextSize(fpsText);

            // Update collision size from text
            g_fpsHud->size = ImVec2(
                textSize.x * g_fpsTextScale + 4.0f,
                fontSize + 4.0f
            );
        }

        // Handle FPS counter drag when menu is open
        if (g_showMenu) {
            g_fpsHud->HandleDrag(true);
            g_fpsHud->ClampToScreen();

            // Draw collision border (cyan)
            ImDrawList* debugDraw = ImGui::GetForegroundDrawList();
            if (debugDraw) {
                ImVec2 p1 = g_fpsHud->pos;
                ImVec2 p2 = ImVec2(g_fpsHud->pos.x + g_fpsHud->size.x, g_fpsHud->pos.y + g_fpsHud->size.y);
                debugDraw->AddRect(p1, p2, ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 0.8f)), 0.0f, 0, 2.0f);
            }
        }

        float easedAnim = Animations::EaseOutExpo(g_fpsCounterAnim);
        float fpsAlpha = easedAnim * 255.0f;

        ImDrawList* fpsDraw = ImGui::GetForegroundDrawList();
        if (fpsDraw && fpsAlpha > 1.0f) {
            if (fpsFont) {
                float fontSize = 18.0f * g_fpsTextScale;
                ImVec2 textSize = ImGui::CalcTextSize(fpsText);

                // Use HUD position
                float posX = g_fpsHud->pos.x;
                float posY = g_fpsHud->pos.y;

                switch (g_fpsCounterAlignment) {
                    case 0: // Left
                        break;
                    case 1: // Center
                        posX += (g_fpsHud->size.x - (textSize.x * g_fpsTextScale)) / 2.0f;
                        break;
                    case 2: // Right
                        posX += g_fpsHud->size.x - (textSize.x * g_fpsTextScale);
                        break;
                }

                posY += (g_fpsHud->size.y - fontSize) / 2.0f;
                ImVec2 finalPos = ImVec2(posX, posY);

                // Shadow if enabled
                if (g_fpsCounterShadow) {
                    ImVec4 shadowCol = g_fpsCounterShadowColor;
                    shadowCol.w = easedAnim;
                    fpsDraw->AddText(fpsFont, fontSize,
                        ImVec2(finalPos.x + g_fpsCounterShadowOffset, finalPos.y + g_fpsCounterShadowOffset),
                        ImGui::GetColorU32(shadowCol),
                        fpsText
                    );
                }

                // Main text
                ImVec4 textCol = g_fpsTextColor;
                textCol.w = easedAnim;
                fpsDraw->AddText(fpsFont, fontSize, finalPos, ImGui::GetColorU32(textCol), fpsText);
            }
        }
    }
}

void FPSCounter::RenderMenu() {
    // Show FPS Counter toggle
    GUI::ToggleButton("FPS Counter", &g_showFpsCounter);

    if (g_showFpsCounter) {
        ImGui::Separator();
        ImGui::Text("FPS Counter Configuration");

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.8f);

        // Color picker
            ImGui::ColorEdit4("FPS Text Color##FPSCounter", (float*)&g_fpsTextColor, ImGuiColorEditFlags_NoInputs);

        // Text Scale
        ImGui::SliderFloat("FPS Text Scale##FPSCounter", &g_fpsTextScale, 0.5f, 2.0f, "%.2f");

        // Alignment
        const char* alignmentItems[] = { "Left", "Center", "Right" };
        ImGui::Combo("FPS Text Alignment##FPSCounter", &g_fpsCounterAlignment, alignmentItems, IM_ARRAYSIZE(alignmentItems));

        // Shadow
        GUI::ToggleButton("FPS Text Shadow##FPSCounter", &g_fpsCounterShadow);
        if (g_fpsCounterShadow) {
            ImGui::SliderFloat("FPS Shadow Offset##FPSCounter", &g_fpsCounterShadowOffset, 0.0f, 10.0f, "%.1f");
            ImGui::ColorEdit4("FPS Shadow Color##FPSCounter", (float*)&g_fpsCounterShadowColor, ImGuiColorEditFlags_NoInputs);
        }

        ImGui::PopStyleVar();
    }
}
