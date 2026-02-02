#include "plugin_manager.h"
#include "plugin_loader_cli.h"
#include "plugin_loader_native.h"
#include "plugins/image_transform.h"
#include <algorithm>
#include <set>

namespace uniconv::core {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // anonymous namespace

PluginManager::PluginManager() {
    register_builtin_plugins();
}

void PluginManager::register_builtin_plugins() {
    // Register image transform plugin
    register_plugin(std::make_unique<plugins::ImageTransformPlugin>());

    // Future: FFmpeg transform/extract plugins
    // register_plugin(std::make_unique<plugins::FFmpegTransformPlugin>());
    // register_plugin(std::make_unique<plugins::FFmpegExtractPlugin>());
}

void PluginManager::load_external_plugins() {
    if (external_loaded_) {
        return;  // Already loaded
    }

    auto manifests = discovery_.discover_all();
    for (const auto& manifest : manifests) {
        std::unique_ptr<plugins::IPlugin> plugin;

        if (CLIPluginLoader::is_cli_plugin(manifest)) {
            plugin = CLIPluginLoader::load(manifest);
        } else if (NativePluginLoader::is_native_plugin(manifest)) {
            plugin = NativePluginLoader::load(manifest);
        }

        if (plugin) {
            register_plugin(std::move(plugin));
        }
    }

    external_loaded_ = true;
}

void PluginManager::load_plugins_from_dir(const std::filesystem::path& dir) {
    auto manifests = discovery_.discover_in_dir(dir);
    for (const auto& manifest : manifests) {
        std::unique_ptr<plugins::IPlugin> plugin;

        if (CLIPluginLoader::is_cli_plugin(manifest)) {
            plugin = CLIPluginLoader::load(manifest);
        } else if (NativePluginLoader::is_native_plugin(manifest)) {
            plugin = NativePluginLoader::load(manifest);
        }

        if (plugin) {
            register_plugin(std::move(plugin));
        }
    }
}

void PluginManager::add_plugin_dir(const std::filesystem::path& dir) {
    discovery_.add_plugin_dir(dir);
}

void PluginManager::register_plugin(std::unique_ptr<plugins::IPlugin> plugin) {
    if (plugin) {
        plugins_.push_back(std::move(plugin));
    }
}

plugins::IPlugin* PluginManager::find_plugin(
    ETLType etl,
    const std::string& target,
    const std::optional<std::string>& explicit_plugin
) {
    auto lower_target = to_lower(target);

    // If explicit plugin specified, find it
    if (explicit_plugin) {
        auto lower_explicit = to_lower(*explicit_plugin);
        for (auto& plugin : plugins_) {
            auto info = plugin->info();
            if (info.etl == etl &&
                to_lower(info.group) == lower_explicit &&
                plugin->supports_target(lower_target)) {
                return plugin.get();
            }
        }
        return nullptr;  // Explicit plugin not found
    }

    // Check for default plugin for this target
    auto default_it = defaults_.find(lower_target);
    if (default_it != defaults_.end()) {
        for (auto& plugin : plugins_) {
            auto info = plugin->info();
            if (info.etl == etl &&
                to_lower(info.group) == default_it->second &&
                plugin->supports_target(lower_target)) {
                return plugin.get();
            }
        }
    }

    // Find first plugin that supports this ETL type and target
    for (auto& plugin : plugins_) {
        auto info = plugin->info();
        if (info.etl == etl && plugin->supports_target(lower_target)) {
            return plugin.get();
        }
    }

    return nullptr;
}

plugins::IPlugin* PluginManager::find_plugin_for_input(
    ETLType etl,
    const std::string& input_format,
    const std::string& target
) {
    auto lower_input = to_lower(input_format);
    auto lower_target = to_lower(target);

    for (auto& plugin : plugins_) {
        auto info = plugin->info();
        if (info.etl == etl &&
            plugin->supports_input(lower_input) &&
            plugin->supports_target(lower_target)) {
            return plugin.get();
        }
    }

    return nullptr;
}

std::vector<PluginInfo> PluginManager::list_plugins() const {
    std::vector<PluginInfo> result;
    result.reserve(plugins_.size());
    for (const auto& plugin : plugins_) {
        result.push_back(plugin->info());
    }
    return result;
}

std::vector<PluginInfo> PluginManager::list_plugins_by_etl(ETLType etl) const {
    std::vector<PluginInfo> result;
    for (const auto& plugin : plugins_) {
        auto info = plugin->info();
        if (info.etl == etl) {
            result.push_back(info);
        }
    }
    return result;
}

std::vector<PluginInfo> PluginManager::list_plugins_for_target(const std::string& target) const {
    auto lower_target = to_lower(target);
    std::vector<PluginInfo> result;
    for (const auto& plugin : plugins_) {
        if (plugin->supports_target(lower_target)) {
            result.push_back(plugin->info());
        }
    }
    return result;
}

void PluginManager::set_default(const std::string& target, const std::string& plugin_group) {
    defaults_[to_lower(target)] = to_lower(plugin_group);
}

std::optional<std::string> PluginManager::get_default(const std::string& target) const {
    auto it = defaults_.find(to_lower(target));
    if (it != defaults_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> PluginManager::get_supported_targets(ETLType etl) const {
    std::set<std::string> targets;
    for (const auto& plugin : plugins_) {
        auto info = plugin->info();
        if (info.etl == etl) {
            for (const auto& target : info.targets) {
                targets.insert(to_lower(target));
            }
        }
    }
    return std::vector<std::string>(targets.begin(), targets.end());
}

std::vector<std::string> PluginManager::get_supported_inputs(ETLType etl) const {
    std::set<std::string> inputs;
    for (const auto& plugin : plugins_) {
        auto info = plugin->info();
        if (info.etl == etl) {
            for (const auto& fmt : info.input_formats) {
                inputs.insert(to_lower(fmt));
            }
        }
    }
    return std::vector<std::string>(inputs.begin(), inputs.end());
}

} // namespace uniconv::core
