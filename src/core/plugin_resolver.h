#pragma once

#include "core/types.h"
#include "plugins/plugin_interface.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::core
{

    // Context for plugin resolution - contains all info about current pipeline state
    struct ResolutionContext
    {
        std::string input_format;                   // e.g., "png", "pdf", "docx"
        std::string target;                         // e.g., "pdf", "docx", "png"
        std::optional<std::string> explicit_plugin; // e.g., "image-convert" (from @plugin)
        std::vector<DataType> input_types;          // e.g., {Image} or {File}
    };

    // Result of plugin resolution with reasoning
    struct ResolutionResult
    {
        plugins::IPlugin *plugin = nullptr;
        std::string matched_by; // "explicit", "default", "type+format", "type", "target", "none"
    };

    class PluginResolver
    {
    public:
        PluginResolver() = default;

        // Main resolution method - finds best plugin for context
        ResolutionResult resolve(
            const ResolutionContext &context,
            const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const;

        // Check if two plugins can be connected in a pipeline
        bool can_connect(const PluginInfo &from, const PluginInfo &to) const;

        // Default plugin management
        void set_default(const std::string &target, const std::string &plugin_scope);
        std::optional<std::string> get_default(const std::string &target) const;

        // Get access to defaults (for PluginManager backward compatibility)
        const std::map<std::string, std::string> &defaults() const { return defaults_; }
        std::map<std::string, std::string> &defaults() { return defaults_; }

    private:
        std::map<std::string, std::string> defaults_; // target -> plugin_scope

        // Resolution steps (in priority order)
        plugins::IPlugin *find_by_explicit(
            const std::string &scope,
            const std::string &target,
            const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const;

        plugins::IPlugin *find_by_default(
            const std::string &target,
            const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const;

        plugins::IPlugin *find_by_type_and_format(
            const ResolutionContext &context,
            const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const;

        plugins::IPlugin *find_by_type_only(
            const ResolutionContext &context,
            const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const;

        plugins::IPlugin *find_by_target_only(
            const std::string &target,
            const std::vector<std::unique_ptr<plugins::IPlugin>> &plugins) const;

        // Helper methods
        bool types_compatible(
            const std::vector<DataType> &input_types,
            const std::vector<DataType> &plugin_input_types) const;
    };

} // namespace uniconv::core
