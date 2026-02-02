#include "info_command.h"
#include "utils/json_output.h"
#include <iostream>

namespace uniconv::cli::commands {

InfoCommand::InfoCommand(std::shared_ptr<core::Engine> engine)
    : engine_(std::move(engine)) {
}

int InfoCommand::execute(const ParsedArgs& args) {
    if (args.subcommand_args.empty()) {
        std::cerr << "Error: No file specified\n";
        return 1;
    }

    const auto& file_path = args.subcommand_args[0];

    try {
        auto info = engine_->get_file_info(file_path);
        utils::output_file_info(std::cout, info, args.core_options.json_output);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

} // namespace uniconv::cli::commands
