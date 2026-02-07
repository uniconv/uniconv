#pragma once

#include "types.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace uniconv::core {

// A single element in a pipeline stage (e.g., "jpg --quality 90" or "jpg@vips")
struct StageElement {
    std::string target;                              // "jpg", "faces", "gdrive", "tee", "clipboard"
    std::optional<std::string> plugin;               // Explicit plugin: "vips", "ffmpeg"
    std::map<std::string, std::string> options;      // Parsed options: {"quality": "90"}
    std::vector<std::string> raw_options;            // Raw option strings for plugin

    bool is_tee() const { return target == "tee"; }
    bool is_clipboard() const { return target == "clipboard"; }
    bool is_passthrough() const {
        return target == "_" || target == "echo" || target == "bypass" ||
               target == "pass" || target == "noop";
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["target"] = target;
        if (plugin) {
            j["plugin"] = *plugin;
        }
        j["options"] = options;
        j["raw_options"] = raw_options;
        return j;
    }
};

// A pipeline stage containing one or more parallel elements
struct PipelineStage {
    std::vector<StageElement> elements;              // Parallel elements (via comma)

    bool is_single() const { return elements.size() == 1; }
    bool has_tee() const {
        for (const auto& elem : elements) {
            if (elem.is_tee()) return true;
        }
        return false;
    }
    bool has_clipboard() const {
        for (const auto& elem : elements) {
            if (elem.is_clipboard()) return true;
        }
        return false;
    }
    size_t element_count() const { return elements.size(); }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["element_count"] = elements.size();
        j["elements"] = nlohmann::json::array();
        for (const auto& elem : elements) {
            j["elements"].push_back(elem.to_json());
        }
        return j;
    }
};

// Complete pipeline from source to final outputs
struct Pipeline {
    std::filesystem::path source;                    // Input file
    std::vector<PipelineStage> stages;               // Sequential stages
    CoreOptions core_options;                        // Global options
    std::optional<std::string> input_format;         // Format hint for stdin/generators

    bool empty() const { return stages.empty(); }
    size_t stage_count() const { return stages.size(); }

    // Validate pipeline structure (element count rules)
    struct ValidationResult {
        bool valid = true;
        std::string error;

        static ValidationResult ok() {
            return ValidationResult{true, ""};
        }

        static ValidationResult fail(const std::string& err) {
            return ValidationResult{false, err};
        }
    };

    ValidationResult validate() const {
        if (stages.empty()) {
            return ValidationResult::fail("Pipeline has no stages");
        }

        // Validate tee position: cannot be last stage (needs consumers)
        if (!stages.empty() && stages.back().has_tee()) {
            return ValidationResult::fail("'tee' cannot be the last stage (needs consumers)");
        }

        // Note: clipboard CAN be any stage (including first) because it receives
        // input from either the source file or the previous stage's output

        // Check each stage transition
        for (size_t i = 0; i < stages.size() - 1; ++i) {
            const auto& current = stages[i];
            const auto& next = stages[i + 1];

            size_t current_count = current.element_count();
            size_t next_count = next.element_count();

            // Rule: 1 → 1 → 1 (OK)
            if (current_count == 1 && next_count == 1) {
                continue;
            }

            // Rule: 1 (tee) → N (OK)
            if (current_count == 1 && current.has_tee()) {
                continue;
            }

            // Rule: N → N (1:1 mapping, OK)
            if (current_count == next_count && current_count > 1) {
                continue;
            }

            // Rule: N → M where N ≠ M (INVALID)
            if (current_count != next_count) {
                return ValidationResult::fail(
                    "Stage " + std::to_string(i) + " has " +
                    std::to_string(current_count) + " elements but stage " +
                    std::to_string(i + 1) + " has " + std::to_string(next_count) +
                    " elements (use 'tee' to branch)"
                );
            }

            // Rule: 1 → N without tee (INVALID)
            if (current_count == 1 && next_count > 1 && !current.has_tee()) {
                return ValidationResult::fail(
                    "Stage " + std::to_string(i) + " has 1 element but stage " +
                    std::to_string(i + 1) + " has " + std::to_string(next_count) +
                    " elements (use 'tee' in stage " + std::to_string(i) + ")"
                );
            }
        }

        return ValidationResult::ok();
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["source"] = source.string();
        j["stage_count"] = stages.size();
        j["core_options"] = core_options.to_json();
        if (input_format) {
            j["input_format"] = *input_format;
        }
        j["stages"] = nlohmann::json::array();
        for (const auto& stage : stages) {
            j["stages"].push_back(stage.to_json());
        }

        auto validation = validate();
        j["valid"] = validation.valid;
        if (!validation.valid) {
            j["validation_error"] = validation.error;
        }

        return j;
    }
};

// Result of a single stage execution
struct StageResult {
    size_t stage_index;
    std::string target;
    std::string plugin_used;
    std::filesystem::path input;
    std::filesystem::path output;
    ResultStatus status;
    std::optional<std::string> error;
    int64_t duration_ms = 0;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["stage_index"] = stage_index;
        j["target"] = target;
        j["plugin_used"] = plugin_used;
        j["input"] = input.string();
        j["output"] = output.string();
        j["status"] = result_status_to_string(status);
        j["success"] = (status == ResultStatus::Success);
        j["duration_ms"] = duration_ms;
        if (error) {
            j["error"] = *error;
        }
        return j;
    }
};

// Result of complete pipeline execution
struct PipelineResult {
    bool success = false;
    std::vector<StageResult> stage_results;
    std::vector<std::filesystem::path> final_outputs;
    std::vector<std::string> warnings;
    int64_t total_duration_ms = 0;
    std::optional<std::string> error;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["success"] = success;
        j["total_duration_ms"] = total_duration_ms;

        // Match SKETCH.md format: "pipeline" array with stage/target/plugin/input/output/duration_ms
        j["pipeline"] = nlohmann::json::array();
        for (const auto& sr : stage_results) {
            nlohmann::json stage_json;
            stage_json["stage"] = sr.stage_index;
            stage_json["target"] = sr.target;
            stage_json["plugin"] = sr.plugin_used;
            stage_json["input"] = sr.input.string();
            stage_json["output"] = sr.output.string();
            stage_json["duration_ms"] = sr.duration_ms;
            if (sr.error) {
                stage_json["error"] = *sr.error;
            }
            j["pipeline"].push_back(stage_json);
        }

        // Also include final_outputs for convenience
        if (!final_outputs.empty()) {
            j["final_outputs"] = nlohmann::json::array();
            for (const auto& out : final_outputs) {
                j["final_outputs"].push_back(out.string());
            }
        }

        if (error) {
            j["error"] = *error;
        }

        if (!warnings.empty()) {
            j["warnings"] = warnings;
        }

        return j;
    }
};

} // namespace uniconv::core
