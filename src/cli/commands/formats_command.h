#pragma once

#include "cli/parser.h"
#include "core/plugin_manager.h"
#include <memory>

namespace uniconv::cli::commands {

class FormatsCommand {
public:
    explicit FormatsCommand(std::shared_ptr<core::PluginManager> plugin_manager);

    int execute(const ParsedArgs& args);

private:
    std::shared_ptr<core::PluginManager> plugin_manager_;

    void print_formats_text();
    void print_formats_json();
};

} // namespace uniconv::cli::commands
