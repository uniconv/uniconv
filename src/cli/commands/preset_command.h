#pragma once

#include "cli/parser.h"
#include "core/preset_manager.h"
#include <memory>

namespace uniconv::cli::commands {

class PresetCommand {
public:
    explicit PresetCommand(std::shared_ptr<core::PresetManager> preset_manager);

    // Execute preset management subcommand
    int execute(const ParsedArgs& args);

    // List all presets
    int list(const ParsedArgs& args);

private:
    std::shared_ptr<core::PresetManager> preset_manager_;

    int create_preset(const ParsedArgs& args);
    int delete_preset(const ParsedArgs& args);
    int show_preset(const ParsedArgs& args);
    int export_preset(const ParsedArgs& args);
    int import_preset(const ParsedArgs& args);
};

} // namespace uniconv::cli::commands
