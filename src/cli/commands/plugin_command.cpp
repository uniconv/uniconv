#include "plugin_command.h"
#include <iostream>
#include <iomanip>

namespace uniconv::cli::commands {

PluginCommand::PluginCommand(
    std::shared_ptr<core::PluginManager> plugin_manager,
    std::shared_ptr<core::ConfigManager> config_manager
) : plugin_manager_(std::move(plugin_manager))
  , config_manager_(std::move(config_manager)) {
}

int PluginCommand::execute(const ParsedArgs& args) {
    if (args.subcommand_args.empty()) {
        return list(args);
    }

    const auto& action = args.subcommand_args[0];

    if (action == "install") {
        return install(args);
    } else if (action == "remove") {
        return remove(args);
    } else if (action == "info") {
        return info(args);
    }

    std::cerr << "Unknown plugin action: " << action << "\n";
    std::cerr << "Available actions: install, remove, info\n";
    return 1;
}

int PluginCommand::list(const ParsedArgs& args) {
    // Discover all plugins
    auto manifests = discovery_.discover_all();

    // Also get built-in plugins from plugin manager
    auto builtin = plugin_manager_->list_plugins();

    if (args.core_options.json_output) {
        nlohmann::json j = nlohmann::json::array();

        // Add built-in plugins
        for (const auto& p : builtin) {
            j.push_back(p.to_json());
        }

        // Add discovered external plugins
        for (const auto& m : manifests) {
            auto info = m.to_plugin_info();
            auto pj = info.to_json();
            pj["path"] = m.plugin_dir.string();
            pj["interface"] = core::plugin_interface_to_string(m.interface);
            j.push_back(pj);
        }

        std::cout << j.dump(2) << std::endl;
        return 0;
    }

    // Table header
    std::cout << std::left
              << std::setw(25) << "NAME"
              << std::setw(12) << "ETL"
              << std::setw(30) << "TARGETS"
              << std::setw(10) << "VERSION"
              << "TYPE\n";
    std::cout << std::string(85, '-') << "\n";

    // Built-in plugins
    for (const auto& p : builtin) {
        std::string targets;
        for (size_t i = 0; i < p.targets.size() && i < 5; ++i) {
            if (i > 0) targets += ",";
            targets += p.targets[i];
        }
        if (p.targets.size() > 5) targets += ",...";

        std::cout << std::left
                  << std::setw(25) << p.id
                  << std::setw(12) << core::etl_type_to_string(p.etl)
                  << std::setw(30) << targets
                  << std::setw(10) << p.version
                  << "built-in\n";
    }

    // External plugins
    for (const auto& m : manifests) {
        std::string targets;
        for (size_t i = 0; i < m.targets.size() && i < 5; ++i) {
            if (i > 0) targets += ",";
            targets += m.targets[i];
        }
        if (m.targets.size() > 5) targets += ",...";

        std::string type = core::plugin_interface_to_string(m.interface);

        std::cout << std::left
                  << std::setw(25) << m.id()
                  << std::setw(12) << core::etl_type_to_string(m.etl)
                  << std::setw(30) << targets
                  << std::setw(10) << m.version
                  << type << "\n";
    }

    if (builtin.empty() && manifests.empty()) {
        std::cout << "(no plugins installed)\n";
    }

    return 0;
}

int PluginCommand::install(const ParsedArgs& args) {
    if (args.subcommand_args.size() < 2) {
        std::cerr << "Usage: uniconv plugin install <path>\n";
        return 1;
    }

    std::filesystem::path source_path = args.subcommand_args[1];

    // Check if source exists
    if (!std::filesystem::exists(source_path)) {
        std::cerr << "Error: Path does not exist: " << source_path << "\n";
        return 1;
    }

    // If source is a file, assume it's a manifest and use parent directory
    if (std::filesystem::is_regular_file(source_path)) {
        if (source_path.filename() == core::PluginDiscovery::kManifestFilename) {
            source_path = source_path.parent_path();
        } else {
            std::cerr << "Error: Expected a directory or plugin.json file\n";
            return 1;
        }
    }

    // Load manifest to get plugin name
    auto manifest = discovery_.load_manifest(source_path);
    if (!manifest) {
        std::cerr << "Error: Could not load plugin manifest from: " << source_path << "\n";
        std::cerr << "Make sure the directory contains a valid plugin.json file\n";
        return 1;
    }

    // Destination in user plugins directory
    auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
    auto dest_path = user_plugins / manifest->name;

    // Check if already installed
    if (std::filesystem::exists(dest_path)) {
        if (!args.core_options.force) {
            std::cerr << "Error: Plugin already installed at: " << dest_path << "\n";
            std::cerr << "Use --force to overwrite\n";
            return 1;
        }
        // Remove existing
        std::filesystem::remove_all(dest_path);
    }

    // Copy plugin
    if (!copy_plugin(source_path, dest_path)) {
        std::cerr << "Error: Failed to copy plugin to: " << dest_path << "\n";
        return 1;
    }

    if (!args.core_options.quiet) {
        std::cout << "Installed plugin: " << manifest->name << " (" << manifest->version << ")\n";
        std::cout << "Location: " << dest_path << "\n";
    }

    if (args.core_options.json_output) {
        nlohmann::json j;
        j["success"] = true;
        j["plugin"] = manifest->name;
        j["version"] = manifest->version;
        j["path"] = dest_path.string();
        std::cout << j.dump(2) << std::endl;
    }

    return 0;
}

int PluginCommand::remove(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Usage: uniconv plugin remove <name>\n";
        return 1;
    }

    const auto& name = args.subcommand;

    // Find plugin
    auto plugin_dir = find_plugin_dir(name);
    if (!plugin_dir) {
        std::cerr << "Error: Plugin not found: " << name << "\n";
        return 1;
    }

    // Don't allow removing built-in plugins
    auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
    if (!plugin_dir->string().starts_with(user_plugins.string())) {
        std::cerr << "Error: Cannot remove non-user plugins\n";
        std::cerr << "Plugin location: " << *plugin_dir << "\n";
        return 1;
    }

    // Remove
    try {
        std::filesystem::remove_all(*plugin_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to remove plugin: " << e.what() << "\n";
        return 1;
    }

    if (!args.core_options.quiet) {
        std::cout << "Removed plugin: " << name << "\n";
    }

    if (args.core_options.json_output) {
        nlohmann::json j;
        j["success"] = true;
        j["plugin"] = name;
        std::cout << j.dump(2) << std::endl;
    }

    return 0;
}

int PluginCommand::info(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Usage: uniconv plugin info <name>\n";
        return 1;
    }

    const auto& name = args.subcommand;

    // First check built-in plugins
    auto builtin = plugin_manager_->list_plugins();
    for (const auto& p : builtin) {
        if (p.id == name || p.group == name) {
            if (args.core_options.json_output) {
                std::cout << p.to_json().dump(2) << std::endl;
            } else {
                std::cout << "Name:        " << p.id << "\n";
                std::cout << "Group:       " << p.group << "\n";
                std::cout << "Version:     " << p.version << "\n";
                std::cout << "Description: " << p.description << "\n";
                std::cout << "ETL:         " << core::etl_type_to_string(p.etl) << "\n";
                std::cout << "Type:        built-in\n";
                std::cout << "Targets:     ";
                for (size_t i = 0; i < p.targets.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << p.targets[i];
                }
                std::cout << "\n";
                std::cout << "Inputs:      ";
                for (size_t i = 0; i < p.input_formats.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << p.input_formats[i];
                }
                std::cout << "\n";
            }
            return 0;
        }
    }

    // Find external plugin
    auto plugin_dir = find_plugin_dir(name);
    if (!plugin_dir) {
        std::cerr << "Error: Plugin not found: " << name << "\n";
        return 1;
    }

    auto manifest = discovery_.load_manifest(*plugin_dir);
    if (!manifest) {
        std::cerr << "Error: Could not load manifest for: " << name << "\n";
        return 1;
    }

    if (args.core_options.json_output) {
        auto j = manifest->to_json();
        j["path"] = plugin_dir->string();
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "Name:        " << manifest->name << "\n";
        std::cout << "Group:       " << manifest->group << "\n";
        std::cout << "Version:     " << manifest->version << "\n";
        std::cout << "Description: " << manifest->description << "\n";
        std::cout << "ETL:         " << core::etl_type_to_string(manifest->etl) << "\n";
        std::cout << "Type:        " << core::plugin_interface_to_string(manifest->interface) << "\n";
        std::cout << "Path:        " << *plugin_dir << "\n";

        if (manifest->interface == core::PluginInterface::CLI) {
            std::cout << "Executable:  " << manifest->executable << "\n";
        } else {
            std::cout << "Library:     " << manifest->library << "\n";
        }

        std::cout << "Targets:     ";
        for (size_t i = 0; i < manifest->targets.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << manifest->targets[i];
        }
        std::cout << "\n";

        std::cout << "Inputs:      ";
        for (size_t i = 0; i < manifest->input_formats.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << manifest->input_formats[i];
        }
        std::cout << "\n";

        if (!manifest->options.empty()) {
            std::cout << "\nOptions:\n";
            for (const auto& opt : manifest->options) {
                std::cout << "  " << opt.name;
                if (!opt.type.empty()) std::cout << " (" << opt.type << ")";
                if (!opt.default_value.empty()) std::cout << " [default: " << opt.default_value << "]";
                std::cout << "\n";
                if (!opt.description.empty()) {
                    std::cout << "      " << opt.description << "\n";
                }
            }
        }
    }

    return 0;
}

bool PluginCommand::copy_plugin(const std::filesystem::path& source, const std::filesystem::path& dest) {
    try {
        // Create destination directory
        std::filesystem::create_directories(dest.parent_path());

        // Copy recursively
        std::filesystem::copy(source, dest, std::filesystem::copy_options::recursive);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<std::filesystem::path> PluginCommand::find_plugin_dir(const std::string& name) {
    auto manifests = discovery_.discover_all();

    for (const auto& m : manifests) {
        if (m.name == name || m.group == name || m.id() == name) {
            return m.plugin_dir;
        }
    }

    return std::nullopt;
}

} // namespace uniconv::cli::commands
