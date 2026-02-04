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
    Version,       // Show version
    Update         // uniconv update
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
    bool list_registry = false;
    bool update_check_only = false;
    std::optional<std::string> preset;
};

class CliParser {
public:
    CliParser();
    ~CliParser() = default;

    // Parse command line arguments
    // Returns exit code via parse_exit_code if help/version was requested
    ParsedArgs parse(int argc, char** argv);

    // Get help text
    std::string help();

    // If parse() detected a help/version request, this holds the exit code (0).
    // -1 means normal parsing succeeded and no help/version was requested.
    int parse_exit_code() const { return parse_exit_code_; }

    // The help/version text produced by CLI11 for the exact subcommand requested
    const std::string& parse_exit_message() const { return parse_exit_message_; }

private:
    void setup_main_options(CLI::App& app, ParsedArgs& args);
    void setup_subcommands(CLI::App& app, ParsedArgs& args);

    // Determine command from parsed state
    Command determine_command(const CLI::App& app, const ParsedArgs& args);

    int parse_exit_code_ = -1;
    std::string parse_exit_message_;
};

} // namespace uniconv::cli
