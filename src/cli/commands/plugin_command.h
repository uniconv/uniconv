#pragma once

#include "cli/parser.h"
#include "core/config_manager.h"
#include "core/plugin_discovery.h"
#include "core/plugin_manager.h"
#include <memory>

namespace uniconv::cli::commands {

// Plugin management command handler
class PluginCommand {
public:
    PluginCommand(
        std::shared_ptr<core::PluginManager> plugin_manager,
        std::shared_ptr<core::ConfigManager> config_manager
    );

    // Execute plugin subcommand
    int execute(const ParsedArgs& args);

    // List installed plugins
    int list(const ParsedArgs& args);

    // Install a plugin from path or URL
    int install(const ParsedArgs& args);

    // Remove a plugin
    int remove(const ParsedArgs& args);

    // Show plugin info
    int info(const ParsedArgs& args);

private:
    std::shared_ptr<core::PluginManager> plugin_manager_;
    std::shared_ptr<core::ConfigManager> config_manager_;
    core::PluginDiscovery discovery_;

    // Copy plugin directory to user plugins folder
    bool copy_plugin(const std::filesystem::path& source, const std::filesystem::path& dest);

    // Find plugin directory by name
    std::optional<std::filesystem::path> find_plugin_dir(const std::string& name);
};

} // namespace uniconv::cli::commands
