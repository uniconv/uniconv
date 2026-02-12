#include "pipeline_executor.h"
#include "builtins/clipboard.h"
#include "builtins/collect.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <sstream>

#ifdef _WIN32
#include <process.h>
#define GETPID _getpid
#else
#include <unistd.h>
#define GETPID getpid
#endif

namespace uniconv::core
{

    PipelineExecutor::PipelineExecutor(std::shared_ptr<Engine> engine)
        : engine_(std::move(engine))
    {
        // Per-run temp directory: /tmp/uniconv/run_<pid>_<timestamp>/
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        std::ostringstream dir_name;
        dir_name << "run_" << GETPID() << "_" << ts;
        temp_dir_ = std::filesystem::temp_directory_path() / "uniconv" / dir_name.str();
        std::filesystem::create_directories(temp_dir_);
    }

    PipelineResult PipelineExecutor::execute(
        const Pipeline &pipeline,
        const std::shared_ptr<output::IOutput> &output)
    {
        auto start_time = std::chrono::steady_clock::now();

        PipelineResult final_result;
        final_result.success = false;

        // Phase 1: Build execution graph
        ExecutionGraph graph;
        build_graph(graph, pipeline);

        // Count non-builtin nodes for progress reporting
        size_t total_conversion_nodes = std::accumulate(
            graph.nodes().begin(), graph.nodes().end(), size_t{0},
            [](size_t count, const ExecutionNode &n) {
                return count + (n.is_builtin() ? 0 : 1);
            });

        // Phase 2: Execute all nodes (outputs go to temp)
        if (!execute_graph(graph, pipeline, output, total_conversion_nodes, final_result))
        {
            cleanup_temp_files();
            return final_result;
        }

        // Phase 3: Finalize outputs (with full visibility of graph)
        if (!finalize_outputs(graph, pipeline, final_result))
        {
            cleanup_temp_files();
            return final_result;
        }

        final_result.success = true;

        auto end_time = std::chrono::steady_clock::now();
        final_result.total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                             end_time - start_time)
                                             .count();

        cleanup_temp_files();
        return final_result;
    }

    void PipelineExecutor::build_graph(ExecutionGraph &graph, const Pipeline &pipeline)
    {
        graph.build_from_pipeline(pipeline);
    }

    bool PipelineExecutor::execute_graph(
        ExecutionGraph &graph,
        const Pipeline &pipeline,
        const std::shared_ptr<output::IOutput> &output,
        size_t total_conversion_nodes,
        PipelineResult &result)
    {
        // Get execution order (topological sort)
        auto order = graph.execution_order();
        size_t current_conversion_node = 0;

        for (size_t node_id : order)
        {
            auto &node = graph.node(node_id);
            if (!execute_node(node, graph, pipeline, output,
                             current_conversion_node, total_conversion_nodes, result))
            {
                return false;
            }
        }

        return true;
    }

    bool PipelineExecutor::execute_node(
        ExecutionNode &node,
        ExecutionGraph &graph,
        const Pipeline &pipeline,
        const std::shared_ptr<output::IOutput> &output,
        size_t &current_conversion_node,
        size_t total_conversion_nodes,
        PipelineResult &result)
    {
        if (node.is_tee)
        {
            return execute_tee_node(node, graph, result);
        }
        else if (node.is_collect)
        {
            return execute_collect_node(node, graph, result);
        }
        else if (node.is_clipboard)
        {
            return execute_clipboard_node(node, graph, pipeline, result);
        }
        else if (node.is_passthrough)
        {
            return execute_passthrough_node(node, graph, result);
        }
        else
        {
            ++current_conversion_node;

            // If pipeline width > 1 (we're in a scattered context),
            // execute this node once per scattered input
            if (pipeline_width_ > 1 && !scattered_paths_.empty())
            {
                return execute_scattered_conversion(node, graph, pipeline, output,
                                                   current_conversion_node, total_conversion_nodes, result);
            }

            return execute_conversion_node(node, graph, pipeline, output,
                                          current_conversion_node, total_conversion_nodes, result);
        }
    }

    bool PipelineExecutor::execute_tee_node(
        ExecutionNode &node,
        ExecutionGraph &graph,
        PipelineResult &result)
    {
        // Tee just passes through - get input from predecessor
        node.input = get_node_input(node, graph);
        node.temp_output = node.input; // Tee outputs same as input
        node.executed = true;
        node.status = ResultStatus::Success;

        // Record stage result
        StageResult stage_result;
        stage_result.stage_index = node.stage_idx;
        stage_result.target = "tee";
        stage_result.plugin_used = "builtin:tee";
        stage_result.input = node.input;
        stage_result.output = node.temp_output;
        stage_result.status = ResultStatus::Success;
        stage_result.duration_ms = 0;
        result.stage_results.push_back(stage_result);

        return true;
    }

    bool PipelineExecutor::execute_collect_node(
        ExecutionNode &node,
        ExecutionGraph &graph,
        PipelineResult &result)
    {
        // Collect gathers all scattered paths into a temp directory
        // The scattered_paths_ vector holds the current N parallel file paths

        if (scattered_paths_.empty())
        {
            // Fall back to collecting from input nodes
            for (size_t pred_id : node.input_nodes)
            {
                const auto &pred = graph.node(pred_id);
                if (!pred.scatter_outputs.empty())
                {
                    scattered_paths_ = pred.scatter_outputs;
                    break;
                }
                else if (!pred.temp_output.empty())
                {
                    scattered_paths_.push_back(pred.temp_output);
                }
            }
        }

        if (scattered_paths_.empty())
        {
            node.status = ResultStatus::Error;
            node.error = "Collect has no inputs to gather";
            result.error = node.error;
            return false;
        }

        node.collect_inputs = scattered_paths_;

        // Execute the collect builtin
        auto collect_result = builtins::Collect::execute(scattered_paths_, temp_dir_);

        if (!collect_result.success)
        {
            node.status = ResultStatus::Error;
            node.error = collect_result.error;
            result.error = collect_result.error;
            return false;
        }

        node.input = scattered_paths_[0]; // Representative input for logging
        node.temp_output = collect_result.output_dir;
        node.executed = true;
        node.status = ResultStatus::Success;
        node.plugin_used = "builtin:collect";

        // Reset pipeline width back to 1
        pipeline_width_ = 1;
        scattered_paths_.clear();

        // Record stage result
        StageResult stage_result;
        stage_result.stage_index = node.stage_idx;
        stage_result.target = "collect";
        stage_result.plugin_used = "builtin:collect";
        stage_result.input = node.input;
        stage_result.output = collect_result.output_dir;
        stage_result.status = ResultStatus::Success;
        stage_result.duration_ms = 0;
        result.stage_results.push_back(stage_result);

        return true;
    }

    bool PipelineExecutor::execute_passthrough_node(
        ExecutionNode &node,
        ExecutionGraph &graph,
        PipelineResult &result)
    {
        // Passthrough just passes input through unchanged
        node.input = get_node_input(node, graph);
        node.temp_output = node.input; // Same as input
        node.executed = true;
        node.status = ResultStatus::Success;
        node.plugin_used = "builtin:passthrough";

        // Record stage result
        StageResult stage_result;
        stage_result.stage_index = node.stage_idx;
        stage_result.target = node.target; // Keep original target name (_, echo, etc.)
        stage_result.plugin_used = "builtin:passthrough";
        stage_result.input = node.input;
        stage_result.output = node.temp_output;
        stage_result.status = ResultStatus::Success;
        stage_result.duration_ms = 0;
        result.stage_results.push_back(stage_result);

        return true;
    }

    bool PipelineExecutor::execute_clipboard_node(
        ExecutionNode &node,
        ExecutionGraph &graph,
        [[maybe_unused]] const Pipeline &pipeline,
        PipelineResult &result)
    {
        node.input = get_node_input(node, graph);

        // No validation needed here - format validation happens in finalize phase
        // where we have full visibility of -o option and can give appropriate error

        // Execute clipboard builtin
        auto clipboard_result = builtins::Clipboard::execute(node.input);

        node.temp_output = clipboard_result.output; // Pass-through
        node.executed = true;
        node.plugin_used = "builtin:clipboard";

        // Check if content was actually copied (not just path)
        std::string ext = node.input.extension().string();
        std::string format;
        if (!ext.empty() && ext[0] == '.')
            format = ext.substr(1);
        node.content_copied_to_clipboard = is_clipboard_content_copyable(format);

        // Record stage result
        StageResult stage_result;
        stage_result.stage_index = node.stage_idx;
        stage_result.target = "clipboard";
        stage_result.plugin_used = "builtin:clipboard";
        stage_result.input = node.input;
        stage_result.output = clipboard_result.output;
        stage_result.status = clipboard_result.success ? ResultStatus::Success : ResultStatus::Error;
        stage_result.duration_ms = 0;
        if (!clipboard_result.success)
        {
            stage_result.error = clipboard_result.error;
        }
        result.stage_results.push_back(stage_result);

        if (!clipboard_result.success)
        {
            node.status = ResultStatus::Error;
            node.error = clipboard_result.error;
            result.error = clipboard_result.error;
            return false;
        }

        node.status = ResultStatus::Success;
        return true;
    }

    bool PipelineExecutor::execute_conversion_node(
        ExecutionNode &node,
        ExecutionGraph &graph,
        const Pipeline &pipeline,
        const std::shared_ptr<output::IOutput> &output,
        size_t current_node,
        size_t total_nodes,
        PipelineResult &result)
    {
        node.input = get_node_input(node, graph);

        // Resolve extension for file naming
        node.resolved_extension = resolve_extension(node);

        // Report progress: stage started
        if (output)
        {
            output->stage_started(current_node, total_nodes, node.target);
        }

        auto node_start = std::chrono::steady_clock::now();

        // Always output to temp during execution phase
        node.temp_output = generate_temp_path(
            node.resolved_extension,
            node.stage_idx,
            node.element_idx);

        // Build request
        Request request;
        request.source = node.input;
        request.target = node.target;
        request.plugin = node.plugin;
        request.core_options = pipeline.core_options;
        request.core_options.output = node.temp_output;
        request.plugin_options = node.plugin_options;

        // For temp files from previous stages, use extension as format hint
        if (is_temp_path(node.input))
        {
            std::string ext = node.input.extension().string();
            if (!ext.empty() && ext[0] == '.')
                ext = ext.substr(1);
            if (!ext.empty())
                request.input_format = ext;
        }

        // For first-stage nodes, use pipeline.input_format if not already set
        if (!request.input_format.has_value() && node.stage_idx == 0 &&
            pipeline.input_format.has_value())
        {
            request.input_format = pipeline.input_format;
        }

        // Execute through engine
        auto etl_result = engine_->execute(request);

        auto node_end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(node_end - node_start).count();

        // Use actual output from plugin if available
        std::filesystem::path actual_output = etl_result.output.value_or(node.temp_output);
        node.temp_output = actual_output;
        node.plugin_used = etl_result.plugin_used;
        node.executed = true;
        node.duration_ms = duration;

        bool success = (etl_result.status == ResultStatus::Success);
        std::string error_msg = etl_result.error.value_or("");

        // Report progress: stage completed
        if (output)
        {
            output->stage_completed(current_node, total_nodes, node.target,
                                   duration, success, error_msg);
        }

        // Record stage result
        StageResult stage_result;
        stage_result.stage_index = node.stage_idx;
        stage_result.target = node.target;
        stage_result.plugin_used = etl_result.plugin_used;
        stage_result.input = node.input;
        stage_result.output = actual_output;
        stage_result.status = etl_result.status;
        stage_result.error = etl_result.error;
        stage_result.duration_ms = duration;
        result.stage_results.push_back(stage_result);

        if (!success)
        {
            node.status = ResultStatus::Error;
            node.error = error_msg.empty() ? "Unknown error" : error_msg;
            result.error = node.error;
            return false;
        }

        // Detect scatter result: plugin returned "outputs" array (1→N)
        if (etl_result.is_scatter())
        {
            node.scatter_outputs = etl_result.outputs;
            scattered_paths_ = etl_result.outputs;
            pipeline_width_ = etl_result.outputs.size();
        }

        node.status = ResultStatus::Success;
        return true;
    }

    bool PipelineExecutor::execute_scattered_conversion(
        ExecutionNode &node,
        [[maybe_unused]] ExecutionGraph &graph,
        const Pipeline &pipeline,
        const std::shared_ptr<output::IOutput> &output,
        size_t current_node,
        size_t total_nodes,
        PipelineResult &result)
    {
        // Resolve extension for file naming
        node.resolved_extension = resolve_extension(node);

        // Execute the conversion once for each scattered input
        std::vector<std::filesystem::path> new_scattered_paths;
        new_scattered_paths.reserve(scattered_paths_.size());

        for (size_t i = 0; i < scattered_paths_.size(); ++i)
        {
            const auto &scattered_input = scattered_paths_[i];

            // Report progress
            if (output)
            {
                output->stage_started(current_node, total_nodes,
                                     node.target + " [" + std::to_string(i + 1) + "/" +
                                     std::to_string(scattered_paths_.size()) + "]");
            }

            auto scatter_start = std::chrono::steady_clock::now();

            // Generate a unique temp path for this scatter index
            auto temp_output = generate_scatter_temp_path(
                node.resolved_extension, node.stage_idx, node.element_idx, i);

            // Build request
            Request request;
            request.source = scattered_input;
            request.target = node.target;
            request.plugin = node.plugin;
            request.core_options = pipeline.core_options;
            request.core_options.output = temp_output;
            request.plugin_options = node.plugin_options;

            // Format hint for temp files
            if (is_temp_path(scattered_input))
            {
                std::string ext = scattered_input.extension().string();
                if (!ext.empty() && ext[0] == '.')
                    ext = ext.substr(1);
                if (!ext.empty())
                    request.input_format = ext;
            }

            // Execute through engine
            auto etl_result = engine_->execute(request);

            auto scatter_end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                scatter_end - scatter_start).count();

            std::filesystem::path actual_output = etl_result.output.value_or(temp_output);

            bool success = (etl_result.status == ResultStatus::Success);
            std::string error_msg = etl_result.error.value_or("");

            if (output)
            {
                output->stage_completed(current_node, total_nodes,
                                       node.target + " [" + std::to_string(i + 1) + "/" +
                                       std::to_string(scattered_paths_.size()) + "]",
                                       duration, success, error_msg);
            }

            // Record stage result for each scatter execution
            StageResult stage_result;
            stage_result.stage_index = node.stage_idx;
            stage_result.target = node.target;
            stage_result.plugin_used = etl_result.plugin_used;
            stage_result.input = scattered_input;
            stage_result.output = actual_output;
            stage_result.status = etl_result.status;
            stage_result.error = etl_result.error;
            stage_result.duration_ms = duration;
            result.stage_results.push_back(stage_result);

            if (!success)
            {
                node.status = ResultStatus::Error;
                node.error = error_msg.empty() ? "Unknown error" : error_msg;
                result.error = node.error;
                return false;
            }

            new_scattered_paths.push_back(actual_output);
        }

        // Update scattered paths for next stage
        scattered_paths_ = new_scattered_paths;
        node.scatter_outputs = new_scattered_paths;

        // Use the first output as the node's temp_output for graph connectivity
        if (!new_scattered_paths.empty())
        {
            node.temp_output = new_scattered_paths[0];
        }

        node.plugin_used = ""; // Multiple executions, no single plugin
        node.executed = true;
        node.status = ResultStatus::Success;
        return true;
    }

    bool PipelineExecutor::finalize_outputs(
        ExecutionGraph &graph,
        const Pipeline &pipeline,
        PipelineResult &result)
    {
        // Get all file-producing nodes (not builtins)
        auto file_nodes = graph.file_producing_nodes();

        for (size_t node_id : file_nodes)
        {
            auto &node = graph.node(node_id);

            // Skip nodes that didn't execute successfully
            if (node.status != ResultStatus::Success)
            {
                continue;
            }

            // Determine if this node needs a file output
            // Use "effectively" methods to look through passthrough nodes
            bool is_terminal = graph.is_effectively_terminal(node_id);
            bool only_clipboard_consumer = graph.is_effectively_only_consumed_by_clipboard(node_id);
            bool content_was_copied = graph.was_content_copied_to_clipboard(node_id);
            bool clipboard_has_save = graph.clipboard_consumer_has_save(node_id);
            bool has_output_option = pipeline.core_options.output.has_value();

            // Decision logic for final output
            bool should_create_file = true;

            if (only_clipboard_consumer)
            {
                if (content_was_copied)
                {
                    should_create_file = has_output_option || clipboard_has_save;
                }
                else
                {
                    if (!has_output_option && !clipboard_has_save)
                    {
                        std::string output_format = get_output_format(node, graph);
                        result.error = "clipboard cannot copy '" + output_format +
                                       "' content directly; use -o or --save to save the file";
                        result.success = false;
                        return false;
                    }
                    should_create_file = true;
                }
            }

            if (!should_create_file)
            {
                continue;
            }

            // Handle scattered terminal node (N final outputs)
            if (is_terminal && !node.scatter_outputs.empty())
            {
                // This node produced multiple outputs via scatter or scattered execution.
                // Each scattered output becomes a final output.
                std::string output_format = get_output_format(node, graph);
                std::string transform = is_known_file_format(node.target) ? "" : node.target;

                // Determine output directory
                std::filesystem::path output_dir;
                if (pipeline.core_options.output.has_value())
                {
                    output_dir = *pipeline.core_options.output;
                    // If -o is a directory (or ends with /), use it
                    // Otherwise, use the parent directory
                    std::string path_str = output_dir.string();
                    if (!path_str.empty() && (path_str.back() == '/' || path_str.back() == '\\'))
                    {
                        // Already a directory path
                    }
                    else if (std::filesystem::is_directory(output_dir))
                    {
                        // Existing directory
                    }
                    else
                    {
                        // -o specifies a file path; for scatter, use its parent
                        output_dir = output_dir.parent_path();
                        if (output_dir.empty())
                            output_dir = std::filesystem::current_path();
                    }
                }
                else
                {
                    output_dir = std::filesystem::current_path();
                }

                std::filesystem::create_directories(output_dir);

                for (size_t i = 0; i < node.scatter_outputs.size(); ++i)
                {
                    const auto &scatter_out = node.scatter_outputs[i];
                    // Use the original filename from the scattered output
                    std::string filename = scatter_out.filename().string();

                    // If it's a temp path, generate a meaningful name
                    if (is_temp_path(scatter_out))
                    {
                        std::string stem = graph.source().stem().string();
                        if (stem.empty()) stem = "output";
                        std::ostringstream fname;
                        fname << stem;
                        if (!transform.empty())
                            fname << "_" << transform;
                        fname << "_" << std::setw(4) << std::setfill('0') << i
                              << "." << output_format;
                        filename = fname.str();
                    }

                    std::filesystem::path final_path = output_dir / filename;

                    try
                    {
                        if (std::filesystem::exists(final_path))
                        {
                            if (pipeline.core_options.force)
                                std::filesystem::remove(final_path);
                        }
                        std::filesystem::rename(scatter_out, final_path);
                        result.final_outputs.push_back(final_path);
                    }
                    catch (const std::filesystem::filesystem_error &)
                    {
                        try
                        {
                            std::filesystem::copy_file(scatter_out, final_path,
                                std::filesystem::copy_options::overwrite_existing);
                            std::filesystem::remove(scatter_out);
                            result.final_outputs.push_back(final_path);
                        }
                        catch (const std::filesystem::filesystem_error &)
                        {
                            result.final_outputs.push_back(scatter_out);
                        }
                    }
                }
                continue;
            }

            // Determine final output path (single output)
            std::filesystem::path final_path;

            // Check for explicit output in node options
            auto output_it = node.options.find("output");
            if (output_it != node.options.end() && !output_it->second.empty())
            {
                final_path = output_it->second;
            }
            else if (is_terminal || only_clipboard_consumer)
            {
                // Terminal node or node that only feeds clipboard - this is a "final" output
                std::string output_format = get_output_format(node, graph);

                // If target is a transformation (not a file format), use it as suffix
                std::string transform = is_known_file_format(node.target) ? "" : node.target;

                if (pipeline.core_options.output.has_value())
                {
                    final_path = resolve_output_option(
                        graph.source(),
                        pipeline.core_options.output,
                        output_format,
                        transform);
                }
                else
                {
                    final_path = generate_final_output_path(
                        graph.source(),
                        output_format,
                        transform);
                }
            }
            else
            {
                // Intermediate node - keep in temp (will be used by consumers)
                continue;
            }

            // Move temp file to final location
            try
            {
                // Ensure parent directory exists
                if (final_path.has_parent_path())
                {
                    std::filesystem::create_directories(final_path.parent_path());
                }

                // Check if we need to overwrite
                if (std::filesystem::exists(final_path))
                {
                    if (pipeline.core_options.force)
                    {
                        std::filesystem::remove(final_path);
                    }
                    // If not force, rename will fail - let it happen naturally
                }

                std::filesystem::rename(node.temp_output, final_path);
                node.final_output = final_path;

                // Update the stage result with final path
                for (auto &sr : result.stage_results)
                {
                    if (sr.stage_index == node.stage_idx &&
                        sr.target == node.target &&
                        sr.output == node.temp_output)
                    {
                        sr.output = final_path;
                        break;
                    }
                }

                result.final_outputs.push_back(final_path);

                // For non-content-copyable formats consumed by clipboard,
                // copy the final path to clipboard (not the temp path)
                if (only_clipboard_consumer && !content_was_copied)
                {
                    builtins::Clipboard::copy_path(final_path);
                }
            }
            catch (const std::filesystem::filesystem_error &e)
            {
                // If rename fails, try copy + delete
                try
                {
                    std::filesystem::copy_file(node.temp_output, final_path,
                                               std::filesystem::copy_options::overwrite_existing);
                    std::filesystem::remove(node.temp_output);
                    node.final_output = final_path;

                    for (auto &sr : result.stage_results)
                    {
                        if (sr.stage_index == node.stage_idx &&
                            sr.target == node.target &&
                            sr.output == node.temp_output)
                        {
                            sr.output = final_path;
                            break;
                        }
                    }

                    result.final_outputs.push_back(final_path);

                    // For non-content-copyable formats consumed by clipboard,
                    // copy the final path to clipboard (not the temp path)
                    if (only_clipboard_consumer && !content_was_copied)
                    {
                        builtins::Clipboard::copy_path(final_path);
                    }
                }
                catch (const std::filesystem::filesystem_error &)
                {
                    // Keep temp file, add to outputs
                    node.final_output = node.temp_output;
                    result.final_outputs.push_back(node.temp_output);
                }
            }
        }

        return true;
    }

    std::filesystem::path PipelineExecutor::get_node_input(
        const ExecutionNode &node,
        const ExecutionGraph &graph)
    {
        // If node has explicit input path, use it
        if (!node.input.empty())
        {
            return node.input;
        }

        // Otherwise, get from predecessor
        if (!node.input_nodes.empty())
        {
            // Use first predecessor's output
            const auto &pred = graph.node(node.input_nodes[0]);
            return pred.temp_output;
        }

        // Fallback to source
        return graph.source();
    }

    static std::string target_to_extension(const std::string &target)
    {
        // Compound archive targets use hyphens (tar-gz) but the file
        // extension uses dots (tar.gz).  Map them here so the final
        // output path gets the correct extension.
        static const std::vector<std::pair<std::string, std::string>> compound_map = {
            {"tar-gz", "tar.gz"},
            {"tar-bz2", "tar.bz2"},
            {"tar-xz", "tar.xz"},
        };
        for (const auto &[key, ext] : compound_map)
        {
            if (target == key)
                return ext;
        }
        return target;
    }

    std::filesystem::path PipelineExecutor::generate_temp_path(
        const std::string &target,
        size_t stage_index,
        size_t element_index)
    {
        // Format: run_dir / "s{idx}_e{idx}.{ext}"
        std::ostringstream filename;
        filename << "s" << stage_index
                 << "_e" << element_index
                 << "." << target_to_extension(target);

        return temp_dir_ / filename.str();
    }

    std::filesystem::path PipelineExecutor::generate_scatter_temp_path(
        const std::string &target,
        size_t stage_index,
        size_t element_index,
        size_t scatter_index)
    {
        // Format: run_dir / "s{idx}_e{idx}_i{scatter_idx}.{ext}"
        std::ostringstream filename;
        filename << "s" << stage_index
                 << "_e" << element_index
                 << "_i" << scatter_index
                 << "." << target_to_extension(target);

        return temp_dir_ / filename.str();
    }

    std::filesystem::path PipelineExecutor::generate_final_output_path(
        const std::filesystem::path &original_source,
        const std::string &target,
        const std::string &transform_suffix)
    {
        // Output to current directory
        std::filesystem::path output = std::filesystem::current_path();

        std::string stem = original_source.empty() ? "generated" : original_source.stem().string();
        output /= stem;
        if (!transform_suffix.empty())
        {
            output += "_" + transform_suffix;
        }
        output += "." + target_to_extension(target);

        return output;
    }

    std::filesystem::path PipelineExecutor::resolve_output_option(
        const std::filesystem::path &original_source,
        const std::optional<std::filesystem::path> &output_option,
        const std::string &target,
        const std::string &transform_suffix)
    {
        if (!output_option.has_value())
        {
            return generate_final_output_path(original_source, target, transform_suffix);
        }

        const auto &out_path = *output_option;

        // Check if output is a directory
        bool is_directory = false;
        std::string path_str = out_path.string();
        if (!path_str.empty() && (path_str.back() == '/' || path_str.back() == '\\'))
        {
            is_directory = true;
        }
        else if (std::filesystem::is_directory(out_path))
        {
            is_directory = true;
        }

        if (is_directory)
        {
            // Directory: use original stem + transform suffix + target extension
            std::string stem = original_source.empty() ? "generated" : original_source.stem().string();
            std::filesystem::path output = out_path / stem;
            if (!transform_suffix.empty())
            {
                output += "_" + transform_suffix;
            }
            output += "." + target_to_extension(target);
            return output;
        }
        else if (out_path.has_extension())
        {
            // Has extension: use as-is
            return out_path;
        }
        else
        {
            // No extension: append target as extension
            std::filesystem::path output = out_path;
            output += "." + target;
            return output;
        }
    }

    bool PipelineExecutor::is_temp_path(const std::filesystem::path &path) const
    {
        // Check if path is inside our per-run temp directory
        auto path_str = path.string();
        auto dir_str = temp_dir_.string();
        return path_str.size() > dir_str.size() &&
               path_str.compare(0, dir_str.size(), dir_str) == 0;
    }

    void PipelineExecutor::cleanup_temp_files()
    {
        // Remove the entire per-run temp directory
        try
        {
            if (std::filesystem::exists(temp_dir_))
            {
                std::filesystem::remove_all(temp_dir_);
            }
        }
        catch (const std::filesystem::filesystem_error &)
        {
            // Ignore cleanup errors
        }
    }

    std::string PipelineExecutor::resolve_extension(const ExecutionNode &node)
    {
        // 1. Explicit extension from user → use as-is
        if (node.extension.has_value() && !node.extension->empty())
        {
            return *node.extension;
        }

        // 2. Look up target in plugin's targets map → use first entry (default)
        auto *plugin = engine_->plugin_manager().find_plugin(node.target, node.plugin);
        if (plugin)
        {
            auto plugin_info = plugin->info();
            auto it = plugin_info.targets.find(node.target);
            if (it != plugin_info.targets.end() && !it->second.empty())
            {
                return it->second[0]; // First extension is the default
            }
        }

        // 3. Fallback to target_to_extension(target) (backward compat)
        return target_to_extension(node.target);
    }

    // Known file format extensions
    static const std::vector<std::string> known_file_formats = {
        // Image formats
        "jpg", "jpeg", "png", "gif", "bmp", "tiff", "tif", "webp", "heic", "heif",
        "ico", "svg", "raw", "cr2", "nef", "arw",
        // Video formats
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "mpeg", "mpg",
        "3gp", "ogv",
        // Audio formats
        "mp3", "wav", "flac", "aac", "ogg", "wma", "m4a", "opus",
        // Document formats
        "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "ods", "odp",
        // Text formats
        "txt", "md", "json", "xml", "csv", "html", "htm", "yaml", "yml", "log",
        "text", "rtf",
        // Archive formats
        "zip", "tar", "gz", "bz2", "xz", "7z", "tgz", "tbz2", "txz",
        "tar-gz", "tar-bz2", "tar-xz"};

    bool PipelineExecutor::is_clipboard_content_copyable(const std::string &target)
    {
        // Image formats - clipboard can copy actual image data
        static const std::vector<std::string> image_formats = {
            "jpg", "jpeg", "png", "gif", "bmp", "tiff", "webp"};

        // Text formats - clipboard can copy actual text content
        static const std::vector<std::string> text_formats = {
            "txt", "md", "json", "xml", "csv", "html", "htm",
            "yaml", "yml", "log", "text"};

        // Convert target to lowercase for comparison
        std::string lower_target = target;
        std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        for (const auto &fmt : image_formats)
        {
            if (lower_target == fmt)
                return true;
        }

        for (const auto &fmt : text_formats)
        {
            if (lower_target == fmt)
                return true;
        }

        return false;
    }

    bool PipelineExecutor::is_known_file_format(const std::string &target)
    {
        std::string lower_target = target;
        std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        for (const auto &fmt : known_file_formats)
        {
            if (lower_target == fmt)
                return true;
        }
        return false;
    }

    std::string PipelineExecutor::get_output_format(
        const ExecutionNode &node,
        const ExecutionGraph &graph)
    {
        // If resolved_extension is set and is a known format, prefer it
        if (!node.resolved_extension.empty() && is_known_file_format(node.resolved_extension))
        {
            return node.resolved_extension;
        }

        // If target is a known file format, use it
        if (is_known_file_format(node.target))
        {
            return node.target;
        }

        // Check the actual output file's extension from the plugin
        if (!node.temp_output.empty())
        {
            std::string out_ext = node.temp_output.extension().string();
            if (!out_ext.empty() && out_ext[0] == '.')
                out_ext = out_ext.substr(1);
            if (!out_ext.empty() && is_known_file_format(out_ext))
                return out_ext;
        }

        // Otherwise, target is a transformation (e.g., "gray", "resize")
        // Use the input file's format
        std::filesystem::path input_path = node.input;
        if (input_path.empty() && !node.input_nodes.empty())
        {
            const auto &pred = graph.node(node.input_nodes[0]);
            input_path = pred.temp_output;
        }
        if (input_path.empty())
        {
            input_path = graph.source();
        }

        std::string ext = input_path.extension().string();
        if (!ext.empty() && ext[0] == '.')
            ext = ext.substr(1);

        // If extension is a known format, use it; otherwise fallback to source
        if (!ext.empty() && is_known_file_format(ext))
            return ext;

        // Final fallback: original source extension
        ext = graph.source().extension().string();
        if (!ext.empty() && ext[0] == '.')
            ext = ext.substr(1);

        return ext;
    }

} // namespace uniconv::core
