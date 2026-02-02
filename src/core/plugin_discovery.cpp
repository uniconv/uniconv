#include "plugin_discovery.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#include <pwd.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

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
    if (home) {
        return std::filesystem::path(home);
    }
    // Fallback
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return std::filesystem::path(path);
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home);
    }
    // Fallback to passwd entry
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::filesystem::path(pw->pw_dir);
    }
#endif
    return std::filesystem::path();
}

// Get executable directory
std::filesystem::path get_executable_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#elif defined(__APPLE__)
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::canonical(path).parent_path();
    }
    return std::filesystem::path();
#else
    // Linux
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

} // anonymous namespace

PluginDiscovery::PluginDiscovery()
    : plugin_dirs_(get_standard_plugin_dirs()) {
}

PluginDiscovery::PluginDiscovery(std::vector<std::filesystem::path> plugin_dirs)
    : plugin_dirs_(std::move(plugin_dirs)) {
}

void PluginDiscovery::add_plugin_dir(const std::filesystem::path& dir) {
    // Avoid duplicates
    if (std::find(plugin_dirs_.begin(), plugin_dirs_.end(), dir) == plugin_dirs_.end()) {
        plugin_dirs_.push_back(dir);
    }
}

std::vector<PluginManifest> PluginDiscovery::discover_all() const {
    std::vector<PluginManifest> manifests;

    for (const auto& dir : plugin_dirs_) {
        auto discovered = discover_in_dir(dir);
        manifests.insert(manifests.end(),
                        std::make_move_iterator(discovered.begin()),
                        std::make_move_iterator(discovered.end()));
    }

    return manifests;
}

std::vector<PluginManifest> PluginDiscovery::discover_in_dir(const std::filesystem::path& dir) const {
    std::vector<PluginManifest> manifests;

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return manifests;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_directory()) {
                auto manifest = load_manifest(entry.path());
                if (manifest) {
                    manifests.push_back(std::move(*manifest));
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore permission errors, etc.
    }

    return manifests;
}

std::optional<PluginManifest> PluginDiscovery::load_manifest(const std::filesystem::path& plugin_dir) const {
    auto manifest_path = plugin_dir / kManifestFilename;
    return load_manifest_file(manifest_path);
}

std::optional<PluginManifest> PluginDiscovery::load_manifest_file(const std::filesystem::path& manifest_path) const {
    if (!std::filesystem::exists(manifest_path)) {
        return std::nullopt;
    }

    try {
        std::ifstream file(manifest_path);
        if (!file) {
            return std::nullopt;
        }

        nlohmann::json j;
        file >> j;

        auto manifest = PluginManifest::from_json(j);
        manifest.manifest_path = manifest_path;
        manifest.plugin_dir = manifest_path.parent_path();

        return manifest;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool PluginDiscovery::is_plugin_dir(const std::filesystem::path& dir) const {
    if (!std::filesystem::is_directory(dir)) {
        return false;
    }
    return std::filesystem::exists(dir / kManifestFilename);
}

std::vector<std::filesystem::path> PluginDiscovery::get_standard_plugin_dirs() {
    std::vector<std::filesystem::path> dirs;

    // User plugins (highest priority)
    auto user_dir = get_user_plugin_dir();
    if (!user_dir.empty()) {
        dirs.push_back(user_dir);
    }

    // Portable plugins (next to executable)
    auto portable_dir = get_portable_plugin_dir();
    if (!portable_dir.empty()) {
        dirs.push_back(portable_dir);
    }

    // System plugins (lowest priority)
    auto system_dir = get_system_plugin_dir();
    if (!system_dir.empty()) {
        dirs.push_back(system_dir);
    }

    return dirs;
}

std::filesystem::path PluginDiscovery::get_user_plugin_dir() {
    auto home = get_home_dir();
    if (home.empty()) {
        return {};
    }
    return home / ".uniconv" / "plugins";
}

std::filesystem::path PluginDiscovery::get_system_plugin_dir() {
#ifdef _WIN32
    // Windows: %ProgramData%\uniconv\plugins
    const char* programdata = std::getenv("ProgramData");
    if (programdata) {
        return std::filesystem::path(programdata) / "uniconv" / "plugins";
    }
    return {};
#elif defined(__APPLE__)
    // macOS: /usr/local/share/uniconv/plugins
    return "/usr/local/share/uniconv/plugins";
#else
    // Linux: /usr/local/share/uniconv/plugins or /usr/share/uniconv/plugins
    if (std::filesystem::exists("/usr/local/share")) {
        return "/usr/local/share/uniconv/plugins";
    }
    return "/usr/share/uniconv/plugins";
#endif
}

std::filesystem::path PluginDiscovery::get_portable_plugin_dir() {
    auto exe_dir = get_executable_dir();
    if (exe_dir.empty()) {
        return {};
    }
    return exe_dir / "plugins";
}

} // namespace uniconv::core
