#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <psapi.h>

#include "minhook/MinHook.h"
#include "ImGui/imgui.h"
#include "ImGui/backend/imgui_impl_dx11.h"
#include "ImGui/backend/imgui_impl_win32.h"
#include "Modules/ModuleHeader.hpp"
#include "Modules/ModuleManager.hpp"
#include "Modules/PatternScan/PatternScan.hpp"
#include "Modules/Globals.hpp"
#include "Animations/Animations.hpp"
#include "GUI/GUI.hpp"
#include "GUI/DX11/ImGuiRenderer.hpp"
#include "Hook/Hook.hpp"
#include "Input/Input.hpp"
#include "Config/ConfigManager.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "psapi.lib");

// TODO: Refactor into multiple files and classes for better organization and maintainability

// Variable declarations and other code...

// Entity motion packet structure

// Declarations and interfaces
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ImGui configuration flags
ImGuiConfigFlags g_imguiConfigFlags = ImGuiConfigFlags_None;

// Global variables
WNDPROC oWndProc = NULL;
HMODULE g_hModule = NULL;

ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
ma_engine g_audioEngine;
HWND g_window = NULL;

bool g_showMenu = false;
bool g_RequestUnload = false;
float g_menuAnim = 0.0f;
ULONGLONG g_lastTime = 0, g_lastToggle = 0, g_notifStart = 0;
bool g_vsync = false;
float g_lastW = 0, g_lastH = 0;

// Tab animation
int g_currentTab = 0;
int g_previousTab = 0;
ULONGLONG g_tabChangeTime = 0;
float g_tabAnim = 0.0f;

// Keyboard input system
// Keyboard and input state - delegated to Input class

bool g_firstTabOpen = true;

// Memory access helpers
uintptr_t g_gameBase = 0;

// Utility declarations
bool IsWindowsCursorVisible() {
    CURSORINFO ci = { 0 };
    ci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&ci)) {
        return (ci.flags & CURSOR_SHOWING) != 0;
    }
    return false;
}

bool IsInWorld() {
    // Check world or menu state for cursor visibility
    return !IsWindowsCursorVisible();
}

// Motion blur data - now handled by MotionBlur class

// HUD elements

// Unlock FPS data moved to UnlockFPS.hpp/cpp

// Keystrokes data - now handled by Keystrokes class

// HUD elements
HudElement g_watermarkHud = { ImVec2(10, 10), ImVec2(400, 80) };
HudElement g_renderInfoHud = { ImVec2(10, 100), ImVec2(220, 120) };
HudElement g_arrayListHud = { ImVec2(0, 10), ImVec2(300, 400) };
HudElement g_keystrokesHud = { ImVec2(30, 0), ImVec2(140, 150) };
HudElement g_cpsHud = { ImVec2(500, 400), ImVec2(80, 30) };  // Will be initialized in CPSCounter
HudElement g_fpsHud = { ImVec2(0, 0), ImVec2(80, 30) };  // Will be initialized in FPSCounter

// Input blocking

// Keyboard hook
LRESULT CALLBACK KeyboardBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(Input::g_keyboardHook, nCode, wParam, lParam);
    }
    
    int result = Input::KeyboardBlockHookProc(nCode, wParam, lParam, g_showMenu);
    if (result == 1) {
        return 1;  // Block
    } else {
        return CallNextHookEx(Input::g_keyboardHook, nCode, wParam, lParam);
    }
}

// Easing animations now handled by Animations class

// Chroma color function moved to Watermark.cpp

// ImVec4 lerp helper
ImVec4 LerpImVec4(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

// CPS format processor moved to CPSCounter.cpp

// --- Motion Blur Infrastructure (delegated to MotionBlur class) ---

void ImageWithOpacity(ID3D11ShaderResourceView* srv, ImVec2 size, float opacity) {
    if (!srv || opacity <= 0.0f) {
        return;
    }

    opacity = opacity > 1.0f ? 1.0f : (opacity < 0.0f ? 0.0f : opacity);
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImVec2 pos = { 0, 0 };
    ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(opacity * 255.0f));
    draw_list->AddImage((ImTextureID)srv, pos, ImVec2(pos.x + size.x, pos.y + size.y), ImVec2(0, 0), ImVec2(1, 1), col);
}


// Hitbox logic
// --- 🏃 AUTOSPRINT ---
// --- 🖱️ INPUT HOOKS DELEGADOS A Hook.cpp ---

// WndProc hook removed for simplicity - menu input handled via ImGui_ImplWin32_NewFrame
LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

void CleanupRenderTarget() {
    if (mainRenderTargetView) { 
        mainRenderTargetView->Release(); 
        mainRenderTargetView = NULL; 
    }
}

// --- 🎨 RENDER PRINCIPAL ---
HRESULT STDMETHODCALLTYPE hkPresent_Impl(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    // Force VSync OFF when disabled
    if (!g_vsync)
        SyncInterval = 0;
    
    if (!pDevice) {
        // Safety: check pSwapChain is valid
        if (!pSwapChain) {
            return Hook::oPresent(pSwapChain, SyncInterval, Flags);
        }
        
        ID3D11Device* tempDevice = NULL;
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&tempDevice))) {
            pDevice = tempDevice;
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd = {0};
            if (SUCCEEDED(pSwapChain->GetDesc(&sd))) {
                g_window = sd.OutputWindow;
            }
            if (!g_window) g_window = GetForegroundWindow();
            
            // Initialize ImGui
            ImGui::CreateContext();
            if (ImGui_ImplWin32_Init(g_window) && ImGui_ImplDX11_Init(pDevice, pContext)) {
                // Cargar fuente personalizada
                GUI::LoadFont();
                ImGui_ImplDX11_CreateDeviceObjects();
                
                // 🎨 APLICAR TEMA
                GUI::ApplyTheme();
                
                // Inicializar watermark para que aparezca por defecto
                Watermark::g_watermarkEnableTime = GetTickCount64();
                Watermark::g_watermarkAnim = 1.0f;
                
                // WndProc hook removed for simplicity - ImGui handles input via ImGui_ImplWin32_NewFrame
                
                g_gameBase = (uintptr_t)GetModuleHandleA(NULL);
                Module::Initialize(g_gameBase, &g_renderInfoHud, &g_watermarkHud, &g_keystrokesHud, &g_cpsHud, &g_fpsHud);
                
                g_notifStart = GetTickCount64();
                g_lastTime = GetTickCount64();
            } else {
                // Cleanup on ImGui init failure
                if (ImGui::GetCurrentContext()) {
                    ImGui::DestroyContext();
                }
                pDevice->Release();
                pDevice = NULL;
                return Hook::oPresent(pSwapChain, SyncInterval, Flags);
            }
        } else {
            return Hook::oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    ID3D11Texture2D* pBackBuffer = NULL;
    float sw = 0, sh = 0;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
        D3D11_TEXTURE2D_DESC desc;
        pBackBuffer->GetDesc(&desc);
        sw = (float)desc.Width; 
        sh = (float)desc.Height;
        
        // Update ImGui display size every frame to fix mouse alignment on window resize
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(sw, sh);
        
        if (!mainRenderTargetView) pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        
        // Motion blur - capture frame
        if (MotionBlur::g_motionBlurEnabled) {
            // Calculate max frames by blur type
            int maxFrames = 1;
            if (MotionBlur::g_blurType == "Time Aware Blur") {
                maxFrames = (int)round(MotionBlur::g_maxHistoryFrames);
            } else if (MotionBlur::g_blurType == "Real Motion Blur") {
                maxFrames = 8;
            } else {
                maxFrames = (int)round(MotionBlur::g_blurIntensity);
            }
            
            if (maxFrames <= 0) maxFrames = 4;
            if (maxFrames > 16) maxFrames = 16;
            
            MotionBlur::InitializeBackbufferStorage(maxFrames);
            
            // Copy backbuffer to SRV
            ID3D11ShaderResourceView* srv = MotionBlur::CopyBackbufferToSRV(pDevice, pContext, pSwapChain);
            if (srv) {
                // Remove oldest frame if exceeding max frames
                if ((int)MotionBlur::g_previousFrames.size() >= maxFrames) {
                    if (MotionBlur::g_previousFrames[0]) MotionBlur::g_previousFrames[0]->Release();
                    MotionBlur::g_previousFrames.erase(MotionBlur::g_previousFrames.begin());
                    MotionBlur::g_frameTimestamps.erase(MotionBlur::g_frameTimestamps.begin());
                }
                
                MotionBlur::g_previousFrames.push_back(srv);
                MotionBlur::g_frameTimestamps.push_back((float)GetTickCount64() / 1000.0f);
            }
        }
        
        pBackBuffer->Release();
    }

    if (sw <= 0) return Hook::oPresent(pSwapChain, 0, Flags);  // VSync disabled

    // Insert key - open menu
    if ((GetAsyncKeyState(VK_INSERT) & 0x8000) && (GetTickCount64() - g_lastToggle) > 400) {
        g_showMenu = !g_showMenu;
        g_lastToggle = GetTickCount64();
    }
    // Sync GUI state
    GUI::g_showMenu = g_showMenu;

    // 📊 ANIMATION UPDATE - Simple y elegante
    ULONGLONG now = GetTickCount64();
    float dt = (float)(now - g_lastTime) / 1000.0f;
    g_lastTime = now;
    
    // 📉 MENU ANIMATION - Delegado a GUI
    GUI::UpdateAnimation(now, dt);
    // 📊 Render Info + module animations
    Module::UpdateAnimation(now);

    // ⌨️ CPS COUNTER - LMB y RMB
    bool lmbPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rmbPressed = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    CPSCounter::UpdateCPS(now, lmbPressed, rmbPressed, Input::g_prevLmbPressed, Input::g_prevRmbPressed);
    Input::g_prevLmbPressed = lmbPressed;
    Input::g_prevRmbPressed = rmbPressed;
    
    // CPS Counter Animation - Fade in/out exponencial
    CPSCounter::UpdateAnimation(now);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    
    // Update input system (keyboard and mouse) - delegated to Input class
    bool drawImGuiCursor = (g_showMenu && IsInWorld());
    Input::Update(g_window, sw, sh, g_showMenu, drawImGuiCursor);

    ImGui::NewFrame();

    // ArrayList - always visible top right
    // Handle drag for ArrayList
    g_arrayListHud.HandleDrag(g_showMenu);
    g_arrayListHud.ClampToScreen();

    // Initialize arrayList position to top-right on first frame
    if (g_arrayListHud.pos.x == 0 && g_arrayListHud.pos.y == 10) {
        g_arrayListHud.pos.x = sw - 250;  // Right side with padding
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 arrayListStart = g_arrayListHud.pos;
    ImVec2 arrayListEnd = arrayListStart;
    float yPos = arrayListStart.y;
    char rBuf[64], hBuf[64];
    const float FADE_OUT_TIME = 0.15f;
    const float FADE_IN_TIME = 0.12f;
    const float SLIDE_TIME = 0.25f;

    Module::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);

    // Debug: Draw ArrayList collision area
    if (g_showMenu) {
        g_arrayListHud.size.y = arrayListEnd.y - arrayListStart.y;
        draw->AddRect(
            arrayListStart,
            ImVec2(arrayListStart.x + g_arrayListHud.size.x, arrayListEnd.y),
            IM_COL32(255, 255, 255, 80)
        );
    }
        
    // Initial notification - delegado a GUI
    GUI::RenderNotification(sw, sh);

    // Menu drawing - delegado a GUI
    if (GUI::g_menuAnim > 0.001f) {
        GUI::RenderMenu(sw, sh);
    }
    
    Module::RenderDisplay(sw, sh);

    ImGui::Render();
    
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    // 🔒 UNLOCK FPS - Frame rate limiting (using UnlockFPS class)
    UnlockFPS::UpdateFPS();
    
    // Forzar VSync OFF para UWP - SyncInterval = 0 desabilita VSync completamente
    return Hook::oPresent(pSwapChain, 0, Flags);
}

// Startup thread function
DWORD WINAPI MainThread(LPVOID lpReserved) {
    // Audio disabled to prevent crashes - enable only if needed
    // if (ma_engine_init(NULL, &g_audioEngine) != MA_SUCCESS) {
    //     // Failed to init audio, continue without it
    // }
    
    UnlockFPS::Initialize();
    UnlockFPS::SetFPS(60.0f);
    Hook::Initialize();
    return 0;
}

// Initialization entry point for the DLL
BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    return TRUE;
}
