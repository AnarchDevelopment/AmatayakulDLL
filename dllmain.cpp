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

#include "minhook\MinHook.h"
#include "ImGui\imgui.h"
#include "ImGui\backend\imgui_impl_dx11.h"
#include "ImGui\backend\imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "psapi.lib");

// HUD Element struct
struct HudElement {
    ImVec2 pos;
    ImVec2 size;

    bool dragging = false;
    ImVec2 dragOffset;

    void HandleDrag(bool menuOpen) {
        if (!menuOpen) return;

        ImVec2 mouse = ImGui::GetIO().MousePos;

        bool hover =
            mouse.x >= pos.x && mouse.x <= pos.x + size.x &&
            mouse.y >= pos.y && mouse.y <= pos.y + size.y;

        if (hover && ImGui::IsMouseClicked(0)) {
            dragging = true;
            dragOffset = ImVec2(mouse.x - pos.x, mouse.y - pos.y);
        }

        if (!ImGui::IsMouseDown(0))
            dragging = false;

        if (dragging) {
            pos = ImVec2(mouse.x - dragOffset.x, mouse.y - dragOffset.y);
        }
    }

    void ClampToScreen() {
        ImVec2 screen = ImGui::GetIO().DisplaySize;

        if (pos.x < 0) pos.x = 0;
        if (pos.y < 0) pos.y = 0;

        if (pos.x + size.x > screen.x)
            pos.x = screen.x - size.x;

        if (pos.y + size.y > screen.y)
            pos.y = screen.y - size.y;
    }
};

// Entity motion packet structure

// Declarations and interfaces
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(STDMETHODCALLTYPE* Present)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffers)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
typedef BOOL(WINAPI* SetCursorPos_t)(int x, int y); 
typedef BOOL(WINAPI* ClipCursor_t)(const RECT* lpRect);

// ImGui configuration flags
ImGuiConfigFlags g_imguiConfigFlags = ImGuiConfigFlags_None;

// Global variables
Present oPresent = NULL;
ResizeBuffers oResizeBuffers = NULL;
SetCursorPos_t oSetCursorPos = NULL;
ClipCursor_t oClipCursor = NULL;
WNDPROC oWndProc = NULL;

ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
HWND g_window = NULL;

bool g_showMenu = false;
float g_menuAnim = 0.0f;
ULONGLONG g_lastTime = 0, g_lastToggle = 0, g_notifStart = 0;
bool g_vsync = false;
float g_lastW = 0, g_lastH = 0;

// Module animations
ULONGLONG g_reachEnableTime = 0;
ULONGLONG g_hitboxEnableTime = 0, g_hitboxDisableTime = 0;
ULONGLONG g_autoSprintEnableTime = 0, g_autoSprintDisableTime = 0;
ULONGLONG g_fullBrightEnableTime = 0, g_fullBrightDisableTime = 0;
ULONGLONG g_timerEnableTime = 0, g_timerDisableTime = 0;
ULONGLONG g_unlockFpsEnableTime = 0, g_unlockFpsDisableTime = 0;

// Tab animation
int g_currentTab = 0;
int g_previousTab = 0;
ULONGLONG g_tabChangeTime = 0;
float g_tabAnim = 0.0f;

// Keyboard input system
bool g_keys[256] = {};
bool g_keysPressed[256] = {};
bool g_keysReleased[256] = {};

// Virtual key to ImGui key mapping
ImGuiKey VKToImGuiKey(int vk)
{
    switch (vk)
    {
        case VK_TAB:            return ImGuiKey_Tab;
        case VK_LEFT:           return ImGuiKey_LeftArrow;
        case VK_RIGHT:          return ImGuiKey_RightArrow;
        case VK_UP:             return ImGuiKey_UpArrow;
        case VK_DOWN:           return ImGuiKey_DownArrow;
        case VK_PRIOR:          return ImGuiKey_PageUp;
        case VK_NEXT:           return ImGuiKey_PageDown;
        case VK_HOME:           return ImGuiKey_Home;
        case VK_END:            return ImGuiKey_End;
        case VK_INSERT:         return ImGuiKey_Insert;
        case VK_DELETE:         return ImGuiKey_Delete;
        case VK_BACK:           return ImGuiKey_Backspace;
        case VK_SPACE:          return ImGuiKey_Space;
        case VK_RETURN:         return ImGuiKey_Enter;
        case VK_ESCAPE:         return ImGuiKey_Escape;
        case VK_SHIFT:          return ImGuiKey_LeftShift;
        case VK_LSHIFT:         return ImGuiKey_LeftShift;
        case VK_RSHIFT:         return ImGuiKey_RightShift;
        case VK_CONTROL:        return ImGuiKey_LeftCtrl;
        case VK_LCONTROL:       return ImGuiKey_LeftCtrl;
        case VK_RCONTROL:       return ImGuiKey_RightCtrl;
        case VK_MENU:           return ImGuiKey_LeftAlt;
        case VK_LMENU:          return ImGuiKey_LeftAlt;
        case VK_RMENU:          return ImGuiKey_RightAlt;
        case VK_LWIN:           return ImGuiKey_LeftSuper;
        case VK_RWIN:           return ImGuiKey_RightSuper;
        case VK_CAPITAL:        return ImGuiKey_CapsLock;
        case VK_SCROLL:         return ImGuiKey_ScrollLock;
        case VK_NUMLOCK:        return ImGuiKey_NumLock;
        case VK_F1:             return ImGuiKey_F1;
        case VK_F2:             return ImGuiKey_F2;
        case VK_F3:             return ImGuiKey_F3;
        case VK_F4:             return ImGuiKey_F4;
        case VK_F5:             return ImGuiKey_F5;
        case VK_F6:             return ImGuiKey_F6;
        case VK_F7:             return ImGuiKey_F7;
        case VK_F8:             return ImGuiKey_F8;
        case VK_F9:             return ImGuiKey_F9;
        case VK_F10:            return ImGuiKey_F10;
        case VK_F11:            return ImGuiKey_F11;
        case VK_F12:            return ImGuiKey_F12;
        default:                return ImGuiKey_None;
    }
}

void UpdateInputSystem(bool menuOpen)
{
    ImGuiIO& io = ImGui::GetIO();

    // Update keyboard state
    for (int i = 0; i < 256; i++)
    {
        // Ignore F13-F24
        if (i >= VK_F13 && i <= VK_F24)
            continue;

        bool down = (GetAsyncKeyState(i) & 0x8000) != 0;

        g_keysPressed[i]  = down && !g_keys[i];
        g_keysReleased[i] = !down && g_keys[i];
        g_keys[i]         = down;

        // Send special keys to ImGui
        ImGuiKey imgui_key = VKToImGuiKey(i);
        if (imgui_key != ImGuiKey_None)
        {
            if (g_keysPressed[i])
                io.AddKeyEvent(imgui_key, true);
            if (g_keysReleased[i])
                io.AddKeyEvent(imgui_key, false);
        }
    }

    // Update modifiers
    io.KeyCtrl  = g_keys[VK_CONTROL];
    io.KeyShift = g_keys[VK_SHIFT];
    io.KeyAlt   = g_keys[VK_MENU];
    io.KeySuper = g_keys[VK_LWIN] || g_keys[VK_RWIN];

    // Add text characters
    if (menuOpen)
    {
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);

        WCHAR buffer[4];

        for (int vk = 0; vk < 256; vk++)
        {
            if (vk >= VK_F13 && vk <= VK_F24)
                continue;

            if (g_keysPressed[vk])
            {
                UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);

                int result = ToUnicode(vk, scanCode, keyboardState, buffer, 4, 0);

                if (result > 0)
                {
                    for (int i = 0; i < result; i++)
                        io.AddInputCharacter((unsigned short)buffer[i]);
                }
            }
        }
    }
}
bool g_firstTabOpen = true;

// Memory access helpers
uintptr_t g_gameBase = 0;
float* g_reachPtr = nullptr;
static float g_reachValue = 3.0f;

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

// Hitbox data
bool g_hitboxEnabled = false;
static float g_hitboxValue = 0.6f;
uintptr_t g_hitboxAddr = 0;
void* g_hitboxCave = nullptr;
BYTE g_hitboxBackup[8] = { 0 };

// AutoSprint data
bool g_autoSprintEnabled = false;
uintptr_t g_autoSprintAddr = 0;
void* g_autoSprintCave = nullptr;
BYTE g_autoSprintBackup[11] = { 0 };

// FullBright data
bool g_fullBrightEnabled = false;
static float g_fullBrightValue = 100.0f;
uintptr_t g_fullBrightAddr = 0;
void* g_fullBrightCave = nullptr;
BYTE g_fullBrightBackup[8] = { 0 };

// Timer data
bool g_timerEnabled = false;
static float g_timerValue = 1.0f;
uintptr_t g_timerAddr = 0;
HANDLE g_timerWriteThread = NULL;
bool g_timerRunning = false;

// Render info data
bool g_showRenderInfo = false;
float g_fpsCounter = 0.0f;
float g_frameTime = 0.0f;
ULONGLONG g_renderInfoEnableTime = 0;
ULONGLONG g_renderInfoDisableTime = 0;
float g_renderInfoAnim = 0.0f;

// Motion blur data
bool g_motionBlurEnabled = false;
std::string g_blurType = "Average Pixel Blur";
float g_blurIntensity = 3.0f;
std::vector<ID3D11ShaderResourceView*> g_previousFrames;
std::vector<ID3D11Texture2D*> g_backbufferTextures;
std::vector<float> g_frameTimestamps;
float g_blurTimeConstant = 0.0667f;
float g_maxHistoryFrames = 8.0f;
bool g_blurDynamicMode = false;
float g_motionBlurAnim = 0.0f;
ULONGLONG g_motionBlurEnableTime = 0;
ULONGLONG g_motionBlurDisableTime = 0;
int g_maxBackbufferFrames = 8;
int g_currentBackbufferIndex = 0;

// Motion blur helper
ID3D11PixelShader* g_avg_pixelShader = nullptr;
ID3D11VertexShader* g_avg_vertexShader = nullptr;
ID3D11InputLayout* g_avg_inputLayout = nullptr;
ID3D11Buffer* g_avg_constantBuffer = nullptr;
ID3D11Buffer* g_avg_vertexBuffer = nullptr;
ID3D11DepthStencilState* g_avg_depthStencilState = nullptr;
ID3D11BlendState* g_avg_blendState = nullptr;
ID3D11RasterizerState* g_avg_rasterizerState = nullptr;
ID3D11SamplerState* g_avg_samplerState = nullptr;
bool g_avg_hlperInitialized = false;

// Real motion blur helper
ID3D11PixelShader* g_real_pixelShader = nullptr;
ID3D11VertexShader* g_real_vertexShader = nullptr;
ID3D11InputLayout* g_real_inputLayout = nullptr;
ID3D11Buffer* g_real_constantBuffer = nullptr;
ID3D11Buffer* g_real_vertexBuffer = nullptr;
ID3D11DepthStencilState* g_real_depthStencilState = nullptr;
ID3D11BlendState* g_real_blendState = nullptr;
ID3D11RasterizerState* g_real_rasterizerState = nullptr;
ID3D11SamplerState* g_real_samplerState = nullptr;
bool g_real_helperInitialized = false;

// Watermark data
bool g_showWatermark = true;
ULONGLONG g_watermarkEnableTime = 0;
ULONGLONG g_watermarkDisableTime = 0;
float g_watermarkAnim = 1.0f;

// Unlock FPS data
bool g_unlockFpsEnabled = false;
static float g_fpsLimit = 60.0f;
ULONGLONG g_lastFrameTime = 0;

// Keystrokes data
bool g_showKeystrokes = false;
float g_keystrokesAnim = 0.0f;
ULONGLONG g_keystrokesEnableTime = 0;
ULONGLONG g_keystrokesDisableTime = 0;

// CPS counter data
#define MAX_CPS_HISTORY 100
ULONGLONG g_lmbClickTimes[MAX_CPS_HISTORY] = {};
ULONGLONG g_rmbClickTimes[MAX_CPS_HISTORY] = {};
int g_lmbClickIndex = 0;
int g_rmbClickIndex = 0;
int g_lmbCps = 0;
int g_rmbCps = 0;

bool g_prevLmbPressed = false;
bool g_prevRmbPressed = false;

// Keystrokes config
float g_keystrokesUIScale = 1.0f;
bool g_keystrokesBlurEffect = false;
float g_keystrokesRounding = 11.0f;
bool g_keystrokesShowBg = false;
bool g_keystrokesRectShadow = false;
float g_keystrokesRectShadowOffset = 0.02f;
bool g_keystrokesBorder = false;
float g_keystrokesBorderWidth = 1.0f;
bool g_keystrokesGlow = false;
float g_keystrokesGlowAmount = 50.0f;
bool g_keystrokesGlowEnabled = false;
float g_keystrokesGlowEnabledAmount = 50.0f;
float g_keystrokesGlowSpeed = 1.0f;
float g_keystrokesKeySpacing = 1.63f;
float g_keystrokesEdSpeed = 1.0f;
float g_keystrokesTextScale = 1.0f;
float g_keystrokesTextScale2 = 1.0f;
float g_keystrokesTextXOffset = 0.5f;
float g_keystrokesTextYOffset = 0.5f;
bool g_keystrokesTextShadow = false;
float g_keystrokesTextShadowOffset = 0.003f;
bool g_keystrokesLMBRMB = true;
bool g_keystrokesHideCPS = true;
std::string g_keystrokesWText = "W";
std::string g_keystrokesAText = "A";
std::string g_keystrokesSText = "S";
std::string g_keystrokesDText = "D";
std::string g_keystrokesLMBText = "LMB";
std::string g_keystrokesRMBText = "RMB";
std::string g_keystrokesLMBCPSText = "{value} CPS";
std::string g_keystrokesRMBCPSText = "{value} CPS";

// Keystrokes colors
ImVec4 g_keystrokesBgColor = ImVec4(0.314f, 0.314f, 0.314f, 0.55f);
ImVec4 g_keystrokesEnabledColor = ImVec4(0.0f, 1.0f, 0.4f, 1.0f);
ImVec4 g_keystrokesTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
ImVec4 g_keystrokesTextEnabledColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
ImVec4 g_keystrokesRectShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
ImVec4 g_keystrokesTextShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
ImVec4 g_keystrokesBorderColor = ImVec4(0.78f, 0.78f, 0.78f, 1.0f);
ImVec4 g_keystrokesGlowColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
ImVec4 g_keystrokesGlowEnabledColor = ImVec4(0.941f, 0.941f, 1.0f, 1.0f);

// Keystrokes state colors
std::vector<ImVec4> g_keystrokesStates(7, ImVec4(0.314f, 0.314f, 0.314f, 0.55f));
std::vector<ImVec4> g_keystrokesTextStates(7, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
std::vector<ImVec4> g_keystrokesShadowStates(7, ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
float g_keystrokesGlowModifier[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

// CPS format variables
std::string g_cpsCounterFormat = "CPS: {lmb} | {rmb}";
float g_cpsTextScale = 1.0f;
ImVec4 g_cpsTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

// CPS animation
bool g_showCpsCounter = true;
float g_cpsCounterAnim = 1.0f;
ULONGLONG g_cpsCounterEnableTime = 0;
ULONGLONG g_cpsCounterDisableTime = 0;

// CPS config
int g_cpsCounterAlignment = 0;
bool g_cpsCounterShadow = true;
float g_cpsCounterShadowOffset = 2.0f;
ImVec4 g_cpsCounterShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
float g_cpsCounterX = 0.5f;
float g_cpsCounterY = 0.95f;
bool g_cpsCounterFirstRender = false;

// Keystrokes mouse buttons config
bool g_keystrokesShowMouseButtons = true;
bool g_keystrokesShowLMBRMB = true;
bool g_keystrokesShowSpacebar = true;
float g_keystrokesSpacebarWidth = 0.5f;
float g_keystrokesSpacebarHeight = 0.09f;

// Keystrokes shadow colors
ImVec4 g_keystrokesDisabledShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
ImVec4 g_keystrokesEnabledShadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

// LMB/RMB format variables
std::string g_keystrokesLMBFormatText = "{value} CPS";
std::string g_keystrokesRMBFormatText = "{value} CPS";

// HUD elements
HudElement g_watermarkHud = { ImVec2(10, 10), ImVec2(400, 80) };
HudElement g_renderInfoHud = { ImVec2(10, 100), ImVec2(220, 120) };
HudElement g_arrayListHud = { ImVec2(0, 10), ImVec2(300, 400) };
HudElement g_keystrokesHud = { ImVec2(30, 0), ImVec2(140, 150) };
HudElement g_cpsHud = { ImVec2(500, 400), ImVec2(80, 30) };

// Input blocking
static HHOOK g_keyboardHook = nullptr;

// Keyboard hook
LRESULT CALLBACK KeyboardBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
    }
    
    // Allow all input when menu is open
    if (g_showMenu) {
        return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
    }
    
    // Block input when menu is closed
    if (nCode == HC_ACTION) {
        PKBDLLHOOKSTRUCT pKey = (PKBDLLHOOKSTRUCT)lParam;
        if (pKey) {
            // Allow INSERT key
            if (pKey->vkCode == VK_INSERT) {
                return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
            }
            // Block other keys
            return 1;
        }
    }
    
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

void BlockGameInput() {
    // Install hook if needed
    if (!g_keyboardHook) {
        HMODULE hMod = GetModuleHandleA("internal_hook");
        if (!hMod) hMod = GetModuleHandleA(nullptr);
        g_keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardBlockHookProc, hMod, 0);
    }
}

void UnblockGameInput() {
    // Remove keyboard hook
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
}

// Easing animations
float SmoothInertia(float t) { 
    // Cubic easing
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float f = 2.0f * t - 2.0f;
        return 0.5f * f * f * f + 1.0f;
    }
}

float EaseInOutQuad(float t) {
    // Quadratic easing
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

float EaseOutExpo(float t) {
    // Exponential easing
    return t == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
}

ImVec4 GetChromaColor(float time) {
    // Chroma color cycle
    
    ImVec4 colors[3] = {
        ImVec4(0x87/255.f, 0xF8/255.f, 0xFF/255.f, 1.0f),
        ImVec4(0x87/255.f, 0x97/255.f, 0xFF/255.f, 1.0f),
        ImVec4(0xE9/255.f, 0x87/255.f, 0xFF/255.f, 1.0f)
    };
    
    float cycleTime = fmodf(time * 2.0f, 3.0f);
    int colorIdx = (int)cycleTime;
    float blend = cycleTime - colorIdx;
    
    int nextIdx = (colorIdx + 1) % 3;
    ImVec4& c1 = colors[colorIdx];
    ImVec4& c2 = colors[nextIdx];
    
    return ImVec4(
        c1.x + (c2.x - c1.x) * blend,
        c1.y + (c2.y - c1.y) * blend,
        c1.z + (c2.z - c1.z) * blend,
        1.0f
    );
}

float EaseInOutElastic(float t) {
    // Elastic bounce effect
    const float c5 = (2.0f * 3.14159265f) / 4.5f;
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f 
        ? -(powf(2.0f, 20.0f * t - 10.0f) * sinf((t * 2.0f - 0.675f) * c5)) / 2.0f
        : (powf(2.0f, -20.0f * t + 10.0f) * sinf((t * 2.0f - 0.675f) * c5)) / 2.0f + 1.0f;
}

// ImVec4 lerp helper
ImVec4 LerpImVec4(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

// CPS format processor
std::string ProcessCPSCounterFormat(const std::string& format, int lmb, int rmb) {
    std::string result = format;
    
    // Replace placeholders
    std::string formatUpper = format;
    for (char& c : formatUpper) c = std::toupper(c);
    
    // Replace LMB placeholder
    size_t pos = formatUpper.find("{LMB}");
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

// Keystrokes format processor
std::string ProcessKeystrokesFormat(const std::string& format, int value) {
    std::string result = format;
    std::string formatUpper = format;
    for (char& c : formatUpper) c = std::toupper(c);
    
    size_t pos = formatUpper.find("{VALUE}");
    if (pos != std::string::npos) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", value);
        result.replace(pos, 7, buffer);
    }
    
    return result;
}

// ImGui resize fix
// Sync ImGui + DX11
void SyncImGuiAndDX11(IDXGISwapChain* pSwapChain, float& width, float& height)
{
    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);

    // Fallback for full screen edge cases
    if (width <= 0 || height <= 0)
    {
        RECT rect;
        GetClientRect(sd.OutputWindow, &rect);
        width  = (float)(rect.right - rect.left);
        height = (float)(rect.bottom - rect.top);
    }

    // Real viewport in pixels
    D3D11_VIEWPORT vp;
    vp.Width    = width;
    vp.Height   = height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    pContext->RSSetViewports(1, &vp);

    // ImGui DisplaySize matches backbuffer size
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);
    
    // DisplayFramebufferScale = 1.0f (backbuffer es la fuente de verdad)
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void* AllocateNear(uintptr_t reference, size_t size) {
    intptr_t deltas[] = { 0x1000000, 0x2000000, 0x4000000, -0x1000000, -0x2000000, -0x4000000 };
    for (int i = 0; i < 6; i++) {
        uintptr_t target = reference + deltas[i];
        void* addr = VirtualAlloc((void*)target, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (addr) {
            // Check if within 32-bit range
            int64_t jmpRel = (uintptr_t)addr - (reference + 5);
            if (jmpRel >= -0x80000000LL && jmpRel <= 0x7FFFFFFFLL) {
                return addr;
            }
            VirtualFree(addr, 0, MEM_RELEASE);
        }
    }
    return NULL; // No near address found
}

uintptr_t PatternScan(uintptr_t start, size_t size, const BYTE* pattern, size_t patternSize) {
    for (size_t i = 0; i <= size - patternSize; ++i) {
        if (memcmp((void*)(start + i), pattern, patternSize) == 0) {
            return start + i;
        }
    }
    return 0;
}

// Motion blur swapchain utilities
// AvgPixelMotionBlurHelper - HLSL Shaders (Inline)
const char* g_avgPixelShaderSrc = R"(
cbuffer FrameCountBuffer : register(b0)
{
    int numFrames;
    float3 padding;
};
Texture2D g_frames[50] : register(t0);
SamplerState g_sampler : register(s0);
struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};
float4 mainPS(VS_OUTPUT input) : SV_Target
{
    float4 color = float4(0, 0, 0, 0);
    int safeNumFrames = max(numFrames, 1);
    [unroll]
    for (int i = 0; i < safeNumFrames && i < 50; i++)
    {
        color += g_frames[i].Sample(g_sampler, input.Tex);
    }
    return color / safeNumFrames;
}
)";

const char* g_avgVertexShaderSrc = R"(
struct VS_INPUT {
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};
struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};
VS_OUTPUT mainVS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Pos = float4(input.Pos, 1.0);
    output.Tex = input.Tex;
    return output;
}
)";

// RealMotionBlurHelper - HLSL Shaders (GPU Gems 3)
const char* g_realVertexShaderSrc = R"(
cbuffer CameraDataBuffer : register(b0)
{
    float4x4 preWorldViewProjection;
    float4x4 invWorldViewProjection;
    float intensity;
    int numSamples;
    float2 padding;
};
struct VS_INPUT {
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};
struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};
VS_OUTPUT mainVS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Pos = float4(input.Pos, 1.0);
    output.Tex = input.Tex;
    return output;
}
)";

const char* g_realPixelShaderSrc = R"(
cbuffer CameraDataBuffer : register(b0)
{
    float4x4 preWorldViewProjection;
    float4x4 invWorldViewProjection;
    float intensity;
    int numSamples;
    float2 padding;
};
Texture2D g_velocityTex : register(t0);
Texture2D g_colorTex : register(t1);
SamplerState g_sampler : register(s0);
struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};
float4 mainPS(VS_OUTPUT input) : SV_Target
{
    float2 velocity = g_velocityTex.Sample(g_sampler, input.Tex).xy;
    float4 color = g_colorTex.Sample(g_sampler, input.Tex);
    
    float2 samplePos = input.Tex;
    int samples = max(numSamples, 1);
    
    [unroll]
    for(int i = 1; i < samples && i < 16; i++)
    {
        float t = (float)i / (float)samples;
        samplePos -= velocity * intensity * 0.1;
        color += g_colorTex.Sample(g_sampler, samplePos);
    }
    
    return color / samples;
}
)";

bool CompileMotionBlurShader(const char* srcData, const char* entryPoint, const char* shaderModel, ID3DBlob** blobOut) {
    UINT compileFlags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompile(srcData, strlen(srcData), nullptr, nullptr, nullptr, entryPoint, shaderModel, compileFlags, 0, blobOut, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    if (errorBlob) errorBlob->Release();
    return true;
}

bool InitializeAvgPixelMotionBlurHelper() {
    if (g_avg_hlperInitialized || !pDevice || !pContext) return false;
    
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    // Compile shaders
    if (!CompileMotionBlurShader(g_avgVertexShaderSrc, "mainVS", "vs_5_0", &vsBlob)) {
        return false;
    }

    hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_avg_vertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float)*3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = pDevice->CreateInputLayout(layoutDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_avg_inputLayout);
    vsBlob->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Compile pixel shader
    if (!CompileMotionBlurShader(g_avgPixelShaderSrc, "mainPS", "ps_5_0", &psBlob)) {
        return false;
    }

    hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_avg_pixelShader);
    psBlob->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Constant buffer for frame count
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = 16;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = pDevice->CreateBuffer(&cbDesc, nullptr, &g_avg_constantBuffer);
    if (FAILED(hr)) {
        return false;
    }

    // Vertex buffer (fullscreen quad)
    struct Vertex { float x, y, z; float u, v; };
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f,     0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f,     1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f,     0.0f, 1.0f },
        {  1.0f, -1.0f, 0.0f,     1.0f, 1.0f },
    };
    
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    
    hr = pDevice->CreateBuffer(&vbDesc, &initData, &g_avg_vertexBuffer);
    if (FAILED(hr)) {
        return false;
    }

    // Depth stencil state
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = false;
    dsd.StencilEnable = false;
    hr = pDevice->CreateDepthStencilState(&dsd, &g_avg_depthStencilState);
    if (FAILED(hr)) return false;

    // Blend state
    D3D11_BLEND_DESC bd = {};
    bd.AlphaToCoverageEnable = false;
    bd.RenderTarget[0].BlendEnable = true;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    hr = pDevice->CreateBlendState(&bd, &g_avg_blendState);
    if (FAILED(hr)) return false;

    // Rasterizer state
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = false;
    rd.ScissorEnable = false;
    
    hr = pDevice->CreateRasterizerState(&rd, &g_avg_rasterizerState);
    if (FAILED(hr)) return false;

    // Sampler state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    
    hr = pDevice->CreateSamplerState(&sampDesc, &g_avg_samplerState);
    if (FAILED(hr)) return false;

    g_avg_hlperInitialized = true;
    return true;
}

bool InitializeRealMotionBlurHelper() {
    if (g_real_helperInitialized || !pDevice || !pContext) return false;
    
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;

    // Compile vertex shader
    if (!CompileMotionBlurShader(g_realVertexShaderSrc, "mainVS", "vs_5_0", &vsBlob)) {
        return false;
    }

    hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_real_vertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float)*3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = pDevice->CreateInputLayout(layoutDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_real_inputLayout);
    vsBlob->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Compile pixel shader
    if (!CompileMotionBlurShader(g_realPixelShaderSrc, "mainPS", "ps_5_0", &psBlob)) {
        return false;
    }

    hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_real_pixelShader);
    psBlob->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(float) * 20;  // Matrix + intensity + padding
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = pDevice->CreateBuffer(&cbDesc, nullptr, &g_real_constantBuffer);
    if (FAILED(hr)) return false;

    // Vertex buffer
    struct Vertex { float x, y, z; float u, v; };
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f,     0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f,     1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f,     0.0f, 1.0f },
        {  1.0f, -1.0f, 0.0f,     1.0f, 1.0f },
    };
    
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    
    hr = pDevice->CreateBuffer(&vbDesc, &initData, &g_real_vertexBuffer);
    if (FAILED(hr)) return false;

    // Depth stencil state
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = false;
    dsd.StencilEnable = false;
    hr = pDevice->CreateDepthStencilState(&dsd, &g_real_depthStencilState);
    if (FAILED(hr)) return false;

    // Blend state
    D3D11_BLEND_DESC bd = {};
    bd.AlphaToCoverageEnable = false;
    bd.RenderTarget[0].BlendEnable = true;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    hr = pDevice->CreateBlendState(&bd, &g_real_blendState);
    if (FAILED(hr)) return false;

    // Rasterizer state
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = false;
    rd.ScissorEnable = false;
    
    hr = pDevice->CreateRasterizerState(&rd, &g_real_rasterizerState);
    if (FAILED(hr)) return false;

    // Sampler state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    
    hr = pDevice->CreateSamplerState(&sampDesc, &g_real_samplerState);
    if (FAILED(hr)) return false;

    g_real_helperInitialized = true;
    return true;
}

void InitializeBackbufferStorage(int maxFrames) {
    if (g_backbufferTextures.size() >= (size_t)maxFrames) return;
    
    while (g_backbufferTextures.size() < (size_t)maxFrames) {
        g_backbufferTextures.push_back(nullptr);
    }
    g_maxBackbufferFrames = maxFrames;
    
    // Initialize GPU helpers if not already done
    if (!g_avg_hlperInitialized) {
        InitializeAvgPixelMotionBlurHelper();
    }
    if (!g_real_helperInitialized) {
        InitializeRealMotionBlurHelper();
    }
}

ID3D11ShaderResourceView* CopyBackbufferToSRV(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
    if (!pSwapChain || !pDevice || !pContext) return nullptr;
    
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
        return nullptr;
    }
    
    D3D11_TEXTURE2D_DESC desc;
    pBackBuffer->GetDesc(&desc);
    
    // Create texture with SHADER_RESOURCE | RENDER_TARGET for proper blending
    D3D11_TEXTURE2D_DESC texDesc = desc;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    
    ID3D11Texture2D* pTexture = nullptr;
    if (FAILED(pDevice->CreateTexture2D(&texDesc, nullptr, &pTexture))) {
        pBackBuffer->Release();
        return nullptr;
    }
    
    // Copy directly from backbuffer to texture
    pContext->CopyResource(pTexture, pBackBuffer);
    
    // Create SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    
    ID3D11ShaderResourceView* pSRV = nullptr;
    if (FAILED(pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV))) {
        pTexture->Release();
        pBackBuffer->Release();
        return nullptr;
    }
    
    pTexture->Release();
    pBackBuffer->Release();
    
    return pSRV;
}

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

void CleanupBackbufferStorage() {
    for (auto tex : g_backbufferTextures) {
        if (tex) tex->Release();
    }
    g_backbufferTextures.clear();
    
    for (auto srv : g_previousFrames) {
        if (srv) srv->Release();
    }
    g_previousFrames.clear();
    g_frameTimestamps.clear();
    g_currentBackbufferIndex = 0;
}

// Hitbox logic
void DisableHitbox() {
    if (!g_hitboxAddr || g_hitboxBackup[0] == 0) return;
    g_hitboxDisableTime = GetTickCount64();
    DWORD old;
    VirtualProtect((void*)g_hitboxAddr, 8, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)g_hitboxAddr, g_hitboxBackup, 8);
    VirtualProtect((void*)g_hitboxAddr, 8, old, &old);
}


void EnableHitbox() {
    if (!g_hitboxAddr) return;
    g_hitboxEnableTime = GetTickCount64();
    if (!g_hitboxCave) g_hitboxCave = AllocateNear(g_hitboxAddr, 1024);
    if (!g_hitboxCave) return;

    memcpy(g_hitboxBackup, (void*)g_hitboxAddr, 8);

    BYTE shellcode[64] = { 0 };
    int p = 0;
    // mov eax, [valor] | mov [rcx+D0], eax
    shellcode[p++] = 0xB8; memcpy(&shellcode[p], &g_hitboxValue, 4); p += 4; 
    shellcode[p++] = 0x89; shellcode[p++] = 0x81; shellcode[p++] = 0xD0; 
    shellcode[p++] = 0x00; shellcode[p++] = 0x00; shellcode[p++] = 0x00; 
    // Original bytes
    memcpy(&shellcode[p], g_hitboxBackup, 8); p += 8; 
    // jmp back
    shellcode[p++] = 0xE9; 
    uintptr_t retAddr = g_hitboxAddr + 8;
    int32_t jmpBack = (int32_t)(retAddr - ((uintptr_t)g_hitboxCave + p + 4));
    memcpy(&shellcode[p], &jmpBack, 4); p += 4;

    memcpy(g_hitboxCave, shellcode, p);

    DWORD old;
    VirtualProtect((void*)g_hitboxAddr, 8, PAGE_EXECUTE_READWRITE, &old);
    BYTE patch[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    int32_t jmpToCave = (int32_t)((uintptr_t)g_hitboxCave - (g_hitboxAddr + 5));
    memcpy(&patch[1], &jmpToCave, 4);
    memcpy((void*)g_hitboxAddr, patch, 8);
    VirtualProtect((void*)g_hitboxAddr, 8, old, &old);
}

// --- 🏃 AUTOSPRINT ---
void DisableAutoSprint() {
    if (!g_autoSprintAddr || g_autoSprintBackup[0] == 0) return;
    g_autoSprintDisableTime = GetTickCount64();
    DWORD old;
    VirtualProtect((void*)g_autoSprintAddr, 11, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)g_autoSprintAddr, g_autoSprintBackup, 11);
    VirtualProtect((void*)g_autoSprintAddr, 11, old, &old);
}

void EnableAutoSprint() {
    if (!g_autoSprintAddr) return;
    g_autoSprintEnableTime = GetTickCount64();
    if (!g_autoSprintCave) g_autoSprintCave = AllocateNear(g_autoSprintAddr, 1024);
    if (!g_autoSprintCave) return;

    memcpy(g_autoSprintBackup, (void*)g_autoSprintAddr, 11);

    BYTE shellcode[64] = { 0 };
    int p = 0;
    // mov eax, 6
    shellcode[p++] = 0xB8; int sprintValue = 6; memcpy(&shellcode[p], &sprintValue, 4); p += 4;
    // jmp back
    shellcode[p++] = 0xE9;
    uintptr_t retAddr = g_autoSprintAddr + 11;
    uintptr_t cur = (uintptr_t)g_autoSprintCave + p;
    int32_t rel = (int32_t)(retAddr - (cur + 4));
    memcpy(&shellcode[p], &rel, 4); p += 4;

    memcpy(g_autoSprintCave, shellcode, p);

    DWORD old;
    VirtualProtect((void*)g_autoSprintAddr, 11, PAGE_EXECUTE_READWRITE, &old);
    BYTE patch[11] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    int32_t jmpToCave = (int32_t)((uintptr_t)g_autoSprintCave - (g_autoSprintAddr + 5));
    memcpy(&patch[1], &jmpToCave, 4);
    memcpy((void*)g_autoSprintAddr, patch, 11);
    VirtualProtect((void*)g_autoSprintAddr, 11, old, &old);
}

// FullBright
void DisableFullBright() {
    if (!g_fullBrightAddr || g_fullBrightBackup[0] == 0) return;
    g_fullBrightDisableTime = GetTickCount64();
    DWORD old;
    VirtualProtect((void*)g_fullBrightAddr, 8, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)g_fullBrightAddr, g_fullBrightBackup, 8);
    VirtualProtect((void*)g_fullBrightAddr, 8, old, &old);
}

void EnableFullBright() {
    if (!g_fullBrightAddr) return;
    g_fullBrightEnableTime = GetTickCount64();
    if (!g_fullBrightCave) g_fullBrightCave = AllocateNear(g_fullBrightAddr, 1024);
    if (!g_fullBrightCave) return;

    memcpy(g_fullBrightBackup, (void*)g_fullBrightAddr, 8);

    BYTE shellcode[64] = { 0 };
    int p = 0;
    // mov eax, [valor]
    shellcode[p++] = 0xB8; memcpy(&shellcode[p], &g_fullBrightValue, 4); p += 4;
    // cvtsi2ss xmm0, eax
    shellcode[p++] = 0xF3; shellcode[p++] = 0x0F; shellcode[p++] = 0x2A; shellcode[p++] = 0xC0;
    // jmp back
    shellcode[p++] = 0xE9;
    uintptr_t retAddr = g_fullBrightAddr + 8;
    int32_t jmpBack = (int32_t)(retAddr - ((uintptr_t)g_fullBrightCave + p + 4));
    memcpy(&shellcode[p], &jmpBack, 4); p += 4;

    memcpy(g_fullBrightCave, shellcode, p);

    DWORD old;
    VirtualProtect((void*)g_fullBrightAddr, 8, PAGE_EXECUTE_READWRITE, &old);
    BYTE patch[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    int32_t jmpToCave = (int32_t)((uintptr_t)g_fullBrightCave - (g_fullBrightAddr + 5));
    memcpy(&patch[1], &jmpToCave, 4);
    memcpy((void*)g_fullBrightAddr, patch, 8);
    VirtualProtect((void*)g_fullBrightAddr, 8, old, &old);
}

// Timer/Multiplier
DWORD WINAPI TimerWriteThread(LPVOID lpParam) {
    if (!g_timerAddr) return 0;
    
    DWORD oldProtect = 0;
    // Cambiar permisos una sola vez al inicio
    BOOL protectChanged = VirtualProtect((void*)g_timerAddr, sizeof(double), PAGE_EXECUTE_READWRITE, &oldProtect);
    
    while (g_timerRunning) {
        if (g_timerAddr && protectChanged) {
            double memValue = (double)g_timerValue * 1000.0;
            // Escribir directamente
            *(double*)g_timerAddr = memValue;
        }
        Sleep(10);  // 100Hz
    }
    
    // Restaurar permisos si fue cambiado
    if (protectChanged) {
        VirtualProtect((void*)g_timerAddr, sizeof(double), oldProtect, &oldProtect);
    }
    return 0;
}

void DisableTimer() {
    if (g_timerWriteThread) {
        g_timerDisableTime = GetTickCount64();
        g_timerRunning = false;
        WaitForSingleObject(g_timerWriteThread, 2000);
        CloseHandle(g_timerWriteThread);
        g_timerWriteThread = NULL;
    }
}

void EnableTimer() {
    if (g_timerWriteThread) DisableTimer();
    
    g_timerEnableTime = GetTickCount64();
    g_timerRunning = true;
    g_timerWriteThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TimerWriteThread, NULL, 0, NULL);
}

// --- 🖱️ INPUT HOOKS ---
BOOL WINAPI hkSetCursorPos(int x, int y) {
    if (g_showMenu) return TRUE;
    return oSetCursorPos(x, y);
}

BOOL WINAPI hkClipCursor(const RECT* lpRect) {
    if (g_showMenu) return oClipCursor(NULL);
    return oClipCursor(lpRect);
}

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_showMenu) {
        // Don't pass to ImGui - manual input handling
        // Only blocks input when menu is open
        switch (uMsg) {
            case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_MBUTTONDOWN:
            case WM_MBUTTONUP: case WM_MOUSEWHEEL: case WM_INPUT: 
                return 1;
        }
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

void CleanupRenderTarget() {
    if (mainRenderTargetView) { 
        mainRenderTargetView->Release(); 
        mainRenderTargetView = NULL; 
    }
}

HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    CleanupRenderTarget();
    g_lastW = 0;
    g_lastH = 0;
    return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

// --- 🎨 RENDER PRINCIPAL ---
HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    // Force VSync OFF when disabled
    if (!g_vsync)
        SyncInterval = 0;
        
    if (!pDevice) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            g_window = sd.OutputWindow;
            if (!g_window) g_window = GetForegroundWindow();

            ImGui::CreateContext();
            ImGui_ImplWin32_Init(g_window);
            ImGui_ImplDX11_Init(pDevice, pContext);
            
            // Cargar fuente Segoe UI del sistema
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = NULL;  // No generar .ini
            // Arial
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16.0f);
            ImGui_ImplDX11_CreateDeviceObjects();
            
            // 🎨 APLICAR TEMA
            {
                ImGuiStyle& style = ImGui::GetStyle();
                
                // 🔹 Bordes y redondeo
                style.WindowRounding = 10.0f;
                style.FrameRounding = 6.0f;
                style.ScrollbarRounding = 6.0f;
                style.GrabRounding = 6.0f;
                style.TabRounding = 6.0f;
                
                // 🔹 Espaciado
                style.WindowPadding = ImVec2(10, 10);
                style.FramePadding = ImVec2(8, 5);
                style.ItemSpacing = ImVec2(8, 6);
                
                // 🔹 Colores
                ImVec4* colors = style.Colors;
                
                // Fondo
                colors[ImGuiCol_WindowBg]         = ImVec4(0.09f, 0.07f, 0.10f, 1.00f);
                
                // Texto
                colors[ImGuiCol_Text]             = ImVec4(0.95f, 0.88f, 0.92f, 1.00f);
                colors[ImGuiCol_TextDisabled]     = ImVec4(0.55f, 0.45f, 0.50f, 1.00f);
                
                // Frames (inputs, sliders)
                colors[ImGuiCol_FrameBg]          = ImVec4(0.18f, 0.12f, 0.16f, 1.00f);
                colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.30f, 0.15f, 0.22f, 1.00f);
                colors[ImGuiCol_FrameBgActive]    = ImVec4(0.35f, 0.18f, 0.25f, 1.00f);
                
                // Botones
                colors[ImGuiCol_Button]           = ImVec4(0.85f, 0.35f, 0.55f, 1.00f);
                colors[ImGuiCol_ButtonHovered]    = ImVec4(0.95f, 0.45f, 0.65f, 1.00f);
                colors[ImGuiCol_ButtonActive]     = ImVec4(0.75f, 0.25f, 0.45f, 1.00f);
                
                // Headers
                colors[ImGuiCol_Header]           = ImVec4(0.80f, 0.30f, 0.50f, 1.00f);
                colors[ImGuiCol_HeaderHovered]    = ImVec4(0.90f, 0.40f, 0.60f, 1.00f);
                colors[ImGuiCol_HeaderActive]     = ImVec4(0.70f, 0.20f, 0.40f, 1.00f);
                
                // Tabs
                colors[ImGuiCol_Tab]              = ImVec4(0.20f, 0.12f, 0.18f, 1.00f);
                colors[ImGuiCol_TabHovered]       = ImVec4(0.85f, 0.35f, 0.55f, 1.00f);
                colors[ImGuiCol_TabActive]        = ImVec4(0.70f, 0.30f, 0.50f, 1.00f);
                
                // Titles
                colors[ImGuiCol_TitleBg]          = ImVec4(0.15f, 0.10f, 0.14f, 1.00f);
                colors[ImGuiCol_TitleBgActive]    = ImVec4(0.30f, 0.15f, 0.22f, 1.00f);
                
                // Scrollbar
                colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.10f, 0.08f, 0.10f, 1.00f);
                colors[ImGuiCol_ScrollbarGrab]    = ImVec4(0.75f, 0.25f, 0.45f, 1.00f);
                colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.85f, 0.35f, 0.55f, 1.00f);
                colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.95f, 0.45f, 0.65f, 1.00f);
                
                // Checkmark
                colors[ImGuiCol_CheckMark]        = ImVec4(1.00f, 0.55f, 0.75f, 1.00f);
                
                // Slider
                colors[ImGuiCol_SliderGrab]       = ImVec4(0.95f, 0.45f, 0.65f, 1.00f);
                colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.55f, 0.75f, 1.00f);
                
                // Otros elementos
                colors[ImGuiCol_Separator]        = ImVec4(0.75f, 0.25f, 0.45f, 1.00f);
                colors[ImGuiCol_SeparatorHovered] = ImVec4(0.85f, 0.35f, 0.55f, 1.00f);
                colors[ImGuiCol_SeparatorActive]  = ImVec4(0.95f, 0.45f, 0.65f, 1.00f);
                style.FrameBorderSize = 1.0f;
                style.WindowBorderSize = 1.0f;
            }
            
            // Inicializar watermark para que aparezca por defecto
            g_watermarkEnableTime = GetTickCount64();
            g_watermarkAnim = 1.0f;
            
            oWndProc = (WNDPROC)SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
            
            g_gameBase = (uintptr_t)GetModuleHandleA(NULL);
            g_reachPtr = (float*)(g_gameBase + 0xB52A70);
            g_hitboxAddr = g_gameBase + 0x4B57B0;
            g_timerAddr = g_gameBase + 0xB52AA8;  // Timer multiplicador offset

            // Scan for AutoSprint pattern
            HMODULE hModule = GetModuleHandleA(NULL);
            MODULEINFO mi;
            GetModuleInformation(GetCurrentProcess(), hModule, &mi, sizeof(mi));
            if (!g_autoSprintAddr) {
                BYTE pattern[] = {0x0F, 0xB6, 0x41, 0x63, 0x48, 0x8D, 0x2D, 0x39, 0xE0, 0xC3, 0x00};
                g_autoSprintAddr = PatternScan(g_gameBase, mi.SizeOfImage, pattern, sizeof(pattern));
            }

            // Scan for FullBright pattern
            if (!g_fullBrightAddr) {
                BYTE pattern[] = {0xF3, 0x0F, 0x10, 0x80, 0xA0, 0x01, 0x00, 0x00};
                g_fullBrightAddr = PatternScan(g_gameBase, mi.SizeOfImage, pattern, sizeof(pattern));
            }

            g_notifStart = GetTickCount64();
            g_lastTime = GetTickCount64();
        }
    }

    ID3D11Texture2D* pBackBuffer = NULL;
    float sw = 0, sh = 0;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
        D3D11_TEXTURE2D_DESC desc;
        pBackBuffer->GetDesc(&desc);
        sw = (float)desc.Width; 
        sh = (float)desc.Height;
        if (!mainRenderTargetView) pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        
        // Motion blur - capture frame
        if (g_motionBlurEnabled) {
            // Calculate max frames by blur type
            int maxFrames = 1;
            if (g_blurType == "Time Aware Blur") {
                maxFrames = (int)round(g_maxHistoryFrames);  // Usar g_maxHistoryFrames para Time Aware
            } else if (g_blurType == "Real Motion Blur") {
                maxFrames = 8;
            } else {
                maxFrames = (int)round(g_blurIntensity);  // Usar g_blurIntensity para otros modos
            }
            
            if (maxFrames <= 0) maxFrames = 4;
            if (maxFrames > 16) maxFrames = 16;
            
            InitializeBackbufferStorage(maxFrames);
            
            // Copy backbuffer to SRV
            ID3D11ShaderResourceView* srv = CopyBackbufferToSRV(pSwapChain, pDevice, pContext);
            if (srv) {
                // Remove oldest frame if exceeding max frames
                if ((int)g_previousFrames.size() >= maxFrames) {
                    if (g_previousFrames[0]) g_previousFrames[0]->Release();
                    g_previousFrames.erase(g_previousFrames.begin());
                    g_frameTimestamps.erase(g_frameTimestamps.begin());
                }
                
                g_previousFrames.push_back(srv);
                g_frameTimestamps.push_back((float)GetTickCount64() / 1000.0f);
            }
        }
        
        pBackBuffer->Release();
    }

    if (sw <= 0) return oPresent(pSwapChain, 0, Flags);  // VSync disabled

    // Insert key - open menu
    if ((GetAsyncKeyState(VK_INSERT) & 0x8000) && (GetTickCount64() - g_lastToggle) > 400) {
        g_showMenu = !g_showMenu;
        g_lastToggle = GetTickCount64();
        
        if (g_showMenu) {
            // 🎮 BLOQUEAR INPUT DEL JUEGO
            BlockGameInput();
            
            oClipCursor(NULL);
            // Si estamos en un mundo, ocultar cursor SO (mostrar cursor ImGui)
            if (IsInWorld()) {
                while (ShowCursor(FALSE) >= 0);
            } else {
                // Si NO estamos en un mundo, ocultar cursor ImGui pero mantener SO visible
                // OS cursor visible by default
            }
        } else {
            // 🎮 DESBLOQUEAR INPUT DEL JUEGO
            UnblockGameInput();
            
            // Menu closed - ensure cursor visible
            while (ShowCursor(TRUE) < 0);
        }
    }

    // Motion blur cleanup
    static bool wasMotionBlurEnabled = false;
    if (!g_motionBlurEnabled && wasMotionBlurEnabled) {
        CleanupBackbufferStorage();
    }
    wasMotionBlurEnabled = g_motionBlurEnabled;

    // 📊 ANIMATION UPDATE - Simple y elegante
    ULONGLONG now = GetTickCount64();
    float dt = (float)(now - g_lastTime) / 1000.0f;
    g_lastTime = now;
    
    // 📉 MENU ANIMATION - Velocidades optimizadas para elegancia
    if (g_showMenu) {
        g_menuAnim += 2.8f * dt;  // Apertura elegante
    } else {
        g_menuAnim -= 3.8f * dt;
    }
    if (g_menuAnim > 1.0f) g_menuAnim = 1.0f;
    if (g_menuAnim < 0.0f) g_menuAnim = 0.0f;
    float easedMenuAnim = SmoothInertia(g_menuAnim);
    
    // Tab change animation
    if (g_tabChangeTime > 0) {
        float tabChangeElapsed = (float)(now - g_tabChangeTime) / 1000.0f;
        g_tabAnim = fminf(1.0f, tabChangeElapsed / 0.25f);
    }
    
    // 📊 Render Info Animation - Exponencial suave
    if (g_showRenderInfo && g_renderInfoEnableTime == 0) {
        g_renderInfoEnableTime = now;
        g_renderInfoDisableTime = 0;
    }
    if (!g_showRenderInfo && g_renderInfoDisableTime == 0 && g_renderInfoEnableTime > 0) {
        g_renderInfoDisableTime = now;
        g_renderInfoEnableTime = 0;  // Resetear enable para que se use el else if
    }
    
    if (g_renderInfoEnableTime > 0) {
        float enableElapsed = (float)(now - g_renderInfoEnableTime) / 1000.0f;
        g_renderInfoAnim = fminf(1.0f, enableElapsed / 0.4f);
    }
    else if (g_renderInfoDisableTime > 0) {
        float disableElapsed = (float)(now - g_renderInfoDisableTime) / 1000.0f;
        float disableAnim = fminf(1.0f, disableElapsed / 0.3f);  // 300ms para desaparecer
        g_renderInfoAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_renderInfoEnableTime = 0;
            g_renderInfoDisableTime = 0;
        }
    }
    
    // Motion Blur Animation - Fade in/out
    if (g_motionBlurEnabled && g_motionBlurEnableTime == 0) {
        g_motionBlurEnableTime = now;
        g_motionBlurDisableTime = 0;
    }
    if (!g_motionBlurEnabled && g_motionBlurDisableTime == 0 && g_motionBlurEnableTime > 0) {
        g_motionBlurDisableTime = now;
        g_motionBlurEnableTime = 0;
    }
    
    if (g_motionBlurEnableTime > 0) {
        float enableElapsed = (float)(now - g_motionBlurEnableTime) / 1000.0f;
        g_motionBlurAnim = fminf(1.0f, enableElapsed / 0.3f);
    }
    else if (g_motionBlurDisableTime > 0) {
        float disableElapsed = (float)(now - g_motionBlurDisableTime) / 1000.0f;
        float disableAnim = fminf(1.0f, disableElapsed / 0.2f);  // 200ms para desaparecer
        g_motionBlurAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_motionBlurEnableTime = 0;
            g_motionBlurDisableTime = 0;
        }
    }
    
    // ⌨️ Keystrokes Animation - Fade in/out exponencial
    if (g_showKeystrokes && g_keystrokesEnableTime == 0) {
        g_keystrokesEnableTime = now;
        g_keystrokesDisableTime = 0;
    }
    if (!g_showKeystrokes && g_keystrokesDisableTime == 0 && g_keystrokesEnableTime > 0) {
        g_keystrokesDisableTime = now;
        g_keystrokesEnableTime = 0;
    }
    
    if (g_keystrokesEnableTime > 0) {
        float enableElapsed = (float)(now - g_keystrokesEnableTime) / 1000.0f;
        g_keystrokesAnim = fminf(1.0f, enableElapsed / 0.4f);
    }
    else if (g_keystrokesDisableTime > 0) {
        float disableElapsed = (float)(now - g_keystrokesDisableTime) / 1000.0f;
        float disableAnim = fminf(1.0f, disableElapsed / 0.3f);  // 300ms para desaparecer
        g_keystrokesAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_keystrokesEnableTime = 0;
            g_keystrokesDisableTime = 0;
            g_keystrokesAnim = 0.0f;
        }
    }
    
    // ⌨️ CPS COUNTER - LMB y RMB
    bool lmbPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rmbPressed = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    
    // LMB CPS Counter
    if (lmbPressed && !g_prevLmbPressed) {
        // Save click timestamp
        g_lmbClickTimes[g_lmbClickIndex] = now;
        g_lmbClickIndex = (g_lmbClickIndex + 1) % MAX_CPS_HISTORY;
        
        // Count clicks in last 1000ms
        int count = 0;
        for (int i = 0; i < MAX_CPS_HISTORY; i++) {
            if (g_lmbClickTimes[i] > 0 && (now - g_lmbClickTimes[i]) < 1000) {
                count++;
            }
        }
        g_lmbCps = count;
    }
    g_prevLmbPressed = lmbPressed;
    
    // RMB CPS Counter
    if (rmbPressed && !g_prevRmbPressed) {
        // Save click timestamp
        g_rmbClickTimes[g_rmbClickIndex] = now;
        g_rmbClickIndex = (g_rmbClickIndex + 1) % MAX_CPS_HISTORY;
        
        // Count clicks in last 1000ms
        int count = 0;
        for (int i = 0; i < MAX_CPS_HISTORY; i++) {
            if (g_rmbClickTimes[i] > 0 && (now - g_rmbClickTimes[i]) < 1000) {
                count++;
            }
        }
        g_rmbCps = count;
    }
    g_prevRmbPressed = rmbPressed;
    
    // CPS Counter Animation - Fade in/out exponencial
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
        float disableAnim = fminf(1.0f, disableElapsed / 0.3f);  // 300ms para desaparecer
        g_cpsCounterAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_cpsCounterEnableTime = 0;
            g_cpsCounterDisableTime = 0;
            g_cpsCounterAnim = 0.0f;
        }
    }
    
    // 💧 Watermark Animation - Fade in/out exponencial
    if (g_showWatermark && g_watermarkEnableTime == 0) {
        g_watermarkEnableTime = now;
        g_watermarkDisableTime = 0;
    }
    if (!g_showWatermark && g_watermarkDisableTime == 0 && g_watermarkEnableTime > 0) {
        g_watermarkDisableTime = now;
        g_watermarkEnableTime = 0;  // Resetear enable para que se use el else if
    }
    
    if (g_watermarkEnableTime > 0) {
        float enableElapsed = (float)(now - g_watermarkEnableTime) / 1000.0f;
        g_watermarkAnim = fminf(1.0f, enableElapsed / 0.4f);
    }
    else if (g_watermarkDisableTime > 0) {
        float disableElapsed = (float)(now - g_watermarkDisableTime) / 1000.0f;
        float disableAnim = fminf(1.0f, disableElapsed / 0.3f);  // 300ms para desaparecer
        g_watermarkAnim = 1.0f - disableAnim;
        if (disableAnim >= 1.0f) {
            g_watermarkEnableTime = 0;
            g_watermarkDisableTime = 0;
        }
    }

    ImGui_ImplDX11_NewFrame();
    // Don't use ImGui_ImplWin32_NewFrame - manual input handling
    
    // Update keyboard input
    UpdateInputSystem(g_showMenu);
    
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(sw, sh);
    
    // Detect window size changes
    if (g_lastW != sw || g_lastH != sh) {
        g_lastW = sw;
        g_lastH = sh;
    }
    
    // Manual input handling - UWP compatible
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(g_window, &p);
    io.MousePos = ImVec2((float)p.x, (float)p.y);
    
    // Mouse buttons - lectura directa clean
    io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    
    // Cursor de ImGui SOLO se dibuja en mundos
    bool drawImGuiCursor = (g_showMenu && IsInWorld());
    io.MouseDrawCursor = drawImGuiCursor;

    ImGui::NewFrame();

    // Motion blur - apply to background at frame start
    if (g_motionBlurEnabled && !g_showMenu && g_previousFrames.size() > 0 && g_motionBlurAnim > 0.01f) {
        float currentTime = (float)GetTickCount64() / 1000.0f;
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        ImDrawList* blurDraw = ImGui::GetBackgroundDrawList();
        
        // Average Pixel Blur: Promedio de frames con falloff
        if (g_blurType == "Average Pixel Blur") {
            float alpha = 0.25f;
            float bleedFactor = 0.95f;
            for (const auto& frame : g_previousFrames) {
                if (frame) {
                    ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * g_motionBlurAnim * 255.0f));
                    blurDraw->AddImage((ImTextureID)frame, ImVec2(0, 0), screenSize, ImVec2(0, 0), ImVec2(1, 1), col);
                    alpha *= bleedFactor;
                }
            }
        } 
        // Ghost frames: More visible
        else if (g_blurType == "Ghost Frames") {
            float alpha = 0.30f;
            float bleedFactor = 0.80f;
            for (const auto& frame : g_previousFrames) {
                if (frame) {
                    ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * g_motionBlurAnim * 255.0f));
                    blurDraw->AddImage((ImTextureID)frame, ImVec2(0, 0), screenSize, ImVec2(0, 0), ImVec2(1, 1), col);
                    alpha *= bleedFactor;
                }
            }
        }
        // Time Aware Blur: Decay exponencial
        else if (g_blurType == "Time Aware Blur") {
            float T = g_blurTimeConstant;
            std::vector<float> weights;
            float totalWeight = 0.0f;
            
            for (size_t i = 0; i < g_previousFrames.size(); i++) {
                float age = currentTime - g_frameTimestamps[i];
                float weight = expf(-age / T);
                weights.push_back(weight);
                totalWeight += weight;
            }
            
            if (totalWeight > 0.0f) {
                for (float& w : weights) {
                    w /= totalWeight;
                }
            }
            
            for (size_t i = 0; i < g_previousFrames.size(); i++) {
                if (g_previousFrames[i] && weights[i] > 0.001f) {
                    ImU32 col = IM_COL32(255, 255, 255, (int)(weights[i] * g_motionBlurAnim * 255.0f));
                    blurDraw->AddImage((ImTextureID)g_previousFrames[i], ImVec2(0, 0), screenSize, ImVec2(0, 0), ImVec2(1, 1), col);
                }
            }
        }
        // Real Motion Blur: Trails suaves
        else if (g_blurType == "Real Motion Blur") {
            float alpha = 0.35f;
            float bleedFactor = 0.85f;
            for (const auto& frame : g_previousFrames) {
                if (frame) {
                    ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * g_motionBlurAnim * 255.0f));
                    blurDraw->AddImage((ImTextureID)frame, ImVec2(0, 0), screenSize, ImVec2(0, 0), ImVec2(1, 1), col);
                    alpha *= bleedFactor;
                }
            }
        }
        // V4 (Onix-style)
        else if (g_blurType == "V4") {
            float alpha = 0.35f;
            float bleedFactor = 0.85f;
            for (const auto& frame : g_previousFrames) {
                if (frame) {
                    ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * g_motionBlurAnim * 255.0f));
                    blurDraw->AddImage((ImTextureID)frame, ImVec2(0, 0), screenSize, ImVec2(0, 0), ImVec2(1, 1), col);
                    alpha *= bleedFactor;
                }
            }
        }
    }

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

    // Reach module (always visible)
    float reachAlpha = 255.0f;
        sprintf_s(rBuf, "Reach - %.2f", g_reachValue);
        float wR = ImGui::CalcTextSize(rBuf).x;
        float xPos = arrayListStart.x + g_arrayListHud.size.x - wR - 10;
        draw->AddText(ImVec2(xPos - 1, yPos + 1), IM_COL32(0, 0, 0, 220), rBuf); // Sombra
        draw->AddText(ImVec2(xPos, yPos), IM_COL32(0, 200, 255, (int)reachAlpha), rBuf);
        yPos += 18.0f;
        arrayListEnd.y = yPos;

        // Hitbox module (with fade animation)
        if (g_hitboxEnabled || g_hitboxDisableTime > 0) {
            float timeSinceEnable = (float)(GetTickCount64() - g_hitboxEnableTime) / 1000.0f;
            float timeSinceDisable = (float)(GetTickCount64() - g_hitboxDisableTime) / 1000.0f;
            
            float hitboxAlpha = 255.0f;
            float slideOffset = 0.0f;  // Offset de desplazamiento
            
            if (g_hitboxEnabled) {
                // Fade in elegante
                hitboxAlpha = SmoothInertia(fminf(1.0f, timeSinceEnable / FADE_IN_TIME)) * 255.0f;
                // Slide in animation - desplazamiento desde -60px
                float slideProgress = fminf(1.0f, timeSinceEnable / SLIDE_TIME);
                slideOffset = SmoothInertia(slideProgress) * 60.0f - 60.0f;  // -60 a 0
            } else if (timeSinceDisable < FADE_OUT_TIME) {
                // Fade out elegante
                hitboxAlpha = SmoothInertia(1.0f - (timeSinceDisable / FADE_OUT_TIME)) * 255.0f;
            } else {
                g_hitboxDisableTime = 0;  // Reset tiempo
            }
            
            if (hitboxAlpha > 1.0f) {
                sprintf_s(hBuf, "Hitbox - %.2f", g_hitboxValue);
                float wH = ImGui::CalcTextSize(hBuf).x;
                float xPosH = arrayListStart.x + g_arrayListHud.size.x - wH - 10;
                draw->AddText(ImVec2(xPosH + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), hBuf); // Sombra
                draw->AddText(ImVec2(xPosH + slideOffset, yPos), IM_COL32(255, 60, 60, (int)hitboxAlpha), hBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }

        // AutoSprint module
        if (g_autoSprintEnabled || g_autoSprintDisableTime > 0) {
            float timeSinceEnable = (float)(GetTickCount64() - g_autoSprintEnableTime) / 1000.0f;
            float timeSinceDisable = (float)(GetTickCount64() - g_autoSprintDisableTime) / 1000.0f;
            
            float autoSprintAlpha = 255.0f;
            float slideOffset = 0.0f;
            
            if (g_autoSprintEnabled) {
                autoSprintAlpha = SmoothInertia(fminf(1.0f, timeSinceEnable / FADE_IN_TIME)) * 255.0f;
                // Slide in animation
                float slideProgress = fminf(1.0f, timeSinceEnable / SLIDE_TIME);
                slideOffset = SmoothInertia(slideProgress) * 60.0f - 60.0f;
            } else if (timeSinceDisable < FADE_OUT_TIME) {
                autoSprintAlpha = SmoothInertia(1.0f - (timeSinceDisable / FADE_OUT_TIME)) * 255.0f;
            } else {
                g_autoSprintDisableTime = 0;
            }
            
            if (autoSprintAlpha > 1.0f) {
                char aBuf[64];
                sprintf_s(aBuf, "AutoSprint");
                float wA = ImGui::CalcTextSize(aBuf).x;
                float xPosA = arrayListStart.x + g_arrayListHud.size.x - wA - 10;
                draw->AddText(ImVec2(xPosA + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), aBuf); // Sombra
                draw->AddText(ImVec2(xPosA + slideOffset, yPos), IM_COL32(60, 255, 60, (int)autoSprintAlpha), aBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }
        
        // FullBright module
        if (g_fullBrightEnabled || g_fullBrightDisableTime > 0) {
            float timeSinceEnable = (float)(GetTickCount64() - g_fullBrightEnableTime) / 1000.0f;
            float timeSinceDisable = (float)(GetTickCount64() - g_fullBrightDisableTime) / 1000.0f;
            
            float fullBrightAlpha = 255.0f;
            float slideOffset = 0.0f;
            
            if (g_fullBrightEnabled) {
                fullBrightAlpha = SmoothInertia(fminf(1.0f, timeSinceEnable / FADE_IN_TIME)) * 255.0f;
                // Slide in animation
                float slideProgress = fminf(1.0f, timeSinceEnable / SLIDE_TIME);
                slideOffset = SmoothInertia(slideProgress) * 60.0f - 60.0f;
            } else if (timeSinceDisable < FADE_OUT_TIME) {
                fullBrightAlpha = SmoothInertia(1.0f - (timeSinceDisable / FADE_OUT_TIME)) * 255.0f;
            } else {
                g_fullBrightDisableTime = 0;
            }
            
            if (fullBrightAlpha > 1.0f) {
                char fBuf[64];
                sprintf_s(fBuf, "FullBright");
                float wF = ImGui::CalcTextSize(fBuf).x;
                float xPosF = arrayListStart.x + g_arrayListHud.size.x - wF - 10;
                draw->AddText(ImVec2(xPosF + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), fBuf); // Sombra
                draw->AddText(ImVec2(xPosF + slideOffset, yPos), IM_COL32(255, 255, 100, (int)fullBrightAlpha), fBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }
        
        // Timer module
        if (g_timerEnabled || g_timerDisableTime > 0) {
            float timeSinceEnable = (float)(GetTickCount64() - g_timerEnableTime) / 1000.0f;
            float timeSinceDisable = (float)(GetTickCount64() - g_timerDisableTime) / 1000.0f;
            
            float timerAlpha = 255.0f;
            float slideOffset = 0.0f;
            
            if (g_timerEnabled) {
                timerAlpha = SmoothInertia(fminf(1.0f, timeSinceEnable / FADE_IN_TIME)) * 255.0f;
                // Slide in animation
                float slideProgress = fminf(1.0f, timeSinceEnable / SLIDE_TIME);
                slideOffset = SmoothInertia(slideProgress) * 60.0f - 60.0f;
            } else if (timeSinceDisable < FADE_OUT_TIME) {
                timerAlpha = SmoothInertia(1.0f - (timeSinceDisable / FADE_OUT_TIME)) * 255.0f;
            } else {
                g_timerDisableTime = 0;
            }
            
            if (timerAlpha > 1.0f) {
                char tBuf[64];
                sprintf_s(tBuf, "Timer - %.1fx", g_timerValue);
                float wT = ImGui::CalcTextSize(tBuf).x;
                float xPosT = arrayListStart.x + g_arrayListHud.size.x - wT - 10;
                draw->AddText(ImVec2(xPosT + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), tBuf); // Sombra
                draw->AddText(ImVec2(xPosT + slideOffset, yPos), IM_COL32(100, 200, 255, (int)timerAlpha), tBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }
        
        // Unlock FPS module
        if (g_unlockFpsEnabled || g_unlockFpsDisableTime > 0) {
            float timeSinceEnable = (float)(GetTickCount64() - g_unlockFpsEnableTime) / 1000.0f;
            float timeSinceDisable = (float)(GetTickCount64() - g_unlockFpsDisableTime) / 1000.0f;
            
            float unlockFpsAlpha = 255.0f;
            float slideOffset = 0.0f;
            
            if (g_unlockFpsEnabled) {
                unlockFpsAlpha = SmoothInertia(fminf(1.0f, timeSinceEnable / FADE_IN_TIME)) * 255.0f;
                // Slide in animation
                float slideProgress = fminf(1.0f, timeSinceEnable / SLIDE_TIME);
                slideOffset = SmoothInertia(slideProgress) * 60.0f - 60.0f;
            } else if (timeSinceDisable < FADE_OUT_TIME) {
                unlockFpsAlpha = SmoothInertia(1.0f - (timeSinceDisable / FADE_OUT_TIME)) * 255.0f;
            } else {
                g_unlockFpsDisableTime = 0;
            }
            
            if (unlockFpsAlpha > 1.0f) {
                char uBuf[64];
                sprintf_s(uBuf, "Unlock FPS - %.0f", g_fpsLimit);
                float wU = ImGui::CalcTextSize(uBuf).x;
                float xPosU = arrayListStart.x + g_arrayListHud.size.x - wU - 10;
                draw->AddText(ImVec2(xPosU + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), uBuf); // Sombra
                draw->AddText(ImVec2(xPosU + slideOffset, yPos), IM_COL32(100, 255, 100, (int)unlockFpsAlpha), uBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }

        // Watermark module
        if (g_showWatermark || (g_watermarkDisableTime > 0 && g_watermarkAnim > 0.01f)) {
            float watermarkAlpha = g_watermarkAnim * 255.0f;
            float slideOffset = -60.0f + (SmoothInertia(g_watermarkAnim) * 60.0f);
            
            if (watermarkAlpha > 1.0f) {
                char wBuf[64];
                sprintf_s(wBuf, "Watermark");
                float wW = ImGui::CalcTextSize(wBuf).x;
                float xPosW = arrayListStart.x + g_arrayListHud.size.x - wW - 10;
                draw->AddText(ImVec2(xPosW + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), wBuf); // Sombra
                draw->AddText(ImVec2(xPosW + slideOffset, yPos), IM_COL32(100, 255, 200, (int)watermarkAlpha), wBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }
        
        // Show Render Info module
        if (g_showRenderInfo || (g_renderInfoDisableTime > 0 && g_renderInfoAnim > 0.01f)) {
            float renderInfoAlpha = g_renderInfoAnim * 255.0f;
            float slideOffset = -60.0f + (SmoothInertia(g_renderInfoAnim) * 60.0f);
            
            if (renderInfoAlpha > 1.0f) {
                char riBuf[64];
                sprintf_s(riBuf, "Render Info");
                float wRI = ImGui::CalcTextSize(riBuf).x;
                float xPosRI = arrayListStart.x + g_arrayListHud.size.x - wRI - 10;
                draw->AddText(ImVec2(xPosRI + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), riBuf); // Sombra
                draw->AddText(ImVec2(xPosRI + slideOffset, yPos), IM_COL32(0, 255, 200, (int)renderInfoAlpha), riBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }

        // Motion Blur module
        if (g_motionBlurEnabled || (g_motionBlurDisableTime > 0 && g_motionBlurAnim > 0.01f)) {
            float motionBlurAlpha = g_motionBlurAnim * 255.0f;
            float slideOffset = -60.0f + (SmoothInertia(g_motionBlurAnim) * 60.0f);
            
            if (motionBlurAlpha > 1.0f) {
                char mbBuf[64];
                sprintf_s(mbBuf, "Motion Blur");
                float wMB = ImGui::CalcTextSize(mbBuf).x;
                float xPosMB = arrayListStart.x + g_arrayListHud.size.x - wMB - 10;
                draw->AddText(ImVec2(xPosMB + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), mbBuf); // Sombra
                draw->AddText(ImVec2(xPosMB + slideOffset, yPos), IM_COL32(100, 255, 150, (int)motionBlurAlpha), mbBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }

        // Keystrokes module
        if (g_showKeystrokes || (g_keystrokesDisableTime > 0 && g_keystrokesAnim > 0.01f)) {
            float keystrokesAlpha = g_keystrokesAnim * 255.0f;
            float slideOffset = -60.0f + (SmoothInertia(g_keystrokesAnim) * 60.0f);
            
            if (keystrokesAlpha > 1.0f) {
                char kBuf[64];
                sprintf_s(kBuf, "Keystrokes");
                float wK = ImGui::CalcTextSize(kBuf).x;
                float xPosK = arrayListStart.x + g_arrayListHud.size.x - wK - 10;
                draw->AddText(ImVec2(xPosK + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), kBuf); // Sombra
                draw->AddText(ImVec2(xPosK + slideOffset, yPos), IM_COL32(100, 200, 255, (int)keystrokesAlpha), kBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }

        // CPS Counter module
        if (g_showCpsCounter || (g_cpsCounterDisableTime > 0 && g_cpsCounterAnim > 0.01f)) {
            float cpsAlpha = g_cpsCounterAnim * 255.0f;
            float slideOffset = -60.0f + (SmoothInertia(g_cpsCounterAnim) * 60.0f);
            
            if (cpsAlpha > 1.0f) {
                char cBuf[64];
                sprintf_s(cBuf, "CPS Counter");
                float wC = ImGui::CalcTextSize(cBuf).x;
                float xPosC = arrayListStart.x + g_arrayListHud.size.x - wC - 10;
                draw->AddText(ImVec2(xPosC + slideOffset - 1, yPos + 1), IM_COL32(0, 0, 0, 220), cBuf); // Sombra
                draw->AddText(ImVec2(xPosC + slideOffset, yPos), IM_COL32(255, 200, 100, (int)cpsAlpha), cBuf);
                yPos += 18.0f;
                arrayListEnd.y = yPos;
            }
        }

    // Debug: Draw ArrayList collision area
    if (g_showMenu) {
        g_arrayListHud.size.y = arrayListEnd.y - arrayListStart.y;
        draw->AddRect(
            arrayListStart,
            ImVec2(arrayListStart.x + g_arrayListHud.size.x, arrayListEnd.y),
            IM_COL32(255, 255, 255, 80)
        );
    }
        
        // Initial notification
    float tNotif = (float)(GetTickCount64() - g_notifStart) / 1000.0f;
    if (tNotif < 5.0f) {
        // Entry: 0-0.6s, Display: 0.6-4.0s, Exit: 4.0-5.0s
        float nT = tNotif < 0.6f ? tNotif / 0.6f : (tNotif > 4.0f ? 1.0f - (tNotif - 4.0f) / 1.0f : 1.0f);
        float nE = SmoothInertia(nT) * 255.0f;
        ImVec2 nS = ImVec2(320, 70);
        ImGui::SetNextWindowPos(ImVec2(sw - ((nS.x + 20.0f) * nE / 255.0f), sh - nS.y - 20.0f));
        ImGui::SetNextWindowSize(nS);
        ImGui::Begin("##Notif", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(0, 0.8f, 1, nE / 255.0f), "Aegleseeker DLL | build: 2026-03");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, nE / 255.0f), "gh: https://github.com/iVyz3r/aegleseeker");
        ImGui::TextColored(ImVec4(1, 1, 1, nE / 255.0f), "Press Insert to Open Menu");
        ImGui::End();
    }

    // Menu drawing
    if (g_menuAnim > 0.001f) {
        float e = easedMenuAnim;
        
        // Background with animation
        float bgAlpha = SmoothInertia(e) * 140.0f;
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), IM_COL32(0, 0, 0, (int)bgAlpha));

        // Render menu content when visible
        if (e > 0.12f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
            
            // Improved scale with easing
            float sc = 0.85f + (0.15f * SmoothInertia(e));
            
            ImVec2 bS = ImVec2(750, 500);
            ImGui::SetNextWindowSize(ImVec2(bS.x * sc, bS.y * sc), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(sw / 2, sh / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

            if (ImGui::Begin("Aegleseeker Config", &g_showMenu, ImGuiWindowFlags_NoCollapse)) {
            // Estilos mejorados para la interfaz
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.95f, 0.45f, 0.65f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.55f, 0.75f, 1.0f));
            
            // Scrollable menu content
            ImGui::BeginChild("MenuScroll", ImVec2(0, 0), false);
            
            if (ImGui::BeginTabBar("Tabs")) {
                // Detect tab change
                int prevTab = g_currentTab;
                
                if (ImGui::BeginTabItem("Combat")) {
                    g_currentTab = 0;
                    if (g_firstTabOpen) {
                        g_firstTabOpen = false;
                        g_tabAnim = 1.0f;  // Saltarse directamente a alpha completo
                    } else if (g_currentTab != prevTab) {
                        g_tabChangeTime = GetTickCount64();
                        g_tabAnim = 0.0f;
                        g_previousTab = prevTab;
                    }
                    
                    // Apply animation if not first render
                    float alphaFade = (g_tabChangeTime > 0) ? SmoothInertia(g_tabAnim) : 1.0f;
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaFade);
                    
                    // Reach Slider
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.40f, 0.18f, 0.28f, 0.8f));
                    ImGui::Text("Reach");
                    if (ImGui::SliderFloat("Reach Distance", &g_reachValue, 3.0f, 15.0f, "%.2f")) {
                        if (g_reachPtr) {
                            DWORD old; VirtualProtect(g_reachPtr, 4, PAGE_EXECUTE_READWRITE, &old);
                            *g_reachPtr = g_reachValue;
                            VirtualProtect(g_reachPtr, 4, old, &old);
                        }
                    }
                    ImGui::PopStyleColor();

                    ImGui::Separator();

                    // Hitbox Checkbox y Slider
                    if (ImGui::Checkbox("Hitbox Expander", &g_hitboxEnabled)) {
                        if (g_hitboxEnabled) EnableHitbox(); else DisableHitbox();
                    }
                    if (g_hitboxEnabled) {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.40f, 0.18f, 0.28f, 0.8f));
                        if (ImGui::SliderFloat("Hitbox Size", &g_hitboxValue, 0.6f, 10.0f, "%.2f")) {
                            if (g_hitboxCave) memcpy((BYTE*)g_hitboxCave + 1, &g_hitboxValue, 4);
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopStyleVar();  // Alpha
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Movement")) {
                    g_currentTab = 1;
                    if (g_currentTab != prevTab) {
                        g_tabChangeTime = GetTickCount64();
                        g_tabAnim = 0.0f;
                        g_previousTab = prevTab;
                    }
                    
                    // Apply animation if not first render
                    float alphaFade = (g_tabChangeTime > 0) ? SmoothInertia(g_tabAnim) : 1.0f;
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaFade);
                    
                    // AutoSprint Checkbox
                    if (ImGui::Checkbox("AutoSprint", &g_autoSprintEnabled)) {
                        if (g_autoSprintEnabled) EnableAutoSprint(); else DisableAutoSprint();
                    }
                    
                    ImGui::Separator();
                    
                    // Timer/Multiplicador
                    if (ImGui::Checkbox("Timer", &g_timerEnabled)) {
                        if (g_timerEnabled) EnableTimer(); else DisableTimer();
                    }
                    
                    if (g_timerEnabled) {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.40f, 0.18f, 0.28f, 0.8f));
                        if (ImGui::SliderFloat("##timerSlider", &g_timerValue, 0.1f, 20.0f, "Multiplicador: %.1fx")) {
                            // Value updates automatically
                        }
                        ImGui::PopStyleColor();
                        ImGui::Text("Value in memory (slider * 1000.0f): %.0f", (double)g_timerValue * 1000.0);
                    }

                    ImGui::PopStyleVar();  // Alpha
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Visuals")) {
                    g_currentTab = 2;
                    if (g_currentTab != prevTab) {
                        g_tabChangeTime = GetTickCount64();
                        g_tabAnim = 0.0f;
                        g_previousTab = prevTab;
                    }
                    
                    // Apply animation if not first render
                    float alphaFade = (g_tabChangeTime > 0) ? SmoothInertia(g_tabAnim) : 1.0f;
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaFade);
                    
                    // FullBright Checkbox
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Unstable Feature - May Cause Performance Issues\nFullBright is a little buggy");
                    if (ImGui::Checkbox("FullBright", &g_fullBrightEnabled)) {
                        if (g_fullBrightEnabled) EnableFullBright(); else DisableFullBright();
                    }
                    
                    ImGui::Separator();
                    
                    // Show Render Info
                    ImGui::Checkbox("Render Info", &g_showRenderInfo);
                    
                    // Show Watermark
                    ImGui::Checkbox("Watermark", &g_showWatermark);
                    ImGui::Separator();
                    // Motion Blur
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Unstable Feature - May Cause Performance Issues\nMotion Blur may cause performance issues on lower-end systems");
                    ImGui::Checkbox("Motion Blur", &g_motionBlurEnabled);
                    
                    if (g_motionBlurEnabled) {
                        ImGui::Separator();
                        ImGui::Text("Motion Blur Settings");
                        
                        // Blur Type Dropdown
                        static int blurTypeIndex = 0;
                        static const char* blurTypes[] = { 
                            "Average Pixel Blur",
                            "Ghost Frames",
                            "Time Aware Blur",
                            "Real Motion Blur",
                            "V4"
                        };
                        if (ImGui::Combo("Blur Type##MB", &blurTypeIndex, blurTypes, IM_ARRAYSIZE(blurTypes))) {
                            g_blurType = blurTypes[blurTypeIndex];
                        }
                        
                        // Intensity slider based on type
                        if (g_blurType == "Time Aware Blur") {
                            ImGui::SliderFloat("Blur Time Constant##MB", &g_blurTimeConstant, 0.01f, 0.2f, "%.4f");
                            ImGui::SliderFloat("Max History Frames##MB", &g_maxHistoryFrames, 4.0f, 16.0f, "%.0f");
                            // NO sobrescribir g_blurIntensity - dejar que Time Aware use g_maxHistoryFrames independientemente
                        } else {
                            ImGui::SliderFloat("Intensity##MB", &g_blurIntensity, 1.0f, 30.0f, "%.0f");
                        }
                        
                        // Dynamic mode for Average Pixel Blur
                        if (g_blurType == "Average Pixel Blur") {
                            ImGui::Checkbox("Dynamic Mode##MB", &g_blurDynamicMode);
                            if (g_blurDynamicMode) {
                                ImGui::TextDisabled("Adjusts intensity based on FPS");
                            }
                        }
                    }
                    
                    // Show Keystrokes
                    ImGui::Checkbox("Keystrokes", &g_showKeystrokes);
                    
                    if (g_showKeystrokes) {
                        ImGui::Separator();
                        ImGui::Text("Keystrokes Configuration");
                        
                        // UI Scale
                        ImGui::SliderFloat("Keystrokes Scale", &g_keystrokesUIScale, 0.5f, 2.0f, "%.2f");
                        
                        // Visual Effects
                        ImGui::Checkbox("Blur Effect##KS", &g_keystrokesBlurEffect);
                        ImGui::SliderFloat("Keystrokes Rounding", &g_keystrokesRounding, 0.0f, 20.0f, "%.1f");
                        
                        // Background & Shadow
                        ImGui::Checkbox("Show Background##KS", &g_keystrokesShowBg);
                        if (g_keystrokesShowBg) {
                            ImGui::Checkbox("Background Shadow##KS", &g_keystrokesRectShadow);
                            if (g_keystrokesRectShadow) {
                                ImGui::SliderFloat("Shadow Offset##KS", &g_keystrokesRectShadowOffset, 0.0f, 0.1f, "%.3f");
                            }
                        }
                        
                        // Border
                        ImGui::Checkbox("Border##KS", &g_keystrokesBorder);
                        if (g_keystrokesBorder) {
                            ImGui::SliderFloat("Border Width##KS", &g_keystrokesBorderWidth, 0.5f, 4.0f, "%.1f");
                        }
                        
                        // Glow Effects
                        ImGui::Checkbox("Glow (Disabled)##KS", &g_keystrokesGlow);
                        if (g_keystrokesGlow) {
                            ImGui::SliderFloat("Glow Amount##KS", &g_keystrokesGlowAmount, 0.0f, 100.0f, "%.0f");
                        }
                        
                        ImGui::Checkbox("Glow (Enabled)##KS", &g_keystrokesGlowEnabled);
                        if (g_keystrokesGlowEnabled) {
                            ImGui::SliderFloat("Enabled Glow Amount##KS", &g_keystrokesGlowEnabledAmount, 0.0f, 100.0f, "%.0f");
                            ImGui::SliderFloat("Glow Speed##KS", &g_keystrokesGlowSpeed, 0.1f, 10.0f, "%.1f");
                        }
                        
                        // Spacing & Speed
                        ImGui::SliderFloat("Key Spacing##KS", &g_keystrokesKeySpacing, 0.5f, 5.0f, "%.2f");
                        ImGui::SliderFloat("Highlight Speed##KS", &g_keystrokesEdSpeed, 0.1f, 10.0f, "%.1f");
                        
                        // Spacebar Configuration
                        ImGui::Separator();
                        ImGui::Text("Spacebar Settings");
                        ImGui::Checkbox("Show Spacebar##KS", &g_keystrokesShowSpacebar);
                        ImGui::SliderFloat("Spacebar Width##KS", &g_keystrokesSpacebarWidth, 0.1f, 2.0f, "%.2f");
                        ImGui::SliderFloat("Spacebar Height##KS", &g_keystrokesSpacebarHeight, 0.05f, 0.5f, "%.2f");
                        
                        // Text Settings
                        ImGui::SliderFloat("WASD Text Scale##KS", &g_keystrokesTextScale, 0.5f, 2.0f, "%.2f");
                        ImGui::SliderFloat("Mouse Text Scale##KS", &g_keystrokesTextScale2, 0.5f, 2.0f, "%.2f");
                        ImGui::SliderFloat("Text X Offset##KS", &g_keystrokesTextXOffset, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Text Y Offset##KS", &g_keystrokesTextYOffset, 0.0f, 1.0f, "%.2f");
                        
                        ImGui::Checkbox("Text Shadow##KS", &g_keystrokesTextShadow);
                        if (g_keystrokesTextShadow) {
                            ImGui::SliderFloat("Text Shadow Offset##KS", &g_keystrokesTextShadowOffset, 0.0f, 0.1f, "%.3f");
                        }
                        
                        ImGui::Separator();
                        ImGui::Text("Keystrokes Colors");
                        
                        // Background Colors
                        float bgColorDisabled[4] = {g_keystrokesBgColor.x, g_keystrokesBgColor.y, g_keystrokesBgColor.z, g_keystrokesBgColor.w};
                        if (ImGui::ColorEdit4("Background Disabled##KS", bgColorDisabled)) {
                            g_keystrokesBgColor = ImVec4(bgColorDisabled[0], bgColorDisabled[1], bgColorDisabled[2], bgColorDisabled[3]);
                        }
                        ImGui::SliderFloat("Background Disabled Opacity##KS", &g_keystrokesBgColor.w, 0.0f, 1.0f, "%.2f");
                        
                        float bgColorEnabled[4] = {g_keystrokesEnabledColor.x, g_keystrokesEnabledColor.y, g_keystrokesEnabledColor.z, g_keystrokesEnabledColor.w};
                        if (ImGui::ColorEdit4("Background Enabled##KS", bgColorEnabled)) {
                            g_keystrokesEnabledColor = ImVec4(bgColorEnabled[0], bgColorEnabled[1], bgColorEnabled[2], bgColorEnabled[3]);
                        }
                        ImGui::SliderFloat("Background Enabled Opacity##KS", &g_keystrokesEnabledColor.w, 0.0f, 1.0f, "%.2f");
                        
                        // Text Colors
                        float textColorDisabled[4] = {g_keystrokesTextColor.x, g_keystrokesTextColor.y, g_keystrokesTextColor.z, g_keystrokesTextColor.w};
                        if (ImGui::ColorEdit4("Text Disabled##KS", textColorDisabled)) {
                            g_keystrokesTextColor = ImVec4(textColorDisabled[0], textColorDisabled[1], textColorDisabled[2], textColorDisabled[3]);
                        }
                        ImGui::SliderFloat("Text Disabled Opacity##KS", &g_keystrokesTextColor.w, 0.0f, 1.0f, "%.2f");
                        
                        float textColorEnabled[4] = {g_keystrokesTextEnabledColor.x, g_keystrokesTextEnabledColor.y, g_keystrokesTextEnabledColor.z, g_keystrokesTextEnabledColor.w};
                        if (ImGui::ColorEdit4("Text Enabled##KS", textColorEnabled)) {
                            g_keystrokesTextEnabledColor = ImVec4(textColorEnabled[0], textColorEnabled[1], textColorEnabled[2], textColorEnabled[3]);
                        }
                        ImGui::SliderFloat("Text Enabled Opacity##KS", &g_keystrokesTextEnabledColor.w, 0.0f, 1.0f, "%.2f");
                        
                        ImGui::Separator();
                        ImGui::Text("Advanced Effects");
                        
                        // Background Shadow Color
                        if (g_keystrokesShowBg && g_keystrokesRectShadow) {
                            float rectShadowCol[4] = {g_keystrokesRectShadowColor.x, g_keystrokesRectShadowColor.y, g_keystrokesRectShadowColor.z, g_keystrokesRectShadowColor.w};
                            if (ImGui::ColorEdit4("Background Shadow Color##KS", rectShadowCol)) {
                                g_keystrokesRectShadowColor = ImVec4(rectShadowCol[0], rectShadowCol[1], rectShadowCol[2], rectShadowCol[3]);
                            }
                            ImGui::SliderFloat("Background Shadow Opacity##KS", &g_keystrokesRectShadowColor.w, 0.0f, 1.0f, "%.2f");
                        }
                        
                        // Text Shadow Color
                        if (g_keystrokesTextShadow) {
                            float textShadowCol[4] = {g_keystrokesTextShadowColor.x, g_keystrokesTextShadowColor.y, g_keystrokesTextShadowColor.z, g_keystrokesTextShadowColor.w};
                            if (ImGui::ColorEdit4("Text Shadow Color##KS", textShadowCol)) {
                                g_keystrokesTextShadowColor = ImVec4(textShadowCol[0], textShadowCol[1], textShadowCol[2], textShadowCol[3]);
                            }
                            ImGui::SliderFloat("Text Shadow Opacity##KS", &g_keystrokesTextShadowColor.w, 0.0f, 1.0f, "%.2f");
                        }
                        
                        // Border Color
                        if (g_keystrokesBorder) {
                            float borderCol[4] = {g_keystrokesBorderColor.x, g_keystrokesBorderColor.y, g_keystrokesBorderColor.z, g_keystrokesBorderColor.w};
                            if (ImGui::ColorEdit4("Border Color##KS", borderCol)) {
                                g_keystrokesBorderColor = ImVec4(borderCol[0], borderCol[1], borderCol[2], borderCol[3]);
                            }
                            ImGui::SliderFloat("Border Opacity##KS", &g_keystrokesBorderColor.w, 0.0f, 1.0f, "%.2f");
                        }
                        
                        // Glow Colors
                        if (g_keystrokesGlow) {
                            float glowCol[4] = {g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w};
                            if (ImGui::ColorEdit4("Glow Color (Disabled)##KS", glowCol)) {
                                g_keystrokesGlowColor = ImVec4(glowCol[0], glowCol[1], glowCol[2], glowCol[3]);
                            }
                            ImGui::SliderFloat("Glow Disabled Opacity##KS", &g_keystrokesGlowColor.w, 0.0f, 1.0f, "%.2f");
                        }
                        
                        if (g_keystrokesGlowEnabled) {
                            float glowEnabledCol[4] = {g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w};
                            if (ImGui::ColorEdit4("Glow Color (Enabled)##KS", glowEnabledCol)) {
                                g_keystrokesGlowEnabledColor = ImVec4(glowEnabledCol[0], glowEnabledCol[1], glowEnabledCol[2], glowEnabledCol[3]);
                            }
                            ImGui::SliderFloat("Glow Enabled Opacity##KS", &g_keystrokesGlowEnabledColor.w, 0.0f, 1.0f, "%.2f");
                        }
                        
                        ImGui::Separator();
                        ImGui::Text("Mouse Buttons Configuration");
                        
                        // Mouse Button Settings
                        ImGui::Checkbox("Show Mouse Buttons##KS", &g_keystrokesShowMouseButtons);
                        ImGui::Checkbox("Show LMB & RMB Labels##KS", &g_keystrokesShowLMBRMB);
                        ImGui::Checkbox("Hide CPS Display##KS", &g_keystrokesHideCPS);
                        
                        if (g_keystrokesShowMouseButtons) {
                            ImGui::Separator();
                            ImGui::Text("CPS Text Formats");
                            
                            static char lmbFormatBuf[256];
                            static char rmbFormatBuf[256];
                            static bool formatBufInit = false;
                            
                            if (!formatBufInit) {
                                strncpy_s(lmbFormatBuf, sizeof(lmbFormatBuf), g_keystrokesLMBFormatText.c_str(), _TRUNCATE);
                                strncpy_s(rmbFormatBuf, sizeof(rmbFormatBuf), g_keystrokesRMBFormatText.c_str(), _TRUNCATE);
                                formatBufInit = true;
                            }
                            
                            if (ImGui::InputText("LMB Format##KS", lmbFormatBuf, sizeof(lmbFormatBuf))) {
                                g_keystrokesLMBFormatText = std::string(lmbFormatBuf);
                            }
                            if (ImGui::InputText("RMB Format##KS", rmbFormatBuf, sizeof(rmbFormatBuf))) {
                                g_keystrokesRMBFormatText = std::string(rmbFormatBuf);
                            }
                            ImGui::TextWrapped("Use {VALUE} to display CPS count");
                            
                            ImGui::Separator();
                            ImGui::Text("Button Texts");
                            
                            static char wTextBuf[64] = "W";
                            static char aTextBuf[64] = "A";
                            static char sTextBuf[64] = "S";
                            static char dTextBuf[64] = "D";
                            static char lmbTextBuf[64] = "LMB";
                            static char rmbTextBuf[64] = "RMB";
                            static bool buttonTextInit = false;
                            
                            if (!buttonTextInit) {
                                strncpy_s(wTextBuf, sizeof(wTextBuf), g_keystrokesWText.c_str(), _TRUNCATE);
                                strncpy_s(aTextBuf, sizeof(aTextBuf), g_keystrokesAText.c_str(), _TRUNCATE);
                                strncpy_s(sTextBuf, sizeof(sTextBuf), g_keystrokesSText.c_str(), _TRUNCATE);
                                strncpy_s(dTextBuf, sizeof(dTextBuf), g_keystrokesDText.c_str(), _TRUNCATE);
                                strncpy_s(lmbTextBuf, sizeof(lmbTextBuf), g_keystrokesLMBText.c_str(), _TRUNCATE);
                                strncpy_s(rmbTextBuf, sizeof(rmbTextBuf), g_keystrokesRMBText.c_str(), _TRUNCATE);
                                buttonTextInit = true;
                            }
                            
                            if (ImGui::InputText("W Text##KS", wTextBuf, sizeof(wTextBuf))) {
                                g_keystrokesWText = std::string(wTextBuf);
                            }
                            if (ImGui::InputText("A Text##KS", aTextBuf, sizeof(aTextBuf))) {
                                g_keystrokesAText = std::string(aTextBuf);
                            }
                            if (ImGui::InputText("S Text##KS", sTextBuf, sizeof(sTextBuf))) {
                                g_keystrokesSText = std::string(sTextBuf);
                            }
                            if (ImGui::InputText("D Text##KS", dTextBuf, sizeof(dTextBuf))) {
                                g_keystrokesDText = std::string(dTextBuf);
                            }
                            if (ImGui::InputText("LMB Text##KS", lmbTextBuf, sizeof(lmbTextBuf))) {
                                g_keystrokesLMBText = std::string(lmbTextBuf);
                            }
                            if (ImGui::InputText("RMB Text##KS", rmbTextBuf, sizeof(rmbTextBuf))) {
                                g_keystrokesRMBText = std::string(rmbTextBuf);
                            }
                        }
                    }
                    
                    ImGui::Separator();
                    
                    // Show CPS Counter
                    ImGui::Checkbox("CPS Counter", &g_showCpsCounter);
                    
                    if (g_showCpsCounter) {
                        ImGui::Separator();
                        ImGui::Text("CPS Counter Configuration");
                        
                        // Format string
                        static char cpsFormatBuf[256];
                        if (!g_cpsCounterFirstRender) {
                            strncpy_s(cpsFormatBuf, sizeof(cpsFormatBuf), g_cpsCounterFormat.c_str(), _TRUNCATE);
                            g_cpsCounterFirstRender = true;
                        }
                        if (ImGui::InputText("Format String##CPS", cpsFormatBuf, sizeof(cpsFormatBuf))) {
                            g_cpsCounterFormat = std::string(cpsFormatBuf);
                        }
                        ImGui::TextWrapped("Use {LMB} and {RMB} for left and right mouse button CPS");
                        
                        // Text Scale
                        ImGui::SliderFloat("CPS Text Scale##CPSCounter", &g_cpsTextScale, 0.5f, 2.0f, "%.2f");
                        
                        // Position
                        ImGui::SliderFloat("CPS X Position##CPSCounter", &g_cpsCounterX, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("CPS Y Position##CPSCounter", &g_cpsCounterY, 0.0f, 1.0f, "%.2f");
                        
                        // Alignment
                        const char* alignmentItems[] = { "Left", "Center", "Right" };
                        ImGui::Combo("CPS Text Alignment##CPSCounter", &g_cpsCounterAlignment, alignmentItems, IM_ARRAYSIZE(alignmentItems));
                        
                        // Shadow
                        ImGui::Checkbox("CPS Text Shadow##CPSCounter", &g_cpsCounterShadow);
                        if (g_cpsCounterShadow) {
                            ImGui::SliderFloat("CPS Shadow Offset##CPSCounter", &g_cpsCounterShadowOffset, 0.0f, 10.0f, "%.1f");
                        }
                    }
                    
                    ImGui::PopStyleVar();  // Alpha
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Misc")) {
                    g_currentTab = 3;
                    if (g_currentTab != prevTab) {
                        g_tabChangeTime = GetTickCount64();
                        g_tabAnim = 0.0f;
                        g_previousTab = prevTab;
                    }
                    
                    // Apply animation if not first render
                    float alphaFade = (g_tabChangeTime > 0) ? SmoothInertia(g_tabAnim) : 1.0f;
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaFade);
                    
                    // Unlock FPS Checkbox
                    if (ImGui::Checkbox("Unlock FPS", &g_unlockFpsEnabled)) {
                        if (g_unlockFpsEnabled) {
                            g_unlockFpsEnableTime = GetTickCount64();
                            g_unlockFpsDisableTime = 0;
                        } else {
                            g_unlockFpsDisableTime = GetTickCount64();
                            g_unlockFpsEnableTime = 0;
                        }
                    }
                    
                    // FPS slider (if enabled)
                    if (g_unlockFpsEnabled) {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.40f, 0.18f, 0.28f, 0.8f));
                        ImGui::SliderFloat("FPS Limit", &g_fpsLimit, 5.0f, 240.0f, "%.0f FPS");
                        ImGui::PopStyleColor();
                    }
                    
                    ImGui::PopStyleVar();  // Alpha
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            
            ImGui::EndChild();
            
            ImGui::PopStyleColor();  // SliderGrabActive
            ImGui::PopStyleColor();  // SliderGrab
            ImGui::PopStyleVar();    // ItemSpacing
            ImGui::PopStyleVar();    // FrameRounding
            ImGui::PopStyleVar();    // WindowRounding
            }
            ImGui::End();
            ImGui::PopStyleVar();
            }  // Cierre del contenido visible (e > 0.12f)
    }  // Cierre del g_showMenu
    
    // Watermark display
    if (g_showWatermark || g_watermarkAnim > 0.01f) {
        g_watermarkHud.HandleDrag(g_showMenu);
        g_watermarkHud.ClampToScreen();

        float easedWatermarkAnim = EaseOutExpo(g_watermarkAnim);
        float watermarkAlpha = easedWatermarkAnim;
        
        ImVec4 chromaColor = GetChromaColor((float)GetTickCount64() / 1000.0f);
        chromaColor.w = watermarkAlpha;
        
        ImDrawList* watermarkDraw = ImGui::GetForegroundDrawList();
        ImVec2 textPos = g_watermarkHud.pos;
        
        // Glow blur (multiple layers with decreasing alpha)
        ImVec4 glowColor = chromaColor;
        float fontSize = 32.0f;  // 2x bigger than default 16pt
        ImFont* font = ImGui::GetFont();
        for (int i = 3; i >= 1; --i) {
            glowColor.w = (chromaColor.w * 0.4f) / i;
            watermarkDraw->AddText(font, fontSize, ImVec2(textPos.x + i, textPos.y), ImGui::GetColorU32(glowColor), "Aegleseeker");
            watermarkDraw->AddText(font, fontSize, ImVec2(textPos.x - i, textPos.y), ImGui::GetColorU32(glowColor), "Aegleseeker");
            watermarkDraw->AddText(font, fontSize, ImVec2(textPos.x, textPos.y + i), ImGui::GetColorU32(glowColor), "Aegleseeker");
            watermarkDraw->AddText(font, fontSize, ImVec2(textPos.x, textPos.y - i), ImGui::GetColorU32(glowColor), "Aegleseeker");
        }
        
        // Main text with chroma color - 2x bigger
        watermarkDraw->AddText(font, fontSize, textPos, ImGui::GetColorU32(chromaColor), "Aegleseeker");

        // Update hitbox size based on actual fontSize (32pt = 2x scale of default 16pt)
        float fontScale = fontSize / 16.0f;  // Calculate scale factor
        ImVec2 textSize = ImGui::CalcTextSize("Aegleseeker");
        g_watermarkHud.size = ImVec2(textSize.x * fontScale + 20, textSize.y * fontScale + 10);

        // DEBUG: show hitbox only when menu is open
        if (g_showMenu) {
            watermarkDraw->AddRect(
                g_watermarkHud.pos,
                ImVec2(g_watermarkHud.pos.x + g_watermarkHud.size.x, g_watermarkHud.pos.y + g_watermarkHud.size.y),
                IM_COL32(255, 100, 100, 80)
            );
        }
    }
    
    // Keystrokes display
    if (g_showKeystrokes || g_keystrokesAnim > 0.01f) {
        // Update color lerping based on key states
        bool handlerRes[7] = {
            (GetAsyncKeyState('W') & 0x8000) != 0,
            (GetAsyncKeyState('A') & 0x8000) != 0,
            (GetAsyncKeyState('S') & 0x8000) != 0,
            (GetAsyncKeyState('D') & 0x8000) != 0,
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0,
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0,
            (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0
        };

        // Update glow modifier based on key state
        for (int i = 0; i < 7; i++) {
            if (handlerRes[i]) {
                g_keystrokesStates[i] = LerpImVec4(g_keystrokesStates[i], g_keystrokesEnabledColor, 0.15f * g_keystrokesEdSpeed);
                g_keystrokesTextStates[i] = LerpImVec4(g_keystrokesTextStates[i], g_keystrokesTextEnabledColor, 0.15f * g_keystrokesEdSpeed);
                if (g_keystrokesTextShadow) {
                    g_keystrokesShadowStates[i] = LerpImVec4(g_keystrokesShadowStates[i], g_keystrokesEnabledShadowColor, 0.15f * g_keystrokesEdSpeed);
                }
                if (g_keystrokesGlowEnabled) {
                    g_keystrokesGlowModifier[i] = fminf(1.0f, g_keystrokesGlowModifier[i] + 0.2f * g_keystrokesGlowSpeed);
                }
            } else {
                g_keystrokesStates[i] = LerpImVec4(g_keystrokesStates[i], g_keystrokesBgColor, 0.15f * g_keystrokesEdSpeed);
                g_keystrokesTextStates[i] = LerpImVec4(g_keystrokesTextStates[i], g_keystrokesTextColor, 0.15f * g_keystrokesEdSpeed);
                if (g_keystrokesTextShadow) {
                    g_keystrokesShadowStates[i] = LerpImVec4(g_keystrokesShadowStates[i], g_keystrokesDisabledShadowColor, 0.15f * g_keystrokesEdSpeed);
                }
                if (g_keystrokesGlowEnabled) {
                    g_keystrokesGlowModifier[i] = fmaxf(0.0f, g_keystrokesGlowModifier[i] - 0.1f * g_keystrokesGlowSpeed);
                }
            }
        }

        float easedAnim = EaseOutExpo(g_keystrokesAnim);
        float keystrokesAlpha = easedAnim * 255.0f;
        
        // Keystrokes layout size calculation
        float scaledSize = 130.0f * g_keystrokesUIScale;
        float scaledSpacebarWidth = (scaledSize * 3.0f + g_keystrokesKeySpacing * 2.0f * g_keystrokesUIScale) * g_keystrokesSpacebarWidth;
        float keySpacing = g_keystrokesKeySpacing * g_keystrokesUIScale;
        float keyHeight = 38.0f * g_keystrokesUIScale;
        ImVec2 keystrokesSize = ImVec2(
            scaledSize, 
            scaledSize * 1.15f + 
            (g_keystrokesShowMouseButtons ? (scaledSize * 0.5f) : 0.0f) +
            (g_keystrokesShowSpacebar ? (keyHeight + keySpacing) : 0.0f)
        );
        g_keystrokesHud.size = keystrokesSize;
        
        // Initialize position on first draw
        if (g_keystrokesHud.pos.y == 0) {
            g_keystrokesHud.pos = ImVec2(30, sh - 250);
        }
        
        // Handle drag and clamp when menu open
        if (g_showMenu) {
            g_keystrokesHud.HandleDrag(true);
            g_keystrokesHud.ClampToScreen();
        }
        
        // Position with smooth animation
        float animatedYOffset = 150.0f - (150.0f * easedAnim);
        ImVec2 finalPos = ImVec2(
            g_keystrokesHud.pos.x,
            g_keystrokesHud.pos.y + animatedYOffset
        );
        
        if (finalPos.x < -10000 || finalPos.x > 10000) finalPos.x = 30;
        if (finalPos.y < -10000 || finalPos.y > 10000) finalPos.y = sh - 250;
        
        ImDrawList* keystrokesDraw = ImGui::GetForegroundDrawList();
        if (keystrokesDraw && keystrokesAlpha > 1.0f) {
            float baseX = finalPos.x;
            float baseY = finalPos.y;
            float containerWidth = scaledSize;
            float padding = 6.0f * g_keystrokesUIScale;
            float rounding = g_keystrokesRounding * g_keystrokesUIScale;
            float textShadowOffset = g_keystrokesTextShadowOffset * g_keystrokesUIScale * 10.0f;
            
            // Apply blur effect to background if enabled
            if (g_keystrokesShowBg && g_keystrokesBlurEffect) {
                keystrokesDraw->AddRectFilled(finalPos, ImVec2(finalPos.x + keystrokesSize.x, finalPos.y + keystrokesSize.y), IM_COL32(0, 0, 0, 20), rounding);
            }
            
            // Draw background shadow if enabled
            if (g_keystrokesShowBg && g_keystrokesRectShadow) {
                float shadowOffset = g_keystrokesRectShadowOffset * g_keystrokesUIScale * 10.0f;
                keystrokesDraw->AddRectFilled(
                    ImVec2(finalPos.x + shadowOffset, finalPos.y + shadowOffset),
                    ImVec2(finalPos.x + keystrokesSize.x + shadowOffset, finalPos.y + keystrokesSize.y + shadowOffset),
                    ImGui::GetColorU32(g_keystrokesRectShadowColor),
                    rounding
                );
            }
            
            // Draw main background if enabled
            if (g_keystrokesShowBg) {
                keystrokesDraw->AddRectFilled(finalPos, ImVec2(finalPos.x + keystrokesSize.x, finalPos.y + keystrokesSize.y), ImGui::GetColorU32(g_keystrokesBgColor), rounding);
            }
            
            bool wPressed = handlerRes[0];
            bool aPressed = handlerRes[1];
            bool sPressed = handlerRes[2];
            bool dPressed = handlerRes[3];
            bool lmbPressed = handlerRes[4];
            bool rmbPressed = handlerRes[5];
            bool spacePressed = handlerRes[6];
            
            ImFont* keystrokesFont = ImGui::GetFont();
            if (keystrokesFont) {
                float asdWidth = (containerWidth - (2 * keySpacing)) / 3.0f;
                float wWidth = asdWidth;
                float wX = baseX + (containerWidth - wWidth) / 2.0f;
                ImVec2 wPos = ImVec2(wX, baseY + padding);

                // W Key - with lerped colors and glow
                ImU32 wBgCol = ImGui::GetColorU32(g_keystrokesStates[0]);
                
                // Glow for disabled state
                if (g_keystrokesGlow && g_keystrokesGlowModifier[0] > 0.05f) {
                    ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[0] * (g_keystrokesGlowAmount / 100.0f)));
                    keystrokesDraw->AddRect(wPos, ImVec2(wPos.x + wWidth, wPos.y + keyHeight), glowCol, rounding);
                }
                
                // Glow for enabled state
                if (g_keystrokesGlowEnabled && wPressed && g_keystrokesGlowModifier[0] > 0.05f) {
                    ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[0] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                    keystrokesDraw->AddRect(wPos, ImVec2(wPos.x + wWidth, wPos.y + keyHeight), glowEnabledCol, rounding);
                }
                
                keystrokesDraw->AddRectFilled(wPos, ImVec2(wPos.x + wWidth, wPos.y + keyHeight), wBgCol, rounding);
                if (g_keystrokesBorder) {
                    keystrokesDraw->AddRect(wPos, ImVec2(wPos.x + wWidth, wPos.y + keyHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                }
                
                float textPosX = wPos.x + (12.0f * g_keystrokesUIScale) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                float textPosY = wPos.y + (8.0f * g_keystrokesUIScale) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                float wasdTextScale = 16.0f * g_keystrokesUIScale * g_keystrokesTextScale;
                
                if (g_keystrokesTextShadow) {
                    keystrokesDraw->AddText(keystrokesFont, wasdTextScale, ImVec2(textPosX + textShadowOffset, textPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[0]), g_keystrokesWText.c_str());
                }
                keystrokesDraw->AddText(keystrokesFont, wasdTextScale, ImVec2(textPosX, textPosY), ImGui::GetColorU32(g_keystrokesTextStates[0]), g_keystrokesWText.c_str());
                
                float row2Y = baseY + padding + keyHeight + keySpacing;
                
                // A Key
                ImVec2 aPos = ImVec2(baseX, row2Y);
                ImU32 aBgCol = ImGui::GetColorU32(g_keystrokesStates[1]);
                
                if (g_keystrokesGlow && g_keystrokesGlowModifier[1] > 0.05f) {
                    ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[1] * (g_keystrokesGlowAmount / 100.0f)));
                    keystrokesDraw->AddRect(aPos, ImVec2(aPos.x + asdWidth, aPos.y + keyHeight), glowCol, rounding);
                }
                
                if (g_keystrokesGlowEnabled && aPressed && g_keystrokesGlowModifier[1] > 0.05f) {
                    ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[1] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                    keystrokesDraw->AddRect(aPos, ImVec2(aPos.x + asdWidth, aPos.y + keyHeight), glowEnabledCol, rounding);
                }
                
                keystrokesDraw->AddRectFilled(aPos, ImVec2(aPos.x + asdWidth, aPos.y + keyHeight), aBgCol, rounding);
                if (g_keystrokesBorder) {
                    keystrokesDraw->AddRect(aPos, ImVec2(aPos.x + asdWidth, aPos.y + keyHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                }
                
                float aTextPosX = aPos.x + (10.0f * g_keystrokesUIScale) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                float aTextPosY = aPos.y + (6.0f * g_keystrokesUIScale) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                float asdTextScale = 18.0f * g_keystrokesUIScale * g_keystrokesTextScale;
                
                if (g_keystrokesTextShadow) {
                    keystrokesDraw->AddText(keystrokesFont, asdTextScale, ImVec2(aTextPosX + textShadowOffset, aTextPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[1]), g_keystrokesAText.c_str());
                }
                keystrokesDraw->AddText(keystrokesFont, asdTextScale, ImVec2(aTextPosX, aTextPosY), ImGui::GetColorU32(g_keystrokesTextStates[1]), g_keystrokesAText.c_str());
                
                // S Key
                ImVec2 sPos = ImVec2(baseX + asdWidth + keySpacing, row2Y);
                ImU32 sBgCol = ImGui::GetColorU32(g_keystrokesStates[2]);
                
                if (g_keystrokesGlow && g_keystrokesGlowModifier[2] > 0.05f) {
                    ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[2] * (g_keystrokesGlowAmount / 100.0f)));
                    keystrokesDraw->AddRect(sPos, ImVec2(sPos.x + asdWidth, sPos.y + keyHeight), glowCol, rounding);
                }
                
                if (g_keystrokesGlowEnabled && sPressed && g_keystrokesGlowModifier[2] > 0.05f) {
                    ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[2] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                    keystrokesDraw->AddRect(sPos, ImVec2(sPos.x + asdWidth, sPos.y + keyHeight), glowEnabledCol, rounding);
                }
                
                keystrokesDraw->AddRectFilled(sPos, ImVec2(sPos.x + asdWidth, sPos.y + keyHeight), sBgCol, rounding);
                if (g_keystrokesBorder) {
                    keystrokesDraw->AddRect(sPos, ImVec2(sPos.x + asdWidth, sPos.y + keyHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                }
                
                float sTextPosX = sPos.x + (10.0f * g_keystrokesUIScale) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                float sTextPosY = sPos.y + (6.0f * g_keystrokesUIScale) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                
                if (g_keystrokesTextShadow) {
                    keystrokesDraw->AddText(keystrokesFont, asdTextScale, ImVec2(sTextPosX + textShadowOffset, sTextPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[2]), g_keystrokesSText.c_str());
                }
                keystrokesDraw->AddText(keystrokesFont, asdTextScale, ImVec2(sTextPosX, sTextPosY), ImGui::GetColorU32(g_keystrokesTextStates[2]), g_keystrokesSText.c_str());
                
                // D Key
                ImVec2 dPos = ImVec2(baseX + (asdWidth + keySpacing) * 2, row2Y);
                ImU32 dBgCol = ImGui::GetColorU32(g_keystrokesStates[3]);
                
                if (g_keystrokesGlow && g_keystrokesGlowModifier[3] > 0.05f) {
                    ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[3] * (g_keystrokesGlowAmount / 100.0f)));
                    keystrokesDraw->AddRect(dPos, ImVec2(dPos.x + asdWidth, dPos.y + keyHeight), glowCol, rounding);
                }
                
                if (g_keystrokesGlowEnabled && dPressed && g_keystrokesGlowModifier[3] > 0.05f) {
                    ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[3] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                    keystrokesDraw->AddRect(dPos, ImVec2(dPos.x + asdWidth, dPos.y + keyHeight), glowEnabledCol, rounding);
                }
                
                keystrokesDraw->AddRectFilled(dPos, ImVec2(dPos.x + asdWidth, dPos.y + keyHeight), dBgCol, rounding);
                if (g_keystrokesBorder) {
                    keystrokesDraw->AddRect(dPos, ImVec2(dPos.x + asdWidth, dPos.y + keyHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                }
                
                float dTextPosX = dPos.x + (10.0f * g_keystrokesUIScale) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                float dTextPosY = dPos.y + (6.0f * g_keystrokesUIScale) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                
                if (g_keystrokesTextShadow) {
                    keystrokesDraw->AddText(keystrokesFont, asdTextScale, ImVec2(dTextPosX + textShadowOffset, dTextPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[3]), g_keystrokesDText.c_str());
                }
                keystrokesDraw->AddText(keystrokesFont, asdTextScale, ImVec2(dTextPosX, dTextPosY), ImGui::GetColorU32(g_keystrokesTextStates[3]), g_keystrokesDText.c_str());
                
                // Row 3: LMB, RMB / Spacebar
                float row3Y = row2Y + keyHeight + keySpacing;
                
                if (g_keystrokesShowMouseButtons) {
                    float lmbRmbWidth = (containerWidth - keySpacing) / 2.0f;
                    
                    // LMB
                    ImVec2 lmbPos = ImVec2(baseX, row3Y);
                    ImU32 lmbBgCol = ImGui::GetColorU32(g_keystrokesStates[4]);
                    
                    if (g_keystrokesGlow && g_keystrokesGlowModifier[4] > 0.05f) {
                        ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[4] * (g_keystrokesGlowAmount / 100.0f)));
                        keystrokesDraw->AddRect(lmbPos, ImVec2(lmbPos.x + lmbRmbWidth, lmbPos.y + keyHeight), glowCol, rounding);
                    }
                    
                    if (g_keystrokesGlowEnabled && lmbPressed && g_keystrokesGlowModifier[4] > 0.05f) {
                        ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[4] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                        keystrokesDraw->AddRect(lmbPos, ImVec2(lmbPos.x + lmbRmbWidth, lmbPos.y + keyHeight), glowEnabledCol, rounding);
                    }
                    
                    keystrokesDraw->AddRectFilled(lmbPos, ImVec2(lmbPos.x + lmbRmbWidth, lmbPos.y + keyHeight), lmbBgCol, rounding);
                    if (g_keystrokesBorder) {
                        keystrokesDraw->AddRect(lmbPos, ImVec2(lmbPos.x + lmbRmbWidth, lmbPos.y + keyHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                    }
                    
                    float lmbTextPosX = lmbPos.x + (15.0f * g_keystrokesUIScale) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                    float lmbTextPosY = lmbPos.y + (3.0f * g_keystrokesUIScale) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                    float mouseTextScale = 14.0f * g_keystrokesUIScale * g_keystrokesTextScale2;
                    
                    if (g_keystrokesTextShadow) {
                        keystrokesDraw->AddText(keystrokesFont, mouseTextScale, ImVec2(lmbTextPosX + textShadowOffset, lmbTextPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[4]), g_keystrokesLMBText.c_str());
                    }
                    keystrokesDraw->AddText(keystrokesFont, mouseTextScale, ImVec2(lmbTextPosX, lmbTextPosY), ImGui::GetColorU32(g_keystrokesTextStates[4]), g_keystrokesLMBText.c_str());
                    
                    // LMB CPS (with {VALUE} replacement)
                    if (!g_keystrokesHideCPS) {
                        std::string lmbCpsStr = ProcessKeystrokesFormat(g_keystrokesLMBFormatText, g_lmbCps);
                        float cpsMiniScale = 11.0f * g_keystrokesUIScale * g_keystrokesTextScale2;
                        if (g_keystrokesTextShadow) {
                            keystrokesDraw->AddText(keystrokesFont, cpsMiniScale, ImVec2(lmbTextPosX + textShadowOffset, lmbTextPosY + 19.0f * g_keystrokesUIScale + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[4]), lmbCpsStr.c_str());
                        }
                        keystrokesDraw->AddText(keystrokesFont, cpsMiniScale, ImVec2(lmbTextPosX, lmbTextPosY + 19.0f * g_keystrokesUIScale), ImGui::GetColorU32(g_keystrokesTextStates[4]), lmbCpsStr.c_str());
                    }
                    
                    // RMB
                    if (g_keystrokesShowLMBRMB) {
                        ImVec2 rmbPos = ImVec2(baseX + lmbRmbWidth + keySpacing, row3Y);
                        ImU32 rmbBgCol = ImGui::GetColorU32(g_keystrokesStates[5]);
                        
                        if (g_keystrokesGlow && g_keystrokesGlowModifier[5] > 0.05f) {
                            ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[5] * (g_keystrokesGlowAmount / 100.0f)));
                            keystrokesDraw->AddRect(rmbPos, ImVec2(rmbPos.x + lmbRmbWidth, rmbPos.y + keyHeight), glowCol, rounding);
                        }
                        
                        if (g_keystrokesGlowEnabled && rmbPressed && g_keystrokesGlowModifier[5] > 0.05f) {
                            ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[5] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                            keystrokesDraw->AddRect(rmbPos, ImVec2(rmbPos.x + lmbRmbWidth, rmbPos.y + keyHeight), glowEnabledCol, rounding);
                        }
                        
                        keystrokesDraw->AddRectFilled(rmbPos, ImVec2(rmbPos.x + lmbRmbWidth, rmbPos.y + keyHeight), rmbBgCol, rounding);
                        if (g_keystrokesBorder) {
                            keystrokesDraw->AddRect(rmbPos, ImVec2(rmbPos.x + lmbRmbWidth, rmbPos.y + keyHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                        }
                        
                        float rmbTextPosX = rmbPos.x + (15.0f * g_keystrokesUIScale) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                        float rmbTextPosY = rmbPos.y + (3.0f * g_keystrokesUIScale) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                        
                        if (g_keystrokesTextShadow) {
                            keystrokesDraw->AddText(keystrokesFont, mouseTextScale, ImVec2(rmbTextPosX + textShadowOffset, rmbTextPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[5]), g_keystrokesRMBText.c_str());
                        }
                        keystrokesDraw->AddText(keystrokesFont, mouseTextScale, ImVec2(rmbTextPosX, rmbTextPosY), ImGui::GetColorU32(g_keystrokesTextStates[5]), g_keystrokesRMBText.c_str());
                        
                        // RMB CPS (with {VALUE} replacement)
                        if (!g_keystrokesHideCPS) {
                            std::string rmbCpsStr = ProcessKeystrokesFormat(g_keystrokesRMBFormatText, g_rmbCps);
                            float cpsMiniScale = 11.0f * g_keystrokesUIScale * g_keystrokesTextScale2;
                            if (g_keystrokesTextShadow) {
                                keystrokesDraw->AddText(keystrokesFont, cpsMiniScale, ImVec2(rmbTextPosX + textShadowOffset, rmbTextPosY + 19.0f * g_keystrokesUIScale + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[5]), rmbCpsStr.c_str());
                            }
                            keystrokesDraw->AddText(keystrokesFont, cpsMiniScale, ImVec2(rmbTextPosX, rmbTextPosY + 19.0f * g_keystrokesUIScale), ImGui::GetColorU32(g_keystrokesTextStates[5]), rmbCpsStr.c_str());
                        }
                    }
                }
                
                // Row 4: Spacebar (under LMB/RMB)
                if (g_keystrokesShowSpacebar) {
                    float row4Y = row3Y + keyHeight + keySpacing;
                    float spacebarWidth = containerWidth;      // Full width
                    float spacebarHeight = keyHeight * 0.7f;   // 70% of key height - thinner
                    float spacebarX = baseX;
                    
                    ImVec2 spacebarPos = ImVec2(spacebarX, row4Y);
                    ImU32 spacebarBgCol = ImGui::GetColorU32(g_keystrokesStates[6]);
                    
                    // Glow for disabled state
                    if (g_keystrokesGlow && g_keystrokesGlowModifier[6] > 0.05f) {
                        ImU32 glowCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowColor.x, g_keystrokesGlowColor.y, g_keystrokesGlowColor.z, g_keystrokesGlowColor.w * g_keystrokesGlowModifier[6] * (g_keystrokesGlowAmount / 100.0f)));
                        keystrokesDraw->AddRect(spacebarPos, ImVec2(spacebarPos.x + spacebarWidth, spacebarPos.y + spacebarHeight), glowCol, rounding);
                    }
                    
                    // Glow for enabled state
                    if (g_keystrokesGlowEnabled && spacePressed && g_keystrokesGlowModifier[6] > 0.05f) {
                        ImU32 glowEnabledCol = ImGui::GetColorU32(ImVec4(g_keystrokesGlowEnabledColor.x, g_keystrokesGlowEnabledColor.y, g_keystrokesGlowEnabledColor.z, g_keystrokesGlowEnabledColor.w * g_keystrokesGlowModifier[6] * (g_keystrokesGlowEnabledAmount / 100.0f)));
                        keystrokesDraw->AddRect(spacebarPos, ImVec2(spacebarPos.x + spacebarWidth, spacebarPos.y + spacebarHeight), glowEnabledCol, rounding);
                    }
                    
                    keystrokesDraw->AddRectFilled(spacebarPos, ImVec2(spacebarPos.x + spacebarWidth, spacebarPos.y + spacebarHeight), spacebarBgCol, rounding);
                    if (g_keystrokesBorder) {
                        keystrokesDraw->AddRect(spacebarPos, ImVec2(spacebarPos.x + spacebarWidth, spacebarPos.y + spacebarHeight), ImGui::GetColorU32(g_keystrokesBorderColor), rounding, 0, g_keystrokesBorderWidth);
                    }
                    
                    // Spacebar text (centered) - use underscores
                    float spaceTextPosX = spacebarPos.x + (spacebarWidth / 2.0f) - (25.0f * g_keystrokesUIScale / 2.0f) + (g_keystrokesTextXOffset * g_keystrokesUIScale);
                    float spaceTextPosY = spacebarPos.y + (spacebarHeight / 2.0f) - (8.0f * g_keystrokesUIScale / 2.0f) + (g_keystrokesTextYOffset * g_keystrokesUIScale);
                    
                    if (g_keystrokesTextShadow) {
                        keystrokesDraw->AddText(keystrokesFont, 16.0f * g_keystrokesUIScale, ImVec2(spaceTextPosX + textShadowOffset, spaceTextPosY + textShadowOffset), ImGui::GetColorU32(g_keystrokesShadowStates[6]), "___");
                    }
                    keystrokesDraw->AddText(keystrokesFont, 16.0f * g_keystrokesUIScale, ImVec2(spaceTextPosX, spaceTextPosY), ImGui::GetColorU32(g_keystrokesTextStates[6]), "___");
                }
                
                // DEBUG: show hitbox only when menu is open
                if (g_showMenu) {
                    keystrokesDraw->AddRect(
                        finalPos,
                        ImVec2(finalPos.x + keystrokesSize.x, finalPos.y + keystrokesSize.y),
                        IM_COL32(0, 150, 255, 100),
                        rounding,
                        0,
                        2.0f
                    );
                }
            }
        }
    }
    
    // --- 📊 RENDER INFO (TOP LEFT, BELOW WATERMARK) - DRAGGABLE ---
    if (g_showRenderInfo || g_renderInfoAnim > 0.01f) {
        float easedAnim = EaseOutExpo(g_renderInfoAnim);
        float infoAlpha = 0.7f * easedAnim;
        
        // Render info dimensions
        ImVec2 renderInfoSize = ImVec2(220, 120);
        g_renderInfoHud.size = renderInfoSize;
        
        // Initialize position on first draw
        if (g_renderInfoHud.pos.x == 0 && g_renderInfoHud.pos.y == 0) {
            g_renderInfoHud.pos = ImVec2(10, 50);
        }
        
        // Handle drag and clamp when menu open
        if (g_showMenu) {
            g_renderInfoHud.HandleDrag(true);
            g_renderInfoHud.ClampToScreen();
        }
        
        // Position with smooth animation
        float animatedXOffset = -150.0f + (160.0f * easedAnim);
        ImVec2 finalPos = ImVec2(
            g_renderInfoHud.pos.x + animatedXOffset,
            g_renderInfoHud.pos.y
        );
        
        // Limitar valores para seguridad
        if (finalPos.x < -10000 || finalPos.x > 10000) finalPos.x = 10;
        if (finalPos.y < -10000 || finalPos.y > 10000) finalPos.y = 50;
        
        ImGui::SetNextWindowPos(finalPos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(infoAlpha);
        ImGui::SetNextWindowSize(renderInfoSize, ImGuiCond_Always);
        
        // Allow drag only when menu open
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
        if (!g_showMenu) {
            flags |= ImGuiWindowFlags_NoMove;
        }
        
        if (ImGui::Begin("##RenderInfo", nullptr, flags)) {
            ImGuiIO& io = ImGui::GetIO();
            // Calculate real FPS from delta time
            if (io.DeltaTime > 0.0f) {
                g_fpsCounter = 1.0f / io.DeltaTime;
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.5f, easedAnim));
            ImGui::Text("FPS: %.0f", g_fpsCounter);
            ImGui::Text("Render: DX11");
            ImGui::Text("OS: Windows");
            ImGui::Text("Build: Release");
            ImGui::Text("Compiler: GCC");
            ImGui::PopStyleColor();
            
            // Draw debug rect when menu open
            if (g_showMenu) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 winPos = ImGui::GetWindowPos();
                ImVec2 winSize = ImGui::GetWindowSize();
                drawList->AddRectFilled(
                    winPos,
                    ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
                    IM_COL32(0, 20, 40, 120),
                    8.0f
                );
                drawList->AddRect(
                    winPos,
                    ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
                    IM_COL32(0, 200, 100, 180),
                    8.0f,
                    0,
                    2.0f
                );
            }
            
            ImGui::End();
        }
    }

    // CPS counter display
    if (g_showCpsCounter || g_cpsCounterAnim > 0.01f) {
        // Initialize position on first draw
        if (g_cpsHud.pos.x == 500 && g_cpsHud.pos.y == 400) {
            g_cpsHud.pos = ImVec2(sw / 2.0f - 50, sh - 100);
        }
        
        // Calculate text size for collision box
        std::string cpsText = ProcessCPSCounterFormat(g_cpsCounterFormat, g_lmbCps, g_rmbCps);
        ImFont* cpsFont = ImGui::GetFont();
        if (cpsFont) {
            float fontSize = 18.0f * g_cpsTextScale;
            ImVec2 textSize = ImGui::CalcTextSize(cpsText.c_str());
            // Update collision size from text
            g_cpsHud.size = ImVec2(
                textSize.x * g_cpsTextScale + 4.0f,  // Small padding
                fontSize + 4.0f
            );
        }
        
        // Handle CPS counter drag when menu is open
        if (g_showMenu) {
            g_cpsHud.HandleDrag(true);
            g_cpsHud.ClampToScreen();
            
            // Draw collision border
            ImDrawList* debugDraw = ImGui::GetForegroundDrawList();
            if (debugDraw) {
                ImVec2 p1 = g_cpsHud.pos;
                ImVec2 p2 = ImVec2(g_cpsHud.pos.x + g_cpsHud.size.x, g_cpsHud.pos.y + g_cpsHud.size.y);
                debugDraw->AddRect(p1, p2, ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 0.8f)), 0.0f, 0, 2.0f);
            }
        }
        
        float easedAnim = EaseOutExpo(g_cpsCounterAnim);
        float cpsAlpha = easedAnim * 255.0f;
        
        ImDrawList* cpsDraw = ImGui::GetForegroundDrawList();
        if (cpsDraw && cpsAlpha > 1.0f) {
            if (cpsFont) {
                float fontSize = 18.0f * g_cpsTextScale;
                ImVec2 textSize = ImGui::CalcTextSize(cpsText.c_str());
                
                // Use HUD position instead
                float posX = g_cpsHud.pos.x;
                float posY = g_cpsHud.pos.y;
                
                switch (g_cpsCounterAlignment) {
                    case 0: // Left
                        break;
                    case 1: // Center
                        posX += (g_cpsHud.size.x - (textSize.x * g_cpsTextScale)) / 2.0f;
                        break;
                    case 2: // Right
                        posX += g_cpsHud.size.x - (textSize.x * g_cpsTextScale);
                        break;
                }
                
                posY += (g_cpsHud.size.y - (fontSize)) / 2.0f;  // Vertical centering
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
                
                // Texto principal
                ImVec4 textCol = g_cpsTextColor;
                textCol.w = easedAnim;
                cpsDraw->AddText(cpsFont, fontSize, finalPos, ImGui::GetColorU32(textCol), cpsText.c_str());
            }
        }
    }

    ImGui::Render();
    
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    // 🔒 UNLOCK FPS - Desabilitar VSync completamente para UWP
    // Forzar VSync OFF para mejor performance
    g_vsync = false;
    
    if (g_unlockFpsEnabled && g_fpsLimit > 0.0f) {
        static LARGE_INTEGER lastFramePC = { 0 };
        static LARGE_INTEGER frequency = { 0 };
        
        // Obtener frecuencia una sola vez
        if (frequency.QuadPart == 0) {
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&lastFramePC);
        }
        
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        
        // Calculate frame time in microseconds
        double targetFrameTimeUs = 1000000.0 / g_fpsLimit;  // microseconds
        double elapsedUs = (double)(now.QuadPart - lastFramePC.QuadPart) * 1000000.0 / frequency.QuadPart;
        
        // Sleep if faster than target frametime
        if (elapsedUs < targetFrameTimeUs) {
            double remainingUs = targetFrameTimeUs - elapsedUs;
            
            // Sleep un poco menos para evitar sobrepase
            if (remainingUs > 500.0) {  // > 0.5ms
                Sleep((DWORD)((remainingUs - 500.0) / 1000.0));  // Convertir a ms
            }
            
            // Busy-wait for precision
            LARGE_INTEGER waitStart;
            QueryPerformanceCounter(&waitStart);
            while (true) {
                LARGE_INTEGER waitNow;
                QueryPerformanceCounter(&waitNow);
                double waitElapsedUs = (double)(waitNow.QuadPart - waitStart.QuadPart) * 1000000.0 / frequency.QuadPart;
                if (waitElapsedUs >= remainingUs) break;
            }
        }
        
        lastFramePC = now;
    }
    
    // Forzar VSync OFF para UWP - SyncInterval = 0 desabilita VSync completamente
    return oPresent(pSwapChain, 0, Flags);
}

// Hook initialization
void* GetVTableAddress(int index) {
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* d; ID3D11DeviceContext* c; IDXGISwapChain* s;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &fl, 1, D3D11_SDK_VERSION, &sd, &s, &d, NULL, &c))) return 0;
    void* a = (*(void***)s)[index];
    s->Release(); d->Release(); c->Release(); return a;
}

DWORD WINAPI MainThread(LPVOID lpReserved) {
    Sleep(2000); 
    MH_Initialize();
    
    void* pPres = GetVTableAddress(8);
    void* pRes = GetVTableAddress(13);
    void* pSetCP = (void*)GetProcAddress(GetModuleHandleA("user32.dll"), "SetCursorPos");
    void* pClipC = (void*)GetProcAddress(GetModuleHandleA("user32.dll"), "ClipCursor");
    
    MH_CreateHook(pPres, (LPVOID)hkPresent, (LPVOID*)&oPresent);
    MH_CreateHook(pRes, (LPVOID)hkResizeBuffers, (LPVOID*)&oResizeBuffers);
    MH_CreateHook(pSetCP, (LPVOID)hkSetCursorPos, (LPVOID*)&oSetCursorPos);
    MH_CreateHook(pClipC, (LPVOID)hkClipCursor, (LPVOID*)&oClipCursor);
    
    MH_EnableHook(MH_ALL_HOOKS);
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0);
    }
    return TRUE;
}