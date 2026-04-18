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

static bool EnsureDirectoryExists(const std::string& directoryPath) {
    try {
        std::filesystem::path path(directoryPath);
        if (std::filesystem::exists(path)) {
            return std::filesystem::is_directory(path);
        }
        return std::filesystem::create_directories(path);
    } catch (...) {
        return false;
    }
}

void ConfigManager::Initialize() {
    try {
        std::filesystem::path baseDir;
        PWSTR localAppDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppDataPath)) && localAppDataPath != nullptr) {
            std::wstring wpath(localAppDataPath);
            CoTaskMemFree(localAppDataPath);
            baseDir = std::filesystem::path(wpath) /
                "Packages" /
                "Microsoft.MinecraftUWP_8wekyb3d8bbwe" /
                "LocalState" /
                "Aegle";
        } else {
            char* localAppData = std::getenv("LOCALAPPDATA");
            if (localAppData && strlen(localAppData) > 0) {
                baseDir = std::filesystem::path(localAppData) /
                    "Packages" /
                    "Microsoft.MinecraftUWP_8wekyb3d8bbwe" /
                    "LocalState" /
                    "Aegle";
            } else {
                baseDir = std::filesystem::current_path() / "Aegle";
            }
        }

        if (!EnsureDirectoryExists(baseDir.string())) {
            baseDir = std::filesystem::current_path() / "Aegle";
            EnsureDirectoryExists(baseDir.string());
        }

        configDir = baseDir.string();
        if (!configDir.empty() && configDir.back() != '\\' && configDir.back() != '/') {
            configDir += "\\";
        }
    } catch (...) {
        configDir = (std::filesystem::current_path() / "configs").string();
        EnsureDirectoryExists(configDir);
        if (!configDir.empty() && configDir.back() != '\\' && configDir.back() != '/') {
            configDir += "\\";
        }
    }
}

bool ConfigManager::SaveConfig(const std::string& name) {
    try {
        if (configDir.empty()) {
            Terminal::AddOutput("ConfigDir is empty! Cannot save config.");
            return false;
        }

        if (name.empty()) {
            Terminal::AddOutput("Config name cannot be empty");
            return false;
        }

        nlohmann::json config = CollectCurrentConfig();
        std::filesystem::path dirPath = std::filesystem::path(configDir);
        std::filesystem::path filepath = dirPath / (name + ".json");
        Terminal::AddOutput("Saving config to: " + filepath.string());

        if (!EnsureDirectoryExists(dirPath.string())) {
            Terminal::AddOutput("Failed to create config directory: " + dirPath.string());
            return false;
        }

        std::ofstream file(filepath, std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file << config.dump(4);
            Terminal::AddOutput("Config saved successfully");
            return true;
        }

        Terminal::AddOutput("Failed to open file for writing: " + filepath.string());
    } catch (const std::exception& e) {
        Terminal::AddOutput("Exception in SaveConfig: " + std::string(e.what()));
    } catch (...) {
        Terminal::AddOutput("Unknown exception in SaveConfig");
    }
    return false;
}

bool ConfigManager::LoadConfig(const std::string& name) {
    try {
        if (configDir.empty()) {
            Terminal::AddOutput("ConfigDir is empty! Cannot load config.");
            return false;
        }

        std::filesystem::path filepath = std::filesystem::path(configDir) / (name + ".json");
        Terminal::AddOutput("Loading config from: " + filepath.string());
        std::ifstream file(filepath);
        if (file.is_open()) {
            nlohmann::json config;
            file >> config;
            ApplyConfig(config);
            ReloadModulesAfterConfig();
            Terminal::AddOutput("Config loaded successfully");
            return true;
        } else {
            Terminal::AddOutput("Failed to open file for reading: " + filepath.string());
        }
    } catch (const std::exception& e) {
        Terminal::AddOutput("Exception in LoadConfig: " + std::string(e.what()));
    } catch (...) {
        Terminal::AddOutput("Unknown exception in LoadConfig");
    }
    return false;
}

bool ConfigManager::DeleteConfig(const std::string& name) {
    try {
        if (configDir.empty()) {
            Terminal::AddOutput("ConfigDir is empty! Cannot delete config.");
            return false;
        }

        std::string filepath = configDir + name + ".json";
        Terminal::AddOutput("Deleting config: " + filepath);
        std::filesystem::path path = filepath;
        if (std::filesystem::remove(path)) {
            Terminal::AddOutput("Config deleted successfully");
            return true;
        } else {
            Terminal::AddOutput("Config file not found");
        }
    } catch (const std::exception& e) {
        Terminal::AddOutput("Exception in DeleteConfig: " + std::string(e.what()));
    } catch (...) {
        Terminal::AddOutput("Unknown exception in DeleteConfig");
    }
    return false;
}

std::vector<std::string> ConfigManager::ListConfigs() {
    std::vector<std::string> configs;
    try {
        if (configDir.empty()) {
            Terminal::AddOutput("ConfigDir is empty! Cannot list configs.");
            return configs;
        }

        for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                configs.push_back(entry.path().stem().string());
            }
        }
    } catch (...) {}
    return configs;
}

bool ConfigManager::OpenConfigDirectory() {
    try {
        if (configDir.empty()) {
            return false;
        }

        std::filesystem::path dirPath = std::filesystem::path(configDir);
        if (!std::filesystem::exists(dirPath)) {
            if (!EnsureDirectoryExists(dirPath.string())) {
                return false;
            }
        }

        HINSTANCE result = ShellExecuteA(NULL, "open", configDir.c_str(), NULL, NULL, SW_SHOWDEFAULT);
        return reinterpret_cast<intptr_t>(result) > 32;
    } catch (...) {
        return false;
    }
}

nlohmann::json ConfigManager::CollectCurrentConfig() {
    nlohmann::json config;

    // Combat modules
    config["Combat"]["Hitbox"]["enabled"] = Hitbox::g_hitboxEnabled;
    config["Combat"]["Hitbox"]["value"] = Hitbox::g_hitboxValue;

    config["Combat"]["Reach"]["enabled"] = Reach::IsEnabled();
    config["Combat"]["Reach"]["value"] = Reach::g_reachValue;

    // Movement modules
    config["Movement"]["AutoSprint"]["enabled"] = AutoSprint::g_autoSprintEnabled;
    config["Movement"]["Timer"]["enabled"] = Timer::g_timerEnabled;
    config["Movement"]["Timer"]["value"] = Timer::g_timerValue;

    // Visuals modules
    config["Visuals"]["FullBright"]["enabled"] = FullBright::g_fullBrightEnabled;
    config["Visuals"]["FullBright"]["value"] = FullBright::g_fullBrightValue;

    config["Visuals"]["RenderInfo"]["enabled"] = RenderInfo::g_showRenderInfo;
    if (RenderInfo::g_renderInfoHud) {
        config["Visuals"]["RenderInfo"]["position"]["x"] = RenderInfo::g_renderInfoHud->pos.x;
        config["Visuals"]["RenderInfo"]["position"]["y"] = RenderInfo::g_renderInfoHud->pos.y;
    }

    config["Visuals"]["Watermark"]["enabled"] = Watermark::g_showWatermark;
    if (Watermark::g_watermarkHud) {
        config["Visuals"]["Watermark"]["position"]["x"] = Watermark::g_watermarkHud->pos.x;
        config["Visuals"]["Watermark"]["position"]["y"] = Watermark::g_watermarkHud->pos.y;
    }

    config["Visuals"]["MotionBlur"]["enabled"] = MotionBlur::g_motionBlurEnabled;
    
    config["Visuals"]["Keystrokes"]["enabled"] = Keystrokes::g_showKeystrokes;
    if (Keystrokes::g_keystrokesHud) {
        config["Visuals"]["Keystrokes"]["position"]["x"] = Keystrokes::g_keystrokesHud->pos.x;
        config["Visuals"]["Keystrokes"]["position"]["y"] = Keystrokes::g_keystrokesHud->pos.y;
    }
    // Save Keystrokes colors
    config["Visuals"]["Keystrokes"]["colors"]["bgColor"] = 
        nlohmann::json::array({Keystrokes::g_keystrokesBgColor.x, Keystrokes::g_keystrokesBgColor.y, 
                               Keystrokes::g_keystrokesBgColor.z, Keystrokes::g_keystrokesBgColor.w});
    config["Visuals"]["Keystrokes"]["colors"]["enabledColor"] = 
        nlohmann::json::array({Keystrokes::g_keystrokesEnabledColor.x, Keystrokes::g_keystrokesEnabledColor.y, 
                               Keystrokes::g_keystrokesEnabledColor.z, Keystrokes::g_keystrokesEnabledColor.w});
    config["Visuals"]["Keystrokes"]["colors"]["textColor"] = 
        nlohmann::json::array({Keystrokes::g_keystrokesTextColor.x, Keystrokes::g_keystrokesTextColor.y, 
                               Keystrokes::g_keystrokesTextColor.z, Keystrokes::g_keystrokesTextColor.w});
    config["Visuals"]["Keystrokes"]["colors"]["textEnabledColor"] = 
        nlohmann::json::array({Keystrokes::g_keystrokesTextEnabledColor.x, Keystrokes::g_keystrokesTextEnabledColor.y, 
                               Keystrokes::g_keystrokesTextEnabledColor.z, Keystrokes::g_keystrokesTextEnabledColor.w});

    config["Visuals"]["CPSCounter"]["enabled"] = CPSCounter::g_showCpsCounter;
    if (CPSCounter::g_cpsHud) {
        config["Visuals"]["CPSCounter"]["position"]["x"] = CPSCounter::g_cpsHud->pos.x;
        config["Visuals"]["CPSCounter"]["position"]["y"] = CPSCounter::g_cpsHud->pos.y;
    }
    // Save CPSCounter colors
    config["Visuals"]["CPSCounter"]["colors"]["textColor"] = 
        nlohmann::json::array({CPSCounter::g_cpsTextColor.x, CPSCounter::g_cpsTextColor.y, 
                               CPSCounter::g_cpsTextColor.z, CPSCounter::g_cpsTextColor.w});
    config["Visuals"]["CPSCounter"]["colors"]["shadowColor"] = 
        nlohmann::json::array({CPSCounter::g_cpsCounterShadowColor.x, CPSCounter::g_cpsCounterShadowColor.y, 
                               CPSCounter::g_cpsCounterShadowColor.z, CPSCounter::g_cpsCounterShadowColor.w});

    // Misc modules
    config["Misc"]["UnlockFPS"]["enabled"] = UnlockFPS::g_unlockFpsEnabled;
    config["Misc"]["UnlockFPS"]["fpsLimit"] = UnlockFPS::g_fpsLimit;

    config["version"] = "1.0";
    config["timestamp"] = std::time(nullptr);

    return config;
}

void ConfigManager::ApplyConfig(const nlohmann::json& config) {
    // Combat modules
    if (config.contains("Combat")) {
        if (config["Combat"].contains("Hitbox")) {
            if (config["Combat"]["Hitbox"].contains("enabled")) {
                Hitbox::g_hitboxEnabled = config["Combat"]["Hitbox"]["enabled"];
            }
            if (config["Combat"]["Hitbox"].contains("value")) {
                Hitbox::g_hitboxValue = config["Combat"]["Hitbox"]["value"];
            }
        }
        if (config["Combat"].contains("Reach")) {
            if (config["Combat"]["Reach"].contains("enabled")) {
                Reach::SetEnabled(config["Combat"]["Reach"]["enabled"]);
            }
            if (config["Combat"]["Reach"].contains("value")) {
                if (Reach::IsEnabled()) {
                    Reach::UpdateValue(config["Combat"]["Reach"]["value"]);
                } else {
                    Reach::g_reachValue = 3.0f;
                }
            }
        }
    }

    // Movement modules
    if (config.contains("Movement")) {
        if (config["Movement"].contains("AutoSprint")) {
            if (config["Movement"]["AutoSprint"].contains("enabled")) {
                AutoSprint::g_autoSprintEnabled = config["Movement"]["AutoSprint"]["enabled"];
            }
        }
        if (config["Movement"].contains("Timer")) {
            if (config["Movement"]["Timer"].contains("enabled")) {
                Timer::g_timerEnabled = config["Movement"]["Timer"]["enabled"];
            }
            if (config["Movement"]["Timer"].contains("value")) {
                Timer::g_timerValue = config["Movement"]["Timer"]["value"];
            }
        }
    }

    // Visuals modules
    if (config.contains("Visuals")) {
        if (config["Visuals"].contains("FullBright")) {
            if (config["Visuals"]["FullBright"].contains("enabled")) {
                FullBright::g_fullBrightEnabled = config["Visuals"]["FullBright"]["enabled"];
            }
            if (config["Visuals"]["FullBright"].contains("value")) {
                FullBright::g_fullBrightValue = config["Visuals"]["FullBright"]["value"];
            }
        }
        if (config["Visuals"].contains("RenderInfo")) {
            if (config["Visuals"]["RenderInfo"].contains("enabled")) {
                RenderInfo::g_showRenderInfo = config["Visuals"]["RenderInfo"]["enabled"];
            }
            if (config["Visuals"]["RenderInfo"].contains("position") && RenderInfo::g_renderInfoHud) {
                RenderInfo::g_renderInfoHud->pos.x = config["Visuals"]["RenderInfo"]["position"]["x"];
                RenderInfo::g_renderInfoHud->pos.y = config["Visuals"]["RenderInfo"]["position"]["y"];
            }
        }
        if (config["Visuals"].contains("Watermark")) {
            if (config["Visuals"]["Watermark"].contains("enabled")) {
                Watermark::g_showWatermark = config["Visuals"]["Watermark"]["enabled"];
            }
            if (config["Visuals"]["Watermark"].contains("position") && Watermark::g_watermarkHud) {
                Watermark::g_watermarkHud->pos.x = config["Visuals"]["Watermark"]["position"]["x"];
                Watermark::g_watermarkHud->pos.y = config["Visuals"]["Watermark"]["position"]["y"];
            }
        }
        if (config["Visuals"].contains("MotionBlur")) {
            if (config["Visuals"]["MotionBlur"].contains("enabled")) {
                MotionBlur::g_motionBlurEnabled = config["Visuals"]["MotionBlur"]["enabled"];
            }
        }
        if (config["Visuals"].contains("Keystrokes")) {
            if (config["Visuals"]["Keystrokes"].contains("enabled")) {
                Keystrokes::g_showKeystrokes = config["Visuals"]["Keystrokes"]["enabled"];
            }
            if (config["Visuals"]["Keystrokes"].contains("position") && Keystrokes::g_keystrokesHud) {
                Keystrokes::g_keystrokesHud->pos.x = config["Visuals"]["Keystrokes"]["position"]["x"];
                Keystrokes::g_keystrokesHud->pos.y = config["Visuals"]["Keystrokes"]["position"]["y"];
            }
            // Load Keystrokes colors
            if (config["Visuals"]["Keystrokes"].contains("colors")) {
                auto& colors = config["Visuals"]["Keystrokes"]["colors"];
                if (colors.contains("bgColor") && colors["bgColor"].size() == 4) {
                    Keystrokes::g_keystrokesBgColor = ImVec4(colors["bgColor"][0], colors["bgColor"][1], 
                                                               colors["bgColor"][2], colors["bgColor"][3]);
                }
                if (colors.contains("enabledColor") && colors["enabledColor"].size() == 4) {
                    Keystrokes::g_keystrokesEnabledColor = ImVec4(colors["enabledColor"][0], colors["enabledColor"][1], 
                                                                    colors["enabledColor"][2], colors["enabledColor"][3]);
                }
                if (colors.contains("textColor") && colors["textColor"].size() == 4) {
                    Keystrokes::g_keystrokesTextColor = ImVec4(colors["textColor"][0], colors["textColor"][1], 
                                                                 colors["textColor"][2], colors["textColor"][3]);
                }
                if (colors.contains("textEnabledColor") && colors["textEnabledColor"].size() == 4) {
                    Keystrokes::g_keystrokesTextEnabledColor = ImVec4(colors["textEnabledColor"][0], colors["textEnabledColor"][1], 
                                                                        colors["textEnabledColor"][2], colors["textEnabledColor"][3]);
                }
            }
        }
        if (config["Visuals"].contains("CPSCounter")) {
            if (config["Visuals"]["CPSCounter"].contains("enabled")) {
                CPSCounter::g_showCpsCounter = config["Visuals"]["CPSCounter"]["enabled"];
            }
            if (config["Visuals"]["CPSCounter"].contains("position") && CPSCounter::g_cpsHud) {
                CPSCounter::g_cpsHud->pos.x = config["Visuals"]["CPSCounter"]["position"]["x"];
                CPSCounter::g_cpsHud->pos.y = config["Visuals"]["CPSCounter"]["position"]["y"];
            }
            // Load CPSCounter colors
            if (config["Visuals"]["CPSCounter"].contains("colors")) {
                auto& colors = config["Visuals"]["CPSCounter"]["colors"];
                if (colors.contains("textColor") && colors["textColor"].size() == 4) {
                    CPSCounter::g_cpsTextColor = ImVec4(colors["textColor"][0], colors["textColor"][1], 
                                                         colors["textColor"][2], colors["textColor"][3]);
                }
                if (colors.contains("shadowColor") && colors["shadowColor"].size() == 4) {
                    CPSCounter::g_cpsCounterShadowColor = ImVec4(colors["shadowColor"][0], colors["shadowColor"][1], 
                                                                   colors["shadowColor"][2], colors["shadowColor"][3]);
                }
            }
        }
    }

    // Misc modules
    if (config.contains("Misc")) {
        if (config["Misc"].contains("UnlockFPS")) {
            if (config["Misc"]["UnlockFPS"].contains("enabled")) {
                UnlockFPS::g_unlockFpsEnabled = config["Misc"]["UnlockFPS"]["enabled"];
            }
            if (config["Misc"]["UnlockFPS"].contains("fpsLimit")) {
                UnlockFPS::g_fpsLimit = config["Misc"]["UnlockFPS"]["fpsLimit"];
            }
        }
    }

    Terminal::AddOutput("Configuration applied successfully.");
}

void ConfigManager::ReloadModulesAfterConfig() {
    // Re-enable or disable modules based on their saved state
    if (Hitbox::g_hitboxEnabled) {
        Hitbox::Enable();
    } else {
        Hitbox::Disable();
    }
    
    if (AutoSprint::g_autoSprintEnabled) {
        AutoSprint::Enable();
    } else {
        AutoSprint::Disable();
    }

    if (FullBright::g_fullBrightEnabled) {
        FullBright::Enable();
    } else {
        FullBright::Disable();
    }

    if (Reach::IsEnabled()) {
        Reach::SetEnabled(true);
    } else {
        Reach::SetEnabled(false);
    }

    if (Timer::g_timerEnabled) {
        Timer::Enable();
    } else {
        Timer::Disable();
    }

    Terminal::AddOutput("Modules reloaded after config load.");
}
