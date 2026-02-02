#include "config_manager.h"
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace uniconv::core {

namespace {

std::filesystem::path get_home_dir() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    if (!home) {
        const char* drive = std::getenv("HOMEDRIVE");
        const char* path = std::getenv("HOMEPATH");
        if (drive && path) {
            return std::filesystem::path(drive) / path;
        }
    }
    if (home) {
        return std::filesystem::path(home);
    }
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return std::filesystem::path(path);
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home);
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::filesystem::path(pw->pw_dir);
    }
#endif
    return std::filesystem::path();
}

} // anonymous namespace

std::filesystem::path ConfigManager::get_default_config_dir() {
    auto home = get_home_dir();
    if (home.empty()) {
        return std::filesystem::path();
    }
    return home / ".uniconv";
}

ConfigManager::ConfigManager()
    : config_dir_(get_default_config_dir())
    , config_file_(config_dir_ / "config.json") {
}

ConfigManager::ConfigManager(const std::filesystem::path& config_dir)
    : config_dir_(config_dir)
    , config_file_(config_dir / "config.json") {
}

void ConfigManager::ensure_dir() const {
    if (!std::filesystem::exists(config_dir_)) {
        std::filesystem::create_directories(config_dir_);
    }
}

bool ConfigManager::exists() const {
    return std::filesystem::exists(config_file_);
}

bool ConfigManager::load() {
    if (!exists()) {
        return false;
    }

    try {
        std::ifstream file(config_file_);
        if (!file) {
            return false;
        }

        nlohmann::json j;
        file >> j;

        // Load defaults
        if (j.contains("defaults") && j["defaults"].is_object()) {
            for (auto& [key, value] : j["defaults"].items()) {
                if (value.is_string()) {
                    defaults_[key] = value.get<std::string>();
                }
            }
        }

        // Load plugin paths
        if (j.contains("plugin_paths") && j["plugin_paths"].is_array()) {
            for (const auto& path : j["plugin_paths"]) {
                if (path.is_string()) {
                    plugin_paths_.emplace_back(path.get<std::string>());
                }
            }
        }

        // Load generic settings
        if (j.contains("settings") && j["settings"].is_object()) {
            for (auto& [key, value] : j["settings"].items()) {
                if (value.is_string()) {
                    settings_[key] = value.get<std::string>();
                }
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigManager::save() const {
    try {
        ensure_dir();

        nlohmann::json j;

        // Save defaults
        if (!defaults_.empty()) {
            j["defaults"] = nlohmann::json::object();
            for (const auto& [key, value] : defaults_) {
                j["defaults"][key] = value;
            }
        }

        // Save plugin paths
        if (!plugin_paths_.empty()) {
            j["plugin_paths"] = nlohmann::json::array();
            for (const auto& path : plugin_paths_) {
                j["plugin_paths"].push_back(path.string());
            }
        }

        // Save generic settings
        if (!settings_.empty()) {
            j["settings"] = nlohmann::json::object();
            for (const auto& [key, value] : settings_) {
                j["settings"][key] = value;
            }
        }

        std::ofstream file(config_file_);
        if (!file) {
            return false;
        }

        file << std::setw(2) << j << std::endl;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ConfigManager::set_default_plugin(const std::string& key, const std::string& plugin_group) {
    defaults_[key] = plugin_group;
}

std::optional<std::string> ConfigManager::get_default_plugin(const std::string& key) const {
    auto it = defaults_.find(key);
    if (it != defaults_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ConfigManager::unset_default_plugin(const std::string& key) {
    return defaults_.erase(key) > 0;
}

void ConfigManager::add_plugin_path(const std::filesystem::path& path) {
    // Avoid duplicates
    if (std::find(plugin_paths_.begin(), plugin_paths_.end(), path) == plugin_paths_.end()) {
        plugin_paths_.push_back(path);
    }
}

bool ConfigManager::remove_plugin_path(const std::filesystem::path& path) {
    auto it = std::find(plugin_paths_.begin(), plugin_paths_.end(), path);
    if (it != plugin_paths_.end()) {
        plugin_paths_.erase(it);
        return true;
    }
    return false;
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    settings_[key] = value;
}

std::optional<std::string> ConfigManager::get(const std::string& key) const {
    auto it = settings_.find(key);
    if (it != settings_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ConfigManager::unset(const std::string& key) {
    return settings_.erase(key) > 0;
}

nlohmann::json ConfigManager::to_json() const {
    nlohmann::json j;

    j["defaults"] = nlohmann::json::object();
    for (const auto& [key, value] : defaults_) {
        j["defaults"][key] = value;
    }

    j["plugin_paths"] = nlohmann::json::array();
    for (const auto& path : plugin_paths_) {
        j["plugin_paths"].push_back(path.string());
    }

    j["settings"] = nlohmann::json::object();
    for (const auto& [key, value] : settings_) {
        j["settings"][key] = value;
    }

    return j;
}

std::vector<std::string> ConfigManager::list_keys() const {
    std::vector<std::string> keys;

    // Add default keys with prefix
    for (const auto& [key, _] : defaults_) {
        keys.push_back("defaults." + key);
    }

    // Add settings keys
    for (const auto& [key, _] : settings_) {
        keys.push_back(key);
    }

    std::sort(keys.begin(), keys.end());
    return keys;
}

} // namespace uniconv::core
