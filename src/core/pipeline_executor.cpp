#include "pipeline_executor.h"
#include <chrono>
#include <sstream>

namespace uniconv::core {

PipelineExecutor::PipelineExecutor(std::shared_ptr<Engine> engine)
    : engine_(std::move(engine))
    , temp_dir_(std::filesystem::temp_directory_path() / "uniconv") {
    // Ensure temp directory exists
    std::filesystem::create_directories(temp_dir_);
}

PipelineResult PipelineExecutor::execute(
    const Pipeline& pipeline,
    const PipelineProgressCallback& progress) {

    auto start_time = std::chrono::steady_clock::now();

    PipelineResult final_result;
    final_result.success = false;

    // Start with the original input
    std::vector<std::filesystem::path> current_inputs = {pipeline.source};

    // Execute each stage sequentially
    for (size_t stage_idx = 0; stage_idx < pipeline.stages.size(); ++stage_idx) {
        const auto& stage = pipeline.stages[stage_idx];

        // Handle tee operation - if this stage has tee, replicate inputs for next stage
        if (stage.has_tee()) {
            if (current_inputs.size() != 1) {
                final_result.error = "tee operation requires exactly one input";
                cleanup_temp_files();
                return final_result;
            }

            // Determine tee count from next stage's element count
            size_t tee_count = 1;
            if (stage_idx + 1 < pipeline.stages.size()) {
                tee_count = pipeline.stages[stage_idx + 1].element_count();
            }

            current_inputs = handle_tee(current_inputs[0], tee_count);
            continue; // tee doesn't execute anything, just replicates paths
        }

        // Validate input/element count
        if (!stage.elements.empty() &&
            current_inputs.size() != stage.elements.size()) {
            std::ostringstream oss;
            oss << "Stage " << stage_idx << " has " << stage.elements.size()
                << " elements but received " << current_inputs.size() << " inputs";
            final_result.error = oss.str();
            cleanup_temp_files();
            return final_result;
        }

        // Execute the stage
        bool is_last_stage = (stage_idx == pipeline.stages.size() - 1);
        auto stage_result = execute_stage(
            stage_idx,
            stage,
            current_inputs,
            pipeline.core_options,
            progress,
            is_last_stage
        );

        if (!stage_result.success) {
            final_result.error = stage_result.error;
            final_result.stage_results = std::move(stage_result.results);
            cleanup_temp_files();
            return final_result;
        }

        // Append stage results
        final_result.stage_results.insert(
            final_result.stage_results.end(),
            stage_result.results.begin(),
            stage_result.results.end()
        );

        // Update inputs for next stage
        current_inputs = std::move(stage_result.outputs);
    }

    // Final outputs
    final_result.final_outputs = std::move(current_inputs);
    final_result.success = true;

    auto end_time = std::chrono::steady_clock::now();
    final_result.total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();

    cleanup_temp_files();
    return final_result;
}

PipelineExecutor::StageExecutionResult PipelineExecutor::execute_stage(
    size_t stage_index,
    const PipelineStage& stage,
    const std::vector<std::filesystem::path>& inputs,
    const CoreOptions& core_options,
    const PipelineProgressCallback& progress,
    bool is_last_stage) {

    StageExecutionResult result;
    result.success = true;

    // If no elements, just pass through inputs
    if (stage.elements.empty()) {
        result.outputs = inputs;
        return result;
    }

    // Execute each element (parallel conceptually, but sequentially for now)
    for (size_t elem_idx = 0; elem_idx < stage.elements.size(); ++elem_idx) {
        const auto& element = stage.elements[elem_idx];
        const auto& input = inputs[elem_idx];

        // Progress callback
        if (progress) {
            std::ostringstream desc;
            desc << "Stage " << stage_index << ", Element " << elem_idx
                 << ": " << element.target;
            progress(stage_index, elem_idx, desc.str());
        }

        // Determine ETL type
        ETLType etl_type = determine_etl_type(element.target);

        // Determine output path
        std::filesystem::path output_path;
        auto output_it = element.options.find("output");
        if (output_it != element.options.end() && !output_it->second.empty()) {
            // Explicit output specified in pipeline element (e.g., jpg --output foo.jpg)
            output_path = output_it->second;
        } else if (is_last_stage && core_options.output.has_value()) {
            // Last stage with CLI -o option specified
            // If -o has no extension, append target as extension
            // If -o has extension, use as-is (for multiple outputs, they overwrite same file)
            const auto& out_path = *core_options.output;
            if (out_path.has_extension()) {
                output_path = out_path;
            } else {
                output_path = out_path;
                output_path += "." + element.target;
            }
        } else if (is_last_stage) {
            // Last stage: output to current directory (same name, new extension)
            output_path = generate_final_output_path(input, element.target);
        } else {
            // Intermediate result: use temp directory
            output_path = generate_temp_path(
                input,
                element.target,
                stage_index,
                elem_idx
            );
        }

        // Build ETL request
        ETLRequest request;
        request.etl = etl_type;
        request.source = input;
        request.target = element.target;
        request.plugin = element.plugin;
        request.core_options = core_options;
        request.core_options.output = output_path;
        request.plugin_options = element.raw_options;

        // Execute through engine
        auto etl_result = engine_->execute(request);

        // Convert to StageResult
        StageResult stage_result;
        stage_result.stage_index = stage_index;
        stage_result.target = element.target;
        stage_result.plugin_used = etl_result.plugin_used;
        stage_result.input = input;
        stage_result.output = output_path;
        stage_result.status = etl_result.status;
        stage_result.error = etl_result.error;
        // Note: duration_ms would need to be tracked separately

        result.results.push_back(std::move(stage_result));

        if (etl_result.status != ResultStatus::Success) {
            result.success = false;
            result.error = etl_result.error.value_or("Unknown error");
            return result;
        }

        result.outputs.push_back(output_path);
    }

    return result;
}

std::vector<std::filesystem::path> PipelineExecutor::handle_tee(
    const std::filesystem::path& input,
    size_t count) {

    // Just replicate the path N times
    // No actual file duplication needed - each subsequent stage will read from same input
    std::vector<std::filesystem::path> outputs;
    for (size_t i = 0; i < count; ++i) {
        outputs.push_back(input);
    }
    return outputs;
}

ETLType PipelineExecutor::determine_etl_type(const std::string& target) {
    // Query plugin manager through engine - single lookup
    auto& plugin_mgr = engine_->plugin_manager();
    auto plugins = plugin_mgr.list_plugins_for_target(target);

    // Check what ETL types are available for this target
    // Priority: Extract > Transform > Load (extract targets are more specific)
    for (const auto& plugin_info : plugins) {
        if (plugin_info.etl == ETLType::Extract) {
            return ETLType::Extract;
        }
    }

    for (const auto& plugin_info : plugins) {
        if (plugin_info.etl == ETLType::Transform) {
            return ETLType::Transform;
        }
    }

    for (const auto& plugin_info : plugins) {
        if (plugin_info.etl == ETLType::Load) {
            return ETLType::Load;
        }
    }

    // Default to transform if not found (engine will handle error)
    return ETLType::Transform;
}

std::filesystem::path PipelineExecutor::generate_temp_path(
    const std::filesystem::path& input,
    const std::string& target,
    size_t stage_index,
    size_t element_index) {

    // Format: temp_dir / "stage{idx}_elem{idx}_{target}_{input_stem}.tmp"
    std::ostringstream filename;
    filename << "stage" << stage_index
             << "_elem" << element_index
             << "_" << target
             << "_" << input.stem().string()
             << ".tmp";

    return temp_dir_ / filename.str();
}

std::filesystem::path PipelineExecutor::generate_final_output_path(
    const std::filesystem::path& input,
    const std::string& target) {

    // Output to current directory with input stem + target as extension
    // e.g., photo.heic → photo.jpg (when target is "jpg")
    std::filesystem::path output = std::filesystem::current_path();
    output /= input.stem();
    output += "." + target;

    return output;
}

void PipelineExecutor::cleanup_temp_files() {
    // Remove all .tmp files from temp directory
    try {
        if (std::filesystem::exists(temp_dir_)) {
            for (const auto& entry : std::filesystem::directory_iterator(temp_dir_)) {
                if (entry.path().extension() == ".tmp") {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore cleanup errors
    }
}

} // namespace uniconv::core
