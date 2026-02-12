#include "plugin_resolver.h"
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

    ResolutionResult PluginResolver::resolve(
        const ResolutionContext &context,
        const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const
    {
        // Priority 1: Explicit plugin (scope/plugin:target syntax)
        // If specified but not found, fail immediately — don't fall through
        if (context.explicit_plugin)
        {
            if (auto *p = find_by_explicit(*context.explicit_plugin, context.target, plugins))
            {
                return {p, "explicit"};
            }
            return {nullptr, "explicit_not_found"};
        }

        // Priority 2: Default plugin for target
        if (auto *p = find_by_default(context.target, plugins))
        {
            return {p, "default"};
        }

        // Priority 3: Type compatible + Format matching (NEW)
        // Only use this if we have input information
        if (!context.input_format.empty())
        {
            if (auto *p = find_by_type_and_format(context, plugins))
            {
                return {p, "type+format"};
            }
        }

        // Priority 4: Type compatible only
        if (!context.input_types.empty())
        {
            if (auto *p = find_by_type_only(context, plugins))
            {
                return {p, "type"};
            }
        }

        // Priority 5: Target only (fallback)
        if (auto *p = find_by_target_only(context.target, plugins))
        {
            return {p, "target"};
        }

        return {nullptr, "none"};
    }

    bool PluginResolver::can_connect(const PluginInfo &from, const PluginInfo &to) const
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

    void PluginResolver::set_default(const std::string &target, const std::string &plugin_scope)
    {
        defaults_[to_lower(target)] = to_lower(plugin_scope);
    }

    std::optional<std::string> PluginResolver::get_default(const std::string &target) const
    {
        auto it = defaults_.find(to_lower(target));
        if (it != defaults_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    plugins::IPlugin *PluginResolver::find_by_explicit(
        const std::string &plugin_specifier,
        const std::string &target,
        const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const
    {
        auto lower_spec = to_lower(plugin_specifier);
        auto lower_target = to_lower(target);

        // Check if specifier contains / (scope/name format)
        auto slash_pos = lower_spec.find('/');
        std::string match_scope, match_name;
        if (slash_pos != std::string::npos)
        {
            // scope/name: match both scope and name
            match_scope = lower_spec.substr(0, slash_pos);
            match_name = lower_spec.substr(slash_pos + 1);
        }

        for (const auto &plugin : plugins)
        {
            auto info = plugin->info();
            bool plugin_match;
            if (!match_name.empty())
            {
                // scope/name: match both scope and name
                plugin_match = to_lower(info.scope) == match_scope &&
                               to_lower(info.name) == match_name;
            }
            else
            {
                // name-only: match by plugin name
                plugin_match = to_lower(info.name) == lower_spec;
            }

            if (plugin_match && plugin->supports_target(lower_target))
            {
                return plugin.get();
            }
        }
        return nullptr;
    }

    plugins::IPlugin *PluginResolver::find_by_default(
        const std::string &target,
        const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const
    {
        auto lower_target = to_lower(target);
        auto default_it = defaults_.find(lower_target);

        if (default_it != defaults_.end())
        {
            for (const auto &plugin : plugins)
            {
                auto info = plugin->info();
                if (to_lower(info.scope) == default_it->second &&
                    plugin->supports_target(lower_target))
                {
                    return plugin.get();
                }
            }
        }
        return nullptr;
    }

    plugins::IPlugin *PluginResolver::find_by_type_and_format(
        const ResolutionContext &context,
        const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const
    {
        auto lower_target = to_lower(context.target);
        auto lower_input = to_lower(context.input_format);

        for (const auto &plugin : plugins)
        {
            auto info = plugin->info();

            // ┌────────────────────────────────────────────────────┐
            // │  PAIR MATCHING: Both input AND output must match   │
            // │                                                    │
            // │  input_format ──┬── plugin.accepts  (INPUT side)   │
            // │                 │                                   │
            // │  target ────────┴── plugin.targets  (OUTPUT side)  │
            // └────────────────────────────────────────────────────┘

            // Check 1: OUTPUT - Plugin can produce the target format
            if (!plugin->supports_target(lower_target))
            {
                continue;
            }

            // Check 2: INPUT TYPE - Data types are compatible (if we have type info)
            if (!context.input_types.empty() && !info.input_types.empty())
            {
                if (!types_compatible(context.input_types, info.input_types))
                {
                    continue;
                }
            }

            // Check 3: INPUT FORMAT - Plugin accepts this specific format
            if (!plugin->supports_input(lower_input))
            {
                continue;
            }

            // All checks passed: this plugin can handle input_format -> target
            return plugin.get();
        }
        return nullptr;
    }

    plugins::IPlugin *PluginResolver::find_by_type_only(
        const ResolutionContext &context,
        const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const
    {
        auto lower_target = to_lower(context.target);

        for (const auto &plugin : plugins)
        {
            auto info = plugin->info();

            // Check if plugin can produce the target
            if (!plugin->supports_target(lower_target))
            {
                continue;
            }

            // Check if data types are compatible
            if (!types_compatible(context.input_types, info.input_types))
            {
                continue;
            }

            return plugin.get();
        }
        return nullptr;
    }

    plugins::IPlugin *PluginResolver::find_by_target_only(
        const std::string &target,
        const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const
    {
        auto lower_target = to_lower(target);

        for (const auto &plugin : plugins)
        {
            if (plugin->supports_target(lower_target))
            {
                return plugin.get();
            }
        }
        return nullptr;
    }

    bool PluginResolver::types_compatible(
        const std::vector<DataType> &input_types,
        const std::vector<DataType> &plugin_input_types) const
    {
        // Empty input types from context means we don't know - allow any plugin
        if (input_types.empty())
        {
            return true;
        }

        // Empty plugin input types means plugin accepts anything
        if (plugin_input_types.empty())
        {
            return true;
        }

        // Check if any input type matches any plugin input type
        for (const auto &in_type : input_types)
        {
            for (const auto &plugin_type : plugin_input_types)
            {
                // Direct match
                if (in_type == plugin_type)
                {
                    return true;
                }
                // File type is always compatible
                if (in_type == DataType::File || plugin_type == DataType::File)
                {
                    return true;
                }
            }
        }

        return false;
    }

} // namespace uniconv::core
