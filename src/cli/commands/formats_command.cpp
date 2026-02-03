#include "formats_command.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>

namespace uniconv::cli::commands
{

    FormatsCommand::FormatsCommand(std::shared_ptr<core::PluginManager> plugin_manager)
        : plugin_manager_(std::move(plugin_manager))
    {
    }

    int FormatsCommand::execute(const ParsedArgs &args)
    {
        if (args.core_options.json_output)
        {
            print_formats_json();
        }
        else
        {
            print_formats_text();
        }
        return 0;
    }

    void FormatsCommand::print_formats_text()
    {
        auto plugins = plugin_manager_->list_plugins();

        std::set<std::string> all_inputs;
        std::set<std::string> all_outputs;

        for (const auto &plugin : plugins)
        {
            for (const auto &fmt : plugin.input_formats)
            {
                all_inputs.insert(fmt);
            }
            for (const auto &target : plugin.targets)
            {
                all_outputs.insert(target);
            }
        }

        std::cout << "Supported formats:\n\n";

        std::cout << "  Input formats: ";
        bool first = true;
        for (const auto &fmt : all_inputs)
        {
            if (!first)
                std::cout << ", ";
            std::cout << fmt;
            first = false;
        }
        std::cout << "\n\n";

        std::cout << "  Output formats: ";
        first = true;
        for (const auto &fmt : all_outputs)
        {
            if (!first)
                std::cout << ", ";
            std::cout << fmt;
            first = false;
        }
        std::cout << "\n";
    }

    void FormatsCommand::print_formats_json()
    {
        auto plugins = plugin_manager_->list_plugins();

        std::set<std::string> all_inputs;
        std::set<std::string> all_outputs;

        for (const auto &plugin : plugins)
        {
            for (const auto &fmt : plugin.input_formats)
            {
                all_inputs.insert(fmt);
            }
            for (const auto &target : plugin.targets)
            {
                all_outputs.insert(target);
            }
        }

        nlohmann::json j;
        j["inputs"] = std::vector<std::string>(all_inputs.begin(), all_inputs.end());
        j["outputs"] = std::vector<std::string>(all_outputs.begin(), all_outputs.end());

        std::cout << j.dump(2) << std::endl;
    }

} // namespace uniconv::cli::commands
