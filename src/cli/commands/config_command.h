#pragma once

#include "cli/parser.h"
#include "core/config_manager.h"
#include <memory>

namespace uniconv::cli::commands {

// Config management command handler
class ConfigCommand {
public:
    explicit ConfigCommand(std::shared_ptr<core::ConfigManager> config_manager);

    // Execute config subcommand
    int execute(const ParsedArgs& args);

    // List all config settings
    int list(const ParsedArgs& args);

    // Get a config value
    int get(const ParsedArgs& args);

    // Set a config value
    int set(const ParsedArgs& args);

    // Unset a config value
    int unset(const ParsedArgs& args);

private:
    std::shared_ptr<core::ConfigManager> config_manager_;
};

} // namespace uniconv::cli::commands
