#pragma once

#include "types.h"
#include <filesystem>
#include <map>
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
        std::optional<double> min_value;
        std::optional<double> max_value;
        std::vector<std::string> choices;
        std::vector<std::string> targets;

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            if (!default_value.empty())
                j["default"] = default_value;
            if (!description.empty())
                j["description"] = description;
            if (min_value.has_value())
                j["min"] = min_value.value();
            if (max_value.has_value())
                j["max"] = max_value.value();
            if (!choices.empty())
                j["choices"] = choices;
            if (!targets.empty())
                j["targets"] = targets;
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
            if (j.contains("min") && j.at("min").is_number())
                opt.min_value = j.at("min").get<double>();
            if (j.contains("max") && j.at("max").is_number())
                opt.max_value = j.at("max").get<double>();
            if (j.contains("choices") && j.at("choices").is_array())
                opt.choices = j.at("choices").get<std::vector<std::string>>();
            if (j.contains("targets") && j.at("targets").is_array())
                opt.targets = j.at("targets").get<std::vector<std::string>>();
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
        std::map<std::string, std::vector<std::string>> targets; // Supported output targets → extensions
        std::vector<std::string> input_formats; // Supported input formats (legacy)
        std::vector<std::string> accepts;       // Accepted input formats (preferred over input_formats)
        std::map<std::string, std::vector<std::string>> target_input_formats; // Per-target input format overrides

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

        // Computed ID: scope/name for 3rd-party, name for official (uniconv) plugins
        std::string id() const
        {
            if (scope == "uniconv" || scope == name)
                return name;
            return scope + "/" + name;
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
            if (!accepts.empty())
                j["accepts"] = accepts;
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

            if (!input_types.empty())
            {
                std::vector<std::string> input_strs;
                for (auto t : input_types)
                    input_strs.push_back(data_type_to_string(t));
                j["input_types"] = input_strs;
            }
            if (!output_types.empty())
            {
                std::vector<std::string> output_strs;
                for (auto t : output_types)
                    output_strs.push_back(data_type_to_string(t));
                j["output_types"] = output_strs;
            }

            if (!target_input_formats.empty())
            {
                j["target_input_formats"] = target_input_formats;
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

            // Targets: support both array (backward compat) and map format
            if (j.contains("targets"))
            {
                if (j.at("targets").is_array())
                {
                    // Old format: ["jpg", "png"] → {"jpg": [], "png": []}
                    for (const auto &t : j.at("targets"))
                    {
                        m.targets[t.get<std::string>()] = {};
                    }
                }
                else if (j.at("targets").is_object())
                {
                    // New format: {"extract": ["geojson", "csv"]}
                    m.targets = j.at("targets").get<std::map<std::string, std::vector<std::string>>>();
                }
            }
            // Input formats: prefer "accepts" over "input_formats"
            if (j.contains("accepts"))
            {
                m.accepts = j.at("accepts").get<std::vector<std::string>>();
            }
            if (j.contains("input_formats"))
            {
                m.input_formats = j.at("input_formats").get<std::vector<std::string>>();
                // Populate accepts from input_formats if accepts not provided
                if (m.accepts.empty())
                {
                    m.accepts = m.input_formats;
                }
            }
            if (j.contains("target_input_formats") && j.at("target_input_formats").is_object())
            {
                m.target_input_formats = j.at("target_input_formats").get<std::map<std::string, std::vector<std::string>>>();
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
            info.name = name;
            info.id = id();
            info.scope = scope;
            info.targets = targets;
            info.input_formats = input_formats;
            info.accepts = accepts;
            info.input_types = input_types;
            info.output_types = output_types;
            info.version = version;
            info.description = description;
            info.target_input_formats = target_input_formats;
            info.builtin = false;
            return info;
        }
    };

} // namespace uniconv::core
