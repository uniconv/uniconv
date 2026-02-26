#include "cli/parser.h"
#include "cli/commands/info_command.h"
#include "cli/commands/formats_command.h"
#include "cli/commands/preset_command.h"
#include "cli/commands/plugin_command.h"
#include "cli/commands/config_command.h"
#include "cli/commands/update_command.h"
#include "cli/commands/detect_command.h"
#include "cli/pipeline_parser.h"
#include "core/engine.h"
#include "core/preset_manager.h"
#include "core/config_manager.h"
#include "core/pipeline_executor.h"
#include "core/watcher.h"
#include "core/output/output.h"
#include "core/output/console_output.h"
#include "core/output/json_output.h"
#include "builtins/clipboard.h"
#include "utils/mime_detector.h"
#include <uniconv/version.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

using namespace uniconv;

// Global watcher pointer for signal handling
static core::Watcher *g_watcher = nullptr;

void signal_handler(int)
{
    if (g_watcher)
    {
        g_watcher->stop();
    }
}

std::shared_ptr<core::output::IOutput> create_output(const cli::ParsedArgs& args) {
    if (args.core_options.json_output) {
        return std::make_shared<core::output::JsonOutput>(
            std::cout, std::cerr, args.core_options.verbose, args.core_options.quiet);
    }
    return std::make_shared<core::output::ConsoleOutput>(
        std::cout, std::cerr, args.core_options.verbose, args.core_options.quiet);
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    // Enable ANSI escape sequences on Windows 10+
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (hErr != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hErr, &mode)) {
                SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
        SetConsoleOutputCP(CP_UTF8);
    }
#endif

    try
    {
        // Initialize core components
        auto config_manager = std::make_shared<core::ConfigManager>();
        config_manager->load(); // Load config if exists

        // Create engine (plugins loaded on-demand when needed)
        auto engine = std::make_shared<core::Engine>();

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
            auto output = create_output(args);
            output->error(e.what());
            output->info("Run 'uniconv --help' for usage information");
            return 1;
        }

        // Create output based on args
        auto output = create_output(args);

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
            cli::commands::InfoCommand cmd(engine, output);
            return cmd.execute(args);
        }

        case cli::Command::Formats:
        {
            auto pm = std::make_shared<core::PluginManager>();
            cli::commands::FormatsCommand cmd(pm, output);
            return cmd.execute(args);
        }

        case cli::Command::Preset:
        {
            cli::commands::PresetCommand cmd(preset_manager, output);
            return cmd.execute(args);
        }

        case cli::Command::Plugin:
        {
            cli::commands::PluginCommand cmd(
                std::make_shared<core::PluginManager>(),
                config_manager,
                output);
            return cmd.execute(args);
        }

        case cli::Command::Config:
        {
            cli::commands::ConfigCommand cmd(config_manager, output);
            return cmd.execute(args);
        }

        case cli::Command::Update:
        {
            cli::commands::UpdateCommand cmd(output);
            return cmd.execute(args);
        }

        case cli::Command::Detect:
        {
            cli::commands::DetectCommand cmd(output);
            return cmd.execute(args);
        }

        case cli::Command::Watch:
        {
            cli::PipelineParser pipeline_parser;
            std::filesystem::path watch_dir = args.watch_dir;
            std::string pipeline_str = args.pipeline;

            if (!std::filesystem::is_directory(watch_dir))
            {
                output->error("Watch directory does not exist or is not a directory: " + watch_dir.string());
                return 1;
            }

            output->info("Watching directory: " + watch_dir.string());
            output->info("Pipeline: " + pipeline_str);
            output->info("Press Ctrl+C to stop");

            core::Watcher watcher;
            g_watcher = &watcher;

            // Set up signal handlers
            std::signal(SIGINT, signal_handler);
            std::signal(SIGTERM, signal_handler);

            watcher.set_callback([&](const std::filesystem::path &file_path, core::FileEvent event)
                                 {
                std::string event_str = (event == core::FileEvent::Created) ? "New" : "Modified";
                output->info(event_str + " file: " + file_path.filename().string());

                // Parse and execute pipeline for this file
                auto file_parse_result = pipeline_parser.parse(pipeline_str, file_path, args.core_options);
                if (!file_parse_result.success)
                {
                    output->error("  Pipeline error: " + file_parse_result.error);
                    return;
                }

                core::PipelineExecutor executor(engine);
                auto result = executor.execute(file_parse_result.pipeline, output);

                if (result.success)
                {
                    output->success("  Completed");
                    for (const auto &out : result.final_outputs)
                    {
                        output->info("    -> " + out.string());
                    }
                }
                else if (result.error)
                {
                    output->error("  Failed: " + *result.error);
                } });

            watcher.watch(watch_dir, args.core_options.recursive);

            g_watcher = nullptr;
            output->info("Watch mode stopped");
            return 0;
        }

        case cli::Command::Pipeline:
        {
            cli::PipelineParser pipeline_parser;

            // Source comes from args.input
            std::filesystem::path source = args.input.value_or("");
            std::string pipeline_str = args.pipeline;
            std::filesystem::path stdin_temp_file; // track for cleanup

            // Validate --from-clipboard usage
            if (args.from_clipboard && (!args.input.has_value() || *args.input != "-"))
            {
                output->error("--from-clipboard requires '-' as the input source");
                return 1;
            }

            if (source == "-")
            {
                if (args.from_clipboard)
                {
                    // Clipboard input mode
                    auto temp_dir = std::filesystem::temp_directory_path() / "uniconv";
                    auto clip_result = builtins::Clipboard::read_to_file(temp_dir, args.input_format);
                    if (!clip_result.success)
                    {
                        output->error(clip_result.error);
                        return 1;
                    }
                    stdin_temp_file = clip_result.file;
                    source = stdin_temp_file;
                    if (!args.input_format.has_value())
                        args.input_format = clip_result.detected_format;
                }
                else if (!ISATTY(FILENO(stdin)))
                {
                    // Piped data -> stdin mode: read into memory then materialize
                    std::string stdin_data(
                        (std::istreambuf_iterator<char>(std::cin)),
                        std::istreambuf_iterator<char>());

                    // Auto-detect format via libmagic (--input-format overrides)
                    std::string ext;
                    if (args.input_format.has_value())
                    {
                        ext = *args.input_format;
                    }
                    else
                    {
                        utils::MimeDetector detector;
                        ext = detector.detect_extension(stdin_data.data(), stdin_data.size());
                    }

                    auto temp_dir = std::filesystem::temp_directory_path() / "uniconv";
                    std::filesystem::create_directories(temp_dir);
                    stdin_temp_file = temp_dir / ("stdin." + ext);

                    std::ofstream ofs(stdin_temp_file, std::ios::binary);
                    ofs.write(stdin_data.data(), static_cast<std::streamsize>(stdin_data.size()));
                    ofs.close();

                    source = stdin_temp_file;
                }
                else
                {
                    // TTY -> generator mode: no input file
                    source = "";
                }
            }
            else if (source.empty())
            {
                output->error("No input file or directory specified");
                output->info("Usage: uniconv <source> \"<pipeline>\"");
                return 1;
            }

            if (pipeline_str.empty())
            {
                output->error("No pipeline specified");
                output->info("Usage: uniconv <source> \"<pipeline>\"");
                return 1;
            }

            // Pipeline execution
            auto parse_result = pipeline_parser.parse(pipeline_str, source, args.core_options);
            if (!parse_result.success)
            {
                output->error("Pipeline error: " + parse_result.error);
                return 1;
            }

            // Thread input_format from CLI into pipeline
            if (args.input_format.has_value())
                parse_result.pipeline.input_format = *args.input_format;

            core::PipelineExecutor executor(engine);
            auto result = executor.execute(parse_result.pipeline, output);

            // Cleanup stdin temp file
            if (!stdin_temp_file.empty())
            {
                std::error_code ec;
                std::filesystem::remove(stdin_temp_file, ec);
            }

            if (args.core_options.json_output)
            {
                output->data(result.to_json());
            }
            else
            {
                // Print any warnings
                for (const auto &warning : result.warnings)
                {
                    output->warning(warning);
                }

                if (result.success)
                {
                    output->success("Pipeline completed successfully");
                    for (const auto &out : result.final_outputs)
                    {
                        output->info("  -> " + out.string());
                    }
                }
                else if (result.error)
                {
                    output->error(*result.error);
                }
            }
            return result.success ? 0 : 1;
        }

        case cli::Command::Interactive:
            // Interactive mode - placeholder for future
            output->error("Interactive mode not yet implemented");
            output->info("Use pipeline syntax: uniconv <source> | <target>");
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
