#pragma once

#include "core/types.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::core {

class PresetManager {
public:
    // Constructor with config directory (defaults to ~/.uniconv)
    explicit PresetManager(const std::filesystem::path& config_dir = get_default_config_dir());
    ~PresetManager() = default;

    // Create a new preset
    void create(const Preset& preset);

    // Remove a preset by name
    bool remove(const std::string& name);

    // Load a preset by name
    std::optional<Preset> load(const std::string& name) const;

    // Check if preset exists
    bool exists(const std::string& name) const;

    // List all presets
    std::vector<Preset> list() const;

    // List preset names only
    std::vector<std::string> list_names() const;

    // Export preset to file
    void export_preset(const std::string& name, const std::filesystem::path& file) const;

    // Import preset from file
    void import_preset(const std::filesystem::path& file);

    // Get the presets directory
    std::filesystem::path presets_dir() const { return presets_dir_; }

    // Get default config directory
    static std::filesystem::path get_default_config_dir();

private:
    std::filesystem::path presets_dir_;

    // Get path to preset file
    std::filesystem::path preset_path(const std::string& name) const;

    // Validate preset name
    static bool is_valid_name(const std::string& name);

    // Ensure presets directory exists
    void ensure_dir() const;
};

} // namespace uniconv::core
