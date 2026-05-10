#include "GUI.hpp"
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_internal.h"
#include "../Modules/ModuleHeader.hpp"
#include "../Assets/resource.h"
#include <windows.h>
#include <algorithm>
#include <d3d11.h>
#include "../Assets/stb/stb_image.h"

#include "../Modules/Terminal/Terminal.hpp"

// Static member initialization
bool GUI::g_showMenu = false;
float GUI::g_menuAnim = 0.0f;

int GUI::g_currentTab = 0;
int GUI::g_previousTab = 0;
ULONGLONG GUI::g_tabChangeTime = 0;
float GUI::g_tabAnim = 0.0f;

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
    
    style->WindowRounding = 8.0f;
    style->ChildRounding = 6.0f;
    style->FrameRounding = 4.0f;
    style->GrabRounding = 4.0f;
    style->PopupRounding = 6.0f;
    style->ScrollbarRounding = 4.0f;
    
    style->WindowBorderSize = 0.0f;
    style->ChildBorderSize = 0.0f;
    style->PopupBorderSize = 1.0f;
    style->FrameBorderSize = 0.0f;
    
    style->ItemSpacing = ImVec2(10, 12);
    style->WindowPadding = ImVec2(15, 15);
    
    ImVec4* colors = style->Colors;
    ImVec4 accent = ImVec4(0.85f, 0.05f, 0.10f, 1.0f); // Deep Blood Red
    
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.00f);
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
    
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
    
    colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered]             = accent;
    colors[ImGuiCol_TabActive]              = accent;
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    
    colors[ImGuiCol_PlotLines]              = accent;
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = accent;
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    
    colors[ImGuiCol_NavHighlight]           = accent;
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
}

void GUI::LoadFont() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Clear fonts to avoid duplicates during re-init
    io.Fonts->Clear();

    HMODULE hModule = GetModuleHandleA("amatayakul.dll");
    if (!hModule) hModule = GetModuleHandleA(nullptr);

    // IDR_FONT_ROBOTO = 106, RCDATA = 10
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCE(106), (LPCSTR)10);
    if (hRes) {
        DWORD size = SizeofResource(hModule, hRes);
        HGLOBAL hGlobal = LoadResource(hModule, hRes);
        if (hGlobal) {
            void* pData = LockResource(hGlobal);
            if (pData && size > 0) {
                // To be 100% safe, we copy the resource to a heap buffer.
                // This prevents issues if the DLL's resource section is somehow paged out or protected.
                void* font_copy = malloc(size);
                if (font_copy) {
                    memcpy(font_copy, pData, size);
                    
                    ImFontConfig config;
                    memset(&config, 0, sizeof(ImFontConfig));
                    new (&config) ImFontConfig(); // Use constructor for safe defaults
                    
                    config.FontDataOwnedByAtlas = true; // ImGui will free(font_copy) automatically
                    config.OversampleH = 2;
                    config.OversampleV = 2;
                    config.PixelSnapH = true;
                    strcpy(config.Name, "Roboto-Regular");
                    
                    ImFont* font = io.Fonts->AddFontFromMemoryTTF(font_copy, (int)size, 19.0f, &config, io.Fonts->GetGlyphRangesDefault());
                    if (font) return; // Success!
                    
                    free(font_copy); // AddFont failed
                }
            }
        }
    }

    // Rock-solid fallback
    io.Fonts->AddFontDefault();
}

#include <cstdio>

// Helper to load texture from resource
ID3D11ShaderResourceView* LoadTextureFromResource(ID3D11Device* pDevice, int resId) {
    HMODULE hMod = GetModuleHandleA("amatayakul.dll");
    if (!hMod) hMod = GetModuleHandleA(NULL);
    
    HRSRC hRes = FindResourceA(hMod, MAKEINTRESOURCEA(resId), RT_RCDATA);
    if (!hRes) return nullptr;
    
    HGLOBAL hData = LoadResource(hMod, hRes);
    if (!hData) return nullptr;
    
    void* pResData = LockResource(hData);
    DWORD resSize = SizeofResource(hMod, hRes);
    
    int width, height, channels;
    unsigned char* data = stbi_load_from_memory((unsigned char*)pResData, resSize, &width, &height, &channels, 4);
    if (!data) {
        Terminal::AddOutput("[GUI] stbi_load_from_memory failed for resource: " + std::to_string(resId));
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = data;
    subResource.SysMemPitch = width * 4;

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = pDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    ID3D11ShaderResourceView* pSRV = nullptr;
    if (SUCCEEDED(hr) && pTexture) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV);
        pTexture->Release();
    }
    
    stbi_image_free(data);
    return pSRV;
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
        {"logo", IDR_ICON_LOGO}
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
}

void GUI::RenderMenu(float screenWidth, float screenHeight) {
    if (!g_showMenu) return;

    ImGui::SetNextWindowSize(ImVec2(850.0f, 580.0f), ImGuiCond_FirstUseEver);
    
    // Standard window for maximum stability
    if (ImGui::Begin("Amatayakul Client", &g_showMenu, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        
        ImGui::Columns(2, "MainLayout", false);
        ImGui::SetColumnWidth(0, 220.0f);

        // --- SIDEBAR COLUMN ---
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.05f, 0.10f, 1.0f));
        ImGui::SetWindowFontScale(1.4f);
        ImGui::Text("  AMATAYAKUL");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto TabButton = [](const char* label, int index) {
            bool active = (GUI::g_currentTab == index);
            
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.05f, 0.08f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.05f, 0.08f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.05f, 0.08f, 0.8f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.4f));
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            if (ImGui::Button(label, ImVec2(200, 45))) {
                GUI::g_currentTab = index;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            ImGui::Spacing();
        };

        TabButton("  DASHBOARD", 0);
        TabButton("  VISUALS", 1);
        TabButton("  MISC", 2);
        TabButton("  INFO", 3);


        ImGui::NextColumn();

        // --- CONTENT COLUMN ---
        ImGui::Spacing();
        const char* tabNames[] = { "DASHBOARD", "VISUALS", "MISC", "INFO" };
        ImGui::Text(tabNames[GUI::g_currentTab]);
        ImGui::Spacing();

        ImGui::BeginChild("Content", ImVec2(0,0), false);
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
            ImGui::TextColored(ImVec4(0.68f, 0.12f, 0.18f, 1.0f), "MODULE SETTINGS > %s", GUI::g_currentSettingsModule.c_str());
            
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
        } else if (GUI::g_currentTab == 3) { // Info
            Info::RenderMenu();
        } else { // Dashboard
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.12f, 0.18f, 1.0f)); // Blood red
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("WELCOME TO AMATAYAKUL");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Text("The best client for minecraft beta");
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("Build: stable-05-10-26");
        }
        ImGui::EndChild();
        
        ImGui::Columns(1);
    }
    ImGui::End();
}

void GUI::RenderModuleCard(const char* name, const char* iconName, bool* enabled, bool* showSettings) {
    float windowWidth = ImGui::GetWindowWidth();
    float spacing = 20.0f;
    int columns = 3;
    float cardWidth = (windowWidth - (spacing * (columns + 1))) / columns;
    ImVec2 size(cardWidth, 140); // Taller cards
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f)); 
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    
    std::string childId = std::string("##Card_") + name;
    ImGui::BeginChild(childId.c_str(), size, true, ImGuiWindowFlags_NoScrollbar);
    
    // Icon
    ImTextureID iconTexture = (iconName && g_icons.count(iconName)) ? g_icons[iconName] : (ImTextureID)0;
    if (iconTexture) {
        ImGui::SetCursorPos(ImVec2(size.x / 2 - 20, 20));
        ImGui::Image(iconTexture, ImVec2(40, 40));
    } else {
        ImGui::SetCursorPos(ImVec2(size.x / 2 - 15, 20));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.5f, 0.6f, 1.0f));
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("[O]");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    
    // Name
    ImVec2 textSize = ImGui::CalcTextSize(name);
    ImGui::SetCursorPos(ImVec2((size.x - textSize.x) / 2, 70));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.95f, 1.0f, 1.0f));
    ImGui::Text("%s", name);
    ImGui::PopStyleColor();
    
    // Bottom Buttons
    ImGui::SetCursorPos(ImVec2(15, size.y - 45));
    
    // Settings Button (Gear)
    ImTextureID gearIcon = g_icons.count("gear") ? g_icons["gear"] : (ImTextureID)0;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.15f, 0.18f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    
    if (gearIcon) {
        if (ImGui::ImageButton(std::string("##Set_" + std::string(name)).c_str(), gearIcon, ImVec2(20, 20))) {
            g_currentSettingsModule = iconName;
        }
    } else {
        if (ImGui::Button(std::string("S##Set_" + std::string(name)).c_str(), ImVec2(30, 30))) {
            g_currentSettingsModule = iconName;
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::SetCursorPosX(55);
    
    // Toggle Button
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    if (*enabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.45f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 1.0f, 1.0f)); 
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.18f, 0.22f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.8f, 1.0f)); 
    }
    
    const char* btnLabel = *enabled ? "Enabled" : "Disabled";
    std::string btnId = std::string(btnLabel) + "##Tog_" + std::string(name);
    
    if (ImGui::Button(btnId.c_str(), ImVec2(size.x - 70, 30))) {
        *enabled = !*enabled;
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}




void GUI::RenderNotification(float screenWidth, float screenHeight) {
    static ULONGLONG startTime = GetTickCount64();
    float elapsed = (float)(GetTickCount64() - startTime) / 1000.0f;
    if (elapsed >= 8.0f) return;

    // Fade in/out effect
    float alpha = 1.0f;
    if (elapsed < 1.0f) alpha = elapsed;
    else if (elapsed > 7.0f) alpha = 1.0f - (elapsed - 7.0f);

    ImVec2 nS = ImVec2(320.0f, 75.0f);
    ImGui::SetNextWindowPos(ImVec2(screenWidth - nS.x - 20.0f, screenHeight - nS.y - 20.0f));
    ImGui::SetNextWindowSize(nS);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                             ImGuiWindowFlags_NoInputs | 
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##Notif", NULL, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        
        ImU32 bgCol = IM_COL32(15, 15, 15, (int)(240 * alpha));
        ImU32 borderCol = IM_COL32(255, 255, 255, (int)(30 * alpha));
        ImU32 accentCol = IM_COL32(173, 31, 46, (int)(255 * alpha));

        // Background with rounding
        drawList->AddRectFilled(pos, ImVec2(pos.x + nS.x, pos.y + nS.y), bgCol, 12.0f);
        drawList->AddRect(pos, ImVec2(pos.x + nS.x, pos.y + nS.y), borderCol, 12.0f);
        
        // Vertical Accent bar - Simplified
        drawList->AddRectFilled(pos, ImVec2(pos.x + 5, pos.y + nS.y), accentCol, 2.0f);

        ImGui::SetCursorPos(ImVec2(20, 18));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        
        ImGui::TextColored(ImVec4(0.68f, 0.12f, 0.18f, 1.0f), "AMATAYAKUL");
        
        ImGui::SetCursorPos(ImVec2(20, 40));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::Text("Press INSERT to open configuration");
        ImGui::PopStyleColor();
        
        ImGui::PopStyleVar();
    }
    ImGui::End();


}

void GUI::ToggleButton(const char* label, bool* v) {
    if (!v || !label) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) return;

    ImVec2 p = ImGui::GetCursorScreenPos();
    float height = ImGui::GetFrameHeight();
    float width = height * 1.8f;
    float radius = height * 0.50f;

    ImGui::InvisibleButton(label, ImVec2(width, height));
    if (ImGui::IsItemClicked())
        *v = !*v;

    float t = *v ? 1.0f : 0.0f;
    
    ImU32 col_bg;
    if (ImGui::IsItemHovered())
        col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.12f, 0.12f, 0.12f, 1.0f), ImVec4(0.55f, 0.12f, 0.18f, 0.6f), t));
    else
        col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.08f, 0.08f, 0.08f, 1.0f), ImVec4(0.55f, 0.12f, 0.18f, 1.00f), t));

    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));
    
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
    ImGui::Text(label);
}
