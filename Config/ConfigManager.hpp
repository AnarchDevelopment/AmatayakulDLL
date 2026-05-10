#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

class ConfigManager {
public:
    // Configuration management
    static bool SaveConfig(const std::string& name);
    static bool LoadConfig(const std::string& name);
    static bool DeleteConfig(const std::string& name);
    static std::vector<std::string> ListConfigs();
    static bool OpenConfigDirectory();

    // Auto-save/load (uses "default" config)
    static void AutoSave();
    static void AutoLoad();

    // Initialize config directory
    static void Initialize();

    // Get config directory
    static const std::string& GetConfigDir() { return configDir; }

    // Reload modules after config load (re-enable hooks, reinitialize, etc)
    static void ReloadModulesAfterConfig();

private:
    static std::string configDir;
    static bool initialized;

    // Helper functions
    static nlohmann::json CollectCurrentConfig();
    static void ApplyConfig(const nlohmann::json& config);
    
    // JSON helpers for ImVec4
    static nlohmann::json Vec4ToJson(float x, float y, float z, float w);
    static void JsonToVec4(const nlohmann::json& j, float& x, float& y, float& z, float& w);
};
