#pragma once

#include "plugin_manifest.h"
#include "plugins/plugin_interface.h"
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>

namespace uniconv::core {

// Forward declaration
struct DepEnvironment;

// CLI Plugin wrapper - adapts external CLI executable to IPlugin interface
class CLIPlugin : public plugins::IPlugin {
public:
    explicit CLIPlugin(PluginManifest manifest);

    // IPlugin interface
    PluginInfo info() const override;
    bool supports_target(const std::string& target) const override;
    bool supports_input(const std::string& format) const override;
    Result execute(const Request& request) override;

    // Get the manifest
    const PluginManifest& manifest() const { return manifest_; }

    // Set execution timeout (default: 5 minutes)
    void set_timeout(std::chrono::seconds timeout) { timeout_ = timeout; }

    // Set the dependency environment for this plugin
    void set_dep_environment(std::optional<DepEnvironment> env);

private:
    PluginManifest manifest_;
    std::chrono::seconds timeout_{300};
    std::optional<std::filesystem::path> dep_env_dir_;

    // Resolve the full path to the executable
    std::filesystem::path resolve_executable() const;

    // Build command line arguments for the plugin
    std::vector<std::string> build_arguments(const Request& request) const;

    // Build environment variables for plugin execution (PATH, PYTHONPATH, etc.)
    std::map<std::string, std::string> build_environment() const;

    // Execute the plugin and capture output
    struct ExecuteResult {
        int exit_code = -1;
        std::string stdout_output;
        std::string stderr_output;
        bool timed_out = false;
    };
    ExecuteResult run_process(const std::filesystem::path& executable,
                              const std::vector<std::string>& args,
                              const std::map<std::string, std::string>& env = {}) const;

    // Parse JSON result from plugin stdout
    Result parse_result(const Request& request, const ExecuteResult& exec_result) const;
};

// CLI Plugin Loader - creates CLIPlugin instances from manifests
class CLIPluginLoader {
public:
    // Load a CLI plugin from a manifest
    static std::unique_ptr<plugins::IPlugin> load(const PluginManifest& manifest);

    // Check if a manifest describes a CLI plugin
    static bool is_cli_plugin(const PluginManifest& manifest);
};

} // namespace uniconv::core
