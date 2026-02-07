#include "utils/mime_detector.h"
#include <stdexcept>

namespace uniconv::utils {

const std::unordered_map<std::string, std::string> MimeDetector::kMimeToExt = {
    // Images
    {"image/png", "png"},
    {"image/jpeg", "jpg"},
    {"image/gif", "gif"},
    {"image/bmp", "bmp"},
    {"image/x-ms-bmp", "bmp"},
    {"image/tiff", "tiff"},
    {"image/webp", "webp"},
    {"image/heic", "heic"},
    {"image/heif", "heif"},
    {"image/svg+xml", "svg"},
    {"image/x-icon", "ico"},
    // Video
    {"video/mp4", "mp4"},
    {"video/x-msvideo", "avi"},
    {"video/x-matroska", "mkv"},
    {"video/quicktime", "mov"},
    {"video/webm", "webm"},
    {"video/x-flv", "flv"},
    {"video/mpeg", "mpeg"},
    {"video/3gpp", "3gp"},
    // Audio
    {"audio/mpeg", "mp3"},
    {"audio/x-wav", "wav"},
    {"audio/flac", "flac"},
    {"audio/aac", "aac"},
    {"audio/ogg", "ogg"},
    {"audio/x-m4a", "m4a"},
    {"audio/opus", "opus"},
    {"audio/mp4", "m4a"},
    // Documents
    {"application/pdf", "pdf"},
    {"application/rtf", "rtf"},
    {"application/msword", "doc"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", "docx"},
    {"application/vnd.ms-excel", "xls"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "xlsx"},
    // Archives
    {"application/zip", "zip"},
    {"application/gzip", "gz"},
    {"application/x-tar", "tar"},
    {"application/x-7z-compressed", "7z"},
    {"application/x-rar-compressed", "rar"},
    // Structured text
    {"application/json", "json"},
    {"text/xml", "xml"},
    {"application/xml", "xml"},
    {"text/html", "html"},
    {"text/csv", "csv"},
    {"text/x-c", "c"},
    {"text/x-c++", "cpp"},
    {"text/x-python", "py"},
    {"text/x-shellscript", "sh"},
    // Plain text (fallback for all text/*)
    {"text/plain", "txt"},
};

MimeDetector::MimeDetector()
{
    cookie_ = magic_open(MAGIC_MIME_TYPE | MAGIC_NO_CHECK_COMPRESS);
    if (!cookie_)
        throw std::runtime_error("magic_open() failed");

    if (magic_load(cookie_, nullptr) != 0)
    {
        std::string err = magic_error(cookie_);
        magic_close(cookie_);
        throw std::runtime_error("magic_load() failed: " + err);
    }
}

MimeDetector::~MimeDetector()
{
    if (cookie_)
        magic_close(cookie_);
}

std::string MimeDetector::detect_extension(const void *data, std::size_t size) const
{
    if (!cookie_ || !data || size == 0)
        return "txt";

    const char *mime = magic_buffer(cookie_, data, size);
    if (!mime)
        return "dat";

    return mime_to_extension(mime);
}

std::string MimeDetector::mime_to_extension(const std::string &mime_type)
{
    auto it = kMimeToExt.find(mime_type);
    if (it != kMimeToExt.end())
        return it->second;

    // Fallback: any text/* -> txt, anything else -> dat
    if (mime_type.compare(0, 5, "text/") == 0)
        return "txt";

    return "dat";
}

} // namespace uniconv::utils
