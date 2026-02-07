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

    // Plugin dependency definition (from manifest)
    struct Dependency
    {
        std::string name; // e.g., "python3", "Pillow"
        std::string type; // "system", "python", "node"
        std::optional<std::string> version;      // e.g., ">=3.8"
        std::optional<std::string> check;        // custom verification command
        std::optional<std::string> install_hint; // e.g., "brew install ghostscript"

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            if (version)
                j["version"] = *version;
            if (check)
                j["check"] = *check;
            if (install_hint)
                j["install_hint"] = *install_hint;
            return j;
        }

        static Dependency from_json(const nlohmann::json &j)
        {
            Dependency d;
            d.name = j.at("name").get<std::string>();
            d.type = j.value("type", "system");
            if (j.contains("version"))
                d.version = j.at("version").get<std::string>();
            if (j.contains("check"))
                d.check = j.at("check").get<std::string>();
            if (j.contains("install_hint"))
                d.install_hint = j.at("install_hint").get<std::string>();
            return d;
        }
    };

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
            if (j.contains("default")) {
                const auto& val = j.at("default");
                if (val.is_string()) {
                    opt.default_value = val.get<std::string>();
                } else if (!val.is_null()) {
                    opt.default_value = val.dump();
                }
            }
            opt.description = j.value("description", "");
            return opt;
        }
    };

    // Plugin manifest - loaded from plugin.json
    struct PluginManifest
    {
        // Identification
        std::string name;    // e.g., "face-extractor"
        std::string scope;   // e.g., "ai-vision"
        std::string version; // e.g., "1.0.0"
        std::string description;

        // Plugin configuration
        std::vector<std::string> targets;       // Supported output targets
        std::vector<std::string> input_formats; // Supported input formats

        // Data types
        std::vector<DataType> input_types;
        std::vector<DataType> output_types;

        // Interface
        PluginInterface iface = PluginInterface::CLI;
        std::string executable; // For CLI: executable name or path
        std::string library;    // For Native: library filename

        // Options
        std::vector<PluginOptionDef> options;

        // Dependencies
        std::vector<Dependency> dependencies;

        // Metadata
        std::filesystem::path manifest_path; // Where this manifest was loaded from
        std::filesystem::path plugin_dir;    // Directory containing the plugin

        // Computed ID (just the scope name now)
        std::string id() const
        {
            return scope;
        }

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["scope"] = scope;
            j["version"] = version;
            j["description"] = description;
            j["targets"] = targets;
            j["input_formats"] = input_formats;
            j["interface"] = plugin_interface_to_string(iface);
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

            if (!dependencies.empty())
            {
                j["dependencies"] = nlohmann::json::array();
                for (const auto &dep : dependencies)
                {
                    j["dependencies"].push_back(dep.to_json());
                }
            }

            return j;
        }

        static PluginManifest from_json(const nlohmann::json &j)
        {
            PluginManifest m;

            m.name = j.at("name").get<std::string>();
            m.scope = j.value("scope", j.value("group", m.name)); // Default scope to name, with backward compat for "group"
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
            m.iface = plugin_interface_from_string(iface_str).value_or(PluginInterface::CLI);

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

            // Dependencies
            if (j.contains("dependencies") && j.at("dependencies").is_array())
            {
                for (const auto &dep_json : j.at("dependencies"))
                {
                    m.dependencies.push_back(Dependency::from_json(dep_json));
                }
            }

            return m;
        }

        // Convert to PluginInfo for compatibility with existing code
        PluginInfo to_plugin_info() const
        {
            PluginInfo info;
            info.id = id();
            info.scope = scope;
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
