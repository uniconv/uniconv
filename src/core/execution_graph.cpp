#include "execution_graph.h"
#include <queue>

namespace uniconv::core {

void ExecutionGraph::build_from_pipeline(const Pipeline& pipeline) {
    nodes_.clear();
    source_ = pipeline.source;

    // Track outputs from previous stage (node IDs)
    std::vector<size_t> prev_stage_outputs;

    for (size_t stage_idx = 0; stage_idx < pipeline.stages.size(); ++stage_idx) {
        const auto& stage = pipeline.stages[stage_idx];
        std::vector<size_t> current_stage_outputs;

        // Handle tee - creates a branching point
        if (stage.has_tee()) {
            // Tee node takes single input and replicates for next stage
            size_t tee_id = add_node();
            auto& tee_node = nodes_[tee_id];
            tee_node.stage_idx = stage_idx;
            tee_node.element_idx = 0;
            tee_node.target = "tee";
            tee_node.is_tee = true;

            // Connect input from previous stage (or source)
            if (prev_stage_outputs.empty()) {
                tee_node.input = source_;
            } else {
                tee_node.input_nodes = prev_stage_outputs;
                for (size_t prev_id : prev_stage_outputs) {
                    nodes_[prev_id].output_nodes.push_back(tee_id);
                }
                // Tee takes input from first predecessor's output
                // (actual path resolved at execution time)
            }

            // Tee output feeds into next stage's elements
            // Determine how many outputs tee produces based on next stage
            size_t tee_count = 1;
            if (stage_idx + 1 < pipeline.stages.size()) {
                tee_count = pipeline.stages[stage_idx + 1].element_count();
            }

            // Tee produces multiple logical outputs (same input replicated)
            for (size_t i = 0; i < tee_count; ++i) {
                current_stage_outputs.push_back(tee_id);
            }

            prev_stage_outputs = current_stage_outputs;
            continue;
        }

        // Handle collect - gathers N inputs into 1
        if (stage.has_collect()) {
            size_t collect_id = add_node();
            auto& collect_node = nodes_[collect_id];
            collect_node.stage_idx = stage_idx;
            collect_node.element_idx = 0;
            collect_node.target = "collect";
            collect_node.is_collect = true;
            collect_node.options = stage.elements[0].options;
            collect_node.plugin_options = stage.elements[0].raw_options;

            // Connect all previous outputs as inputs
            if (prev_stage_outputs.empty()) {
                collect_node.input = source_;
            } else {
                collect_node.input_nodes = prev_stage_outputs;
                for (size_t prev_id : prev_stage_outputs) {
                    nodes_[prev_id].output_nodes.push_back(collect_id);
                }
            }

            current_stage_outputs.push_back(collect_id);
            prev_stage_outputs = current_stage_outputs;
            continue;
        }

        // Regular conversion elements (including builtins like clipboard, passthrough)
        for (size_t elem_idx = 0; elem_idx < stage.elements.size(); ++elem_idx) {
            const auto& element = stage.elements[elem_idx];

            size_t node_id = add_node();
            auto& node = nodes_[node_id];
            node.stage_idx = stage_idx;
            node.element_idx = elem_idx;
            node.target = element.target;
            node.plugin = element.plugin;
            node.extension = element.extension;
            node.plugin_options = element.raw_options;
            node.options = element.options;

            // Set builtin flags
            if (element.is_clipboard()) {
                node.is_clipboard = true;
            } else if (element.is_passthrough()) {
                node.is_passthrough = true;
            }

            // Connect input
            if (prev_stage_outputs.empty()) {
                node.input = source_;
            } else if (elem_idx < prev_stage_outputs.size()) {
                // Connect to corresponding predecessor
                size_t prev_id = prev_stage_outputs[elem_idx];
                node.input_nodes.push_back(prev_id);
                nodes_[prev_id].output_nodes.push_back(node_id);
            }

            current_stage_outputs.push_back(node_id);
        }

        prev_stage_outputs = current_stage_outputs;
    }
}

size_t ExecutionGraph::add_node() {
    size_t id = nodes_.size();
    nodes_.emplace_back();
    nodes_.back().id = id;
    return id;
}

std::vector<size_t> ExecutionGraph::terminal_nodes() const {
    std::vector<size_t> terminals;
    for (const auto& node : nodes_) {
        if (node.is_terminal()) {
            terminals.push_back(node.id);
        }
    }
    return terminals;
}

std::vector<size_t> ExecutionGraph::file_producing_nodes() const {
    std::vector<size_t> producers;
    for (const auto& node : nodes_) {
        if (node.has_file_output()) {
            producers.push_back(node.id);
        }
    }
    return producers;
}

bool ExecutionGraph::is_only_consumed_by_clipboard(size_t node_id) const {
    const auto& node = nodes_[node_id];

    // If no consumers, it's not consumed by clipboard
    if (node.output_nodes.empty()) {
        return false;
    }

    // Check if ALL consumers are clipboard nodes
    for (size_t consumer_id : node.output_nodes) {
        if (!nodes_[consumer_id].is_clipboard) {
            return false;
        }
    }
    return true;
}

bool ExecutionGraph::was_content_copied_to_clipboard(size_t node_id) const {
    const auto& node = nodes_[node_id];

    // Check if any consumer clipboard node copied this node's content
    for (size_t consumer_id : node.output_nodes) {
        const auto& consumer = nodes_[consumer_id];
        if (consumer.is_clipboard && consumer.content_copied_to_clipboard) {
            return true;
        }
    }
    return false;
}

bool ExecutionGraph::clipboard_consumer_has_save(size_t node_id) const {
    const auto& node = nodes_[node_id];

    // Check if any clipboard consumer has --save option
    for (size_t consumer_id : node.output_nodes) {
        const auto& consumer = nodes_[consumer_id];
        if (consumer.is_clipboard) {
            auto it = consumer.options.find("save");
            if (it != consumer.options.end()) {
                // Option exists - check if it's true (or just present as a flag)
                return it->second.empty() || it->second == "true" || it->second == "1";
            }
        }
    }
    return false;
}

bool ExecutionGraph::is_effectively_terminal(size_t node_id) const {
    const auto& node = nodes_[node_id];

    // If no consumers, it's terminal
    if (node.output_nodes.empty()) {
        return true;
    }

    // Check all consumers - if all are passthrough nodes that are effectively terminal,
    // then this node is effectively terminal
    for (size_t consumer_id : node.output_nodes) {
        const auto& consumer = nodes_[consumer_id];
        if (consumer.is_passthrough) {
            // Recursively check if passthrough consumer is effectively terminal
            if (!is_effectively_terminal(consumer_id)) {
                return false;
            }
        } else {
            // Non-passthrough consumer means not terminal
            return false;
        }
    }
    return true;
}

bool ExecutionGraph::is_effectively_only_consumed_by_clipboard(size_t node_id) const {
    const auto& node = nodes_[node_id];

    // If no consumers, not consumed by clipboard
    if (node.output_nodes.empty()) {
        return false;
    }

    // Check all consumers - looking through passthrough nodes
    for (size_t consumer_id : node.output_nodes) {
        const auto& consumer = nodes_[consumer_id];
        if (consumer.is_clipboard) {
            // Direct clipboard consumer - good
            continue;
        } else if (consumer.is_passthrough) {
            // Look through passthrough - check what IT is consumed by
            // If passthrough doesn't lead exclusively to clipboard, then this node
            // is not only consumed by clipboard
            if (!is_effectively_only_consumed_by_clipboard(consumer_id)) {
                return false;
            }
            // Passthrough leads to clipboard, continue
        } else {
            // Non-clipboard, non-passthrough consumer
            return false;
        }
    }
    return true;
}

std::vector<size_t> ExecutionGraph::execution_order() const {
    // Topological sort using Kahn's algorithm
    std::vector<size_t> order;
    std::vector<size_t> in_degree(nodes_.size(), 0);

    // Calculate in-degrees
    for (const auto& node : nodes_) {
        in_degree[node.id] = node.input_nodes.size();
    }

    // Find nodes with no dependencies (in_degree = 0)
    std::queue<size_t> ready;
    for (const auto& node : nodes_) {
        // Nodes with no input_nodes but have input path are roots
        if (node.input_nodes.empty()) {
            ready.push(node.id);
        }
    }

    while (!ready.empty()) {
        size_t current = ready.front();
        ready.pop();
        order.push_back(current);

        // Reduce in-degree for all consumers
        for (size_t consumer_id : nodes_[current].output_nodes) {
            in_degree[consumer_id]--;
            if (in_degree[consumer_id] == 0) {
                ready.push(consumer_id);
            }
        }
    }

    return order;
}

} // namespace uniconv::core
