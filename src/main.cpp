#include "cli/parser.h"
#include "cli/commands/info_command.h"
#include "cli/commands/formats_command.h"
#include "cli/commands/preset_command.h"
#include "cli/commands/plugin_command.h"
#include "cli/commands/config_command.h"
#include "cli/commands/update_command.h"
#include "cli/pipeline_parser.h"
#include "core/engine.h"
#include "core/preset_manager.h"
#include "core/config_manager.h"
#include "core/pipeline_executor.h"
#include <uniconv/version.h>
#include <iostream>
#include <memory>

using namespace uniconv;

int main(int argc, char **argv)
{
    try
    {
        // Initialize core components
        auto config_manager = std::make_shared<core::ConfigManager>();
        config_manager->load(); // Load config if exists

        // Create engine and load external plugins
        auto engine = std::make_shared<core::Engine>();
        engine->plugin_manager().load_external_plugins();

        auto preset_manager = std::make_shared<core::PresetManager>();

        // Parse command line
        cli::CliParser parser;
        cli::ParsedArgs args;

        try
        {
            args = parser.parse(argc, argv);
        }
        catch (const CLI::ParseError &e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }

        // Route to appropriate command handler
        switch (args.command)
        {
        case cli::Command::Help:
            if (parser.parse_exit_code() >= 0)
            {
                // CLI11 produced subcommand-specific help or version text
                std::cout << parser.parse_exit_message();
            }
            else
            {
                // No args at all — show top-level help
                std::cout << parser.help() << std::endl;
            }
            return 0;

        case cli::Command::Version:
            std::cout << "uniconv " << UNICONV_VERSION << std::endl;
            return 0;

        case cli::Command::Info:
        {
            cli::commands::InfoCommand cmd(engine);
            return cmd.execute(args);
        }

        case cli::Command::Formats:
        {
            auto pm = std::make_shared<core::PluginManager>();
            cli::commands::FormatsCommand cmd(pm);
            return cmd.execute(args);
        }

        case cli::Command::Preset:
        {
            cli::commands::PresetCommand cmd(preset_manager);
            return cmd.execute(args);
        }

        case cli::Command::Plugin:
        {
            cli::commands::PluginCommand cmd(
                std::make_shared<core::PluginManager>(),
                config_manager);
            return cmd.execute(args);
        }

        case cli::Command::Config:
        {
            cli::commands::ConfigCommand cmd(config_manager);
            return cmd.execute(args);
        }

        case cli::Command::Update:
        {
            cli::commands::UpdateCommand cmd;
            return cmd.execute(args);
        }

        case cli::Command::Pipeline:
        {
            cli::PipelineParser pipeline_parser;
            // Join all sources with spaces to get the full pipeline string
            std::string full_pipeline;
            for (size_t i = 0; i < args.sources.size(); ++i)
            {
                if (i > 0)
                    full_pipeline += " ";
                full_pipeline += args.sources[i];
            }

            // Extract source (before first |) and pipeline (after first |)
            std::filesystem::path source;
            std::string pipeline_str;
            auto pipe_pos = full_pipeline.find('|');
            if (pipe_pos != std::string::npos)
            {
                // Source is everything before |, trimmed
                std::string source_str = full_pipeline.substr(0, pipe_pos);
                // Trim whitespace
                size_t start = source_str.find_first_not_of(" \t");
                size_t end = source_str.find_last_not_of(" \t");
                if (start != std::string::npos)
                {
                    source = source_str.substr(start, end - start + 1);
                }
                // Pipeline is everything after |
                pipeline_str = full_pipeline.substr(pipe_pos + 1);
            }
            else
            {
                // No pipe found - treat entire thing as source with empty pipeline
                source = full_pipeline;
                pipeline_str = "";
            }

            if (source.empty())
            {
                std::cerr << "Pipeline error: No source file specified\n";
                return 1;
            }

            if (pipeline_str.empty())
            {
                std::cerr << "Pipeline error: No pipeline targets specified after '|'\n";
                return 1;
            }

            auto parse_result = pipeline_parser.parse(pipeline_str, source, args.core_options);
            if (!parse_result.success)
            {
                std::cerr << "Pipeline error: " << parse_result.error << "\n";
                return 1;
            }

            core::PipelineExecutor executor(engine);
            auto result = executor.execute(parse_result.pipeline);

            if (args.core_options.json_output)
            {
                std::cout << result.to_json().dump(2) << std::endl;
            }
            else
            {
                // Print human-readable output
                for (const auto &sr : result.stage_results)
                {
                    std::cout << "Stage " << sr.stage_index << ": " << sr.target;
                    if (sr.status == core::ResultStatus::Success)
                    {
                        std::cout << " OK";
                    }
                    else
                    {
                        std::cout << " FAIL";
                        if (sr.error)
                            std::cout << " - " << *sr.error;
                    }
                    std::cout << "\n";
                }
                if (result.success)
                {
                    std::cout << "\nPipeline completed successfully\n";
                    for (const auto &out : result.final_outputs)
                    {
                        std::cout << "  -> " << out.string() << "\n";
                    }
                }
            }
            return result.success ? 0 : 1;
        }

        case cli::Command::Interactive:
            // Interactive mode - placeholder for future
            std::cerr << "Interactive mode not yet implemented\n";
            std::cerr << "Use pipeline syntax: uniconv <source> | <target>\n";
            return 1;

        default:
            std::cout << parser.help() << std::endl;
            return 0;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
