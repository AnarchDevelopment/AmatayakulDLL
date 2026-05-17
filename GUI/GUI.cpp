#include "GUI.hpp"
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_internal.h"
#include "../Modules/ModuleHeader.hpp"
#include "../Assets/resource.h"
#include <windows.h>
#include <algorithm>
#include <d3d11.h>
#include "../Assets/stb/stb_image.h"
#include "../Animations/Animations.hpp"

#include "../Modules/Terminal/Terminal.hpp"

extern HMODULE g_hModule;
extern ID3D11Device* pDevice;

// Static member initialization
bool GUI::g_showMenu = false;
float GUI::g_menuAnim = 0.0f;

int GUI::g_currentTab = 0;
int GUI::g_previousTab = 0;
ULONGLONG GUI::g_tabChangeTime = 0;
float GUI::g_tabAnim = 1.0f;

std::map<std::string, ImTextureID> GUI::g_icons;
std::string GUI::g_currentSettingsModule = "";

// Spectrum color constants (Adobe Spectrum Design System)
namespace Spectrum {
    static const ImU32 GRAY50  = 0xFFF8F8F8;
    static const ImU32 GRAY75  = 0xFFF0F0F0;
    static const ImU32 GRAY100 = 0xFFEAEAEA;
    static const ImU32 GRAY200 = 0xFFE0E0E0;
    static const ImU32 GRAY300 = 0xFFD4D4D4;
    static const ImU32 GRAY400 = 0xFFB8B8B8;
    static const ImU32 GRAY500 = 0xFFA0A0A0;
    static const ImU32 GRAY600 = 0xFF808080;
    static const ImU32 GRAY700 = 0xFF606060;
    static const ImU32 GRAY800 = 0xFF383838;
    static const ImU32 GRAY900 = 0xFF1A1A1A;
    static const ImU32 BLUE400 = 0xFF2680EB;
    static const ImU32 BLUE500 = 0xFF1473E6;
    static const ImU32 BLUE600 = 0xFF0D5CD6;
    static const ImU32 NONE    = 0x00000000;
}

void GUI::ApplyTheme() {
    ImGuiStyle* style = &ImGui::GetStyle();
    
    style->WindowRounding = 12.0f;
    style->ChildRounding = 10.0f;
    style->FrameRounding = 6.0f;
    style->GrabRounding = 6.0f;
    style->PopupRounding = 8.0f;
    style->ScrollbarRounding = 6.0f;
    
    style->WindowBorderSize = 0.0f;
    style->ChildBorderSize = 0.0f;
    style->PopupBorderSize = 1.0f;
    style->FrameBorderSize = 0.0f;
    
    style->ItemSpacing = ImVec2(10, 12);
    style->WindowPadding = ImVec2(0, 0); // No padding for main window to allow full sidebar
    
    ImVec4* colors = style->Colors;
    ImVec4 accent = ImVec4(0.85f, 0.05f, 0.10f, 1.0f); // Deep Blood Red
    
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.07f, 0.07f, 0.07f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    
    colors[ImGuiCol_CheckMark]              = accent;
    colors[ImGuiCol_SliderGrab]             = accent;
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(1.00f, 0.15f, 0.20f, 1.00f);
    
    colors[ImGuiCol_Button]                 = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = accent;
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.65f, 0.03f, 0.08f, 1.00f);
    
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = accent;
    colors[ImGuiCol_HeaderActive]           = accent;
    
    colors[ImGuiCol_Separator]              = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = accent;
    colors[ImGuiCol_SeparatorActive]        = accent;
}

static HMODULE GetCurrentModule() {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)&GetCurrentModule, &mbi, sizeof(mbi)) != 0) {
        return (HMODULE)mbi.AllocationBase;
    }
    return GetModuleHandleA("amatayakul.dll"); // Last resort fallback
}

static ID3D11ShaderResourceView* LoadTextureFromResource(ID3D11Device* device, int resourceId) {
    HMODULE hMod = GetCurrentModule();
    if (!hMod) return NULL;

    HRSRC hRes = FindResourceA(hMod, MAKEINTRESOURCEA(resourceId), (LPCSTR)10); // RT_RCDATA
    if (!hRes) return NULL;
    
    DWORD imageSize = SizeofResource(hMod, hRes);
    HGLOBAL hData = LoadResource(hMod, hRes);
    if (!hData) return NULL;
    LPVOID pData = LockResource(hData);
    
    int width, height, channels;
    unsigned char* image = stbi_load_from_memory((unsigned char*)pData, imageSize, &width, &height, &channels, 4);
    if (!image) return NULL;
    
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    
    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    device->CreateTexture2D(&desc, &subResource, &pTexture);
    
    ID3D11ShaderResourceView* out_srv = NULL;
    if (pTexture) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(pTexture, &srvDesc, &out_srv);
        pTexture->Release();
    }
    
    stbi_image_free(image);
    return out_srv;
}

void GUI::LoadFont() {
    ImGuiIO& io = ImGui::GetIO();
    HMODULE hMod = GetCurrentModule();
    if (!hMod) return;

    HRSRC hRes = FindResourceA(hMod, MAKEINTRESOURCEA(IDR_FONT_ROBOTO), (LPCSTR)10); // RT_RCDATA
    if (hRes) {
        DWORD fontSize = SizeofResource(hMod, hRes);
        HGLOBAL hData = LoadResource(hMod, hRes);
        if (hData) {
            LPVOID pFontData = LockResource(hData);
            
            // Let ImGui manage the memory to avoid allocation mismatches
            void* pFontCopy = ImGui::MemAlloc(fontSize);
            if (pFontCopy) {
                memcpy(pFontCopy, pFontData, fontSize);
                io.Fonts->AddFontFromMemoryTTF(pFontCopy, (int)fontSize, 18.0f);
            }
        }
    }
}

void GUI::LoadIcons(void* pDevice) {
    ID3D11Device* device = (ID3D11Device*)pDevice;
    
    struct IconRes { const char* name; int id; };
    IconRes icons[] = {
        {"cpscounter", IDR_ICON_CPS},
        {"fpscounter", IDR_ICON_FPS},
        {"gear", IDR_ICON_GEAR},
        {"keystrokes", IDR_ICON_KEYSTROKES},
        {"renderinfo", IDR_ICON_RENDERINFO},
        {"unlockfps", IDR_ICON_UNLOCKFPS},
        {"watermark", IDR_ICON_WATERMARK},
        {"arraylist", IDR_ICON_ARRAYLIST},
        {"back", IDR_ICON_BACK},
        {"logo", IDR_ICON_LOGO},
        {"dashboard", IDR_ICON_DASHBOARD},
        {"visuals", IDR_ICON_VISUALS},
        {"misc", IDR_ICON_MISC}
    };
    
    for (auto& ic : icons) {
        ID3D11ShaderResourceView* srv = LoadTextureFromResource(device, ic.id);
        if (srv) {
            Terminal::AddOutput("[GUI] Embedded icon loaded: " + std::string(ic.name));
        } else {
            Terminal::AddOutput("[GUI] FAILED to load embedded icon: " + std::string(ic.name));
        }
        g_icons[ic.name] = (ImTextureID)srv;
    }
}

void GUI::UpdateAnimation(ULONGLONG now, float dt) {
    g_menuAnim = g_showMenu ? 1.0f : 0.0f;
    
    // Tab animation (fade/slide transition)
    if (now - g_tabChangeTime < 300) {
        float elapsed = (float)(now - g_tabChangeTime) / 300.0f;
        g_tabAnim = elapsed;
    } else {
        g_tabAnim = 1.0f;
    }
}

#include "DX11/Shaders.hpp"

void GUI::RenderMenu(float screenWidth, float screenHeight) {
    if (!g_showMenu) return;

    // --- GAUSSIAN BLUR BACKGROUND ---
    // Apply a high-quality blur effect to the background using the Shaders system
    Shaders::SetDevice(pDevice); // pDevice is global from dllmain.cpp
    Shaders::NewFrame();
    Shaders::GausenBlur(ImGui::GetBackgroundDrawList(), ImVec4(0, 0, 0, 0.5f));
    Shaders::ClearBlur();

    ImGui::SetNextWindowSize(ImVec2(880.0f, 600.0f), ImGuiCond_FirstUseEver);
    
    // Smooth window scale animation (micro-animation)
    float windowAlpha = Animations::EaseOutExpo(g_menuAnim);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha);

    if (ImGui::Begin("Amatayakul Client", &g_showMenu, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        
        ImDrawList* windowDrawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();

        // 1. Sidebar Background & Rounded Corners
        float sidebarWidth = 220.0f;
        windowDrawList->AddRectFilled(windowPos, ImVec2(windowPos.x + sidebarWidth, windowPos.y + windowSize.y), 
                                     ImGui::GetColorU32(ImVec4(0.04f, 0.04f, 0.04f, 1.0f)), 12.0f, ImDrawFlags_RoundCornersLeft);
        
        // --- SIDEBAR CONTENT ---
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0), false, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::Spacing(); ImGui::Spacing();
        ImTextureID logoTex = g_icons.count("logo") ? g_icons["logo"] : (ImTextureID)0;
        if (logoTex) {
            ImGui::SetCursorPosX(20);
            ImGui::Image(logoTex, ImVec2(180, 72));
        } else {
            ImGui::SetCursorPosX(20);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.05f, 0.10f, 1.0f));
            ImGui::SetWindowFontScale(1.4f);
            ImGui::Text("  AMATAYAKUL");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Styled Tab Buttons
        auto StyledTabButton = [](const char* label, const char* iconName, int index) {
            ImGuiContext& g = *GImGui;
            ImGuiWindow* window = g.CurrentWindow;
            
            ImVec2 pos = window->DC.CursorPos;
            ImVec2 size(200, 48);
            ImGuiID id = window->GetID(label);
            ImRect bb(pos.x + 10, pos.y, pos.x + 10 + size.x, pos.y + size.y);
            
            ImGui::ItemSize(size);
            if (!ImGui::ItemAdd(bb, id)) return;

            bool hovered, held;
            bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
            if (pressed) {
                if (GUI::g_currentTab != index) {
                    GUI::g_previousTab = GUI::g_currentTab;
                    GUI::g_currentTab = index;
                    GUI::g_tabChangeTime = GetTickCount64();
                    GUI::g_currentSettingsModule = ""; // Reset settings view on tab change
                }
            }

            bool active = (GUI::g_currentTab == index);
            
            // Animation values
            static std::map<ImGuiID, float> hoverAnims;
            float& hoverAnim = hoverAnims[id];
            hoverAnim = Animations::Lerp(hoverAnim, hovered ? 1.0f : 0.0f, 0.15f);

            // Background rendering
            ImU32 colBg = ImGui::GetColorU32(active ? ImVec4(0.25f, 0.05f, 0.07f, 0.8f) : (hovered ? ImVec4(1, 1, 1, 0.05f) : ImVec4(0, 0, 0, 0)));
            window->DrawList->AddRectFilled(bb.Min, bb.Max, colBg, 6.0f);

            // Selection indicator (Red bar on left)
            if (active) {
                window->DrawList->AddRectFilled(ImVec2(bb.Min.x - 2, bb.Min.y + 10), ImVec2(bb.Min.x + 2, bb.Max.y - 10), 
                                                ImGui::GetColorU32(ImVec4(0.85f, 0.05f, 0.10f, 1.0f)), 2.0f);
            }

            // Icon & Text
            ImTextureID icon = g_icons.count(iconName) ? g_icons[iconName] : (ImTextureID)0;
            if (icon) {
                window->DrawList->AddImage(icon, ImVec2(bb.Min.x + 15, bb.Min.y + 12), ImVec2(bb.Min.x + 39, bb.Min.y + 36), 
                                          ImVec2(0,0), ImVec2(1,1), ImGui::GetColorU32(active ? ImVec4(1,1,1,1) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f)));
                window->DrawList->AddText(ImVec2(bb.Min.x + 50, bb.Min.y + 14), active ? IM_COL32_WHITE : IM_COL32(180, 180, 180, 255), label);
            } else {
                window->DrawList->AddText(ImVec2(bb.Min.x + 20, bb.Min.y + 14), active ? IM_COL32_WHITE : IM_COL32(180, 180, 180, 255), label);
            }

            ImGui::Spacing();
        };

        StyledTabButton("DASHBOARD", "dashboard", 0);
        StyledTabButton("VISUALS", "visuals", 1);
        StyledTabButton("MISC", "misc", 2);

        ImGui::EndChild(); // End Sidebar

        // --- CONTENT AREA ---
        ImGui::SetCursorPos(ImVec2(sidebarWidth + 20, 20));
        
        // 2. Tab Content Animation (Fade & Slide)
        float contentAlpha = Animations::EaseOutExpo(g_tabAnim);
        float contentSlide = (1.0f - contentAlpha) * 15.0f;
        
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, contentAlpha);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + contentSlide);

        ImGui::BeginChild("ContentArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        
        const char* tabNames[] = { "DASHBOARD", "VISUALS", "MISC" };
        ImGui::TextDisabled("AMATAYAKUL > %s", tabNames[GUI::g_currentTab]);
        ImGui::Spacing();

        ImGui::BeginChild("ContentScroll", ImVec2(0, 0), false);
        
        float windowWidth = ImGui::GetWindowWidth();
        float spacing = 20.0f;
        int columns = 3;
        float cardWidth = (windowWidth - (spacing * (columns + 1))) / columns;
        int currentColumn = 0;

        auto NextCard = [&]() {
            currentColumn++;
            if (currentColumn >= columns) {
                currentColumn = 0;
            } else {
                ImGui::SameLine(0, spacing);
            }
        };

        if (!GUI::g_currentSettingsModule.empty()) {
            // Settings Header
            ImTextureID backIcon = g_icons.count("back") ? g_icons["back"] : (ImTextureID)0;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.15f, 0.18f, 1.0f));
            
            if (backIcon) {
                if (ImGui::ImageButton("##Back", backIcon, ImVec2(24, 24))) {
                    GUI::g_currentSettingsModule = "";
                }
            } else {
                if (ImGui::Button("< Back", ImVec2(80, 32))) {
                    GUI::g_currentSettingsModule = "";
                }
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            ImGui::TextColored(ImVec4(0.85f, 0.05f, 0.10f, 1.0f), "MODULE SETTINGS > %s", GUI::g_currentSettingsModule.c_str());
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Render specific module settings
            if (GUI::g_currentSettingsModule == "renderinfo") RenderInfo::RenderMenu();
            else if (GUI::g_currentSettingsModule == "watermark") Watermark::RenderMenu();
            else if (GUI::g_currentSettingsModule == "arraylist") ArrayList::RenderMenu();
            else if (GUI::g_currentSettingsModule == "keystrokes") Keystrokes::RenderMenu();
            else if (GUI::g_currentSettingsModule == "cpscounter") CPSCounter::RenderMenu();
            else if (GUI::g_currentSettingsModule == "fpscounter") FPSCounter::RenderMenu();
            else if (GUI::g_currentSettingsModule == "unlockfps") UnlockFPS::RenderMenu();
            
        } else if (GUI::g_currentTab == 1) { // Visuals
            GUI::RenderModuleCard("Render Info", "renderinfo", &RenderInfo::g_showRenderInfo, nullptr);
            NextCard();
            GUI::RenderModuleCard("Watermark", "watermark", &Watermark::g_showWatermark, nullptr);
            NextCard();
            GUI::RenderModuleCard("ArrayList", "arraylist", &ArrayList::g_showArrayList, nullptr);
            NextCard();
            GUI::RenderModuleCard("Keystrokes", "keystrokes", &Keystrokes::g_showKeystrokes, nullptr);
            NextCard();
            GUI::RenderModuleCard("CPS Counter", "cpscounter", &CPSCounter::g_showCpsCounter, nullptr);
            NextCard();
            GUI::RenderModuleCard("FPS Counter", "fpscounter", &FPSCounter::g_showFpsCounter, nullptr);
        } else if (GUI::g_currentTab == 2) { // Misc
            GUI::RenderModuleCard("Unlock FPS", "unlockfps", &UnlockFPS::g_unlockFpsEnabled, nullptr);
        } else { // Dashboard
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.12f, 0.18f, 1.0f)); // Blood red
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("WELCOME TO");
            ImGui::SetWindowFontScale(1.0f);
            
            if (logoTex) {
                ImGui::Image(logoTex, ImVec2(300, 120));
            } else {
                ImGui::SetWindowFontScale(1.8f);
                ImGui::Text("AMATAYAKUL");
                ImGui::SetWindowFontScale(1.0f);
            }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Text("The best client for minecraft beta 0.15.10");
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextWrapped("Amatayakul Client is a premium PvP client designed for performance and aesthetics. It includes a modular HUD system with CPS/FPS counters, Keystrokes, and advanced watermark rendering.");
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Build: stable-v1.0.3");
        }

        ImGui::EndChild(); // End ContentScroll
        ImGui::EndChild(); // End ContentArea
        
        ImGui::PopStyleVar(); // Pop Content Alpha
    }
    ImGui::End();
    ImGui::PopStyleVar(); // Pop Window Alpha
}

void GUI::RenderModuleCard(const char* name, const char* iconName, bool* enabled, bool* showSettings) {
    float windowWidth = ImGui::GetWindowWidth();
    float spacing = 20.0f;
    int columns = 3;
    float cardWidth = (windowWidth - (spacing * (columns + 1))) / columns;
    ImVec2 size(cardWidth, 200); // Slightly taller for padding
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.07f, 0.08f, 0.6f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    std::string childId = std::string("##Card_") + name;
    ImGui::BeginChild(childId.c_str(), size, true, ImGuiWindowFlags_NoScrollbar);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 1. Icon Section (Top)
    ImTextureID iconTexture = (iconName && g_icons.count(iconName)) ? g_icons[iconName] : (ImTextureID)0;
    if (iconTexture) {
        ImGui::SetCursorPos(ImVec2((size.x - 48) / 2, 20));
        ImGui::Image(iconTexture, ImVec2(48, 48));
    } else {
        ImGui::SetCursorPos(ImVec2((size.x - 30) / 2, 20));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.5f, 0.6f, 1.0f));
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("[O]");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    
    // 2. Name Section (Middle)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    ImVec2 textSize = ImGui::CalcTextSize(name);
    ImGui::SetCursorPos(ImVec2((size.x - textSize.x) / 2, 75));
    ImGui::Text("%s", name);
    ImGui::PopStyleColor();

    float elementWidth = size.x - 20.0f;
    float elementX = 10.0f;

    // 3. Options Button (Clickable Bar)
    float barY = 110.0f;
    float barHeight = 35.0f;
    ImGui::SetCursorPos(ImVec2(elementX, barY));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.16f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.20f, 0.22f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.11f, 0.12f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    
    if (ImGui::Button((std::string("##OptionsBtn_") + name).c_str(), ImVec2(elementWidth, barHeight))) {
        g_currentSettingsModule = iconName;
    }
    
    // Draw Options Content (Text + Gear) over the button
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::SetCursorPos(ImVec2(elementX + (elementWidth / 2) - 30, barY + (barHeight - ImGui::GetTextLineHeight()) / 2));
    ImGui::SetWindowFontScale(0.85f);
    ImGui::Text("OPTIONS");
    ImGui::SetWindowFontScale(1.0f);
    
    ImTextureID gearIcon = g_icons.count("gear") ? g_icons["gear"] : (ImTextureID)0;
    if (gearIcon) {
        ImGui::SetCursorPos(ImVec2(elementX + elementWidth - 28, barY + (barHeight - 16) / 2));
        ImGui::Image(gearIcon, ImVec2(16, 16));
    }
    ImGui::PopStyleColor(4); // Text + Button colors

    // 4. Toggle Button (Very Bottom)
    float btnY = barY + barHeight + 8.0f; // 8px gap
    float btnHeight = 35.0f;
    ImGui::SetCursorPos(ImVec2(elementX, btnY));
    
    ImVec4 btnColor = *enabled ? ImVec4(0.0f, 0.68f, 0.45f, 1.0f) : ImVec4(0.75f, 0.12f, 0.28f, 1.0f);
    ImVec4 btnHover = btnColor; btnHover.w *= 0.85f;
    ImVec4 btnActive = btnColor; btnActive.w *= 0.7f;

    ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnActive);
    
    if (ImGui::Button((std::string(*enabled ? "ENABLED##" : "DISABLED##") + name).c_str(), ImVec2(elementWidth, btnHeight))) {
        *enabled = !(*enabled);
    }
    
    ImGui::PopStyleVar(); // FrameRounding
    ImGui::PopStyleColor(3); // Button colors
    
    ImGui::EndChild();
    ImGui::PopStyleVar(2); // ChildRounding + WindowPadding
    ImGui::PopStyleColor(); // ChildBg
}

void GUI::ToggleButton(const char* label, bool* v) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems) return;

    ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(60.0f, 28.0f);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !(*v);

    float t = *v ? 1.0f : 0.0f;
    static std::map<ImGuiID, float> anims;
    float& anim = anims[id];
    anim = Animations::Lerp(anim, t, 0.15f);

    ImU32 col_bg = ImGui::GetColorU32(Animations::Lerp(ImVec4(0.15f, 0.15f, 0.15f, 1.0f), ImVec4(0.85f, 0.05f, 0.10f, 1.0f), anim));
    window->DrawList->AddRectFilled(bb.Min, bb.Max, col_bg, size.y / 2.0f);
    window->DrawList->AddCircleFilled(ImVec2(bb.Min.x + size.y / 2.0f + anim * (size.x - size.y), bb.Min.y + size.y / 2.0f), size.y / 2.0f - 3.0f, IM_COL32_WHITE);
}

void GUI::RenderNotification(float screenWidth, float screenHeight) {
    static ULONGLONG startTime = GetTickCount64();
    ULONGLONG now = GetTickCount64();
    float elapsed = (float)(now - startTime) / 1000.0f;
    
    if (elapsed > 8.0f) return;
    
    float alpha = 1.0f;
    if (elapsed < 0.5f) alpha = elapsed / 0.5f;
    else if (elapsed > 7.5f) alpha = 1.0f - (elapsed - 7.5f) / 0.5f;
    
    ImGui::SetNextWindowPos(ImVec2(screenWidth / 2.0f, 40.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 15));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.06f, 0.85f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.05f, 0.10f, 0.4f * alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    
    if (ImGui::Begin("##WelcomeNotif", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, alpha));
        ImGui::Text("Welcome to Amatayakul Client");
        
        ImGui::SetWindowFontScale(0.9f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        ImGui::TextColored(ImVec4(0.85f, 0.05f, 0.10f, alpha), "Press RIGHT SHIFT to open mod menu");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        
        // Red accent line at the bottom
        ImVec2 pMin = ImGui::GetWindowPos();
        ImVec2 pMax = ImVec2(pMin.x + ImGui::GetWindowWidth(), pMin.y + ImGui::GetWindowHeight());
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pMin.x, pMax.y - 3), pMax, ImGui::GetColorU32(ImVec4(0.85f, 0.05f, 0.10f, alpha)), 10.0f, ImDrawFlags_RoundCornersBottom);
    }
    ImGui::End();
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
