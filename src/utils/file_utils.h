#pragma once

#include "core/types.h"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace uniconv::utils {

// Detect format from file extension
std::string detect_format(const std::filesystem::path& path);

// Detect file category from format
core::FileCategory detect_category(const std::string& format);

// Get MIME type from format
std::string get_mime_type(const std::string& format);

// Get file information
core::FileInfo get_file_info(const std::filesystem::path& path);

// Ensure directory exists
bool ensure_directory(const std::filesystem::path& dir);

// Expand glob pattern to list of files
std::vector<std::filesystem::path> expand_glob(const std::string& pattern);

// Check if string is a URL
bool is_url(const std::string& str);

// Check if path is a directory
bool is_directory(const std::filesystem::path& path);

// Get all files in directory (optionally recursive)
std::vector<std::filesystem::path> get_files_in_directory(
    const std::filesystem::path& dir,
    bool recursive = false,
    const std::vector<std::string>& extensions = {}
);

// Generate unique output filename if exists
std::filesystem::path unique_path(const std::filesystem::path& path);

} // namespace uniconv::utils
