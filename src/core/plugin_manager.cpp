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

    void PluginManager::ensure_discovered() const
    {
        if (discovered_)
            return;
        manifests_ = discovery_.discover_all();
        discovered_ = true;
    }

    void PluginManager::load_matching(const ResolutionContext &context)
    {
        ensure_discovered();

        auto target = to_lower(context.target);
        auto it = manifests_.begin();
        while (it != manifests_.end())
        {
            bool matches = false;

            // Match by target
            for (const auto &[t, _] : it->targets)
            {
                if (to_lower(t) == target)
                {
                    matches = true;
                    break;
                }
            }

            // Match by explicit plugin (scope/plugin:target syntax)
            if (!matches && context.explicit_plugin)
            {
                auto ep = to_lower(*context.explicit_plugin);
                auto slash_pos = ep.find('/');
                if (slash_pos != std::string::npos)
                {
                    // scope/name format (e.g., "geo/postgis")
                    auto ep_scope = ep.substr(0, slash_pos);
                    auto ep_name = ep.substr(slash_pos + 1);
                    if (to_lower(it->scope) == ep_scope && to_lower(it->name) == ep_name)
                        matches = true;
                }
                else
                {
                    // name-only format (e.g., "postgis")
                    if (to_lower(it->name) == ep)
                        matches = true;
                }
            }

            if (matches)
            {
                std::unique_ptr<plugins::IPlugin> plugin;

                if (CLIPluginLoader::is_cli_plugin(*it))
                {
                    plugin = CLIPluginLoader::load(*it);
                    if (auto *cp = dynamic_cast<CLIPlugin *>(plugin.get()))
                    {
                        cp->set_dep_environment(dep_installer_.get_env(it->name));
                    }
                }
                else if (NativePluginLoader::is_native_plugin(*it))
                {
                    plugin = NativePluginLoader::load(*it);
                }

                if (plugin)
                {
                    register_plugin(std::move(plugin));
                }
                it = manifests_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void PluginManager::load_external_plugins()
    {
        if (external_loaded_)
        {
            return; // Already loaded
        }

        ensure_discovered();

        for (const auto &manifest : manifests_)
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

        manifests_.clear();
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

        return find_plugin(context);
    }

    // Find plugin with full resolution context (new enhanced method)
    plugins::IPlugin *PluginManager::find_plugin(const ResolutionContext &context)
    {
        ensure_discovered();

        // Try already-loaded plugins first
        auto result = resolver_.resolve(context, plugins_);
        if (result.plugin)
            return result.plugin;

        // Load matching manifests and try again
        load_matching(context);
        result = resolver_.resolve(context, plugins_);
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

        return find_plugin(context);
    }

    // Check if two plugins can be connected - delegate to resolver
    bool PluginManager::can_connect(const PluginInfo &from, const PluginInfo &to) const
    {
        return resolver_.can_connect(from, to);
    }

    std::vector<PluginInfo> PluginManager::list_plugins() const
    {
        ensure_discovered();

        std::vector<PluginInfo> result;
        result.reserve(plugins_.size() + manifests_.size());
        for (const auto &plugin : plugins_)
        {
            result.push_back(plugin->info());
        }
        for (const auto &m : manifests_)
        {
            result.push_back(m.to_plugin_info());
        }
        return result;
    }

    std::vector<PluginInfo> PluginManager::list_plugins_for_target(const std::string &target) const
    {
        ensure_discovered();

        auto lower_target = to_lower(target);
        std::vector<PluginInfo> result;
        for (const auto &plugin : plugins_)
        {
            if (plugin->supports_target(lower_target))
            {
                result.push_back(plugin->info());
            }
        }
        for (const auto &m : manifests_)
        {
            for (const auto &[t, _] : m.targets)
            {
                if (to_lower(t) == lower_target)
                {
                    result.push_back(m.to_plugin_info());
                    break;
                }
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
