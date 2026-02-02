#include "preset_manager.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace uniconv::core {

namespace {

// Get home directory cross-platform
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
#else
    const char* home = std::getenv("HOME");
#endif
    if (home) {
        return std::filesystem::path(home);
    }
    throw std::runtime_error("Could not determine home directory");
}

} // anonymous namespace

std::filesystem::path PresetManager::get_default_config_dir() {
    return get_home_dir() / ".uniconv";
}

PresetManager::PresetManager(const std::filesystem::path& config_dir)
    : presets_dir_(config_dir / "presets") {
}

void PresetManager::ensure_dir() const {
    if (!std::filesystem::exists(presets_dir_)) {
        std::filesystem::create_directories(presets_dir_);
    }
}

std::filesystem::path PresetManager::preset_path(const std::string& name) const {
    return presets_dir_ / (name + ".json");
}

bool PresetManager::is_valid_name(const std::string& name) {
    if (name.empty() || name.length() > 64) {
        return false;
    }
    // Allow alphanumeric, underscore, and hyphen
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    });
}

void PresetManager::create(const Preset& preset) {
    if (!is_valid_name(preset.name)) {
        throw std::invalid_argument(
            "Invalid preset name. Use alphanumeric characters, underscore, or hyphen (max 64 chars)."
        );
    }

    ensure_dir();

    auto path = preset_path(preset.name);

    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Could not create preset file: " + path.string());
    }

    file << std::setw(2) << preset.to_json() << std::endl;
}

bool PresetManager::remove(const std::string& name) {
    auto path = preset_path(name);
    if (std::filesystem::exists(path)) {
        return std::filesystem::remove(path);
    }
    return false;
}

std::optional<Preset> PresetManager::load(const std::string& name) const {
    auto path = preset_path(name);

    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    try {
        nlohmann::json j;
        file >> j;
        return Preset::from_json(j);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool PresetManager::exists(const std::string& name) const {
    return std::filesystem::exists(preset_path(name));
}

std::vector<Preset> PresetManager::list() const {
    std::vector<Preset> presets;

    if (!std::filesystem::exists(presets_dir_)) {
        return presets;
    }

    for (const auto& entry : std::filesystem::directory_iterator(presets_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            auto name = entry.path().stem().string();
            auto preset = load(name);
            if (preset) {
                presets.push_back(*preset);
            }
        }
    }

    // Sort by name
    std::sort(presets.begin(), presets.end(),
              [](const Preset& a, const Preset& b) { return a.name < b.name; });

    return presets;
}

std::vector<std::string> PresetManager::list_names() const {
    std::vector<std::string> names;

    if (!std::filesystem::exists(presets_dir_)) {
        return names;
    }

    for (const auto& entry : std::filesystem::directory_iterator(presets_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().string());
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}

void PresetManager::export_preset(const std::string& name, const std::filesystem::path& file) const {
    auto preset = load(name);
    if (!preset) {
        throw std::runtime_error("Preset not found: " + name);
    }

    std::ofstream out(file);
    if (!out) {
        throw std::runtime_error("Could not create export file: " + file.string());
    }

    out << std::setw(2) << preset->to_json() << std::endl;
}

void PresetManager::import_preset(const std::filesystem::path& file) {
    if (!std::filesystem::exists(file)) {
        throw std::runtime_error("File not found: " + file.string());
    }

    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("Could not read file: " + file.string());
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid JSON in preset file: " + std::string(e.what()));
    }

    Preset preset;
    try {
        preset = Preset::from_json(j);
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid preset format: " + std::string(e.what()));
    }

    create(preset);
}

} // namespace uniconv::core
