#pragma once

#include "plugin_manifest.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::core {

// Represents a single installed dependency
struct InstalledDependency {
    std::string name;
    std::string type;         // "python" or "node"
    std::string version;
    std::string installed_at; // ISO timestamp

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["name"] = name;
        j["type"] = type;
        j["version"] = version;
        j["installed_at"] = installed_at;
        return j;
    }

    static InstalledDependency from_json(const nlohmann::json& j) {
        InstalledDependency d;
        d.name = j.at("name").get<std::string>();
        d.type = j.at("type").get<std::string>();
        d.version = j.value("version", "");
        d.installed_at = j.value("installed_at", "");
        return d;
    }
};

// Represents a plugin's isolated dependency environment
struct DepEnvironment {
    std::string plugin_name;
    std::filesystem::path env_dir;
    std::vector<InstalledDependency> dependencies;

    // Get Python virtualenv directory
    std::filesystem::path python_dir() const {
        return env_dir / "python";
    }

    // Get Node.js modules directory
    std::filesystem::path node_dir() const {
        return env_dir / "node";
    }

    // Get deps tracking file path
    std::filesystem::path deps_file() const {
        return env_dir / "deps.json";
    }

    // Get Python binary path (platform-specific)
    std::filesystem::path python_bin() const {
#ifdef _WIN32
        return python_dir() / "Scripts" / "python.exe";
#else
        return python_dir() / "bin" / "python";
#endif
    }

    // Get pip binary path (platform-specific)
    std::filesystem::path pip_bin() const {
#ifdef _WIN32
        return python_dir() / "Scripts" / "pip.exe";
#else
        return python_dir() / "bin" / "pip";
#endif
    }

    // Get node_modules/.bin directory
    std::filesystem::path node_bin_dir() const {
        return node_dir() / "node_modules" / ".bin";
    }

    // Check if Python venv exists
    bool has_python_env() const {
        return std::filesystem::exists(python_bin());
    }

    // Check if Node env exists
    bool has_node_env() const {
        return std::filesystem::exists(node_dir() / "node_modules");
    }

    // Load deps.json
    bool load();

    // Save deps.json
    bool save() const;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["plugin_name"] = plugin_name;
        j["dependencies"] = nlohmann::json::array();
        for (const auto& dep : dependencies) {
            j["dependencies"].push_back(dep.to_json());
        }
        return j;
    }
};

// Result of dependency installation
struct DepInstallResult {
    bool success = false;
    std::string message;
    std::vector<std::string> installed;
    std::vector<std::string> failed;
    std::vector<std::string> skipped; // e.g., system deps

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["success"] = success;
        j["message"] = message;
        j["installed"] = installed;
        j["failed"] = failed;
        j["skipped"] = skipped;
        return j;
    }
};

// Progress callback for installation
using DepProgressCallback = std::function<void(const std::string& message)>;

// Dependency installer - manages isolated environments per plugin
class DependencyInstaller {
public:
    // Run a command and capture output
    struct CommandResult {
        int exit_code = -1;
        std::string stdout_output;
        std::string stderr_output;
    };

    // Constructor with base directory for deps (~/.uniconv/deps)
    explicit DependencyInstaller(const std::filesystem::path& deps_base_dir);

    // Install all dependencies from a plugin manifest
    DepInstallResult install_all(const PluginManifest& manifest,
                                  DepProgressCallback progress = nullptr);

    // Get or create environment for a plugin
    DepEnvironment get_or_create_env(const std::string& plugin_name);

    // Get existing environment (returns nullopt if not exists)
    std::optional<DepEnvironment> get_env(const std::string& plugin_name) const;

    // Remove a plugin's dependency environment
    bool remove_env(const std::string& plugin_name);

    // Check if dependencies are satisfied for a plugin
    struct DepCheckResult {
        bool satisfied = false;
        std::vector<std::string> missing;
        std::vector<std::string> present;
    };
    DepCheckResult check_deps(const PluginManifest& manifest) const;

    // Clean up orphaned environments (plugins that no longer exist)
    std::vector<std::string> clean_orphaned(
        const std::vector<std::string>& installed_plugin_names);

    // Get base directory
    const std::filesystem::path& base_dir() const { return deps_base_dir_; }

private:
    std::filesystem::path deps_base_dir_;

    // Create a Python virtualenv
    bool create_python_venv(const std::filesystem::path& venv_dir);

    // Install a single Python package
    DepInstallResult install_python_package(const DepEnvironment& env,
                                             const Dependency& dep);

    // Set up Node.js environment directory
    bool setup_node_env(const std::filesystem::path& node_dir);

    // Install a single Node package
    DepInstallResult install_node_package(const DepEnvironment& env,
                                           const Dependency& dep);

    // Run command helper
    static CommandResult run_command(const std::string& command,
                                      const std::vector<std::string>& args,
                                      const std::filesystem::path& working_dir = {});

    // Get current ISO timestamp
    static std::string current_timestamp();
};

} // namespace uniconv::core
