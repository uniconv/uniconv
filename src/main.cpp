#include "cli/parser.h"
#include "cli/commands/info_command.h"
#include "cli/commands/formats_command.h"
#include "cli/commands/preset_command.h"
#include "cli/commands/etl_command.h"
#include "cli/commands/plugin_command.h"
#include "cli/commands/config_command.h"
#include "core/engine.h"
#include "core/preset_manager.h"
#include "core/config_manager.h"
#include <uniconv/version.h>
#include <iostream>
#include <memory>

using namespace uniconv;

int main(int argc, char** argv) {
    try {
        // Initialize core components
        auto config_manager = std::make_shared<core::ConfigManager>();
        config_manager->load();  // Load config if exists

        // Create engine and load external plugins
        auto engine = std::make_shared<core::Engine>();
        engine->plugin_manager().load_external_plugins();

        auto preset_manager = std::make_shared<core::PresetManager>();

        // Parse command line
        cli::CliParser parser;
        cli::ParsedArgs args;

        try {
            args = parser.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }

        // Route to appropriate command handler
        switch (args.command) {
            case cli::Command::Help:
                std::cout << parser.help() << std::endl;
                return 0;

            case cli::Command::Version:
                std::cout << "uniconv " << UNICONV_VERSION << std::endl;
                return 0;

            case cli::Command::Info: {
                cli::commands::InfoCommand cmd(engine);
                return cmd.execute(args);
            }

            case cli::Command::Formats: {
                auto pm = std::make_shared<core::PluginManager>();
                cli::commands::FormatsCommand cmd(pm);
                return cmd.execute(args);
            }

            case cli::Command::Presets: {
                cli::commands::PresetCommand cmd(preset_manager);
                return cmd.list(args);
            }

            case cli::Command::Preset: {
                cli::commands::PresetCommand cmd(preset_manager);
                return cmd.execute(args);
            }

            case cli::Command::Plugins: {
                cli::commands::PluginCommand cmd(
                    std::make_shared<core::PluginManager>(),
                    config_manager
                );
                return cmd.list(args);
            }

            case cli::Command::Plugin: {
                cli::commands::PluginCommand cmd(
                    std::make_shared<core::PluginManager>(),
                    config_manager
                );
                return cmd.execute(args);
            }

            case cli::Command::Config: {
                cli::commands::ConfigCommand cmd(config_manager);
                return cmd.execute(args);
            }

            case cli::Command::ETL: {
                cli::commands::ETLCommand cmd(engine, preset_manager);
                return cmd.execute(args);
            }

            case cli::Command::Interactive:
                // Interactive mode - placeholder for future
                std::cerr << "Interactive mode not yet implemented\n";
                std::cerr << "Use explicit ETL flags: -t (transform), -e (extract), -l (load)\n";
                return 1;

            default:
                std::cout << parser.help() << std::endl;
                return 0;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
