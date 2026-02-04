#include "plugin_manager.h"
#include "plugin_loader_cli.h"
#include "plugin_loader_native.h"
#include <algorithm>
#include <set>

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

    // Find plugin by target
    plugins::IPlugin *PluginManager::find_plugin(
        const std::string &target,
        const std::optional<std::string> &explicit_plugin)
    {
        auto lower_target = to_lower(target);

        // If explicit plugin specified, find it
        if (explicit_plugin)
        {
            auto lower_explicit = to_lower(*explicit_plugin);
            for (auto &plugin : plugins_)
            {
                auto info = plugin->info();
                if (to_lower(info.scope) == lower_explicit &&
                    plugin->supports_target(lower_target))
                {
                    return plugin.get();
                }
            }
            return nullptr; // Explicit plugin not found
        }

        // Check for default plugin for this target
        auto default_it = defaults_.find(lower_target);
        if (default_it != defaults_.end())
        {
            for (auto &plugin : plugins_)
            {
                auto info = plugin->info();
                if (to_lower(info.scope) == default_it->second &&
                    plugin->supports_target(lower_target))
                {
                    return plugin.get();
                }
            }
        }

        // Find first plugin that supports this target
        for (auto &plugin : plugins_)
        {
            if (plugin->supports_target(lower_target))
            {
                return plugin.get();
            }
        }

        return nullptr;
    }

    // Find plugin for input format and target
    plugins::IPlugin *PluginManager::find_plugin_for_input(
        const std::string &input_format,
        const std::string &target)
    {
        auto lower_input = to_lower(input_format);
        auto lower_target = to_lower(target);

        for (auto &plugin : plugins_)
        {
            if (plugin->supports_input(lower_input) &&
                plugin->supports_target(lower_target))
            {
                return plugin.get();
            }
        }

        return nullptr;
    }

    // Check if two plugins can be connected
    bool PluginManager::can_connect(const PluginInfo &from, const PluginInfo &to) const
    {
        // If either plugin doesn't specify types, assume File type (always compatible)
        if (from.output_types.empty() || to.input_types.empty())
        {
            return true;
        }

        // Check if any output type from 'from' matches any input type in 'to'
        for (const auto &out_type : from.output_types)
        {
            for (const auto &in_type : to.input_types)
            {
                if (out_type == in_type ||
                    out_type == DataType::File ||
                    in_type == DataType::File)
                {
                    return true;
                }
            }
        }

        return false;
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
        defaults_[to_lower(target)] = to_lower(plugin_scope);
    }

    std::optional<std::string> PluginManager::get_default(const std::string &target) const
    {
        auto it = defaults_.find(to_lower(target));
        if (it != defaults_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

} // namespace uniconv::core
