#include "Info.hpp"
#include "../Globals.hpp"
#include <windows.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// External globals from dllmain.cpp
extern HMODULE g_hModule;

// Static member initialization
uint8_t* Info::g_audioData = nullptr;
uint32_t Info::g_audioDataSize = 0;

void Info::Initialize() {
    // Simple initialization - no logo or audio
}

void Info::Shutdown() {
    // Cleanup if needed
}

void Info::RenderMenu() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.88f, 0.92f, 1.0f));
    
    // Title
    ImGui::TextWrapped("Amatayakul Client");
    ImGui::Separator();
    
    // Information
    ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.55f, 1.0f), "Version");
    ImGui::TextWrapped("Build 2026-05");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Description
    ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.55f, 1.0f), "About");
    ImGui::TextWrapped("Amatayakul Client is a legit PvP client for Minecraft Windows 10 Edition Beta (0.15.10). Features include CPS Counter, FPS Counter, FPS Unlock, Keystrokes display, and Watermark.");
    
    ImGui::Spacing();
    
    // Features list
    ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.55f, 1.0f), "Features");
    ImGui::BulletText("CPS Counter - {cps} CPS format");
    ImGui::BulletText("FPS Counter - {fps} FPS format");
    ImGui::BulletText("FPS Unlock - Removes 60 FPS limit for UWP");
    ImGui::BulletText("Keystrokes - WASD + LMB/RMB display");
    ImGui::BulletText("Watermark - Amatayakul text with chroma effect");
    ImGui::BulletText("OLED Dark Theme");
    ImGui::BulletText("Roboto Font (embedded in DLL)");
    
    ImGui::PopStyleColor();
}

bool Info::LoadAudioFromResource() {
    return false;
}

void Info::InitSoundFromMemory() {
}

void Info::PlayClickSound() {
}
