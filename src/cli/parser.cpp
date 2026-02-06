#include "parser.h"
#include "pipeline_parser.h"
#include <uniconv/version.h>
#include <sstream>

namespace uniconv::cli
{

    CliParser::CliParser() = default;

    ParsedArgs CliParser::parse(int argc, char **argv)
    {
        ParsedArgs args;

        CLI::App app{"uniconv - Universal Converter & Content Intelligence Tool"};
        app.set_version_flag("-v,--version", UNICONV_VERSION);
        app.set_help_flag("-h,--help", "Show help");
        app.positionals_at_end(true);
        app.footer("\nPipeline Examples:\n"
                   "  uniconv photo.heic \"jpg\"                      # Convert HEIC to JPG\n"
                   "  uniconv photo.heic \"jpg --quality 90\"         # With quality option\n"
                   "  uniconv photo.heic \"jpg | ascii\"              # Multi-stage pipeline\n"
                   "  uniconv -o out.png photo.heic \"png\"           # Specify output path\n"
                   "  uniconv -o ./out -r ./photos \"jpg\"            # Batch convert directory\n"
                   "  uniconv watch ./incoming \"jpg\"                # Watch directory for changes");

        setup_main_options(app, args);
        setup_subcommands(app, args);

        try
        {
            app.parse(argc, argv);

            // Determine command
            args.command = determine_command(app, args);
        }
        catch (const CLI::ParseError &e)
        {
            if (e.get_exit_code() == 0)
            {
                // Help or version was requested — let CLI11 produce the
                // correct output (including subcommand-specific help).
                std::ostringstream oss;
                auto old_buf = std::cout.rdbuf(oss.rdbuf());
                app.exit(e);
                std::cout.rdbuf(old_buf);

                parse_exit_code_ = 0;
                parse_exit_message_ = oss.str();
                args.command = Command::Help;
            }
            else
            {
                throw;
            }
        }

        return args;
    }

    std::string CliParser::help()
    {
        CLI::App app{"uniconv - Universal Converter & Content Intelligence Tool"};
        app.footer("\nPipeline Examples:\n"
                   "  uniconv photo.heic \"jpg\"                      # Convert HEIC to JPG\n"
                   "  uniconv photo.heic \"jpg --quality 90\"         # With quality option\n"
                   "  uniconv photo.heic \"jpg | ascii\"              # Multi-stage pipeline\n"
                   "  uniconv -o out.png photo.heic \"png\"           # Specify output path\n"
                   "  uniconv -o ./out -r ./photos \"jpg\"            # Batch convert directory\n"
                   "  uniconv watch ./incoming \"jpg\"                # Watch directory for changes");
        ParsedArgs dummy;
        setup_main_options(app, dummy);
        setup_subcommands(app, dummy);
        return app.help();
    }

    void CliParser::setup_main_options(CLI::App &app, ParsedArgs &args)
    {
        // Core options
        app.add_option("-o,--output", args.core_options.output, "Output path");

        // Flags
        app.add_flag("-f,--force", args.core_options.force, "Overwrite existing files");
        app.add_flag("--json", args.core_options.json_output, "Output as JSON");
        app.add_flag("--verbose", args.core_options.verbose, "Verbose output");
        app.add_flag("--quiet", args.core_options.quiet, "Suppress output");
        app.add_flag("--dry-run", args.core_options.dry_run, "Show what would be done");
        app.add_flag("-r,--recursive", args.core_options.recursive, "Process directories recursively");

        // Interactive mode
        app.add_flag("--interactive", args.interactive, "Force interactive mode");
        app.add_flag("--no-interactive", args.no_interactive, "Disable interactive mode");

        // Preset
        app.add_option("-p,--preset", args.preset, "Use preset");

        // Positional arguments: <source> "<pipeline>"
        app.add_option("input", args.input, "Input file or directory")
            ->type_name("FILE|DIR");
        app.add_option("pipeline", args.pipeline, "Pipeline transformation stages")
            ->type_name("PIPELINE");
    }

    void CliParser::setup_subcommands(CLI::App &app, ParsedArgs &args)
    {
        // Info command
        auto *info_cmd = app.add_subcommand("info", "Show file information");
        info_cmd->add_option("file", args.subcommand_args, "File to analyze")->required();
        info_cmd->footer("\nExamples:\n"
                         "  uniconv info photo.jpg\n"
                         "  uniconv --json info photo.jpg  # JSON output");
        info_cmd->callback([&args]()
                           { args.command = Command::Info; });

        // Formats command (hidden — not yet implemented)
        auto *formats_cmd = app.add_subcommand("formats", "List supported formats");
        formats_cmd->group("");
        formats_cmd->callback([&args]()
                              { args.command = Command::Formats; });

        // Preset command (hidden — not yet implemented)
        auto *preset_cmd = app.add_subcommand("preset", "Manage presets");
        preset_cmd->group("");
        preset_cmd->require_subcommand(1);

        auto *preset_list = preset_cmd->add_subcommand("list", "List all presets");
        preset_list->callback([&args]()
                              {
        args.command = Command::Preset;
        args.subcommand_args.insert(args.subcommand_args.begin(), "list"); });

        auto *preset_create = preset_cmd->add_subcommand("create", "Create a preset");
        preset_create->add_option("name", args.subcommand, "Preset name")->required();
        preset_create->callback([&args]()
                                {
        args.command = Command::Preset;
        args.subcommand_args.insert(args.subcommand_args.begin(), "create"); });

        auto *preset_delete = preset_cmd->add_subcommand("delete", "Delete a preset");
        preset_delete->add_option("name", args.subcommand, "Preset name")->required();
        preset_delete->callback([&args]()
                                {
        args.command = Command::Preset;
        args.subcommand_args.insert(args.subcommand_args.begin(), "delete"); });

        auto *preset_show = preset_cmd->add_subcommand("show", "Show preset details");
        preset_show->add_option("name", args.subcommand, "Preset name")->required();
        preset_show->callback([&args]()
                              {
        args.command = Command::Preset;
        args.subcommand_args.insert(args.subcommand_args.begin(), "show"); });

        auto *preset_export = preset_cmd->add_subcommand("export", "Export preset to file");
        preset_export->add_option("name", args.subcommand, "Preset name")->required();
        preset_export->add_option("file", args.subcommand_args, "Output file")->required();
        preset_export->callback([&args]()
                                {
        args.command = Command::Preset;
        args.subcommand_args.insert(args.subcommand_args.begin(), "export"); });

        auto *preset_import = preset_cmd->add_subcommand("import", "Import preset from file");
        preset_import->add_option("file", args.subcommand_args, "Input file")->required();
        preset_import->callback([&args]()
                                {
        args.command = Command::Preset;
        args.subcommand_args.insert(args.subcommand_args.begin(), "import"); });

        // Plugin command (manage)
        auto *plugin_cmd = app.add_subcommand("plugin", "Manage plugins");
        plugin_cmd->require_subcommand(1);
        plugin_cmd->footer("\nExamples:\n"
                           "  uniconv plugin list              # List installed plugins\n"
                           "  uniconv plugin install image-convert  # Install from registry\n"
                           "  uniconv plugin remove image-convert   # Remove a plugin");

        auto *plugin_list = plugin_cmd->add_subcommand("list", "List installed plugins");
        plugin_list->add_flag("--registry", args.list_registry, "List all plugins available in the registry");
        plugin_list->footer("\nExamples:\n"
                            "  uniconv plugin list              # List installed plugins\n"
                            "  uniconv plugin list --registry   # List all available plugins");
        plugin_list->callback([&args]()
                              {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "list"); });

        auto *plugin_install = plugin_cmd->add_subcommand("install", "Install a plugin");
        plugin_install->add_option("source", args.subcommand_args, "Plugin name[@version] or local path")->required();
        plugin_install->footer("\nExamples:\n"
                               "  uniconv plugin install image-convert        # Install from registry\n"
                               "  uniconv plugin install image-convert@1.0.0  # Install specific version\n"
                               "  uniconv plugin install ./my-plugin          # Install from local path\n"
                               "  uniconv plugin install +core                # Install plugin collection");
        plugin_install->callback([&args]()
                                 {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "install"); });

        auto *plugin_remove = plugin_cmd->add_subcommand("remove", "Remove a plugin");
        plugin_remove->add_option("name", args.subcommand, "Plugin name")->required();
        plugin_remove->footer("\nExamples:\n"
                              "  uniconv plugin remove image-convert");
        plugin_remove->callback([&args]()
                                {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "remove"); });

        auto *plugin_info = plugin_cmd->add_subcommand("info", "Show plugin information");
        plugin_info->add_option("name", args.subcommand, "Plugin name")->required();
        plugin_info->footer("\nExamples:\n"
                            "  uniconv plugin info image-convert");
        plugin_info->callback([&args]()
                              {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "info"); });

        auto *plugin_search = plugin_cmd->add_subcommand("search", "Search plugin registry");
        plugin_search->add_option("query", args.subcommand, "Search query")->required();
        plugin_search->footer("\nExamples:\n"
                              "  uniconv plugin search image\n"
                              "  uniconv plugin search video");
        plugin_search->callback([&args]()
                                {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "search"); });

        auto *plugin_update = plugin_cmd->add_subcommand("update", "Update plugin(s)");
        plugin_update->add_option("name", args.subcommand, "Plugin name (optional, updates all if omitted)");
        plugin_update->footer("\nExamples:\n"
                              "  uniconv plugin update                # Update all plugins\n"
                              "  uniconv plugin update image-convert  # Update specific plugin");
        plugin_update->callback([&args]()
                                {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "update"); });

        // Update command (self-update)
        auto *update_cmd = app.add_subcommand("update", "Update uniconv to latest version");
        update_cmd->add_flag("--check", args.update_check_only, "Only check for updates, don't install");
        update_cmd->footer("\nExamples:\n"
                           "  uniconv update          # Update to latest version\n"
                           "  uniconv update --check  # Check for updates without installing");
        update_cmd->callback([&args]()
                             { args.command = Command::Update; });

        // Watch command
        auto *watch_cmd = app.add_subcommand("watch", "Watch directory for changes and process new files");
        watch_cmd->add_option("-o,--output", args.core_options.output, "Output directory");
        watch_cmd->add_flag("-f,--force", args.core_options.force, "Overwrite existing files");
        watch_cmd->add_flag("-r,--recursive", args.core_options.recursive, "Watch directories recursively");
        watch_cmd->add_flag("--json", args.core_options.json_output, "Output as JSON");
        watch_cmd->add_flag("--verbose", args.core_options.verbose, "Verbose output");
        watch_cmd->add_flag("--quiet", args.core_options.quiet, "Suppress output");
        watch_cmd->add_flag("--dry-run", args.core_options.dry_run, "Show what would be done");
        watch_cmd->add_option("directory", args.watch_dir, "Directory to watch")->required()->type_name("DIR");
        watch_cmd->add_option("pipeline", args.pipeline, "Pipeline transformation stages")->required()->type_name("PIPELINE");
        watch_cmd->footer("\nExamples:\n"
                          "  uniconv watch ./incoming \"jpg\"              # Watch and convert to JPG\n"
                          "  uniconv watch ./incoming \"jpg | png\"        # Multi-stage pipeline\n"
                          "  uniconv watch -o ./output ./incoming \"jpg\"  # With output directory\n"
                          "  uniconv watch -r ./incoming \"jpg\"           # Watch recursively");
        watch_cmd->callback([&args]()
                            { args.command = Command::Watch; });

        // Config command (hidden — not yet implemented)
        auto *config_cmd = app.add_subcommand("config", "Manage configuration");
        config_cmd->group("");
        config_cmd->require_subcommand(1);

        auto *config_get = config_cmd->add_subcommand("get", "Get config value");
        config_get->add_option("key", args.subcommand, "Config key")->required();
        config_get->callback([&args]()
                             {
        args.command = Command::Config;
        args.subcommand_args.insert(args.subcommand_args.begin(), "get"); });

        auto *config_set = config_cmd->add_subcommand("set", "Set config value");
        config_set->add_option("key", args.subcommand, "Config key")->required();
        config_set->add_option("value", args.subcommand_args, "Config value")->required();
        config_set->callback([&args]()
                             {
        args.command = Command::Config;
        args.subcommand_args.insert(args.subcommand_args.begin(), "set"); });

        auto *config_list = config_cmd->add_subcommand("list", "List all config");
        config_list->callback([&args]()
                              {
        args.command = Command::Config;
        args.subcommand_args.insert(args.subcommand_args.begin(), "list"); });
    }

    Command CliParser::determine_command([[maybe_unused]] const CLI::App &app, const ParsedArgs &args)
    {
        // If a subcommand was invoked, its callback already set the command
        if (args.command != Command::Help)
        {
            return args.command;
        }

        // Pipeline command: have input and pipeline string
        if (args.input.has_value() && !args.pipeline.empty())
        {
            return Command::Pipeline;
        }

        // Preset usage with input
        if (args.preset.has_value() && args.input.has_value())
        {
            return Command::Pipeline;
        }

        // If input provided but no pipeline, enter interactive mode
        if (args.input.has_value() && !args.no_interactive)
        {
            return Command::Interactive;
        }

        // Default to help
        return Command::Help;
    }

} // namespace uniconv::cli
