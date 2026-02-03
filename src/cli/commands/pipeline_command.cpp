#include "pipeline_command.h"
#include "utils/json_output.h"
#include <iostream>
#include <iomanip>

namespace uniconv::cli::commands {

PipelineCommand::PipelineCommand(
    std::shared_ptr<core::Engine> engine,
    std::shared_ptr<core::PresetManager> preset_manager
) : engine_(std::move(engine)), preset_manager_(std::move(preset_manager)) {
}

int PipelineCommand::execute(const ParsedArgs& args) {
    if (args.sources.empty()) {
        std::cerr << "Error: No source files specified\n";
        return 1;
    }

    auto requests = build_requests(args);
    if (requests.empty()) {
        std::cerr << "Error: No files to process\n";
        return 1;
    }

    core::ProgressCallback progress_cb = nullptr;
    if (!args.core_options.quiet && !args.core_options.json_output) {
        progress_cb = [this](const std::string& file, size_t current, size_t total) {
            show_progress(file, current, total);
        };
    }

    if (requests.size() == 1) {
        auto result = engine_->execute(requests[0]);
        print_result(result, args);
        return result.status == core::ResultStatus::Success ? 0 : 1;
    } else {
        auto batch = engine_->execute_batch(requests, progress_cb);
        print_batch_result(batch, args);
        return batch.failed > 0 ? 1 : 0;
    }
}

std::vector<core::Request> PipelineCommand::build_requests(const ParsedArgs& args) {
    std::string target;
    std::optional<std::string> plugin;
    core::CoreOptions core_opts = args.core_options;
    std::vector<std::string> plugin_opts;

    // If using a preset, load it first
    if (args.preset) {
        auto preset = preset_manager_->load(*args.preset);
        if (!preset) {
            std::cerr << "Error: Preset not found: " << *args.preset << "\n";
            return {};
        }

        target = preset->target;
        plugin = preset->plugin;

        // Merge options (command line overrides preset)
        if (!core_opts.quality) core_opts.quality = preset->core_options.quality;
        if (!core_opts.width) core_opts.width = preset->core_options.width;
        if (!core_opts.height) core_opts.height = preset->core_options.height;
        if (!core_opts.target_size) core_opts.target_size = preset->core_options.target_size;

        if (plugin_opts.empty()) {
            plugin_opts = preset->plugin_options;
        }
    }

    // Convert source strings to paths
    std::vector<std::filesystem::path> sources;
    for (const auto& s : args.sources) {
        sources.push_back(s);
    }

    return engine_->create_requests(sources, target, plugin, core_opts, plugin_opts);
}

void PipelineCommand::print_result(const core::Result& result, const ParsedArgs& args) {
    if (args.core_options.json_output) {
        std::cout << result.to_json().dump(2) << std::endl;
        return;
    }

    if (args.core_options.quiet && result.status == core::ResultStatus::Success) {
        return;
    }

    switch (result.status) {
        case core::ResultStatus::Success:
            std::cout << "OK " << result.input.filename().string();
            if (result.output) {
                std::cout << " -> " << result.output->filename().string();
            }
            if (result.input_size > 0 && result.output_size) {
                double ratio = static_cast<double>(*result.output_size) / result.input_size * 100;
                std::cout << " (" << std::fixed << std::setprecision(1) << ratio << "%)";
            }
            std::cout << "\n";
            break;

        case core::ResultStatus::Skipped:
            std::cout << "SKIP " << result.input.filename().string();
            if (result.error) {
                std::cout << " (" << *result.error << ")";
            }
            std::cout << "\n";
            break;

        case core::ResultStatus::Error:
            std::cerr << "FAIL " << result.input.filename().string();
            if (result.error) {
                std::cerr << ": " << *result.error;
            }
            std::cerr << "\n";
            break;
    }
}

void PipelineCommand::print_batch_result(const core::BatchResult& result, const ParsedArgs& args) {
    if (args.core_options.json_output) {
        std::cout << result.to_json().dump(2) << std::endl;
        return;
    }

    for (const auto& r : result.results) {
        print_result(r, args);
    }

    if (!args.core_options.quiet) {
        std::cout << "\nProcessed " << result.results.size() << " files: "
                  << result.succeeded << " succeeded, "
                  << result.failed << " failed, "
                  << result.skipped << " skipped\n";
    }
}

void PipelineCommand::show_progress(const std::string& file, size_t current, size_t total) {
    if (file.empty()) {
        std::cout << "\r" << std::string(60, ' ') << "\r" << std::flush;
        return;
    }

    int percent = total > 0 ? static_cast<int>(current * 100 / total) : 0;
    std::cout << "\r[" << std::setw(3) << percent << "%] Processing: " << file
              << std::string(20, ' ') << "\r" << std::flush;
}

} // namespace uniconv::cli::commands
