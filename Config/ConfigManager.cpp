#include "ConfigManager.hpp"
#include "Modules/ModuleHeader.hpp"
#include "Modules/Terminal/Terminal.hpp"
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
#include <ctime>

std::string ConfigManager::configDir;
bool ConfigManager::initialized = false;

static bool EnsureDirectoryExists(const std::string& directoryPath) {
    try {
        std::filesystem::path path(directoryPath);
        if (std::filesystem::exists(path)) return std::filesystem::is_directory(path);
        return std::filesystem::create_directories(path);
    } catch (...) { return false; }
}

nlohmann::json ConfigManager::Vec4ToJson(float x, float y, float z, float w) {
    return nlohmann::json::array({x, y, z, w});
}

void ConfigManager::JsonToVec4(const nlohmann::json& j, float& x, float& y, float& z, float& w) {
    if (j.is_array() && j.size() == 4) { x = j[0]; y = j[1]; z = j[2]; w = j[3]; }
}

void ConfigManager::Initialize() {
    if (initialized) return;
    try {
        std::filesystem::path baseDir;
        PWSTR localAppDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppDataPath)) && localAppDataPath) {
            std::wstring wpath(localAppDataPath);
            CoTaskMemFree(localAppDataPath);
            baseDir = std::filesystem::path(wpath) / "Packages" / "Microsoft.MinecraftUWP_8wekyb3d8bbwe" / "LocalState" / "games" / "com.mojang" / "amatayakul";
        } else {
            char* la = std::getenv("LOCALAPPDATA");
            if (la && strlen(la) > 0)
                baseDir = std::filesystem::path(la) / "Packages" / "Microsoft.MinecraftUWP_8wekyb3d8bbwe" / "LocalState" / "games" / "com.mojang" / "amatayakul";
            else
                baseDir = std::filesystem::current_path() / "amatayakul";
        }
        if (!EnsureDirectoryExists(baseDir.string())) {
            baseDir = std::filesystem::current_path() / "amatayakul";
            EnsureDirectoryExists(baseDir.string());
        }
        configDir = baseDir.string();
        if (!configDir.empty() && configDir.back() != '\\' && configDir.back() != '/')
            configDir += "\\";
        initialized = true;
    } catch (...) {
        configDir = (std::filesystem::current_path() / "amatayakul").string();
        EnsureDirectoryExists(configDir);
        if (!configDir.empty() && configDir.back() != '\\' && configDir.back() != '/')
            configDir += "\\";
        initialized = true;
    }
}

bool ConfigManager::SaveConfig(const std::string& name) {
    try {
        if (configDir.empty()) return false;
        if (name.empty()) return false;
        nlohmann::json config = CollectCurrentConfig();
        auto dirPath = std::filesystem::path(configDir);
        auto filepath = dirPath / (name + ".json");
        EnsureDirectoryExists(dirPath.string());
        std::ofstream file(filepath, std::ios::out | std::ios::trunc);
        if (file.is_open()) { file << config.dump(4); return true; }
    } catch (...) {}
    return false;
}

bool ConfigManager::LoadConfig(const std::string& name) {
    try {
        if (configDir.empty()) return false;
        auto filepath = std::filesystem::path(configDir) / (name + ".json");
        if (!std::filesystem::exists(filepath)) return false;
        std::ifstream file(filepath);
        if (file.is_open()) {
            nlohmann::json config;
            file >> config;
            ApplyConfig(config);
            ReloadModulesAfterConfig();
            return true;
        }
    } catch (...) {}
    return false;
}

void ConfigManager::AutoSave() { Initialize(); SaveConfig("default"); }
void ConfigManager::AutoLoad() { Initialize(); LoadConfig("default"); }

bool ConfigManager::DeleteConfig(const std::string& name) {
    try {
        if (configDir.empty()) return false;
        auto path = std::filesystem::path(configDir) / (name + ".json");
        return std::filesystem::remove(path);
    } catch (...) { return false; }
}

std::vector<std::string> ConfigManager::ListConfigs() {
    std::vector<std::string> configs;
    try {
        if (configDir.empty()) return configs;
        for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                configs.push_back(entry.path().stem().string());
        }
    } catch (...) {}
    return configs;
}

bool ConfigManager::OpenConfigDirectory() {
    try {
        if (configDir.empty()) return false;
        EnsureDirectoryExists(configDir);
        HINSTANCE result = ShellExecuteA(NULL, "open", configDir.c_str(), NULL, NULL, SW_SHOWDEFAULT);
        return reinterpret_cast<intptr_t>(result) > 32;
    } catch (...) { return false; }
}

// ==================== COLLECT ALL SETTINGS ====================

nlohmann::json ConfigManager::CollectCurrentConfig() {
    nlohmann::json c;
    c["version"] = "2.0";
    c["timestamp"] = std::time(nullptr);

    // --- Watermark ---
    auto& wm = c["Watermark"];
    wm["enabled"] = Watermark::g_showWatermark;
    wm["fancyMode"] = Watermark::g_fancyMode;
    wm["chromaEnabled"] = Watermark::g_chromaEnabled;
    wm["textColor"] = Vec4ToJson(Watermark::g_textColor.x, Watermark::g_textColor.y, Watermark::g_textColor.z, Watermark::g_textColor.w);
    wm["logoScale"] = Watermark::g_logoScale;
    if (Watermark::g_watermarkHud) { wm["posX"] = Watermark::g_watermarkHud->pos.x; wm["posY"] = Watermark::g_watermarkHud->pos.y; }

    // --- RenderInfo ---
    auto& ri = c["RenderInfo"];
    ri["enabled"] = RenderInfo::g_showRenderInfo;
    if (RenderInfo::g_renderInfoHud) { ri["posX"] = RenderInfo::g_renderInfoHud->pos.x; ri["posY"] = RenderInfo::g_renderInfoHud->pos.y; }

    // --- ArrayList ---
    c["ArrayList"]["enabled"] = ArrayList::g_showArrayList;

    // --- Keystrokes ---
    auto& ks = c["Keystrokes"];
    ks["enabled"] = Keystrokes::g_showKeystrokes;
    if (Keystrokes::g_keystrokesHud) { ks["posX"] = Keystrokes::g_keystrokesHud->pos.x; ks["posY"] = Keystrokes::g_keystrokesHud->pos.y; }
    ks["uiScale"] = Keystrokes::g_keystrokesUIScale;
    ks["blurEffect"] = Keystrokes::g_keystrokesBlurEffect;
    ks["rounding"] = Keystrokes::g_keystrokesRounding;
    ks["showBg"] = Keystrokes::g_keystrokesShowBg;
    ks["rectShadow"] = Keystrokes::g_keystrokesRectShadow;
    ks["rectShadowOffset"] = Keystrokes::g_keystrokesRectShadowOffset;
    ks["border"] = Keystrokes::g_keystrokesBorder;
    ks["borderWidth"] = Keystrokes::g_keystrokesBorderWidth;
    ks["glow"] = Keystrokes::g_keystrokesGlow;
    ks["glowAmount"] = Keystrokes::g_keystrokesGlowAmount;
    ks["glowEnabled"] = Keystrokes::g_keystrokesGlowEnabled;
    ks["glowEnabledAmount"] = Keystrokes::g_keystrokesGlowEnabledAmount;
    ks["glowSpeed"] = Keystrokes::g_keystrokesGlowSpeed;
    ks["keySpacing"] = Keystrokes::g_keystrokesKeySpacing;
    ks["edSpeed"] = Keystrokes::g_keystrokesEdSpeed;
    ks["textScale"] = Keystrokes::g_keystrokesTextScale;
    ks["textScale2"] = Keystrokes::g_keystrokesTextScale2;
    ks["textXOffset"] = Keystrokes::g_keystrokesTextXOffset;
    ks["textYOffset"] = Keystrokes::g_keystrokesTextYOffset;
    ks["textShadow"] = Keystrokes::g_keystrokesTextShadow;
    ks["textShadowOffset"] = Keystrokes::g_keystrokesTextShadowOffset;
    ks["lmbrmb"] = Keystrokes::g_keystrokesLMBRMB;
    ks["hideCPS"] = Keystrokes::g_keystrokesHideCPS;
    ks["showSpacebar"] = Keystrokes::g_keystrokesShowSpacebar;
    ks["spacebarWidth"] = Keystrokes::g_keystrokesSpacebarWidth;
    ks["spacebarHeight"] = Keystrokes::g_keystrokesSpacebarHeight;
    ks["showMouseButtons"] = Keystrokes::g_keystrokesShowMouseButtons;
    // Colors
    ks["bgColor"] = Vec4ToJson(Keystrokes::g_keystrokesBgColor.x, Keystrokes::g_keystrokesBgColor.y, Keystrokes::g_keystrokesBgColor.z, Keystrokes::g_keystrokesBgColor.w);
    ks["enabledColor"] = Vec4ToJson(Keystrokes::g_keystrokesEnabledColor.x, Keystrokes::g_keystrokesEnabledColor.y, Keystrokes::g_keystrokesEnabledColor.z, Keystrokes::g_keystrokesEnabledColor.w);
    ks["textColor"] = Vec4ToJson(Keystrokes::g_keystrokesTextColor.x, Keystrokes::g_keystrokesTextColor.y, Keystrokes::g_keystrokesTextColor.z, Keystrokes::g_keystrokesTextColor.w);
    ks["textEnabledColor"] = Vec4ToJson(Keystrokes::g_keystrokesTextEnabledColor.x, Keystrokes::g_keystrokesTextEnabledColor.y, Keystrokes::g_keystrokesTextEnabledColor.z, Keystrokes::g_keystrokesTextEnabledColor.w);
    ks["rectShadowColor"] = Vec4ToJson(Keystrokes::g_keystrokesRectShadowColor.x, Keystrokes::g_keystrokesRectShadowColor.y, Keystrokes::g_keystrokesRectShadowColor.z, Keystrokes::g_keystrokesRectShadowColor.w);
    ks["textShadowColor"] = Vec4ToJson(Keystrokes::g_keystrokesTextShadowColor.x, Keystrokes::g_keystrokesTextShadowColor.y, Keystrokes::g_keystrokesTextShadowColor.z, Keystrokes::g_keystrokesTextShadowColor.w);
    ks["borderColor"] = Vec4ToJson(Keystrokes::g_keystrokesBorderColor.x, Keystrokes::g_keystrokesBorderColor.y, Keystrokes::g_keystrokesBorderColor.z, Keystrokes::g_keystrokesBorderColor.w);
    ks["glowColor"] = Vec4ToJson(Keystrokes::g_keystrokesGlowColor.x, Keystrokes::g_keystrokesGlowColor.y, Keystrokes::g_keystrokesGlowColor.z, Keystrokes::g_keystrokesGlowColor.w);
    ks["glowEnabledColor"] = Vec4ToJson(Keystrokes::g_keystrokesGlowEnabledColor.x, Keystrokes::g_keystrokesGlowEnabledColor.y, Keystrokes::g_keystrokesGlowEnabledColor.z, Keystrokes::g_keystrokesGlowEnabledColor.w);

    // --- CPSCounter ---
    auto& cps = c["CPSCounter"];
    cps["enabled"] = CPSCounter::g_showCpsCounter;
    if (CPSCounter::g_cpsHud) { cps["posX"] = CPSCounter::g_cpsHud->pos.x; cps["posY"] = CPSCounter::g_cpsHud->pos.y; }
    cps["format"] = CPSCounter::g_cpsCounterFormat;
    cps["textScale"] = CPSCounter::g_cpsTextScale;
    cps["alignment"] = CPSCounter::g_cpsCounterAlignment;
    cps["shadow"] = CPSCounter::g_cpsCounterShadow;
    cps["shadowOffset"] = CPSCounter::g_cpsCounterShadowOffset;
    cps["textColor"] = Vec4ToJson(CPSCounter::g_cpsTextColor.x, CPSCounter::g_cpsTextColor.y, CPSCounter::g_cpsTextColor.z, CPSCounter::g_cpsTextColor.w);
    cps["shadowColor"] = Vec4ToJson(CPSCounter::g_cpsCounterShadowColor.x, CPSCounter::g_cpsCounterShadowColor.y, CPSCounter::g_cpsCounterShadowColor.z, CPSCounter::g_cpsCounterShadowColor.w);

    // --- FPSCounter ---
    auto& fps = c["FPSCounter"];
    fps["enabled"] = FPSCounter::g_showFpsCounter;
    if (FPSCounter::g_fpsHud) { fps["posX"] = FPSCounter::g_fpsHud->pos.x; fps["posY"] = FPSCounter::g_fpsHud->pos.y; }
    fps["textScale"] = FPSCounter::g_fpsTextScale;
    fps["alignment"] = FPSCounter::g_fpsCounterAlignment;
    fps["shadow"] = FPSCounter::g_fpsCounterShadow;
    fps["shadowOffset"] = FPSCounter::g_fpsCounterShadowOffset;
    fps["textColor"] = Vec4ToJson(FPSCounter::g_fpsTextColor.x, FPSCounter::g_fpsTextColor.y, FPSCounter::g_fpsTextColor.z, FPSCounter::g_fpsTextColor.w);
    fps["shadowColor"] = Vec4ToJson(FPSCounter::g_fpsCounterShadowColor.x, FPSCounter::g_fpsCounterShadowColor.y, FPSCounter::g_fpsCounterShadowColor.z, FPSCounter::g_fpsCounterShadowColor.w);

    // --- MotionBlur ---
    auto& mb = c["MotionBlur"];
    mb["enabled"] = MotionBlur::g_motionBlurEnabled;
    mb["intensity"] = MotionBlur::g_blurIntensity;
    mb["maxHistoryFrames"] = MotionBlur::g_maxHistoryFrames;
    mb["timeConstant"] = MotionBlur::g_blurTimeConstant;
    mb["type"] = MotionBlur::g_blurType;

    // --- UnlockFPS ---
    auto& uf = c["UnlockFPS"];
    uf["enabled"] = UnlockFPS::g_unlockFpsEnabled;
    uf["fpsLimit"] = UnlockFPS::g_fpsLimit;

    return c;
}

// ==================== APPLY ALL SETTINGS ====================

#define LOAD_BOOL(section, key, var) if (c.contains(section) && c[section].contains(key)) var = c[section][key].get<bool>()
#define LOAD_FLOAT(section, key, var) if (c.contains(section) && c[section].contains(key)) var = c[section][key].get<float>()
#define LOAD_INT(section, key, var) if (c.contains(section) && c[section].contains(key)) var = c[section][key].get<int>()
#define LOAD_STRING(section, key, var) if (c.contains(section) && c[section].contains(key)) var = c[section][key].get<std::string>()
#define LOAD_VEC4(section, key, var) if (c.contains(section) && c[section].contains(key)) JsonToVec4(c[section][key], var.x, var.y, var.z, var.w)

void ConfigManager::ApplyConfig(const nlohmann::json& c) {
    // --- Watermark ---
    LOAD_BOOL("Watermark", "enabled", Watermark::g_showWatermark);
    if (c.contains("Watermark") && Watermark::g_watermarkHud) {
        if (c["Watermark"].contains("posX")) Watermark::g_watermarkHud->pos.x = c["Watermark"]["posX"];
        if (c["Watermark"].contains("posY")) Watermark::g_watermarkHud->pos.y = c["Watermark"]["posY"];
    }

    // --- RenderInfo ---
    LOAD_BOOL("RenderInfo", "enabled", RenderInfo::g_showRenderInfo);
    if (c.contains("RenderInfo") && RenderInfo::g_renderInfoHud) {
        if (c["RenderInfo"].contains("posX")) RenderInfo::g_renderInfoHud->pos.x = c["RenderInfo"]["posX"];
        if (c["RenderInfo"].contains("posY")) RenderInfo::g_renderInfoHud->pos.y = c["RenderInfo"]["posY"];
    }

    // --- ArrayList ---
    LOAD_BOOL("ArrayList", "enabled", ArrayList::g_showArrayList);

    // --- Keystrokes ---
    LOAD_BOOL("Keystrokes", "enabled", Keystrokes::g_showKeystrokes);
    if (c.contains("Keystrokes") && Keystrokes::g_keystrokesHud) {
        if (c["Keystrokes"].contains("posX")) Keystrokes::g_keystrokesHud->pos.x = c["Keystrokes"]["posX"];
        if (c["Keystrokes"].contains("posY")) Keystrokes::g_keystrokesHud->pos.y = c["Keystrokes"]["posY"];
    }
    LOAD_FLOAT("Keystrokes", "uiScale", Keystrokes::g_keystrokesUIScale);
    LOAD_BOOL("Keystrokes", "blurEffect", Keystrokes::g_keystrokesBlurEffect);
    LOAD_FLOAT("Keystrokes", "rounding", Keystrokes::g_keystrokesRounding);
    LOAD_BOOL("Keystrokes", "showBg", Keystrokes::g_keystrokesShowBg);
    LOAD_BOOL("Keystrokes", "rectShadow", Keystrokes::g_keystrokesRectShadow);
    LOAD_FLOAT("Keystrokes", "rectShadowOffset", Keystrokes::g_keystrokesRectShadowOffset);
    LOAD_BOOL("Keystrokes", "border", Keystrokes::g_keystrokesBorder);
    LOAD_FLOAT("Keystrokes", "borderWidth", Keystrokes::g_keystrokesBorderWidth);
    LOAD_BOOL("Keystrokes", "glow", Keystrokes::g_keystrokesGlow);
    LOAD_FLOAT("Keystrokes", "glowAmount", Keystrokes::g_keystrokesGlowAmount);
    LOAD_BOOL("Keystrokes", "glowEnabled", Keystrokes::g_keystrokesGlowEnabled);
    LOAD_FLOAT("Keystrokes", "glowEnabledAmount", Keystrokes::g_keystrokesGlowEnabledAmount);
    LOAD_FLOAT("Keystrokes", "glowSpeed", Keystrokes::g_keystrokesGlowSpeed);
    LOAD_FLOAT("Keystrokes", "keySpacing", Keystrokes::g_keystrokesKeySpacing);
    LOAD_FLOAT("Keystrokes", "edSpeed", Keystrokes::g_keystrokesEdSpeed);
    LOAD_FLOAT("Keystrokes", "textScale", Keystrokes::g_keystrokesTextScale);
    LOAD_FLOAT("Keystrokes", "textScale2", Keystrokes::g_keystrokesTextScale2);
    LOAD_FLOAT("Keystrokes", "textXOffset", Keystrokes::g_keystrokesTextXOffset);
    LOAD_FLOAT("Keystrokes", "textYOffset", Keystrokes::g_keystrokesTextYOffset);
    LOAD_BOOL("Keystrokes", "textShadow", Keystrokes::g_keystrokesTextShadow);
    LOAD_FLOAT("Keystrokes", "textShadowOffset", Keystrokes::g_keystrokesTextShadowOffset);
    LOAD_BOOL("Keystrokes", "lmbrmb", Keystrokes::g_keystrokesLMBRMB);
    LOAD_BOOL("Keystrokes", "hideCPS", Keystrokes::g_keystrokesHideCPS);
    LOAD_BOOL("Keystrokes", "showSpacebar", Keystrokes::g_keystrokesShowSpacebar);
    LOAD_FLOAT("Keystrokes", "spacebarWidth", Keystrokes::g_keystrokesSpacebarWidth);
    LOAD_FLOAT("Keystrokes", "spacebarHeight", Keystrokes::g_keystrokesSpacebarHeight);
    LOAD_BOOL("Keystrokes", "showMouseButtons", Keystrokes::g_keystrokesShowMouseButtons);
    LOAD_VEC4("Keystrokes", "bgColor", Keystrokes::g_keystrokesBgColor);
    LOAD_VEC4("Keystrokes", "enabledColor", Keystrokes::g_keystrokesEnabledColor);
    LOAD_VEC4("Keystrokes", "textColor", Keystrokes::g_keystrokesTextColor);
    LOAD_VEC4("Keystrokes", "textEnabledColor", Keystrokes::g_keystrokesTextEnabledColor);
    LOAD_VEC4("Keystrokes", "rectShadowColor", Keystrokes::g_keystrokesRectShadowColor);
    LOAD_VEC4("Keystrokes", "textShadowColor", Keystrokes::g_keystrokesTextShadowColor);
    LOAD_VEC4("Keystrokes", "borderColor", Keystrokes::g_keystrokesBorderColor);
    LOAD_VEC4("Keystrokes", "glowColor", Keystrokes::g_keystrokesGlowColor);
    LOAD_VEC4("Keystrokes", "glowEnabledColor", Keystrokes::g_keystrokesGlowEnabledColor);

    // --- CPSCounter ---
    LOAD_BOOL("CPSCounter", "enabled", CPSCounter::g_showCpsCounter);
    if (c.contains("CPSCounter") && CPSCounter::g_cpsHud) {
        if (c["CPSCounter"].contains("posX")) CPSCounter::g_cpsHud->pos.x = c["CPSCounter"]["posX"];
        if (c["CPSCounter"].contains("posY")) CPSCounter::g_cpsHud->pos.y = c["CPSCounter"]["posY"];
    }
    LOAD_STRING("CPSCounter", "format", CPSCounter::g_cpsCounterFormat);
    LOAD_FLOAT("CPSCounter", "textScale", CPSCounter::g_cpsTextScale);
    LOAD_INT("CPSCounter", "alignment", CPSCounter::g_cpsCounterAlignment);
    LOAD_BOOL("CPSCounter", "shadow", CPSCounter::g_cpsCounterShadow);
    LOAD_FLOAT("CPSCounter", "shadowOffset", CPSCounter::g_cpsCounterShadowOffset);
    LOAD_VEC4("CPSCounter", "textColor", CPSCounter::g_cpsTextColor);
    LOAD_VEC4("CPSCounter", "shadowColor", CPSCounter::g_cpsCounterShadowColor);

    // --- FPSCounter ---
    LOAD_BOOL("FPSCounter", "enabled", FPSCounter::g_showFpsCounter);
    if (c.contains("FPSCounter") && FPSCounter::g_fpsHud) {
        if (c["FPSCounter"].contains("posX")) FPSCounter::g_fpsHud->pos.x = c["FPSCounter"]["posX"];
        if (c["FPSCounter"].contains("posY")) FPSCounter::g_fpsHud->pos.y = c["FPSCounter"]["posY"];
    }
    LOAD_FLOAT("FPSCounter", "textScale", FPSCounter::g_fpsTextScale);
    LOAD_INT("FPSCounter", "alignment", FPSCounter::g_fpsCounterAlignment);
    LOAD_BOOL("FPSCounter", "shadow", FPSCounter::g_fpsCounterShadow);
    LOAD_FLOAT("FPSCounter", "shadowOffset", FPSCounter::g_fpsCounterShadowOffset);
    LOAD_VEC4("FPSCounter", "textColor", FPSCounter::g_fpsTextColor);
    LOAD_VEC4("FPSCounter", "shadowColor", FPSCounter::g_fpsCounterShadowColor);

    // --- MotionBlur ---
    LOAD_BOOL("MotionBlur", "enabled", MotionBlur::g_motionBlurEnabled);
    LOAD_FLOAT("MotionBlur", "intensity", MotionBlur::g_blurIntensity);
    LOAD_FLOAT("MotionBlur", "maxHistoryFrames", MotionBlur::g_maxHistoryFrames);
    LOAD_FLOAT("MotionBlur", "timeConstant", MotionBlur::g_blurTimeConstant);
    LOAD_STRING("MotionBlur", "type", MotionBlur::g_blurType);

    // --- UnlockFPS ---
    LOAD_BOOL("UnlockFPS", "enabled", UnlockFPS::g_unlockFpsEnabled);
    LOAD_FLOAT("UnlockFPS", "fpsLimit", UnlockFPS::g_fpsLimit);
}

void ConfigManager::ReloadModulesAfterConfig() {
    // Reset animation times so modules re-animate properly
    ULONGLONG now = GetTickCount64();
    
    // Watermark
    if (Watermark::g_showWatermark) {
        Watermark::g_watermarkEnableTime = now;
        Watermark::g_watermarkDisableTime = 0;
    } else {
        Watermark::g_watermarkAnim = 0.0f;
        Watermark::g_watermarkEnableTime = 0;
    }

    // Keystrokes
    if (Keystrokes::g_showKeystrokes) {
        Keystrokes::g_keystrokesEnableTime = now;
        Keystrokes::g_keystrokesDisableTime = 0;
    } else {
        Keystrokes::g_keystrokesAnim = 0.0f;
        Keystrokes::g_keystrokesEnableTime = 0;
    }

    // CPS Counter
    if (CPSCounter::g_showCpsCounter) {
        CPSCounter::g_cpsCounterEnableTime = now;
        CPSCounter::g_cpsCounterDisableTime = 0;
    } else {
        CPSCounter::g_cpsCounterAnim = 0.0f;
        CPSCounter::g_cpsCounterEnableTime = 0;
    }

    // FPS Counter
    if (FPSCounter::g_showFpsCounter) {
        FPSCounter::g_fpsCounterEnableTime = now;
        FPSCounter::g_fpsCounterDisableTime = 0;
    } else {
        FPSCounter::g_fpsCounterAnim = 0.0f;
        FPSCounter::g_fpsCounterEnableTime = 0;
    }

    // RenderInfo
    if (RenderInfo::g_showRenderInfo) {
        RenderInfo::g_renderInfoEnableTime = now;
        RenderInfo::g_renderInfoDisableTime = 0;
    } else {
        RenderInfo::g_renderInfoAnim = 0.0f;
        RenderInfo::g_renderInfoEnableTime = 0;
    }
}
