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
#pragma comment(lib, "psapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global variables
WNDPROC oWndProc = NULL;
HMODULE g_hModule = NULL;

ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
HWND g_window = NULL;

bool g_showMenu = false;
bool g_prevShowMenu = false;
bool g_RequestUnload = false;
float g_lastW = 0, g_lastH = 0;
ULONGLONG g_tabChangeTime = 0;
float g_tabAnim = 0.0f;

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

ULONGLONG g_lastTime = 0, g_lastToggle = 0, g_notifStart = 0;
bool g_vsync = false;
uintptr_t g_gameBase = 0;

// HUD elements
HudElement g_watermarkHud = { ImVec2(10, 10), ImVec2(400, 80) };
HudElement g_renderInfoHud = { ImVec2(10, 100), ImVec2(220, 120) };
HudElement g_arrayListHud = { ImVec2(0, 10), ImVec2(300, 400) };
HudElement g_keystrokesHud = { ImVec2(30, 0), ImVec2(140, 150) };
HudElement g_cpsHud = { ImVec2(500, 400), ImVec2(80, 30) };
HudElement g_fpsHud = { ImVec2(0, 0), ImVec2(80, 30) };

bool IsWindowsCursorVisible() {
    CURSORINFO ci = { 0 };
    ci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&ci)) {
        return (ci.flags & CURSOR_SHOWING) != 0;
    }
    return false;
}

bool IsInWorld() {
    return !IsWindowsCursorVisible();
}

void CleanupRenderTarget() {
    if (mainRenderTargetView) { 
        mainRenderTargetView->Release(); 
        mainRenderTargetView = NULL; 
    }
}

HRESULT STDMETHODCALLTYPE hkPresent_Impl(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_vsync)
        SyncInterval = 0;
    
    if (!pDevice) {
        if (!pSwapChain) return Hook::oPresent(pSwapChain, SyncInterval, Flags);
        
        ID3D11Device* tempDevice = NULL;
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&tempDevice))) {
            pDevice = tempDevice;
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd = {0};
            if (SUCCEEDED(pSwapChain->GetDesc(&sd))) {
                g_window = sd.OutputWindow;
            }
            if (!g_window) g_window = GetForegroundWindow();
            
            ImGui::CreateContext();
            if (ImGui_ImplWin32_Init(g_window) && ImGui_ImplDX11_Init(pDevice, pContext)) {
                GUI::LoadFont();
                GUI::LoadIcons(pDevice);
                ImGui_ImplDX11_CreateDeviceObjects();
                GUI::ApplyTheme();
                
                Watermark::g_watermarkEnableTime = GetTickCount64();
                Watermark::g_watermarkAnim = 1.0f;
                
                g_gameBase = (uintptr_t)GetModuleHandleA(NULL);
                Module::Initialize(g_gameBase, &g_renderInfoHud, &g_watermarkHud, &g_keystrokesHud, &g_cpsHud, &g_fpsHud);
                
                // Auto-load saved config
                ConfigManager::AutoLoad();
                
                g_notifStart = GetTickCount64();
                g_lastTime = GetTickCount64();
            }
        }
    }

    ID3D11Texture2D* pBackBuffer = NULL;
    float sw = 0, sh = 0;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
        D3D11_TEXTURE2D_DESC desc;
        pBackBuffer->GetDesc(&desc);
        sw = (float)desc.Width; 
        sh = (float)desc.Height;
        
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(sw, sh);
        
        if (!mainRenderTargetView) pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        
        if (MotionBlur::g_motionBlurEnabled) {
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
            ID3D11ShaderResourceView* srv = MotionBlur::CopyBackbufferToSRV(pDevice, pContext, pSwapChain);
            if (srv) {
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

    if (sw <= 0) return Hook::oPresent(pSwapChain, 0, Flags);

    if ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) && (GetTickCount64() - g_lastToggle) > 400) {
        g_showMenu = !g_showMenu;
        g_lastToggle = GetTickCount64();
    }
    // Auto-save config when menu closes
    if (g_prevShowMenu && !g_showMenu) {
        ConfigManager::AutoSave();
    }
    g_prevShowMenu = g_showMenu;
    GUI::g_showMenu = g_showMenu;

    ULONGLONG now = GetTickCount64();
    float dt = (float)(now - g_lastTime) / 1000.0f;
    g_lastTime = now;
    
    GUI::UpdateAnimation(now, dt);
    Module::UpdateAnimation(now);

    bool lmbPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rmbPressed = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    CPSCounter::UpdateCPS(now, lmbPressed, rmbPressed, Input::g_prevLmbPressed, Input::g_prevRmbPressed);
    Input::g_prevLmbPressed = lmbPressed;
    Input::g_prevRmbPressed = rmbPressed;
    CPSCounter::UpdateAnimation(now);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    
    bool drawImGuiCursor = (g_showMenu && IsInWorld());
    Input::Update(g_window, sw, sh, g_showMenu, drawImGuiCursor);

    ImGui::NewFrame();

    // ArrayList
    if (ArrayList::g_showArrayList) {
        g_arrayListHud.HandleDrag(g_showMenu);
        g_arrayListHud.ClampToScreen();
        if (g_arrayListHud.pos.x == 0 && g_arrayListHud.pos.y == 10) g_arrayListHud.pos.x = sw - 250;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec2 arrayListStart = g_arrayListHud.pos;
        float yPos = arrayListStart.y;
        ImVec2 arrayListEnd = arrayListStart;
        Module::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);

        if (g_showMenu) {
            g_arrayListHud.size.y = arrayListEnd.y - arrayListStart.y;
            draw->AddRect(arrayListStart, ImVec2(arrayListStart.x + g_arrayListHud.size.x, arrayListEnd.y), IM_COL32(255, 255, 255, 80));
        }
    }
        
    GUI::RenderNotification(sw, sh);
    if (GUI::g_menuAnim > 0.001f) GUI::RenderMenu(sw, sh);
    Module::RenderDisplay(sw, sh);

    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    UnlockFPS::UpdateFPS();
    return Hook::oPresent(pSwapChain, 0, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved) {
    UnlockFPS::Initialize();
    UnlockFPS::SetFPS(60.0f);
    Hook::Initialize();
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    return TRUE;
}
