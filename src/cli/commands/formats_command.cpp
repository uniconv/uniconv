#include "formats_command.h"
#include <set>

namespace uniconv::cli::commands
{

    FormatsCommand::FormatsCommand(std::shared_ptr<core::PluginManager> plugin_manager,
                                   std::shared_ptr<core::output::IOutput> output)
        : plugin_manager_(std::move(plugin_manager)), output_(std::move(output))
    {
    }

    int FormatsCommand::execute(const ParsedArgs & /*args*/)
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
            for (const auto &[target, _] : plugin.targets)
            {
                all_outputs.insert(target);
            }
        }

        nlohmann::json j;
        j["inputs"] = std::vector<std::string>(all_inputs.begin(), all_inputs.end());
        j["outputs"] = std::vector<std::string>(all_outputs.begin(), all_outputs.end());

        // Build text representation
        std::string text_output = "Supported formats:\n\n  Input formats: ";
        bool first = true;
        for (const auto &fmt : all_inputs)
        {
            if (!first)
                text_output += ", ";
            text_output += fmt;
            first = false;
        }
        text_output += "\n\n  Output formats: ";
        first = true;
        for (const auto &fmt : all_outputs)
        {
            if (!first)
                text_output += ", ";
            text_output += fmt;
            first = false;
        }

        output_->data(j, text_output);
        return 0;
    }

} // namespace uniconv::cli::commands
