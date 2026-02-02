#include "config_command.h"
#include <iostream>

namespace uniconv::cli::commands {

ConfigCommand::ConfigCommand(std::shared_ptr<core::ConfigManager> config_manager)
    : config_manager_(std::move(config_manager)) {
}

int ConfigCommand::execute(const ParsedArgs& args) {
    if (args.subcommand_args.empty()) {
        return list(args);
    }

    const auto& action = args.subcommand_args[0];

    if (action == "list") {
        return list(args);
    } else if (action == "get") {
        return get(args);
    } else if (action == "set") {
        return set(args);
    } else if (action == "unset") {
        return unset(args);
    }

    std::cerr << "Unknown config action: " << action << "\n";
    std::cerr << "Available actions: list, get, set, unset\n";
    return 1;
}

int ConfigCommand::list(const ParsedArgs& args) {
    // Load current config
    config_manager_->load();

    if (args.core_options.json_output) {
        std::cout << config_manager_->to_json().dump(2) << std::endl;
        return 0;
    }

    auto keys = config_manager_->list_keys();

    if (keys.empty()) {
        std::cout << "(no configuration set)\n";
        return 0;
    }

    // Print defaults
    auto defaults = config_manager_->get_all_defaults();
    if (!defaults.empty()) {
        std::cout << "Default plugins:\n";
        for (const auto& [key, value] : defaults) {
            std::cout << "  " << key << " = " << value << "\n";
        }
    }

    // Print plugin paths
    auto paths = config_manager_->get_plugin_paths();
    if (!paths.empty()) {
        std::cout << "\nPlugin paths:\n";
        for (const auto& path : paths) {
            std::cout << "  " << path.string() << "\n";
        }
    }

    return 0;
}

int ConfigCommand::get(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Usage: uniconv config get <key>\n";
        return 1;
    }

    const auto& key = args.subcommand;

    // Load current config
    config_manager_->load();

    // Check if it's a default plugin key
    if (key.starts_with("defaults.") || key.find('.') != std::string::npos) {
        // Try as default plugin key
        std::string lookup_key = key;
        if (key.starts_with("defaults.")) {
            lookup_key = key.substr(9);  // Remove "defaults." prefix
        }

        auto value = config_manager_->get_default_plugin(lookup_key);
        if (value) {
            if (args.core_options.json_output) {
                nlohmann::json j;
                j["key"] = key;
                j["value"] = *value;
                std::cout << j.dump(2) << std::endl;
            } else {
                std::cout << *value << "\n";
            }
            return 0;
        }
    }

    // Try as generic setting
    auto value = config_manager_->get(key);
    if (value) {
        if (args.core_options.json_output) {
            nlohmann::json j;
            j["key"] = key;
            j["value"] = *value;
            std::cout << j.dump(2) << std::endl;
        } else {
            std::cout << *value << "\n";
        }
        return 0;
    }

    std::cerr << "Config key not found: " << key << "\n";
    return 1;
}

int ConfigCommand::set(const ParsedArgs& args) {
    if (args.subcommand.empty() || args.subcommand_args.size() < 2) {
        std::cerr << "Usage: uniconv config set <key> <value>\n";
        return 1;
    }

    const auto& key = args.subcommand;
    const auto& value = args.subcommand_args[1];

    // Load current config
    config_manager_->load();

    // Check if it's a default plugin key (format: etl.target or defaults.etl.target)
    std::string lookup_key = key;
    bool is_default = false;

    if (key.starts_with("defaults.")) {
        lookup_key = key.substr(9);
        is_default = true;
    } else if (key.starts_with("transform.") || key.starts_with("extract.") || key.starts_with("load.")) {
        is_default = true;
    }

    if (is_default) {
        config_manager_->set_default_plugin(lookup_key, value);
    } else {
        config_manager_->set(key, value);
    }

    // Save config
    if (!config_manager_->save()) {
        std::cerr << "Error: Failed to save configuration\n";
        return 1;
    }

    if (!args.core_options.quiet) {
        std::cout << "Set " << key << " = " << value << "\n";
    }

    if (args.core_options.json_output) {
        nlohmann::json j;
        j["success"] = true;
        j["key"] = key;
        j["value"] = value;
        std::cout << j.dump(2) << std::endl;
    }

    return 0;
}

int ConfigCommand::unset(const ParsedArgs& args) {
    if (args.subcommand.empty()) {
        std::cerr << "Usage: uniconv config unset <key>\n";
        return 1;
    }

    const auto& key = args.subcommand;

    // Load current config
    config_manager_->load();

    bool removed = false;

    // Try as default plugin key
    std::string lookup_key = key;
    if (key.starts_with("defaults.")) {
        lookup_key = key.substr(9);
    }
    removed = config_manager_->unset_default_plugin(lookup_key);

    // Try as generic setting
    if (!removed) {
        removed = config_manager_->unset(key);
    }

    if (!removed) {
        std::cerr << "Config key not found: " << key << "\n";
        return 1;
    }

    // Save config
    if (!config_manager_->save()) {
        std::cerr << "Error: Failed to save configuration\n";
        return 1;
    }

    if (!args.core_options.quiet) {
        std::cout << "Removed " << key << "\n";
    }

    if (args.core_options.json_output) {
        nlohmann::json j;
        j["success"] = true;
        j["key"] = key;
        std::cout << j.dump(2) << std::endl;
    }

    return 0;
}

} // namespace uniconv::cli::commands
