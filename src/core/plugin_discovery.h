#pragma once

#include "plugin_manifest.h"
#include <filesystem>
#include <optional>
#include <vector>

namespace uniconv::core {

// Plugin discovery - finds and loads plugin manifests from filesystem
class PluginDiscovery {
public:
    // Manifest filename
    static constexpr const char* kManifestFilename = "plugin.json";

    // Default constructor uses standard plugin directories
    PluginDiscovery();

    // Constructor with custom plugin directories
    explicit PluginDiscovery(std::vector<std::filesystem::path> plugin_dirs);

    // Add a plugin directory to search
    void add_plugin_dir(const std::filesystem::path& dir);

    // Get all plugin directories
    const std::vector<std::filesystem::path>& plugin_dirs() const { return plugin_dirs_; }

    // Discover all plugins in all directories
    // Returns list of loaded manifests
    std::vector<PluginManifest> discover_all() const;

    // Discover plugins in a specific directory
    std::vector<PluginManifest> discover_in_dir(const std::filesystem::path& dir) const;

    // Load a single plugin manifest from a directory
    std::optional<PluginManifest> load_manifest(const std::filesystem::path& plugin_dir) const;

    // Load manifest from a specific file path
    std::optional<PluginManifest> load_manifest_file(const std::filesystem::path& manifest_path) const;

    // Check if a directory contains a valid plugin
    bool is_plugin_dir(const std::filesystem::path& dir) const;

    // Get standard plugin directories for current platform
    static std::vector<std::filesystem::path> get_standard_plugin_dirs();

    // Get user plugin directory (~/.uniconv/plugins)
    static std::filesystem::path get_user_plugin_dir();

    // Get system plugin directory (/usr/local/share/uniconv/plugins or equivalent)
    static std::filesystem::path get_system_plugin_dir();

    // Get portable plugin directory (relative to executable)
    static std::filesystem::path get_portable_plugin_dir();

private:
    std::vector<std::filesystem::path> plugin_dirs_;
};

} // namespace uniconv::core
