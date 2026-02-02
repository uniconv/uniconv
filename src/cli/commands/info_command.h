#pragma once

#include "cli/parser.h"
#include "core/engine.h"
#include <memory>

namespace uniconv::cli::commands {

class InfoCommand {
public:
    explicit InfoCommand(std::shared_ptr<core::Engine> engine);

    int execute(const ParsedArgs& args);

private:
    std::shared_ptr<core::Engine> engine_;
};

} // namespace uniconv::cli::commands
