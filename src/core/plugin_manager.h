#pragma once

#include "core/types.h"
#include "core/plugin_discovery.h"
#include "plugins/plugin_interface.h"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::core {

class PluginManager {
public:
    PluginManager();
    ~PluginManager() = default;

    // Non-copyable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // Register built-in plugins
    void register_builtin_plugins();

    // Load external plugins from discovery directories
    void load_external_plugins();

    // Load external plugins from a specific directory
    void load_plugins_from_dir(const std::filesystem::path& dir);

    // Add a custom plugin directory
    void add_plugin_dir(const std::filesystem::path& dir);

    // Register a plugin
    void register_plugin(std::unique_ptr<plugins::IPlugin> plugin);

    // Find plugin for ETL + target
    // Returns nullptr if not found
    // If multiple match and no explicit plugin specified, returns default or first
    plugins::IPlugin* find_plugin(
        ETLType etl,
        const std::string& target,
        const std::optional<std::string>& explicit_plugin = std::nullopt
    );

    // Find plugin that can handle a specific input format
    plugins::IPlugin* find_plugin_for_input(
        ETLType etl,
        const std::string& input_format,
        const std::string& target
    );

    // List all plugins
    std::vector<PluginInfo> list_plugins() const;

    // List plugins by ETL type
    std::vector<PluginInfo> list_plugins_by_etl(ETLType etl) const;

    // List plugins that support a specific target
    std::vector<PluginInfo> list_plugins_for_target(const std::string& target) const;

    // Get/set default plugin for target
    void set_default(const std::string& target, const std::string& plugin_group);
    std::optional<std::string> get_default(const std::string& target) const;

    // Get all supported targets for an ETL type
    std::vector<std::string> get_supported_targets(ETLType etl) const;

    // Get all supported input formats for an ETL type
    std::vector<std::string> get_supported_inputs(ETLType etl) const;

private:
    std::vector<std::unique_ptr<plugins::IPlugin>> plugins_;
    std::map<std::string, std::string> defaults_;  // target → plugin_group
    PluginDiscovery discovery_;
    bool external_loaded_ = false;
};

} // namespace uniconv::core
