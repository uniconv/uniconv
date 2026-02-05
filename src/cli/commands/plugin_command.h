#pragma once

#include "cli/parser.h"
#include "core/config_manager.h"
#include "core/dependency_checker.h"
#include "core/dependency_installer.h"
#include "core/installed_plugins.h"
#include "core/output/output.h"
#include "core/plugin_discovery.h"
#include "core/plugin_manager.h"
#include "core/registry_client.h"
#include <memory>
#include <optional>
#include <utility>

namespace uniconv::cli::commands {

// Plugin management command handler
class PluginCommand {
public:
    PluginCommand(
        std::shared_ptr<core::PluginManager> plugin_manager,
        std::shared_ptr<core::ConfigManager> config_manager,
        std::shared_ptr<core::output::IOutput> output
    );

    // Execute plugin subcommand
    int execute(const ParsedArgs& args);

    // List installed plugins
    int list(const ParsedArgs& args);

    // Install a plugin from path or registry
    int install(const ParsedArgs& args);

    // Remove a plugin
    int remove(const ParsedArgs& args);

    // Show plugin info
    int info(const ParsedArgs& args);

    // Search plugin registry
    int search(const ParsedArgs& args);

    // Update plugin(s)
    int update(const ParsedArgs& args);

private:
    std::shared_ptr<core::PluginManager> plugin_manager_;
    std::shared_ptr<core::ConfigManager> config_manager_;
    std::shared_ptr<core::output::IOutput> output_;
    core::PluginDiscovery discovery_;
    core::InstalledPlugins installed_;
    core::DependencyChecker dep_checker_;
    core::DependencyInstaller dep_installer_;

    // List all plugins available in the registry
    int list_registry(const ParsedArgs& args);

    // Copy plugin directory to user plugins folder
    bool copy_plugin(const std::filesystem::path& source, const std::filesystem::path& dest);

    // Find plugin directory by name
    std::optional<std::filesystem::path> find_plugin_dir(const std::string& name);

    // Create registry client from config
    std::unique_ptr<core::RegistryClient> make_registry_client() const;

    // Install from registry
    int install_from_registry(const std::string& name,
                              const std::optional<std::string>& version,
                              const ParsedArgs& args);

    // Install a collection of plugins
    int install_collection(const std::string& collection_name,
                           const ParsedArgs& args);

    // Parse "name@version" syntax
    static std::pair<std::string, std::optional<std::string>>
        parse_install_arg(const std::string& arg);
};

} // namespace uniconv::cli::commands
