#include "plugin_manager.h"
#include "config_manager.h"
#include "plugin_loader_cli.h"
#include "plugin_loader_native.h"
#include "utils/file_utils.h"
#include <algorithm>

namespace uniconv::core
{

    namespace
    {

        std::string to_lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });
            return s;
        }

    } // anonymous namespace

    PluginManager::PluginManager()
        : dep_installer_(ConfigManager::get_default_config_dir() / "deps")
    {
        register_builtin_plugins();
    }

    void PluginManager::register_builtin_plugins()
    {
        // All plugins are now external (loaded via load_external_plugins)
    }

    void PluginManager::load_external_plugins()
    {
        if (external_loaded_)
        {
            return; // Already loaded
        }

        auto manifests = discovery_.discover_all();
        for (const auto &manifest : manifests)
        {
            std::unique_ptr<plugins::IPlugin> plugin;

            if (CLIPluginLoader::is_cli_plugin(manifest))
            {
                plugin = CLIPluginLoader::load(manifest);
                // Set dependency environment for CLI plugins
                if (auto* cli_plugin = dynamic_cast<CLIPlugin*>(plugin.get()))
                {
                    auto dep_env = dep_installer_.get_env(manifest.name);
                    cli_plugin->set_dep_environment(dep_env);
                }
            }
            else if (NativePluginLoader::is_native_plugin(manifest))
            {
                plugin = NativePluginLoader::load(manifest);
            }

            if (plugin)
            {
                register_plugin(std::move(plugin));
            }
        }

        external_loaded_ = true;
    }

    void PluginManager::load_plugins_from_dir(const std::filesystem::path &dir)
    {
        auto manifests = discovery_.discover_in_dir(dir);
        for (const auto &manifest : manifests)
        {
            std::unique_ptr<plugins::IPlugin> plugin;

            if (CLIPluginLoader::is_cli_plugin(manifest))
            {
                plugin = CLIPluginLoader::load(manifest);
                // Set dependency environment for CLI plugins
                if (auto* cli_plugin = dynamic_cast<CLIPlugin*>(plugin.get()))
                {
                    auto dep_env = dep_installer_.get_env(manifest.name);
                    cli_plugin->set_dep_environment(dep_env);
                }
            }
            else if (NativePluginLoader::is_native_plugin(manifest))
            {
                plugin = NativePluginLoader::load(manifest);
            }

            if (plugin)
            {
                register_plugin(std::move(plugin));
            }
        }
    }

    void PluginManager::add_plugin_dir(const std::filesystem::path &dir)
    {
        discovery_.add_plugin_dir(dir);
    }

    void PluginManager::register_plugin(std::unique_ptr<plugins::IPlugin> plugin)
    {
        if (plugin)
        {
            plugins_.push_back(std::move(plugin));
        }
    }

    // Find plugin by target (basic - backward compatible)
    plugins::IPlugin *PluginManager::find_plugin(
        const std::string &target,
        const std::optional<std::string> &explicit_plugin)
    {
        // Build a minimal resolution context
        ResolutionContext context;
        context.target = target;
        context.explicit_plugin = explicit_plugin;
        // No input_format or input_types - will fall through to target-only matching

        auto result = resolver_.resolve(context, plugins_);
        return result.plugin;
    }

    // Find plugin with full resolution context (new enhanced method)
    plugins::IPlugin *PluginManager::find_plugin(const ResolutionContext &context)
    {
        auto result = resolver_.resolve(context, plugins_);
        return result.plugin;
    }

    // Find plugin for input format and target (backward compatible)
    plugins::IPlugin *PluginManager::find_plugin_for_input(
        const std::string &input_format,
        const std::string &target)
    {
        // Build resolution context with input format and type information
        ResolutionContext context;
        context.input_format = input_format;
        context.target = target;
        context.input_types = utils::detect_input_types(input_format);

        auto result = resolver_.resolve(context, plugins_);
        return result.plugin;
    }

    // Check if two plugins can be connected - delegate to resolver
    bool PluginManager::can_connect(const PluginInfo &from, const PluginInfo &to) const
    {
        return resolver_.can_connect(from, to);
    }

    std::vector<PluginInfo> PluginManager::list_plugins() const
    {
        std::vector<PluginInfo> result;
        result.reserve(plugins_.size());
        for (const auto &plugin : plugins_)
        {
            result.push_back(plugin->info());
        }
        return result;
    }

    std::vector<PluginInfo> PluginManager::list_plugins_for_target(const std::string &target) const
    {
        auto lower_target = to_lower(target);
        std::vector<PluginInfo> result;
        for (const auto &plugin : plugins_)
        {
            if (plugin->supports_target(lower_target))
            {
                result.push_back(plugin->info());
            }
        }
        return result;
    }

    void PluginManager::set_default(const std::string &target, const std::string &plugin_scope)
    {
        resolver_.set_default(target, plugin_scope);
    }

    std::optional<std::string> PluginManager::get_default(const std::string &target) const
    {
        return resolver_.get_default(target);
    }

} // namespace uniconv::core
