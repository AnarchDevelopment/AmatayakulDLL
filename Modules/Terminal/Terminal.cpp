#include "Terminal.hpp"
#include "Modules/ModuleHeader.hpp"
#include "Modules/Globals.hpp"
#include "Config/ConfigManager.hpp"
#include "Hook/Hook.hpp"
#include "ImGui/imgui.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <windows.h>
#include <shlobj.h>
#define INITGUID
#include <knownfolders.h>
#undef INITGUID
#ifndef KF_FLAG_NO_PACKAGE_REDIRECTION
#define KF_FLAG_NO_PACKAGE_REDIRECTION 0x00002000
#endif
#include <shellapi.h>
#include <filesystem>
#include <cstdlib>
#include <ctime>
#include "minhook/MinHook.h"
#include "ImGui/backend/imgui_impl_dx11.h"
#include "ImGui/backend/imgui_impl_win32.h"
#include "miniaudio/miniaudio.h"

// Static member initialization
std::vector<std::string> Terminal::outputLines;
char Terminal::inputBuffer[256] = {0};
bool Terminal::scrollToBottom = false;

// Unload confirmation dialog state
static bool g_showUnloadDialog = false;

bool Terminal::SaveConfig(const std::string& name) {
    return ConfigManager::SaveConfig(name);
}

bool Terminal::LoadConfig(const std::string& name) {
    return ConfigManager::LoadConfig(name);
}

bool Terminal::DeleteConfig(const std::string& name) {
    return ConfigManager::DeleteConfig(name);
}

std::vector<std::string> Terminal::ListConfigs() {
    return ConfigManager::ListConfigs();
}

bool Terminal::OpenConfigDirectory() {
    return ConfigManager::OpenConfigDirectory();
}

void Terminal::Initialize() {
    ConfigManager::Initialize();
    AddOutput("To use commands type .help");
    AddOutput(".config save <name>      Save the config");
    AddOutput(".config load <name>      Load existent config");
    AddOutput(".config delete <name>    Delete the typed config");
    AddOutput("All configs is saved in %localappdata%\\Packages\\Microsoft.MinecraftUWP_8wekyb3d8bbwe\\LocalState\\Aegle\\");
}

void Terminal::RenderConsole() {
    ImGui::BeginChild("TerminalOutput", ImVec2(0, 250), true, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& line : outputLines) {
        ImGui::TextUnformatted(line.c_str());
    }
    
    if (scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom = false;
    }
    
    ImGui::EndChild();
    
    ImGui::Separator();
    
    // Input field
    if (ImGui::InputText("##TerminalInput", inputBuffer, sizeof(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (strlen(inputBuffer) > 0) {
            ExecuteCommand(std::string(inputBuffer));
            inputBuffer[0] = '\0';
            scrollToBottom = true;
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Execute")) {
        if (strlen(inputBuffer) > 0) {
            ExecuteCommand(std::string(inputBuffer));
            inputBuffer[0] = '\0';
            scrollToBottom = true;
        }
    }
    
    // Render unload confirmation dialog if active
    RenderUnloadDialog();
}

void Terminal::ExecuteCommand(const std::string& command) {
    AddOutput("> " + command);
    
    if (command == ".help") {
        ShowHelp();
    } else if (command.substr(0, 13) == ".config save ") {
        std::string name = command.substr(13);
        if (SaveConfig(name)) {
            AddOutput("Config saved: " + name);
        } else {
            AddOutput("Failed to save config: " + name);
            
        }
    } else if (command.substr(0, 13) == ".config load ") {
        std::string name = command.substr(13);
        if (LoadConfig(name)) {
            AddOutput("Config loaded: " + name);
        } else {
            AddOutput("Failed to load config: " + name);
        }
    } else if (command.substr(0, 15) == ".config delete ") {
        std::string name = command.substr(15);
        if (DeleteConfig(name)) {
            AddOutput("Config deleted: " + name);
        } else {
            AddOutput("Failed to delete config: " + name);
        }
    } else if (command == ".config list") {
        auto configs = ListConfigs();
        if (configs.empty()) {
            AddOutput("No configs found.");
        } else {
            AddOutput("Available configs:");
            for (const auto& config : configs) {
                AddOutput("  " + config);
            }
        }
    } else if (command == ".config opendirectory") {
        if (OpenConfigDirectory()) {
            AddOutput("Opened config directory: " + ConfigManager::GetConfigDir());
        } else {
            AddOutput("Failed to open config directory: " + ConfigManager::GetConfigDir());
        }
    } else if (command == ".deattach") {
        AddOutput("Detaching DLL...");
        Detach();
    } else {
        AddOutput("Unknown command. Type .help for available commands.");
    }
}

void Terminal::AddOutput(const std::string& text) {
    outputLines.push_back(text);
    if (outputLines.size() > 1000) { // Limit output lines
        outputLines.erase(outputLines.begin());
    }
}

void Terminal::Detach() {
    // Show confirmation dialog
    g_showUnloadDialog = true;
    ImGui::OpenPopup("Confirm Unload##UnloadDialog");
}

void Terminal::PerformUnload() {
    AddOutput("Unloading DLL...");
    
    // Signal to render thread to do cleanup
    extern bool g_RequestUnload;
    g_RequestUnload = true;
}

void Terminal::ShowHelp() {
    AddOutput("Available commands:");
    AddOutput("  .help                    - Show this help");
    AddOutput("  .config save <name>      - Save current config");
    AddOutput("  .config load <name>      - Load config");
    AddOutput("  .config delete <name>    - Delete config");
    AddOutput("  .config list             - List available configs");
    AddOutput("  .config opendirectory    - Open the config directory");
    AddOutput("  .deattach                - Detach DLL safely");
}


void Terminal::RenderUnloadDialog() {
    if (!g_showUnloadDialog) {
        return;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 180), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Confirm Unload##UnloadDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextWrapped("Are you sure you want to unload the DLL?");
        ImGui::TextWrapped("This action will disable Aegleseeker.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        float buttonWidth = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float offsetX = (availWidth - totalWidth) / 2.0f;

        if (offsetX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        }

        if (ImGui::Button("Yes, Unload##UnloadYes", ImVec2(buttonWidth, 0))) {
            g_showUnloadDialog = false;
            ImGui::CloseCurrentPopup();
            Terminal::PerformUnload();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel##UnloadNo", ImVec2(buttonWidth, 0))) {
            g_showUnloadDialog = false;
            ImGui::CloseCurrentPopup();
            AddOutput("Unload cancelled.");
        }

        ImGui::EndPopup();
    }
}
