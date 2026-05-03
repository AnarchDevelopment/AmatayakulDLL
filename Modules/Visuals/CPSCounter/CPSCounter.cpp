#include "CPSCounter.hpp"
#include "../../../Animations/Animations.hpp"
#include "../../../Utils/HudElement.hpp"
#include "../../../ImGui/imgui.h"
#include "../../../GUI/GUI.hpp"
#include <windows.h>
#include <cmath>
#include <algorithm>

// Forward declarations for helper functions
extern bool g_showMenu;
bool CPSCounter::g_showCpsCounter = true;
std::string CPSCounter::g_cpsCounterFormat = "{CPS} CPS";
float CPSCounter::g_cpsTextScale = 1.0f;
ImVec4 CPSCounter::g_cpsTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

float CPSCounter::g_cpsCounterAnim = 1.0f;
ULONGLONG CPSCounter::g_cpsCounterEnableTime = 0;
ULONGLONG CPSCounter::g_cpsCounterDisableTime = 0;

int CPSCounter::g_cpsCounterAlignment = 0;
bool CPSCounter::g_cpsCounterShadow = true;
float CPSCounter::g_cpsCounterShadowOffset = 2.0f;
ImVec4 CPSCounter::g_cpsCounterShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
float CPSCounter::g_cpsCounterX = 0.5f;
float CPSCounter::g_cpsCounterY = 0.95f;
bool CPSCounter::g_cpsCounterFirstRender = false;

HudElement* CPSCounter::g_cpsHud = nullptr;

// CPS Click tracking
ULONGLONG CPSCounter::g_lmbClickTimes[100] = {};
ULONGLONG CPSCounter::g_rmbClickTimes[100] = {};
int CPSCounter::g_lmbClickIndex = 0;
int CPSCounter::g_rmbClickIndex = 0;
int CPSCounter::g_lmbCps = 0;
int CPSCounter::g_rmbCps = 0;
ULONGLONG CPSCounter::g_lastLmbClickTime = 0;
ULONGLONG CPSCounter::g_lastRmbClickTime = 0;

// Forward declarations for easing and HudElement (these will be linked from dllmain.cpp)
extern bool g_showMenu;

void CPSCounter::Initialize(HudElement* hudElement) {
    g_cpsHud = hudElement;
}

void CPSCounter::UpdateCPS(ULONGLONG now, bool lmbPressed, bool rmbPressed, bool prevLmbPressed, bool prevRmbPressed) {
    // LMB CPS Counter - only save timestamp on click, with debounce
    if (lmbPressed && !prevLmbPressed) {
        if (now - g_lastLmbClickTime > 50) {
            g_lmbClickTimes[g_lmbClickIndex] = now;
            g_lmbClickIndex = (g_lmbClickIndex + 1) % MAX_CPS_HISTORY;
            g_lastLmbClickTime = now;
        }
    }
    
    // Always calculate LMB CPS from timestamp array
    int count = 0;
    for (int i = 0; i < MAX_CPS_HISTORY; i++) {
        if (g_lmbClickTimes[i] > 0 && (now - g_lmbClickTimes[i]) < 1000) {
            count++;
        }
    }
    g_lmbCps = count;
    
    // RMB CPS Counter - only save timestamp on click, with debounce
    if (rmbPressed && !prevRmbPressed) {
        if (now - g_lastRmbClickTime > 50) {
            g_rmbClickTimes[g_rmbClickIndex] = now;
            g_rmbClickIndex = (g_rmbClickIndex + 1) % MAX_CPS_HISTORY;
            g_lastRmbClickTime = now;
        }
    }
    
    // Always calculate RMB CPS from timestamp array
    count = 0;
    for (int i = 0; i < MAX_CPS_HISTORY; i++) {
        if (g_rmbClickTimes[i] > 0 && (now - g_rmbClickTimes[i]) < 1000) {
            count++;
        }
    }
    g_rmbCps = count;
}

void CPSCounter::UpdateAnimation(ULONGLONG now) {
    if (g_showCpsCounter && g_cpsCounterEnableTime == 0) {
        g_cpsCounterEnableTime = now;
        g_cpsCounterDisableTime = 0;
    }
    if (!g_showCpsCounter && g_cpsCounterDisableTime == 0 && g_cpsCounterEnableTime > 0) {
        g_cpsCounterDisableTime = now;
        g_cpsCounterEnableTime = 0;
    }

    if (g_cpsCounterEnableTime > 0) {
        float enableElapsed = (float)(now - g_cpsCounterEnableTime) / 1000.0f;
        g_cpsCounterAnim = fminf(1.0f, enableElapsed / 0.4f);
    }
    else if (g_cpsCounterDisableTime > 0) {
        float disableElapsed = (float)(now - g_cpsCounterDisableTime) / 1000.0f;
        float disableAnim = fminf(1.0f, disableElapsed / 0.3f);
        g_cpsCounterAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_cpsCounterEnableTime = 0;
            g_cpsCounterDisableTime = 0;
            g_cpsCounterAnim = 0.0f;
        }
    }
}

void CPSCounter::RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd) {
    if (g_showCpsCounter || (g_cpsCounterDisableTime > 0 && g_cpsCounterAnim > 0.01f)) {
        float cpsAlpha = g_cpsCounterAnim * 255.0f;
        float slideOffset = -60.0f + (Animations::EaseOutExpo(g_cpsCounterAnim) * 60.0f);
        
        if (cpsAlpha > 1.0f && draw) {
            char cBuf[64];
            sprintf_s(cBuf, sizeof(cBuf), "CPS Counter");
            ImVec2 textSize = ImGui::CalcTextSize(cBuf);
            float xPosC = arrayListStart.x + 300.0f - textSize.x - 10.0f;
            
            draw->AddText(ImVec2(xPosC + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), cBuf);
            draw->AddText(ImVec2(xPosC + slideOffset, yPos), IM_COL32(255, 200, 100, (int)cpsAlpha), cBuf);
            yPos += 18.0f;
            arrayListEnd.y = yPos;
        }
    }
}

void CPSCounter::RenderDisplay(int screenWidth, int screenHeight) {
    if (!g_cpsHud) return;
    
    if (g_showCpsCounter || g_cpsCounterAnim > 0.01f) {
        // Initialize position on first draw
        if (g_cpsHud->pos.x == 500 && g_cpsHud->pos.y == 400) {
            g_cpsHud->pos = ImVec2(screenWidth / 2.0f - 50, screenHeight - 100);
        }
        
        // Calculate text size for collision box
        std::string cpsText = ProcessCPSCounterFormat(g_cpsCounterFormat, g_lmbCps, g_rmbCps);
        ImFont* cpsFont = ImGui::GetFont();
        if (cpsFont) {
            float fontSize = 18.0f * g_cpsTextScale;
            ImVec2 textSize = ImGui::CalcTextSize(cpsText.c_str());
            // Update collision size from text
            g_cpsHud->size = ImVec2(
                textSize.x * g_cpsTextScale + 4.0f,  // Small padding
                fontSize + 4.0f
            );
        }
        
        // Handle CPS counter drag when menu is open
        if (g_showMenu) {
            g_cpsHud->HandleDrag(true);
            g_cpsHud->ClampToScreen();
            
            // Draw collision border (cyan)
            ImDrawList* debugDraw = ImGui::GetForegroundDrawList();
            if (debugDraw) {
                ImVec2 p1 = g_cpsHud->pos;
                ImVec2 p2 = ImVec2(g_cpsHud->pos.x + g_cpsHud->size.x, g_cpsHud->pos.y + g_cpsHud->size.y);
                debugDraw->AddRect(p1, p2, ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 0.8f)), 0.0f, 0, 2.0f);
            }
        }
        
        float easedAnim = Animations::EaseOutExpo(g_cpsCounterAnim);
        float cpsAlpha = easedAnim * 255.0f;
        
        ImDrawList* cpsDraw = ImGui::GetForegroundDrawList();
        if (cpsDraw && cpsAlpha > 1.0f) {
            if (cpsFont) {
                float fontSize = 18.0f * g_cpsTextScale;
                ImVec2 textSize = ImGui::CalcTextSize(cpsText.c_str());
                
                // Use HUD position
                float posX = g_cpsHud->pos.x;
                float posY = g_cpsHud->pos.y;
                
                switch (g_cpsCounterAlignment) {
                    case 0: // Left
                        break;
                    case 1: // Center
                        posX += (g_cpsHud->size.x - (textSize.x * g_cpsTextScale)) / 2.0f;
                        break;
                    case 2: // Right
                        posX += g_cpsHud->size.x - (textSize.x * g_cpsTextScale);
                        break;
                }
                
                posY += (g_cpsHud->size.y - fontSize) / 2.0f;  // Vertical centering
                ImVec2 finalPos = ImVec2(posX, posY);
                
                // Shadow if enabled
                if (g_cpsCounterShadow) {
                    ImVec4 shadowCol = g_cpsCounterShadowColor;
                    shadowCol.w = easedAnim;
                    cpsDraw->AddText(cpsFont, fontSize, 
                        ImVec2(finalPos.x + g_cpsCounterShadowOffset, finalPos.y + g_cpsCounterShadowOffset),
                        ImGui::GetColorU32(shadowCol),
                        cpsText.c_str()
                    );
                }
                
                // Main text
                ImVec4 textCol = g_cpsTextColor;
                textCol.w = easedAnim;
                cpsDraw->AddText(cpsFont, fontSize, finalPos, ImGui::GetColorU32(textCol), cpsText.c_str());
            }
        }
    }
}

void CPSCounter::RenderMenu() {
    static char cpsFormatBuf[256] = "{CPS} CPS";
    
    // Show CPS Counter toggle
    GUI::ToggleButton("CPS Counter", &g_showCpsCounter);
    
    if (g_showCpsCounter) {
        ImGui::Separator();
        ImGui::Text("CPS Counter Configuration");
        
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.8f);
        
        // Color picker
        ImGui::ColorEdit4("CPS Text Color##CPSCounter", (float*)&g_cpsTextColor, ImGuiColorEditFlags_NoInputs);
        
        // Format string
        if (!g_cpsCounterFirstRender) {
            strncpy_s(cpsFormatBuf, sizeof(cpsFormatBuf), g_cpsCounterFormat.c_str(), _TRUNCATE);
            g_cpsCounterFirstRender = true;
        }
        if (ImGui::InputText("Format String##CPS", cpsFormatBuf, sizeof(cpsFormatBuf))) {
            g_cpsCounterFormat = std::string(cpsFormatBuf);
        }
        ImGui::TextWrapped("Use {CPS} for CPS counter");
        
        // Text Scale
        ImGui::SliderFloat("CPS Text Scale##CPSCounter", &g_cpsTextScale, 0.5f, 2.0f, "%.2f");
        
        // Alignment
        const char* alignmentItems[] = { "Left", "Center", "Right" };
        ImGui::Combo("CPS Text Alignment##CPSCounter", &g_cpsCounterAlignment, alignmentItems, IM_ARRAYSIZE(alignmentItems));
        
        // Shadow
        GUI::ToggleButton("CPS Text Shadow##CPSCounter", &g_cpsCounterShadow);
        if (g_cpsCounterShadow) {
            ImGui::SliderFloat("CPS Shadow Offset##CPSCounter", &g_cpsCounterShadowOffset, 0.0f, 10.0f, "%.1f");
            ImGui::ColorEdit4("CPS Shadow Color##CPSCounter", (float*)&g_cpsCounterShadowColor, ImGuiColorEditFlags_NoInputs);
        }
        
        ImGui::PopStyleVar();  // Alpha
    }
}

std::string CPSCounter::ProcessCPSCounterFormat(const std::string& format, int lmb, int rmb) {
    std::string result = format;
    
    // Replace placeholders (case-insensitive)
    std::string formatUpper = format;
    for (char& c : formatUpper) c = std::toupper(c);
    
    // Replace CPS placeholder (uses LMB CPS)
    size_t pos = formatUpper.find("{CPS}");
    if (pos != std::string::npos) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", lmb);
        result.replace(pos, 5, buffer);
    }
    
    // Replace LMB placeholder
    formatUpper = result;
    for (char& c : formatUpper) c = std::toupper(c);
    pos = formatUpper.find("{LMB}");
    if (pos != std::string::npos) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", lmb);
        result.replace(pos, 5, buffer);
    }
    
    // Replace RMB placeholder
    formatUpper = result;
    for (char& c : formatUpper) c = std::toupper(c);
    pos = formatUpper.find("{RMB}");
    if (pos != std::string::npos) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", rmb);
        result.replace(pos, 5, buffer);
    }
    
    return result;
}
