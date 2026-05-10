#pragma once

#include "ImGui/imgui.h"
#include <windows.h>
#include <cstdint>

/// @brief Info module - Displays information about Amatayakul Client
class Info {
public:
    /// @brief Initialize the Info module
    static void Initialize();
    
    /// @brief Render the Info menu tab
    static void RenderMenu();
    
    /// @brief Cleanup resources
    static void Shutdown();

private:
    // Audio resources (disabled)
    static uint8_t* g_audioData;
    static uint32_t g_audioDataSize;
    
    /// @brief Load audio from RC resource (disabled)
    static bool LoadAudioFromResource();
    
    /// @brief Initialize sound from loaded audio data (disabled)
    static void InitSoundFromMemory();
    
    /// @brief Play click sound (disabled)
    static void PlayClickSound();
};
