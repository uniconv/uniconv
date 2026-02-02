#pragma once

#include "cli/parser.h"
#include "core/engine.h"
#include "core/preset_manager.h"
#include <memory>

namespace uniconv::cli::commands {

class ETLCommand {
public:
    ETLCommand(
        std::shared_ptr<core::Engine> engine,
        std::shared_ptr<core::PresetManager> preset_manager
    );

    int execute(const ParsedArgs& args);

private:
    std::shared_ptr<core::Engine> engine_;
    std::shared_ptr<core::PresetManager> preset_manager_;

    // Build requests from args (possibly using preset)
    std::vector<core::ETLRequest> build_requests(const ParsedArgs& args);

    // Print result
    void print_result(const core::ETLResult& result, const ParsedArgs& args);
    void print_batch_result(const core::BatchResult& result, const ParsedArgs& args);

    // Progress callback
    void show_progress(const std::string& file, size_t current, size_t total);
};

} // namespace uniconv::cli::commands
