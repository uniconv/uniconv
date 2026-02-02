#pragma once

#include "core/types.h"
#include <CLI/CLI.hpp>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::cli {

// Parsed command type
enum class Command {
    ETL,           // Transform/Extract/Load operation
    Info,          // uniconv info <file>
    Formats,       // uniconv formats
    Presets,       // uniconv presets
    Preset,        // uniconv preset <subcommand>
    Plugins,       // uniconv plugins
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

    // ETL specific
    std::optional<core::ETLType> etl;
    std::string target;                    // "jpg", "faces@ai-vision", etc.
    std::optional<std::string> plugin;     // Extracted from target@plugin

    // Core options
    core::CoreOptions core_options;

    // Plugin options (after --)
    std::vector<std::string> plugin_options;

    // Subcommand specific
    std::string subcommand;                // For preset/plugin/config
    std::vector<std::string> subcommand_args;

    // Flags
    bool interactive = false;
    bool no_interactive = false;
    bool watch = false;
    std::optional<std::string> preset;

    // Temporary string for target_size parsing
    std::string target_size_str;

    // Temporary strings for ETL target parsing (must persist for CLI11)
    std::string transform_target_str;
    std::string extract_target_str;
    std::string load_target_str;

    // Helper to check if this is a valid ETL request
    bool is_etl_request() const {
        return command == Command::ETL && etl.has_value() && !target.empty();
    }
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
    void setup_etl_options(CLI::App& app, ParsedArgs& args);
    void setup_subcommands(CLI::App& app, ParsedArgs& args);

    // Parse target[@plugin] format
    std::pair<std::string, std::optional<std::string>> parse_target(const std::string& target);

    // Determine command from parsed state
    Command determine_command(const CLI::App& app, const ParsedArgs& args);
};

} // namespace uniconv::cli
