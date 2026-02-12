#include "json_output.h"
#include "string_utils.h"
#include <iomanip>

namespace uniconv::utils
{

    void output_json(std::ostream &os, const nlohmann::json &j, bool pretty)
    {
        if (pretty)
        {
            os << std::setw(2) << j << std::endl;
        }
        else
        {
            os << j << std::endl;
        }
    }

    void output_result(std::ostream &os, const core::Result &result, bool json_mode, bool verbose)
    {
        if (json_mode)
        {
            output_json(os, result.to_json(), verbose);
            return;
        }

        if (result.status == core::ResultStatus::Success)
        {
            os << "✓ " << result.input.filename().string();
            if (result.output)
            {
                os << " → " << result.output->filename().string();
            }
            if (result.input_size > 0 && result.output_size)
            {
                os << " (" << format_size(result.input_size)
                   << " → " << format_size(*result.output_size) << ")";
            }
            os << std::endl;

            if (verbose && !result.extra.empty())
            {
                os << "  Details: " << result.extra.dump() << std::endl;
            }
        }
        else if (result.status == core::ResultStatus::Skipped)
        {
            os << "- " << result.input.filename().string() << " (skipped)";
            if (result.error)
            {
                os << ": " << *result.error;
            }
            os << std::endl;
        }
        else
        {
            os << "✗ " << result.input.filename().string();
            if (result.error)
            {
                os << ": " << *result.error;
            }
            os << std::endl;
        }
    }

    void output_results(std::ostream &os, const std::vector<core::Result> &results, bool json_mode, bool verbose)
    {
        if (json_mode)
        {
            nlohmann::json j = nlohmann::json::array();
            for (const auto &result : results)
            {
                j.push_back(result.to_json());
            }

            // Add summary
            nlohmann::json summary;
            int success_count = 0, error_count = 0, skipped_count = 0;
            for (const auto &r : results)
            {
                switch (r.status)
                {
                case core::ResultStatus::Success:
                    success_count++;
                    break;
                case core::ResultStatus::Error:
                    error_count++;
                    break;
                case core::ResultStatus::Skipped:
                    skipped_count++;
                    break;
                }
            }

            nlohmann::json output;
            output["results"] = j;
            output["summary"] = {
                {"total", results.size()},
                {"success", success_count},
                {"error", error_count},
                {"skipped", skipped_count}};

            output_json(os, output, verbose);
            return;
        }

        for (const auto &result : results)
        {
            output_result(os, result, false, verbose);
        }

        // Summary
        if (results.size() > 1)
        {
            int success_count = 0, error_count = 0;
            for (const auto &r : results)
            {
                if (r.status == core::ResultStatus::Success)
                    success_count++;
                else if (r.status == core::ResultStatus::Error)
                    error_count++;
            }
            os << "\nProcessed " << results.size() << " files: "
               << success_count << " succeeded, " << error_count << " failed" << std::endl;
        }
    }

    void output_file_info(std::ostream &os, const core::FileInfo &info, bool json_mode)
    {
        if (json_mode)
        {
            output_json(os, info.to_json(), true);
            return;
        }

        os << "File: " << info.path.filename().string() << std::endl;
        os << "Format: " << to_upper(info.format) << " (" << core::file_category_to_string(info.category) << ")" << std::endl;
        os << "MIME: " << info.mime_type << std::endl;
        os << "Size: " << format_size(info.size) << std::endl;

        if (info.dimensions)
        {
            os << "Dimensions: " << info.dimensions->first << " x " << info.dimensions->second << std::endl;
        }
        if (info.duration)
        {
            os << "Duration: " << std::fixed << std::setprecision(1) << *info.duration << "s" << std::endl;
        }
    }

    void output_plugin_info(std::ostream &os, const core::PluginInfo &info, bool json_mode)
    {
        if (json_mode)
        {
            output_json(os, info.to_json(), true);
            return;
        }

        os << info.scope;
        if (info.builtin)
        {
            os << " [built-in]";
        }
        os << std::endl;
        os << "  Version: " << info.version << std::endl;
        os << "  Description: " << info.description << std::endl;
        {
            std::string target_str;
            for (const auto &[t, _] : info.targets)
            {
                if (!target_str.empty())
                    target_str += ", ";
                target_str += t;
            }
            os << "  Targets: " << target_str << std::endl;
        }
        if (info.accepts.has_value() && !info.accepts->empty())
        {
            os << "  Input formats: " << join(*info.accepts, ", ") << std::endl;
        }
    }

    void output_plugins(std::ostream &os, const std::vector<core::PluginInfo> &plugins, bool json_mode)
    {
        if (json_mode)
        {
            nlohmann::json j = nlohmann::json::array();
            for (const auto &plugin : plugins)
            {
                j.push_back(plugin.to_json());
            }
            output_json(os, {{"plugins", j}}, true);
            return;
        }

        if (plugins.empty())
        {
            os << "No plugins found." << std::endl;
            return;
        }

        os << "Installed plugins:" << std::endl;
        for (const auto &plugin : plugins)
        {
            os << "\n";
            output_plugin_info(os, plugin, false);
        }
    }

    void output_preset(std::ostream &os, const core::Preset &preset, bool json_mode)
    {
        if (json_mode)
        {
            output_json(os, preset.to_json(), true);
            return;
        }

        os << "Preset: " << preset.name << std::endl;
        if (!preset.description.empty())
        {
            os << "Description: " << preset.description << std::endl;
        }
        os << "Target: " << preset.target << std::endl;
        if (preset.plugin)
        {
            os << "Plugin: " << *preset.plugin << std::endl;
        }

        // Plugin options
        if (!preset.plugin_options.empty())
        {
            os << "Plugin options: " << join(preset.plugin_options, " ") << std::endl;
        }
    }

    void output_presets(std::ostream &os, const std::vector<core::Preset> &presets, bool json_mode)
    {
        if (json_mode)
        {
            nlohmann::json j = nlohmann::json::array();
            for (const auto &preset : presets)
            {
                j.push_back(preset.to_json());
            }
            output_json(os, {{"presets", j}}, true);
            return;
        }

        if (presets.empty())
        {
            os << "No presets found." << std::endl;
            return;
        }

        os << "Available presets:" << std::endl;
        for (const auto &preset : presets)
        {
            os << "  " << preset.name;
            if (!preset.description.empty())
            {
                os << " - " << preset.description;
            }
            os << " [→ " << preset.target << "]";
            os << std::endl;
        }
    }

    void output_error(std::ostream &os, const std::string &message, bool json_mode)
    {
        if (json_mode)
        {
            output_json(os, {{"success", false}, {"error", message}}, false);
            return;
        }
        os << "Error: " << message << std::endl;
    }

    void output_success(std::ostream &os, const std::string &message, bool json_mode)
    {
        if (json_mode)
        {
            output_json(os, {{"success", true}, {"message", message}}, false);
            return;
        }
        os << message << std::endl;
    }

} // namespace uniconv::utils
