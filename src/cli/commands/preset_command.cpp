#include "preset_command.h"
#include "utils/json_output.h"
#include <iostream>

namespace uniconv::cli::commands {

PresetCommand::PresetCommand(std::shared_ptr<core::PresetManager> preset_manager)
    : preset_manager_(std::move(preset_manager)) {
}

int PresetCommand::execute(const ParsedArgs& args) {
    if (args.subcommand_args.empty()) {
        std::cerr << "Error: No subcommand specified\n";
        return 1;
    }

    const auto& subcmd = args.subcommand_args[0];

    if (subcmd == "create") return create_preset(args);
    if (subcmd == "delete") return delete_preset(args);
    if (subcmd == "show") return show_preset(args);
    if (subcmd == "export") return export_preset(args);
    if (subcmd == "import") return import_preset(args);

    std::cerr << "Error: Unknown preset subcommand: " << subcmd << "\n";
    return 1;
}

int PresetCommand::list(const ParsedArgs& args) {
    auto presets = preset_manager_->list();

    if (args.core_options.json_output) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& p : presets) {
            j.push_back(p.to_json());
        }
        std::cout << j.dump(2) << std::endl;
    } else {
        if (presets.empty()) {
            std::cout << "No presets found.\n";
            std::cout << "Create one with: uniconv preset create <name> -t <format> [options]\n";
        } else {
            std::cout << "Available presets:\n";
            for (const auto& p : presets) {
                std::cout << "  " << p.name;
                if (!p.description.empty()) {
                    std::cout << " - " << p.description;
                }
                std::cout << " (" << core::etl_type_to_string(p.etl) << " -> " << p.target << ")\n";
            }
        }
    }

    return 0;
}

int PresetCommand::create_preset(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Error: Preset name required\n";
        return 1;
    }

    if (!args.etl.has_value()) {
        std::cerr << "Error: ETL operation required (-t, -e, or -l)\n";
        return 1;
    }

    core::Preset preset;
    preset.name = args.subcommand;
    preset.etl = *args.etl;
    preset.target = args.target;
    preset.plugin = args.plugin;
    preset.core_options = args.core_options;
    preset.plugin_options = args.plugin_options;

    try {
        preset_manager_->create(preset);
        if (!args.core_options.quiet) {
            std::cout << "Created preset: " << preset.name << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int PresetCommand::delete_preset(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Error: Preset name required\n";
        return 1;
    }

    if (preset_manager_->remove(args.subcommand)) {
        if (!args.core_options.quiet) {
            std::cout << "Deleted preset: " << args.subcommand << "\n";
        }
        return 0;
    } else {
        std::cerr << "Error: Preset not found: " << args.subcommand << "\n";
        return 1;
    }
}

int PresetCommand::show_preset(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Error: Preset name required\n";
        return 1;
    }

    auto preset = preset_manager_->load(args.subcommand);
    if (!preset) {
        std::cerr << "Error: Preset not found: " << args.subcommand << "\n";
        return 1;
    }

    if (args.core_options.json_output) {
        std::cout << preset->to_json().dump(2) << std::endl;
    } else {
        std::cout << "Name: " << preset->name << "\n";
        if (!preset->description.empty()) {
            std::cout << "Description: " << preset->description << "\n";
        }
        std::cout << "ETL: " << core::etl_type_to_string(preset->etl) << "\n";
        std::cout << "Target: " << preset->target << "\n";
        if (preset->plugin) {
            std::cout << "Plugin: " << *preset->plugin << "\n";
        }
        if (preset->core_options.quality) {
            std::cout << "Quality: " << *preset->core_options.quality << "\n";
        }
        if (preset->core_options.width) {
            std::cout << "Width: " << *preset->core_options.width << "\n";
        }
        if (preset->core_options.height) {
            std::cout << "Height: " << *preset->core_options.height << "\n";
        }
    }

    return 0;
}

int PresetCommand::export_preset(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Error: Preset name required\n";
        return 1;
    }

    if (args.subcommand_args.size() < 2) {
        std::cerr << "Error: Output file required\n";
        return 1;
    }

    try {
        preset_manager_->export_preset(args.subcommand, args.subcommand_args[1]);
        if (!args.core_options.quiet) {
            std::cout << "Exported preset to: " << args.subcommand_args[1] << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int PresetCommand::import_preset(const ParsedArgs& args) {
    if (args.subcommand_args.size() < 2) {
        std::cerr << "Error: Input file required\n";
        return 1;
    }

    try {
        preset_manager_->import_preset(args.subcommand_args[1]);
        if (!args.core_options.quiet) {
            std::cout << "Imported preset from: " << args.subcommand_args[1] << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

} // namespace uniconv::cli::commands
