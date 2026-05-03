#include "MotionBlur.hpp"
#include "../../../ImGui/imgui.h"
#include "../../../GUI/GUI.hpp"
#include <cstdio>
#include <cmath>

// Static member initialization
bool MotionBlur::g_motionBlurEnabled = false;
float MotionBlur::g_motionBlurAnim = 0.0f;
float MotionBlur::g_blurIntensity = 5.0f;
float MotionBlur::g_maxHistoryFrames = 8.0f;
float MotionBlur::g_blurTimeConstant = 0.5f;
std::string MotionBlur::g_blurType = "Average Pixel Blur";
std::vector<ID3D11ShaderResourceView*> MotionBlur::g_previousFrames;
std::vector<float> MotionBlur::g_frameTimestamps;

void MotionBlur::Initialize() {
    // Initialize motion blur
}

void MotionBlur::UpdateAnimation(ULONGLONG now) {
    // Update animation if needed
}

void MotionBlur::InitializeBackbufferStorage(int maxFrames) {
    // Initialize storage for motion blur frames
    // Clean up old frames if needed
    while ((int)g_previousFrames.size() >= maxFrames) {
        if (g_previousFrames[0]) {
            g_previousFrames[0]->Release();
        }
        g_previousFrames.erase(g_previousFrames.begin());
        if (!g_frameTimestamps.empty()) {
            g_frameTimestamps.erase(g_frameTimestamps.begin());
        }
    }
}

ID3D11ShaderResourceView* MotionBlur::CopyBackbufferToSRV(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, IDXGISwapChain* pSwapChain) {
    if (!pDevice || !pContext || !pSwapChain) return nullptr;

    ID3D11Texture2D* pBackBuffer = nullptr;
    if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
        return nullptr;
    }

    // Create SRV from backbuffer
    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(pDevice->CreateShaderResourceView(pBackBuffer, nullptr, &srv))) {
        pBackBuffer->Release();
        return srv;
    }

    pBackBuffer->Release();
    return nullptr;
}

void MotionBlur::CleanupBackbufferStorage() {
    for (auto frame : g_previousFrames) {
        if (frame) frame->Release();
    }
    g_previousFrames.clear();
    g_frameTimestamps.clear();
}

void MotionBlur::RenderMotionBlur(ImDrawList* draw, ImVec2 screenSize) {
    if (!g_motionBlurEnabled || g_previousFrames.empty()) return;

    // Render motion blur effect based on g_blurType
    float currentTime = (float)GetTickCount64() / 1000.0f;

    if (g_blurType == "Average Pixel Blur") {
        float alpha = 0.25f;
        float bleedFactor = 0.95f;
        for (const auto& frame : g_previousFrames) {
            if (frame) {
                ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * g_motionBlurAnim * 255.0f));
                draw->AddImage((ImTextureID)frame, ImVec2(0, 0), screenSize, ImVec2(0, 0), ImVec2(1, 1), col);
                alpha *= bleedFactor;
            }
        }
    }
    // Add other blur types as needed
}

void MotionBlur::RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd) {
    if (g_motionBlurEnabled) {
        char buf[64];
        sprintf_s(buf, "Motion Blur");
        float textWidth = ImGui::CalcTextSize(buf).x;
        float xPos = arrayListStart.x + 290.0f - textWidth - 10;
        draw->AddText(ImVec2(xPos, yPos), IM_COL32(200, 100, 255, 255), buf);
        yPos += 18.0f;
        arrayListEnd.y = yPos;
    }
}

void MotionBlur::RenderMenu() {
    GUI::ToggleButton("Motion Blur", &g_motionBlurEnabled);

    if (g_motionBlurEnabled) {
        ImGui::SliderFloat("Blur Intensity", &g_blurIntensity, 1.0f, 10.0f);
        ImGui::SliderFloat("Max History Frames", &g_maxHistoryFrames, 2.0f, 16.0f);
        ImGui::SliderFloat("Time Constant", &g_blurTimeConstant, 0.1f, 2.0f);

        const char* blurTypes[] = { "Average Pixel Blur", "Time Aware Blur", "Ghost Frames", "Real Motion Blur" };
        int currentType = 0;
        for (int i = 0; i < IM_ARRAYSIZE(blurTypes); i++) {
            if (g_blurType == blurTypes[i]) {
                currentType = i;
                break;
            }
        }
        if (ImGui::Combo("Blur Type", &currentType, blurTypes, IM_ARRAYSIZE(blurTypes))) {
            g_blurType = blurTypes[currentType];
        }
    }
}
