#include "file_utils.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>

namespace uniconv::utils {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Format to MIME type mapping
const std::unordered_map<std::string, std::string> kMimeTypes = {
    // Images
    {"heic", "image/heic"},
    {"heif", "image/heif"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"webp", "image/webp"},
    {"gif", "image/gif"},
    {"bmp", "image/bmp"},
    {"tiff", "image/tiff"},
    {"tif", "image/tiff"},
    {"svg", "image/svg+xml"},
    {"pdf", "application/pdf"},
    // Videos
    {"mp4", "video/mp4"},
    {"mov", "video/quicktime"},
    {"mkv", "video/x-matroska"},
    {"avi", "video/x-msvideo"},
    {"webm", "video/webm"},
    {"m4v", "video/x-m4v"},
    // Audio
    {"mp3", "audio/mpeg"},
    {"wav", "audio/wav"},
    {"m4a", "audio/mp4"},
    {"ogg", "audio/ogg"},
    {"flac", "audio/flac"},
    {"aac", "audio/aac"},
};

// Format to category mapping
const std::unordered_map<std::string, core::FileCategory> kCategories = {
    // Images
    {"heic", core::FileCategory::Image},
    {"heif", core::FileCategory::Image},
    {"jpg", core::FileCategory::Image},
    {"jpeg", core::FileCategory::Image},
    {"png", core::FileCategory::Image},
    {"webp", core::FileCategory::Image},
    {"gif", core::FileCategory::Image},
    {"bmp", core::FileCategory::Image},
    {"tiff", core::FileCategory::Image},
    {"tif", core::FileCategory::Image},
    {"svg", core::FileCategory::Image},
    // Videos
    {"mp4", core::FileCategory::Video},
    {"mov", core::FileCategory::Video},
    {"mkv", core::FileCategory::Video},
    {"avi", core::FileCategory::Video},
    {"webm", core::FileCategory::Video},
    {"m4v", core::FileCategory::Video},
    // Audio
    {"mp3", core::FileCategory::Audio},
    {"wav", core::FileCategory::Audio},
    {"m4a", core::FileCategory::Audio},
    {"ogg", core::FileCategory::Audio},
    {"flac", core::FileCategory::Audio},
    {"aac", core::FileCategory::Audio},
    // Documents
    {"pdf", core::FileCategory::Document},
    {"doc", core::FileCategory::Document},
    {"docx", core::FileCategory::Document},
    {"txt", core::FileCategory::Document},
};

} // anonymous namespace

std::string detect_format(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    return to_lower(ext);
}

core::FileCategory detect_category(const std::string& format) {
    auto lower = to_lower(format);
    auto it = kCategories.find(lower);
    if (it != kCategories.end()) {
        return it->second;
    }
    return core::FileCategory::Unknown;
}

std::string get_mime_type(const std::string& format) {
    auto lower = to_lower(format);
    auto it = kMimeTypes.find(lower);
    if (it != kMimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

core::FileInfo get_file_info(const std::filesystem::path& path) {
    core::FileInfo info;
    info.path = path;
    info.format = detect_format(path);
    info.mime_type = get_mime_type(info.format);
    info.category = detect_category(info.format);

    if (std::filesystem::exists(path)) {
        info.size = std::filesystem::file_size(path);
    }

    // Note: dimensions and duration would require format-specific libraries
    // (libvips for images, ffmpeg for video/audio)
    // For now, leave them as nullopt - they can be filled by plugins

    return info;
}

bool ensure_directory(const std::filesystem::path& dir) {
    if (dir.empty()) return true;
    if (std::filesystem::exists(dir)) {
        return std::filesystem::is_directory(dir);
    }
    return std::filesystem::create_directories(dir);
}

std::vector<std::filesystem::path> expand_glob(const std::string& pattern) {
    std::vector<std::filesystem::path> results;

    // Simple glob expansion for * patterns
    // For more complex patterns, consider using glob.h on Unix or FindFirstFile on Windows

    std::filesystem::path p(pattern);
    auto parent = p.parent_path();
    auto filename = p.filename().string();

    if (parent.empty()) {
        parent = ".";
    }

    if (!std::filesystem::exists(parent) || !std::filesystem::is_directory(parent)) {
        return results;
    }

    // Convert glob pattern to regex
    std::string regex_pattern;
    for (char c : filename) {
        switch (c) {
            case '*': regex_pattern += ".*"; break;
            case '?': regex_pattern += "."; break;
            case '.': regex_pattern += "\\."; break;
            case '[': regex_pattern += "["; break;
            case ']': regex_pattern += "]"; break;
            default: regex_pattern += c;
        }
    }

    std::regex re(regex_pattern, std::regex::icase);

    for (const auto& entry : std::filesystem::directory_iterator(parent)) {
        if (entry.is_regular_file()) {
            auto name = entry.path().filename().string();
            if (std::regex_match(name, re)) {
                results.push_back(entry.path());
            }
        }
    }

    // Sort results
    std::sort(results.begin(), results.end());

    return results;
}

bool is_url(const std::string& str) {
    static const std::regex url_regex(
        R"(^(https?|ftp)://[^\s/$.?#].[^\s]*$)",
        std::regex::icase
    );
    return std::regex_match(str, url_regex);
}

bool is_directory(const std::filesystem::path& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

std::vector<std::filesystem::path> get_files_in_directory(
    const std::filesystem::path& dir,
    bool recursive,
    const std::vector<std::string>& extensions
) {
    std::vector<std::filesystem::path> results;

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return results;
    }

    auto should_include = [&extensions](const std::filesystem::path& p) {
        if (extensions.empty()) return true;
        auto ext = to_lower(detect_format(p));
        return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && should_include(entry.path())) {
                results.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && should_include(entry.path())) {
                results.push_back(entry.path());
            }
        }
    }

    std::sort(results.begin(), results.end());
    return results;
}

std::filesystem::path unique_path(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return path;
    }

    auto parent = path.parent_path();
    auto stem = path.stem().string();
    auto ext = path.extension().string();

    int counter = 1;
    std::filesystem::path new_path;
    do {
        new_path = parent / (stem + "_" + std::to_string(counter++) + ext);
    } while (std::filesystem::exists(new_path) && counter < 10000);

    return new_path;
}

} // namespace uniconv::utils
