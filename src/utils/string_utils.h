#pragma once

#include <string>
#include <vector>
#include <optional>

namespace uniconv::utils {

// Convert string to lowercase
std::string to_lower(const std::string& s);

// Convert string to uppercase
std::string to_upper(const std::string& s);

// Trim whitespace from both ends
std::string trim(const std::string& s);

// Split string by delimiter
std::vector<std::string> split(const std::string& s, char delimiter);

// Join strings with delimiter
std::string join(const std::vector<std::string>& parts, const std::string& delimiter);

// Parse size string (e.g., "25MB", "1.5GB") to bytes
std::optional<size_t> parse_size(const std::string& s);

// Format bytes as human-readable string (e.g., "25 MB", "1.5 GB")
std::string format_size(size_t bytes);

// Check if string starts with prefix
bool starts_with(const std::string& s, const std::string& prefix);

// Check if string ends with suffix
bool ends_with(const std::string& s, const std::string& suffix);

// Replace all occurrences of 'from' with 'to'
std::string replace_all(const std::string& s, const std::string& from, const std::string& to);

} // namespace uniconv::utils
