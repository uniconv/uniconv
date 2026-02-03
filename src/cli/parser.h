#pragma once

#include "core/types.h"
#include <CLI/CLI.hpp>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::cli {

// Parsed command type
enum class Command {
    Pipeline,      // Pipeline syntax (source | target | target)
    Info,          // uniconv info <file>
    Formats,       // uniconv formats
    Preset,        // uniconv preset <subcommand>
    Plugin,        // uniconv plugin <subcommand>
    Config,        // uniconv config <subcommand>
    Interactive,   // No command, enter interactive mode
    Help,          // Show help
    Version        // Show version
};

// Parsed arguments structure
struct ParsedArgs {
    Command command = Command::Help;

    // Source files/directories
    std::vector<std::string> sources;

    // Core options
    core::CoreOptions core_options;

    // Subcommand specific
    std::string subcommand;                // For preset/plugin/config
    std::vector<std::string> subcommand_args;

    // Flags
    bool interactive = false;
    bool no_interactive = false;
    bool watch = false;
    std::optional<std::string> preset;
};

class CliParser {
public:
    CliParser();
    ~CliParser() = default;

    // Parse command line arguments
    ParsedArgs parse(int argc, char** argv);

    // Get help text
    std::string help();

private:
    void setup_main_options(CLI::App& app, ParsedArgs& args);
    void setup_subcommands(CLI::App& app, ParsedArgs& args);

    // Determine command from parsed state
    Command determine_command(const CLI::App& app, const ParsedArgs& args);
};

} // namespace uniconv::cli
