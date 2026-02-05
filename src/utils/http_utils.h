#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace uniconv::utils
{

    struct HttpResponse
    {
        int status_code = 0;
        std::string body;
        std::string error;
        bool success = false;
    };

    // Fetch a URL using system curl. Returns the response body.
    HttpResponse http_get(const std::string &url,
                          std::chrono::seconds timeout = std::chrono::seconds{30});

    // Download a URL to a file path. Returns true on success.
    bool download_file(const std::string &url,
                       const std::filesystem::path &dest,
                       std::chrono::seconds timeout = std::chrono::seconds{300});

    // Compute SHA-256 of a file using system sha256sum/shasum.
    std::optional<std::string> sha256_file(const std::filesystem::path &path);

    // Detect the current platform string (e.g., "linux-x86_64", "darwin-aarch64").
    std::string get_platform_string();

    // Get the effective URL after following redirects (useful for GitHub /releases/latest).
    std::optional<std::string> get_redirect_url(const std::string &url,
                                                 std::chrono::seconds timeout = std::chrono::seconds{15});

} // namespace uniconv::utils
