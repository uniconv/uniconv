#pragma once

#include <magic.h>
#include <optional>
#include <string>
#include <unordered_map>

namespace uniconv::utils {

class MimeDetector {
public:
    MimeDetector();
    ~MimeDetector();

    MimeDetector(const MimeDetector&) = delete;
    MimeDetector& operator=(const MimeDetector&) = delete;

    // Detect file extension from buffer content (e.g., "png", "json", "txt")
    // Returns "dat" if format cannot be determined
    std::string detect_extension(const void* data, std::size_t size) const;

private:
    magic_t cookie_ = nullptr;

    // Convert MIME type to file extension
    static std::string mime_to_extension(const std::string& mime_type);

    static const std::unordered_map<std::string, std::string> kMimeToExt;
};

} // namespace uniconv::utils
