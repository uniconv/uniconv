#include "engine.h"
#include "utils/file_utils.h"
#include <algorithm>

namespace uniconv::core {

Engine::Engine()
    : plugin_manager_(std::make_shared<PluginManager>()) {
}

Engine::Engine(std::shared_ptr<PluginManager> plugin_manager)
    : plugin_manager_(std::move(plugin_manager)) {
    if (!plugin_manager_) {
        plugin_manager_ = std::make_shared<PluginManager>();
    }
}

ETLResult Engine::execute(const ETLRequest& request) {
    // Check if source exists
    if (!std::filesystem::exists(request.source)) {
        return ETLResult::failure(
            request.etl,
            request.target,
            request.source,
            "Source file not found: " + request.source.string()
        );
    }

    // Get input file info
    auto input_format = utils::detect_format(request.source);
    size_t input_size = std::filesystem::file_size(request.source);

    // Find appropriate plugin
    auto* plugin = plugin_manager_->find_plugin_for_input(
        request.etl,
        input_format,
        request.target
    );

    if (!plugin) {
        // Try finding by target only
        plugin = plugin_manager_->find_plugin(
            request.etl,
            request.target,
            request.plugin
        );
    }

    if (!plugin) {
        return ETLResult::failure(
            request.etl,
            request.target,
            request.source,
            "No plugin found for " + etl_type_to_string(request.etl) +
            " operation: " + input_format + " -> " + request.target
        );
    }

    // Check if plugin supports the input format
    if (!plugin->supports_input(input_format)) {
        return ETLResult::failure(
            request.etl,
            request.target,
            request.source,
            "Plugin '" + plugin->info().group + "' does not support input format: " + input_format
        );
    }

    // Resolve output path
    auto output_path = resolve_output_path(request, request.target);

    // Check for existing output
    if (should_skip_existing(output_path, request.core_options)) {
        ETLResult result;
        result.status = ResultStatus::Skipped;
        result.etl = request.etl;
        result.target = request.target;
        result.plugin_used = plugin->info().group;
        result.input = request.source;
        result.input_size = input_size;
        result.output = output_path;
        result.error = "Output file exists (use --force to overwrite)";
        return result;
    }

    // Dry run - don't execute
    if (request.core_options.dry_run) {
        ETLResult result;
        result.status = ResultStatus::Success;
        result.etl = request.etl;
        result.target = request.target;
        result.plugin_used = plugin->info().group;
        result.input = request.source;
        result.input_size = input_size;
        result.output = output_path;
        result.extra["dry_run"] = true;
        return result;
    }

    // Ensure output directory exists
    if (output_path.has_parent_path()) {
        utils::ensure_directory(output_path.parent_path());
    }

    // Build the request with resolved output
    ETLRequest resolved_request = request;
    resolved_request.core_options.output = output_path;

    // Execute plugin
    auto result = plugin->execute(resolved_request);
    result.plugin_used = plugin->info().group;
    result.input_size = input_size;

    // Get output size if successful
    if (result.status == ResultStatus::Success && result.output) {
        if (std::filesystem::exists(*result.output)) {
            result.output_size = std::filesystem::file_size(*result.output);
        }
    }

    return result;
}

BatchResult Engine::execute_batch(
    const std::vector<ETLRequest>& requests,
    const ProgressCallback& progress
) {
    BatchResult batch;
    batch.results.reserve(requests.size());

    size_t current = 0;
    size_t total = requests.size();

    for (const auto& request : requests) {
        if (progress) {
            progress(request.source.string(), current, total);
        }

        auto result = execute(request);

        switch (result.status) {
            case ResultStatus::Success:
                batch.succeeded++;
                break;
            case ResultStatus::Error:
                batch.failed++;
                break;
            case ResultStatus::Skipped:
                batch.skipped++;
                break;
        }

        batch.results.push_back(std::move(result));
        current++;
    }

    if (progress) {
        progress("", total, total);  // Signal completion
    }

    return batch;
}

std::vector<ETLRequest> Engine::create_requests(
    ETLType etl,
    const std::vector<std::filesystem::path>& sources,
    const std::string& target,
    const std::optional<std::string>& plugin,
    const CoreOptions& options,
    const std::vector<std::string>& plugin_options
) {
    std::vector<ETLRequest> requests;
    requests.reserve(sources.size());

    for (const auto& source : sources) {
        // If source is a directory, expand it
        if (std::filesystem::is_directory(source)) {
            auto files = utils::get_files_in_directory(source, options.recursive);
            for (const auto& file : files) {
                ETLRequest req;
                req.etl = etl;
                req.source = file;
                req.target = target;
                req.plugin = plugin;
                req.core_options = options;
                req.plugin_options = plugin_options;
                requests.push_back(std::move(req));
            }
        } else {
            ETLRequest req;
            req.etl = etl;
            req.source = source;
            req.target = target;
            req.plugin = plugin;
            req.core_options = options;
            req.plugin_options = plugin_options;
            requests.push_back(std::move(req));
        }
    }

    return requests;
}

FileInfo Engine::get_file_info(const std::filesystem::path& path) const {
    return utils::get_file_info(path);
}

std::filesystem::path Engine::resolve_output_path(
    const ETLRequest& request,
    const std::string& target_format
) const {
    // If explicit output specified, use it
    if (request.core_options.output) {
        auto output = *request.core_options.output;

        // If output is a directory, put file in it
        if (std::filesystem::is_directory(output)) {
            auto filename = request.source.stem().string() + "." + target_format;
            return output / filename;
        }

        return output;
    }

    // Default: same directory, same name, new extension
    auto output = request.source;
    output.replace_extension(target_format);

    // If same as input, add suffix
    if (output == request.source) {
        output = request.source.parent_path() /
            (request.source.stem().string() + "_converted." + target_format);
    }

    return output;
}

bool Engine::should_skip_existing(
    const std::filesystem::path& output,
    const CoreOptions& options
) const {
    if (!std::filesystem::exists(output)) {
        return false;
    }

    // If force is enabled, don't skip (will overwrite)
    if (options.force) {
        return false;
    }

    return true;
}

} // namespace uniconv::core
