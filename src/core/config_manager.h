#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace uniconv::core {

// Configuration manager - handles persistent settings
// Stored in ~/.uniconv/config.json
class ConfigManager {
public:
    // Default config directory
    static std::filesystem::path get_default_config_dir();

    // Constructor with default config directory
    ConfigManager();

    // Constructor with custom config directory
    explicit ConfigManager(const std::filesystem::path& config_dir);

    // Load configuration from disk
    bool load();

    // Save configuration to disk
    bool save() const;

    // Check if config file exists
    bool exists() const;

    // --- Default plugin settings ---

    // Set default plugin for a target (e.g., "transform.jpg" -> "vips")
    void set_default_plugin(const std::string& key, const std::string& plugin_group);

    // Get default plugin for a target
    std::optional<std::string> get_default_plugin(const std::string& key) const;

    // Remove default plugin setting
    bool unset_default_plugin(const std::string& key);

    // Get all default plugin settings
    const std::map<std::string, std::string>& get_all_defaults() const { return defaults_; }

    // --- Plugin paths ---

    // Add a custom plugin directory
    void add_plugin_path(const std::filesystem::path& path);

    // Remove a custom plugin directory
    bool remove_plugin_path(const std::filesystem::path& path);

    // Get all custom plugin paths
    const std::vector<std::filesystem::path>& get_plugin_paths() const { return plugin_paths_; }

    // --- Generic key-value settings ---

    // Set a string value
    void set(const std::string& key, const std::string& value);

    // Get a string value
    std::optional<std::string> get(const std::string& key) const;

    // Remove a setting
    bool unset(const std::string& key);

    // Get all settings as JSON
    nlohmann::json to_json() const;

    // List all keys
    std::vector<std::string> list_keys() const;

private:
    std::filesystem::path config_dir_;
    std::filesystem::path config_file_;

    // Default plugins: "etl.target" -> "plugin_group"
    // e.g., "transform.jpg" -> "vips", "extract.faces" -> "ai-vision"
    std::map<std::string, std::string> defaults_;

    // Additional plugin directories
    std::vector<std::filesystem::path> plugin_paths_;

    // Generic settings
    std::map<std::string, std::string> settings_;

    void ensure_dir() const;
};

} // namespace uniconv::core
