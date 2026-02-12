#include "collect.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace uniconv::builtins {

// Simple cross-platform glob matching (supports * and ? wildcards)
static bool glob_match(const char* pattern, const char* str) {
    while (*pattern && *str) {
        if (*pattern == '*') {
            // Skip consecutive stars
            while (*pattern == '*') ++pattern;
            if (!*pattern) return true;
            // Try matching rest of pattern at each position
            while (*str) {
                if (glob_match(pattern, str)) return true;
                ++str;
            }
            return glob_match(pattern, str);
        } else if (*pattern == '?' || *pattern == *str) {
            ++pattern;
            ++str;
        } else {
            return false;
        }
    }
    // Skip trailing stars
    while (*pattern == '*') ++pattern;
    return !*pattern && !*str;
}

Collect::Result Collect::execute(
    const std::vector<std::filesystem::path>& inputs,
    const std::filesystem::path& temp_dir
) {
    Result result;

    if (inputs.empty()) {
        result.success = false;
        result.error = "Collect requires at least one input file";
        return result;
    }

    // Create the collect directory inside the temp dir
    std::filesystem::path collect_dir = temp_dir / "collected";
    try {
        std::filesystem::create_directories(collect_dir);
    } catch (const std::filesystem::filesystem_error& e) {
        result.success = false;
        result.error = "Failed to create collect directory: " + std::string(e.what());
        return result;
    }

    // Copy/link each input file into the collect directory with ordered names
    // Format: 000_originalname.ext, 001_originalname.ext, ...
    for (size_t i = 0; i < inputs.size(); ++i) {
        const auto& input = inputs[i];

        if (!std::filesystem::exists(input)) {
            result.success = false;
            result.error = "Input file does not exist: " + input.string();
            return result;
        }

        // Build ordered filename: {index}_{original_filename}
        std::ostringstream prefix;
        prefix << std::setw(4) << std::setfill('0') << i;
        std::string ordered_name = prefix.str() + "_" + input.filename().string();
        std::filesystem::path dest = collect_dir / ordered_name;

        try {
            std::filesystem::copy_file(input, dest,
                std::filesystem::copy_options::overwrite_existing);
        } catch (const std::filesystem::filesystem_error& e) {
            result.success = false;
            result.error = "Failed to collect file: " + std::string(e.what());
            return result;
        }
    }

    result.success = true;
    result.output_dir = collect_dir;
    return result;
}

Collect::Result Collect::execute_directory(
    const std::filesystem::path& directory,
    const std::filesystem::path& temp_dir,
    bool recursive,
    const std::string& glob_pattern
) {
    Result result;

    if (!std::filesystem::exists(directory)) {
        result.success = false;
        result.error = "Directory does not exist: " + directory.string();
        return result;
    }

    if (!std::filesystem::is_directory(directory)) {
        result.success = false;
        result.error = "Not a directory: " + directory.string();
        return result;
    }

    // Enumerate files from directory
    std::vector<std::filesystem::path> files;

    auto collect_entries = [&](auto iterator) {
        for (const auto& entry : iterator) {
            if (!entry.is_regular_file()) continue;

            // Apply glob filter if specified
            if (!glob_pattern.empty()) {
                std::string filename = entry.path().filename().string();
                if (!glob_match(glob_pattern.c_str(), filename.c_str())) {
                    continue;
                }
            }

            files.push_back(entry.path());
        }
    };

    try {
        if (recursive) {
            collect_entries(std::filesystem::recursive_directory_iterator(directory));
        } else {
            collect_entries(std::filesystem::directory_iterator(directory));
        }
    } catch (const std::filesystem::filesystem_error& e) {
        result.success = false;
        result.error = "Failed to enumerate directory: " + std::string(e.what());
        return result;
    }

    if (files.empty()) {
        result.success = false;
        result.error = glob_pattern.empty()
            ? "Directory is empty: " + directory.string()
            : "No files matching '" + glob_pattern + "' in: " + directory.string();
        return result;
    }

    // Sort alphabetically for consistent ordering
    std::sort(files.begin(), files.end());

    return execute(files, temp_dir);
}

Collect::ValidationResult Collect::validate(
    size_t current_stage_index,
    size_t total_stages
) {
    ValidationResult result;

    // First-stage validation is handled at the pipeline level (where source path is available).
    // Here we only validate structural constraints that don't depend on the source.
    (void)current_stage_index;
    (void)total_stages;
    result.valid = true;
    return result;
}

bool Collect::is_collect(const std::string& target) {
    std::string lower_target = target;
    std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lower_target == "collect";
}

} // namespace uniconv::builtins
