#include "tee.h"
#include <algorithm>
#include <cctype>

namespace uniconv::builtins {

Tee::Result Tee::execute(
    const std::filesystem::path& input,
    size_t count
) {
    Result result;

    // Validate input exists
    if (!std::filesystem::exists(input)) {
        result.success = false;
        result.error = "Input file does not exist: " + input.string();
        return result;
    }

    // Validate count is reasonable
    if (count == 0) {
        result.success = false;
        result.error = "Tee count must be greater than 0";
        return result;
    }

    if (count == 1) {
        result.success = false;
        result.error = "Tee with count=1 is pointless (no duplication needed)";
        return result;
    }

    // Create N references to the same input path
    // The actual copying/processing happens in the next stage
    result.outputs.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        result.outputs.push_back(input);
    }

    result.success = true;
    return result;
}

Tee::ValidationResult Tee::validate(
    size_t current_stage_index,
    size_t total_stages,
    size_t next_stage_element_count
) {
    ValidationResult result;

    // Tee cannot be the first stage (needs input)
    if (current_stage_index == 0) {
        result.valid = false;
        result.error = "Tee cannot be the first stage (it requires input)";
        return result;
    }

    // Tee must not be the last stage (needs consumers)
    if (current_stage_index >= total_stages - 1) {
        result.valid = false;
        result.error = "Tee cannot be the last stage (it needs consumers)";
        return result;
    }

    // Next stage must have multiple elements (otherwise tee is pointless)
    if (next_stage_element_count <= 1) {
        result.valid = false;
        result.error = "Tee requires next stage to have multiple elements (found " +
                      std::to_string(next_stage_element_count) + ")";
        return result;
    }

    result.valid = true;
    return result;
}

bool Tee::is_tee(const std::string& target) {
    // Case-insensitive comparison
    std::string lower_target = target;
    std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lower_target == "tee";
}

} // namespace uniconv::builtins
