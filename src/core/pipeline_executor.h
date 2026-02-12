#pragma once

#include "pipeline.h"
#include "execution_graph.h"
#include "engine.h"
#include "output/output.h"
#include <memory>

namespace uniconv::core
{

    class PipelineExecutor
    {
    public:
        explicit PipelineExecutor(std::shared_ptr<Engine> engine);

        // Execute a complete pipeline
        PipelineResult execute(const Pipeline &pipeline,
                               const std::shared_ptr<output::IOutput> &output = nullptr);

    private:
        std::shared_ptr<Engine> engine_;
        std::filesystem::path temp_dir_;

        // Scatter/collect pipeline width tracking
        // When a plugin returns "outputs" (scatter), width increases from 1 to N.
        // Subsequent stages execute N times (once per scattered output).
        // "collect" reduces width back to 1.
        size_t pipeline_width_ = 1;

        // When width > 1, these track the parallel file paths at current width
        std::vector<std::filesystem::path> scattered_paths_;

        // Phase 1: Build execution graph from pipeline
        void build_graph(ExecutionGraph &graph, const Pipeline &pipeline);

        // Phase 2: Execute all nodes in topological order (all outputs to temp)
        bool execute_graph(ExecutionGraph &graph,
                          const Pipeline &pipeline,
                          const std::shared_ptr<output::IOutput> &output,
                          size_t total_conversion_nodes,
                          PipelineResult &result);

        // Phase 3: Finalize outputs (move/keep/delete based on full visibility)
        // Returns false if there was an error (e.g., non-copyable format without -o)
        bool finalize_outputs(ExecutionGraph &graph,
                             const Pipeline &pipeline,
                             PipelineResult &result);

        // Execute a single node
        bool execute_node(ExecutionNode &node,
                         ExecutionGraph &graph,
                         const Pipeline &pipeline,
                         const std::shared_ptr<output::IOutput> &output,
                         size_t &current_conversion_node,
                         size_t total_conversion_nodes,
                         PipelineResult &result);

        // Execute tee node (just pass through)
        bool execute_tee_node(ExecutionNode &node,
                             ExecutionGraph &graph,
                             PipelineResult &result);

        // Execute collect node (N→1 fan-in)
        bool execute_collect_node(ExecutionNode &node,
                                 ExecutionGraph &graph,
                                 PipelineResult &result);

        // Execute clipboard node
        bool execute_clipboard_node(ExecutionNode &node,
                                   ExecutionGraph &graph,
                                   const Pipeline &pipeline,
                                   PipelineResult &result);

        // Execute passthrough node (just pass through unchanged)
        bool execute_passthrough_node(ExecutionNode &node,
                                     ExecutionGraph &graph,
                                     PipelineResult &result);

        // Execute conversion node
        bool execute_conversion_node(ExecutionNode &node,
                                    ExecutionGraph &graph,
                                    const Pipeline &pipeline,
                                    const std::shared_ptr<output::IOutput> &output,
                                    size_t current_node,
                                    size_t total_nodes,
                                    PipelineResult &result);

        // Execute a conversion for each scattered input (N→N parallel transform)
        bool execute_scattered_conversion(ExecutionNode &node,
                                         ExecutionGraph &graph,
                                         const Pipeline &pipeline,
                                         const std::shared_ptr<output::IOutput> &output,
                                         size_t current_node,
                                         size_t total_nodes,
                                         PipelineResult &result);

        // Generate temp file path: run_dir / s{N}_e{M}.{target}
        std::filesystem::path generate_temp_path(
            const std::string &target,
            size_t stage_index,
            size_t element_index);

        // Generate temp file path for scatter index: run_dir / s{N}_e{M}_i{I}.{target}
        std::filesystem::path generate_scatter_temp_path(
            const std::string &target,
            size_t stage_index,
            size_t element_index,
            size_t scatter_index);

        // Generate final output path (current directory)
        std::filesystem::path generate_final_output_path(
            const std::filesystem::path &original_source,
            const std::string &target,
            const std::string &transform_suffix);

        // Resolve the -o option into an actual path
        std::filesystem::path resolve_output_option(
            const std::filesystem::path &original_source,
            const std::optional<std::filesystem::path> &output_option,
            const std::string &target,
            const std::string &transform_suffix);

        // Check if a temp path is from our run directory
        bool is_temp_path(const std::filesystem::path &path) const;

        // Get the input path for a node (resolve from predecessor or source)
        std::filesystem::path get_node_input(const ExecutionNode &node,
                                            const ExecutionGraph &graph);

        // Cleanup temp files
        void cleanup_temp_files();

        // Resolve extension for a node:
        // 1. Explicit extension from user → use as-is
        // 2. Look up target in plugin's targets map → use first entry
        // 3. Fallback to target_to_extension(target)
        std::string resolve_extension(const ExecutionNode &node);

        // Check if a target format can have its content copied to clipboard
        static bool is_clipboard_content_copyable(const std::string &target);

        // Check if a target is a known file format (vs a transformation like "gray")
        static bool is_known_file_format(const std::string &target);

        // Get the actual output format for a node (handles transformations)
        std::string get_output_format(const ExecutionNode &node,
                                      const ExecutionGraph &graph);
    };

} // namespace uniconv::core
