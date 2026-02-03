#pragma once

#include "types.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::core
{

    // Plugin interface type
    enum class PluginInterface
    {
        Native, // Shared library (.so, .dylib, .dll)
        CLI     // External executable
    };

    inline std::string plugin_interface_to_string(PluginInterface iface)
    {
        switch (iface)
        {
        case PluginInterface::Native:
            return "native";
        case PluginInterface::CLI:
            return "cli";
        }
        return "unknown";
    }

    inline std::optional<PluginInterface> plugin_interface_from_string(const std::string &s)
    {
        if (s == "native")
            return PluginInterface::Native;
        if (s == "cli")
            return PluginInterface::CLI;
        return std::nullopt;
    }

    // Plugin option definition (from manifest)
    struct PluginOptionDef
    {
        std::string name; // e.g., "--confidence"
        std::string type; // "string", "int", "float", "bool"
        std::string default_value;
        std::string description;

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            if (!default_value.empty())
                j["default"] = default_value;
            if (!description.empty())
                j["description"] = description;
            return j;
        }

        static PluginOptionDef from_json(const nlohmann::json &j)
        {
            PluginOptionDef opt;
            opt.name = j.at("name").get<std::string>();
            opt.type = j.value("type", "string");
            opt.default_value = j.value("default", "");
            opt.description = j.value("description", "");
            return opt;
        }
    };

    // Plugin manifest - loaded from plugin.json
    struct PluginManifest
    {
        // Identification
        std::string name;    // e.g., "face-extractor"
        std::string group;   // e.g., "ai-vision"
        std::string version; // e.g., "1.0.0"
        std::string description;

        // Plugin configuration
        std::vector<std::string> targets;       // Supported output targets
        std::vector<std::string> input_formats; // Supported input formats

        // Data types
        std::vector<DataType> input_types;
        std::vector<DataType> output_types;

        // Interface
        PluginInterface interface = PluginInterface::CLI;
        std::string executable; // For CLI: executable name or path
        std::string library;    // For Native: library filename

        // Options
        std::vector<PluginOptionDef> options;

        // Metadata
        std::filesystem::path manifest_path; // Where this manifest was loaded from
        std::filesystem::path plugin_dir;    // Directory containing the plugin

        // Computed ID (just the group name now)
        std::string id() const
        {
            return group;
        }

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["group"] = group;
            j["version"] = version;
            j["description"] = description;
            j["targets"] = targets;
            j["input_formats"] = input_formats;
            j["interface"] = plugin_interface_to_string(interface);
            if (!executable.empty())
                j["executable"] = executable;
            if (!library.empty())
                j["library"] = library;

            if (!options.empty())
            {
                j["options"] = nlohmann::json::array();
                for (const auto &opt : options)
                {
                    j["options"].push_back(opt.to_json());
                }
            }

            return j;
        }

        static PluginManifest from_json(const nlohmann::json &j)
        {
            PluginManifest m;

            m.name = j.at("name").get<std::string>();
            m.group = j.value("group", m.name); // Default group to name
            m.version = j.value("version", "0.0.0");
            m.description = j.value("description", "");

            // Targets and input formats
            if (j.contains("targets"))
            {
                m.targets = j.at("targets").get<std::vector<std::string>>();
            }
            if (j.contains("input_formats"))
            {
                m.input_formats = j.at("input_formats").get<std::vector<std::string>>();
            }

            // Interface
            auto iface_str = j.value("interface", "cli");
            m.interface = plugin_interface_from_string(iface_str).value_or(PluginInterface::CLI);

            m.executable = j.value("executable", "");
            m.library = j.value("library", "");

            // Options
            if (j.contains("options") && j.at("options").is_array())
            {
                for (const auto &opt_json : j.at("options"))
                {
                    m.options.push_back(PluginOptionDef::from_json(opt_json));
                }
            }

            return m;
        }

        // Convert to PluginInfo for compatibility with existing code
        PluginInfo to_plugin_info() const
        {
            PluginInfo info;
            info.id = id();
            info.group = group;
            info.targets = targets;
            info.input_formats = input_formats;
            info.input_types = input_types;
            info.output_types = output_types;
            info.version = version;
            info.description = description;
            info.builtin = false;
            return info;
        }
    };

} // namespace uniconv::core
