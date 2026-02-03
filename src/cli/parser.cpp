#include "parser.h"
#include "pipeline_parser.h"
#include <uniconv/version.h>

namespace uniconv::cli
{

    CliParser::CliParser() = default;

    ParsedArgs CliParser::parse(int argc, char **argv)
    {
        ParsedArgs args;

        CLI::App app{"uniconv - Universal Converter & Content Intelligence Tool"};
        app.usage("uniconv [OPTIONS] \"<source> | <target> [target-options] | ...\"\n"
                  "       uniconv [OPTIONS] <subcommand>\n\n"
                  "Pipeline Examples:\n"
                  "  uniconv \"photo.heic | jpg\"                    # Convert HEIC to JPG\n"
                  "  uniconv \"photo.heic | jpg --quality 90\"       # With quality option\n"
                  "  uniconv \"photo.heic | jpg | tee | gdrive, s3\" # Multi-stage with branching\n"
                  "  uniconv -o out.png \"photo.jpg | png\"          # Specify output path");
        app.set_version_flag("-v,--version", UNICONV_VERSION);
        app.set_help_flag("-h,--help", "Show help");
        app.allow_extras(true); // Allow remaining args for pipeline

        setup_main_options(app, args);
        setup_subcommands(app, args);

        try
        {
            app.parse(argc, argv);

            // Handle remaining args as sources (for pipeline)
            auto remaining = app.remaining();
            for (const auto &r : remaining)
            {
                args.sources.push_back(r);
            }

            // Determine command
            args.command = determine_command(app, args);
        }
        catch (const CLI::ParseError &e)
        {
            if (e.get_exit_code() == 0)
            {
                // Help or version was requested
                if (app.get_help_ptr()->count() > 0)
                {
                    args.command = Command::Help;
                }
                else
                {
                    args.command = Command::Version;
                }
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
        app.usage("uniconv [OPTIONS] \"<source> | <target> [target-options] | ...\"\n"
                  "uniconv [OPTIONS] <subcommand>\n\n"
                  "Pipeline Examples:\n"
                  "  uniconv \"photo.heic | jpg\"                    # Convert HEIC to JPG\n"
                  "  uniconv \"photo.heic | jpg --quality 90\"       # With quality option\n"
                  "  uniconv \"photo.heic | jpg | tee | gdrive, s3\" # Multi-stage with branching\n"
                  "  uniconv -o out.png \"photo.jpg | png\"          # Specify output path");
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
        app.add_flag("-i,--interactive", args.interactive, "Force interactive mode");
        app.add_flag("--no-interactive", args.no_interactive, "Disable interactive mode");

        // Watch mode
        app.add_flag("--watch", args.watch, "Watch directory for changes");

        // Preset
        app.add_option("-p,--preset", args.preset, "Use preset");
    }

    void CliParser::setup_subcommands(CLI::App &app, ParsedArgs &args)
    {
        // Info command
        auto *info_cmd = app.add_subcommand("info", "Show file information");
        info_cmd->add_option("file", args.subcommand_args, "File to analyze")->required();
        info_cmd->callback([&args]()
                           { args.command = Command::Info; });

        // Formats command
        auto *formats_cmd = app.add_subcommand("formats", "List supported formats");
        formats_cmd->callback([&args]()
                              { args.command = Command::Formats; });

        // Preset command (manage)
        auto *preset_cmd = app.add_subcommand("preset", "Manage presets");
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

        auto *plugin_list = plugin_cmd->add_subcommand("list", "List installed plugins");
        plugin_list->callback([&args]()
                              {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "list"); });

        auto *plugin_install = plugin_cmd->add_subcommand("install", "Install a plugin");
        plugin_install->add_option("path", args.subcommand_args, "Plugin path or URL")->required();
        plugin_install->callback([&args]()
                                 {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "install"); });

        auto *plugin_remove = plugin_cmd->add_subcommand("remove", "Remove a plugin");
        plugin_remove->add_option("name", args.subcommand, "Plugin name")->required();
        plugin_remove->callback([&args]()
                                {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "remove"); });

        auto *plugin_info = plugin_cmd->add_subcommand("info", "Show plugin information");
        plugin_info->add_option("name", args.subcommand, "Plugin name")->required();
        plugin_info->callback([&args]()
                              {
        args.command = Command::Plugin;
        args.subcommand_args.insert(args.subcommand_args.begin(), "info"); });

        // Config command
        auto *config_cmd = app.add_subcommand("config", "Manage configuration");
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

        // Check for pipeline syntax
        // Pipeline syntax: sources contain " | " (pipe operator)
        if (!args.sources.empty() && PipelineParser::is_pipeline_syntax(args.sources))
        {
            return Command::Pipeline;
        }

        // Check for preset usage
        if (args.preset.has_value() && !args.sources.empty())
        {
            return Command::Pipeline;
        }

        // If sources provided, enter interactive mode
        if (!args.sources.empty() && !args.no_interactive)
        {
            return Command::Interactive;
        }

        // Default to help
        return Command::Help;
    }

} // namespace uniconv::cli
