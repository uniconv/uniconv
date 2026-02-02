#include "formats_command.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace uniconv::cli::commands {

FormatsCommand::FormatsCommand(std::shared_ptr<core::PluginManager> plugin_manager)
    : plugin_manager_(std::move(plugin_manager)) {
}

int FormatsCommand::execute(const ParsedArgs& args) {
    if (args.core_options.json_output) {
        print_formats_json();
    } else {
        print_formats_text();
    }
    return 0;
}

void FormatsCommand::print_formats_text() {
    std::cout << "Transform (-t):\n";
    std::cout << "  Input formats: ";
    auto inputs = plugin_manager_->get_supported_inputs(core::ETLType::Transform);
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << inputs[i];
    }
    std::cout << "\n  Output formats: ";
    auto targets = plugin_manager_->get_supported_targets(core::ETLType::Transform);
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << targets[i];
    }
    std::cout << "\n\n";

    std::cout << "Extract (-e):\n";
    inputs = plugin_manager_->get_supported_inputs(core::ETLType::Extract);
    targets = plugin_manager_->get_supported_targets(core::ETLType::Extract);
    if (inputs.empty() && targets.empty()) {
        std::cout << "  (no extract plugins installed)\n\n";
    } else {
        std::cout << "  Targets: ";
        for (size_t i = 0; i < targets.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << targets[i];
        }
        std::cout << "\n\n";
    }

    std::cout << "Load (-l):\n";
    inputs = plugin_manager_->get_supported_inputs(core::ETLType::Load);
    targets = plugin_manager_->get_supported_targets(core::ETLType::Load);
    if (inputs.empty() && targets.empty()) {
        std::cout << "  (no load plugins installed)\n";
    } else {
        std::cout << "  Destinations: ";
        for (size_t i = 0; i < targets.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << targets[i];
        }
        std::cout << "\n";
    }
}

void FormatsCommand::print_formats_json() {
    nlohmann::json j;

    j["transform"]["inputs"] = plugin_manager_->get_supported_inputs(core::ETLType::Transform);
    j["transform"]["outputs"] = plugin_manager_->get_supported_targets(core::ETLType::Transform);

    j["extract"]["inputs"] = plugin_manager_->get_supported_inputs(core::ETLType::Extract);
    j["extract"]["targets"] = plugin_manager_->get_supported_targets(core::ETLType::Extract);

    j["load"]["inputs"] = plugin_manager_->get_supported_inputs(core::ETLType::Load);
    j["load"]["destinations"] = plugin_manager_->get_supported_targets(core::ETLType::Load);

    std::cout << j.dump(2) << std::endl;
}

} // namespace uniconv::cli::commands
