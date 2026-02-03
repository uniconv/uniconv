#include "preset_command.h"
#include "utils/json_output.h"
#include <iostream>

namespace uniconv::cli::commands
{

    PresetCommand::PresetCommand(std::shared_ptr<core::PresetManager> preset_manager)
        : preset_manager_(std::move(preset_manager))
    {
    }

    int PresetCommand::execute(const ParsedArgs &args)
    {
        if (args.subcommand_args.empty())
        {
            std::cerr << "Error: No subcommand specified\n";
            return 1;
        }

        const auto &subcmd = args.subcommand_args[0];

        if (subcmd == "list")
            return list(args);
        if (subcmd == "create")
            return create_preset(args);
        if (subcmd == "delete")
            return delete_preset(args);
        if (subcmd == "show")
            return show_preset(args);
        if (subcmd == "export")
            return export_preset(args);
        if (subcmd == "import")
            return import_preset(args);

        std::cerr << "Error: Unknown preset subcommand: " << subcmd << "\n";
        return 1;
    }

    int PresetCommand::list(const ParsedArgs &args)
    {
        auto presets = preset_manager_->list();

        if (args.core_options.json_output)
        {
            nlohmann::json j = nlohmann::json::array();
            for (const auto &p : presets)
            {
                j.push_back(p.to_json());
            }
            std::cout << j.dump(2) << std::endl;
        }
        else
        {
            if (presets.empty())
            {
                std::cout << "No presets found.\n";
                std::cout << "Create one with: uniconv preset create <name> -t <format> [options]\n";
            }
            else
            {
                std::cout << "Available presets:\n";
                for (const auto &p : presets)
                {
                    std::cout << "  " << p.name;
                    if (!p.description.empty())
                    {
                        std::cout << " - " << p.description;
                    }
                    std::cout << " (-> " << p.target << ")\n";
                }
            }
        }

        return 0;
    }

    int PresetCommand::create_preset(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Error: Preset name required\n";
            return 1;
        }

        // TODO: Implement preset creation from pipeline string
        // For now, presets should be created by specifying a pipeline string
        // Example: uniconv preset create instagram "jpg --quality 85 --width 1080"
        std::cerr << "Error: Preset creation not yet implemented for pipeline syntax\n";
        std::cerr << "Usage: uniconv preset create <name> \"<pipeline>\"\n";
        return 1;
    }

    int PresetCommand::delete_preset(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Error: Preset name required\n";
            return 1;
        }

        if (preset_manager_->remove(args.subcommand))
        {
            if (!args.core_options.quiet)
            {
                std::cout << "Deleted preset: " << args.subcommand << "\n";
            }
            return 0;
        }
        else
        {
            std::cerr << "Error: Preset not found: " << args.subcommand << "\n";
            return 1;
        }
    }

    int PresetCommand::show_preset(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Error: Preset name required\n";
            return 1;
        }

        auto preset = preset_manager_->load(args.subcommand);
        if (!preset)
        {
            std::cerr << "Error: Preset not found: " << args.subcommand << "\n";
            return 1;
        }

        if (args.core_options.json_output)
        {
            std::cout << preset->to_json().dump(2) << std::endl;
        }
        else
        {
            std::cout << "Name: " << preset->name << "\n";
            if (!preset->description.empty())
            {
                std::cout << "Description: " << preset->description << "\n";
            }
            std::cout << "Target: " << preset->target << "\n";
            if (preset->plugin)
            {
                std::cout << "Plugin: " << *preset->plugin << "\n";
            }
            if (!preset->plugin_options.empty())
            {
                std::cout << "Options: ";
                for (size_t i = 0; i < preset->plugin_options.size(); ++i)
                {
                    if (i > 0)
                        std::cout << " ";
                    std::cout << preset->plugin_options[i];
                }
                std::cout << "\n";
            }
        }

        return 0;
    }

    int PresetCommand::export_preset(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Error: Preset name required\n";
            return 1;
        }

        if (args.subcommand_args.size() < 2)
        {
            std::cerr << "Error: Output file required\n";
            return 1;
        }

        try
        {
            preset_manager_->export_preset(args.subcommand, args.subcommand_args[1]);
            if (!args.core_options.quiet)
            {
                std::cout << "Exported preset to: " << args.subcommand_args[1] << "\n";
            }
            return 0;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    int PresetCommand::import_preset(const ParsedArgs &args)
    {
        if (args.subcommand_args.size() < 2)
        {
            std::cerr << "Error: Input file required\n";
            return 1;
        }

        try
        {
            preset_manager_->import_preset(args.subcommand_args[1]);
            if (!args.core_options.quiet)
            {
                std::cout << "Imported preset from: " << args.subcommand_args[1] << "\n";
            }
            return 0;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

} // namespace uniconv::cli::commands
