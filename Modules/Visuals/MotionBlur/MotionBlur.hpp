#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <d3d11.h>

// Forward declarations
class ImDrawList;
struct ImVec2;

/// @brief MotionBlur module - Provides motion blur effects for visual enhancement
class MotionBlur {
public:
    static bool g_motionBlurEnabled;
    static float g_motionBlurAnim;
    static float g_blurIntensity;
    static float g_maxHistoryFrames;
    static float g_blurTimeConstant;
    static std::string g_blurType;
    static std::vector<ID3D11ShaderResourceView*> g_previousFrames;
    static std::vector<float> g_frameTimestamps;

    /// @brief Initialize MotionBlur
    static void Initialize();

    /// @brief Update animation state
    static void UpdateAnimation(ULONGLONG now);

    /// @brief Initialize backbuffer storage
    static void InitializeBackbufferStorage(int maxFrames);

    /// @brief Copy backbuffer to SRV
    static ID3D11ShaderResourceView* CopyBackbufferToSRV(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, IDXGISwapChain* pSwapChain);

    /// @brief Cleanup backbuffer storage
    static void CleanupBackbufferStorage();

    /// @brief Render motion blur effect
    static void RenderMotionBlur(ImDrawList* draw, ImVec2 screenSize);

    /// @brief Render array list
    static void RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd);

    /// @brief Render menu
    static void RenderMenu();
};
