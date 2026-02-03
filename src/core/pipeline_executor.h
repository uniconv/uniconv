#pragma once

#include "pipeline.h"
#include "engine.h"
#include <memory>
#include <functional>

namespace uniconv::core
{

    using PipelineProgressCallback = std::function<void(
        size_t stage_index,
        size_t element_index,
        const std::string &description)>;

    class PipelineExecutor
    {
    public:
        explicit PipelineExecutor(std::shared_ptr<Engine> engine);

        // Execute a complete pipeline
        PipelineResult execute(const Pipeline &pipeline,
                               const PipelineProgressCallback &progress = nullptr);

    private:
        std::shared_ptr<Engine> engine_;
        std::filesystem::path temp_dir_;

        // Execute a single stage, may produce multiple outputs
        struct StageExecutionResult
        {
            bool success = false;
            std::vector<std::filesystem::path> outputs;
            std::vector<StageResult> results;
            std::string error;
        };

        StageExecutionResult execute_stage(
            size_t stage_index,
            const PipelineStage &stage,
            const std::vector<std::filesystem::path> &inputs,
            const std::filesystem::path &original_source,
            const CoreOptions &core_options,
            const PipelineProgressCallback &progress,
            bool is_last_stage);

        // Handle tee: replicate input N times for next stage
        std::vector<std::filesystem::path> handle_tee(
            const std::filesystem::path &input,
            size_t count);

        // Generate temp file path for intermediate results
        std::filesystem::path generate_temp_path(
            const std::filesystem::path &input,
            const std::string &target,
            size_t stage_index,
            size_t element_index);

        // Generate final output path (current directory)
        // Uses original_source for base name, extracts transform from intermediate input if present
        std::filesystem::path generate_final_output_path(
            const std::filesystem::path &original_source,
            const std::filesystem::path &intermediate_input,
            const std::string &target);

        // Extract transform name from intermediate temp filename
        // Returns empty string if not a temp file pattern
        std::string extract_transform_from_temp(const std::filesystem::path &temp_path);

        // Cleanup temp files
        void cleanup_temp_files();
    };

} // namespace uniconv::core
