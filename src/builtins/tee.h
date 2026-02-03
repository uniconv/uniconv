#pragma once

#include <filesystem>
#include <vector>
#include <string>

namespace uniconv::builtins {

class Tee {
public:
    struct Result {
        bool success = true;
        std::vector<std::filesystem::path> outputs;
        std::string error;
    };

    // Execute tee operation
    // Takes single input, produces N copies (by reference, not actual copies)
    // count = number of elements in next stage
    static Result execute(
        const std::filesystem::path& input,
        size_t count
    );

    // Validate tee usage in pipeline context
    // - tee must not be first stage (needs input)
    // - next stage must have >1 element (otherwise tee is pointless)
    // - tee stage must have exactly 1 element (the tee itself)
    struct ValidationResult {
        bool valid = true;
        std::string error;
    };

    static ValidationResult validate(
        size_t current_stage_index,
        size_t total_stages,
        size_t next_stage_element_count
    );

    // Check if a target name is the tee builtin
    static bool is_tee(const std::string& target);
};

} // namespace uniconv::builtins
