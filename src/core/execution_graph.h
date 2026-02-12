#pragma once

#include "pipeline.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uniconv::core {

// Represents a single execution node in the pipeline graph
struct ExecutionNode {
    size_t id = 0;
    size_t stage_idx = 0;
    size_t element_idx = 0;

    std::string target;                              // "jpg", "clipboard", "tee"
    std::optional<std::string> plugin;               // Explicit plugin if specified
    std::optional<std::string> extension;            // Explicit .ext from identifier
    std::string resolved_extension;                  // Resolved for file naming
    std::vector<std::string> plugin_options;         // Options for the plugin
    std::map<std::string, std::string> options;      // Parsed options

    std::filesystem::path input;                     // Input path
    std::filesystem::path temp_output;               // Temp output (always temp during execution)
    std::filesystem::path final_output;              // Final output (resolved in finalize phase)

    std::string plugin_used;                         // Plugin that was used
    ResultStatus status = ResultStatus::Success;
    std::optional<std::string> error;
    int64_t duration_ms = 0;

    // Graph relationships
    std::vector<size_t> input_nodes;                 // Nodes that feed into this node
    std::vector<size_t> output_nodes;                // Nodes that consume this node's output

    // Builtin flags
    bool is_tee = false;
    bool is_collect = false;
    bool is_clipboard = false;
    bool is_passthrough = false;

    // Scatter/collect state
    std::vector<std::filesystem::path> scatter_outputs;  // Populated at runtime when plugin returns "outputs"
    std::vector<std::filesystem::path> collect_inputs;   // Populated at runtime for collect node

    // Execution state
    bool executed = false;
    bool content_copied_to_clipboard = false;        // For clipboard nodes

    // Helper methods
    bool is_builtin() const { return is_tee || is_collect || is_clipboard || is_passthrough; }
    bool is_terminal() const { return output_nodes.empty(); }
    // Only conversion nodes produce new files
    // tee, clipboard, collect, and passthrough do not produce new files
    bool has_file_output() const { return !is_tee && !is_collect && !is_clipboard && !is_passthrough; }
};

// The execution graph tracks all nodes and their relationships
class ExecutionGraph {
public:
    ExecutionGraph() = default;

    // Build graph from pipeline
    void build_from_pipeline(const Pipeline& pipeline);

    // Get all nodes
    std::vector<ExecutionNode>& nodes() { return nodes_; }
    const std::vector<ExecutionNode>& nodes() const { return nodes_; }

    // Get node by id
    ExecutionNode& node(size_t id) { return nodes_[id]; }
    const ExecutionNode& node(size_t id) const { return nodes_[id]; }

    // Get terminal nodes (no consumers)
    std::vector<size_t> terminal_nodes() const;

    // Get nodes that produce files (not builtins)
    std::vector<size_t> file_producing_nodes() const;

    // Check if a node's output is only consumed by clipboard
    bool is_only_consumed_by_clipboard(size_t node_id) const;

    // Check if clipboard content was copied for a given file node
    bool was_content_copied_to_clipboard(size_t node_id) const;

    // Check if clipboard consumer has --save option
    bool clipboard_consumer_has_save(size_t node_id) const;

    // Check if a node is effectively terminal (terminal when looking through passthrough)
    // A node is effectively terminal if all paths through passthrough nodes lead to terminal
    bool is_effectively_terminal(size_t node_id) const;

    // Check if a node's output is only consumed by clipboard (looking through passthrough)
    bool is_effectively_only_consumed_by_clipboard(size_t node_id) const;

    // Get the source file path
    const std::filesystem::path& source() const { return source_; }

    // Get execution order (topological sort)
    std::vector<size_t> execution_order() const;

private:
    std::vector<ExecutionNode> nodes_;
    std::filesystem::path source_;

    size_t add_node();
};

} // namespace uniconv::core
