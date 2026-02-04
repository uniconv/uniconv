#include "update_command.h"
#include "utils/http_utils.h"
#include "utils/version_utils.h"
#include <uniconv/version.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

namespace uniconv::cli::commands
{

    int UpdateCommand::execute(const ParsedArgs &args)
    {
        std::string current_version = UNICONV_VERSION;

        std::cout << "Current version: v" << current_version << "\n";
        std::cout << "Checking for updates...\n";

        auto latest_version = fetch_latest_version();
        if (latest_version.empty())
        {
            std::cerr << "Error: Failed to check for latest version\n";
            return 1;
        }

        std::cout << "Latest version:  v" << latest_version << "\n";

        int cmp = utils::compare_versions(latest_version, current_version);
        if (cmp <= 0)
        {
            std::cout << "Already up to date.\n";
            return 0;
        }

        if (args.update_check_only)
        {
            std::cout << "Update available: v" << current_version
                      << " -> v" << latest_version << "\n";
            std::cout << "Run 'uniconv update' to install.\n";
            return 0;
        }

        return perform_update(latest_version);
    }

    std::string UpdateCommand::fetch_latest_version()
    {
        // Use curl to follow the /releases/latest redirect and extract
        // the final URL which contains the tag name.
        auto response = utils::http_get(
            "https://github.com/uniconv/uniconv/releases/latest",
            std::chrono::seconds{15});

        if (!response.success)
            return {};

        // If http_get followed the redirect, the body is the release page HTML.
        // Instead, we look at the effective URL by doing a HEAD-like request.
        // Actually, our http_get uses -L so it follows redirects.
        // We need a different approach: use the GitHub API.
        // Fetch the tag from the GitHub API redirect.
        auto api_response = utils::http_get(
            "https://api.github.com/repos/uniconv/uniconv/releases/latest",
            std::chrono::seconds{15});

        if (!api_response.success)
            return {};

        // Parse the tag_name from JSON response
        // Look for "tag_name": "v0.1.11" pattern
        auto body = api_response.body;
        auto tag_pos = body.find("\"tag_name\"");
        if (tag_pos == std::string::npos)
            return {};

        auto colon_pos = body.find(':', tag_pos);
        if (colon_pos == std::string::npos)
            return {};

        auto quote1 = body.find('"', colon_pos + 1);
        if (quote1 == std::string::npos)
            return {};

        auto quote2 = body.find('"', quote1 + 1);
        if (quote2 == std::string::npos)
            return {};

        std::string tag = body.substr(quote1 + 1, quote2 - quote1 - 1);

        // Strip leading 'v' if present
        if (!tag.empty() && tag[0] == 'v')
            tag = tag.substr(1);

        return tag;
    }

    int UpdateCommand::perform_update(const std::string &latest_version)
    {
        std::string platform = utils::get_platform_string();
        std::string tag = "v" + latest_version;

#ifdef _WIN32
        std::string ext = ".zip";
#else
        std::string ext = ".tar.gz";
#endif

        std::string asset_name = "uniconv-" + tag + "-" + platform + ext;
        std::string asset_url =
            "https://github.com/uniconv/uniconv/releases/download/" +
            tag + "/" + asset_name;
        std::string checksums_url =
            "https://github.com/uniconv/uniconv/releases/download/" +
            tag + "/checksums.txt";

        // Create temp directory
        auto tmp_dir = std::filesystem::temp_directory_path() / ("uniconv-update-" + tag);
        std::filesystem::create_directories(tmp_dir);

        auto archive_path = tmp_dir / asset_name;
        auto checksums_path = tmp_dir / "checksums.txt";

        std::cout << "Downloading " << asset_name << "...\n";

        // Download asset and checksums
        if (!utils::download_file(asset_url, archive_path))
        {
            std::cerr << "Error: Failed to download " << asset_url << "\n";
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        if (!utils::download_file(checksums_url, checksums_path))
        {
            std::cerr << "Warning: Could not download checksums, skipping verification\n";
        }
        else
        {
            // Verify checksum
            auto computed = utils::sha256_file(archive_path);
            if (!computed)
            {
                std::cerr << "Error: Failed to compute checksum\n";
                std::filesystem::remove_all(tmp_dir);
                return 1;
            }

            // Read checksums.txt and find our asset
            std::ifstream cs_file(checksums_path);
            std::string line;
            bool verified = false;
            while (std::getline(cs_file, line))
            {
                // Format: "<hash>  <filename>" or "<hash> <filename>"
                if (line.find(asset_name) != std::string::npos)
                {
                    auto space_pos = line.find(' ');
                    if (space_pos != std::string::npos)
                    {
                        std::string expected = line.substr(0, space_pos);
                        if (*computed == expected)
                        {
                            verified = true;
                        }
                        else
                        {
                            std::cerr << "Error: Checksum mismatch!\n"
                                      << "  Expected: " << expected << "\n"
                                      << "  Got:      " << *computed << "\n";
                            std::filesystem::remove_all(tmp_dir);
                            return 1;
                        }
                    }
                    break;
                }
            }

            if (verified)
            {
                std::cout << "Checksum verified.\n";
            }
            else
            {
                std::cerr << "Warning: Asset not found in checksums.txt, skipping verification\n";
            }
        }

        // Extract archive
        auto extract_dir = tmp_dir / "extracted";
        std::filesystem::create_directories(extract_dir);

        std::cout << "Extracting...\n";
        if (!extract_archive(archive_path, extract_dir))
        {
            std::cerr << "Error: Failed to extract archive\n";
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Find the new binary
        auto new_binary = extract_dir / "uniconv";
#ifdef _WIN32
        new_binary = extract_dir / "uniconv.exe";
#endif

        if (!std::filesystem::exists(new_binary))
        {
            std::cerr << "Error: Binary not found in archive\n";
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Get current executable path
        auto self_path = get_self_path();
        if (self_path.empty())
        {
            std::cerr << "Error: Could not determine current executable path\n";
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Replace the current binary
        std::cout << "Installing to " << self_path.string() << "...\n";

        auto backup_path = self_path;
        backup_path += ".bak";

        std::error_code ec;

        // Backup current binary
        std::filesystem::rename(self_path, backup_path, ec);
        if (ec)
        {
            std::cerr << "Error: Could not back up current binary: " << ec.message() << "\n";
            std::cerr << "You may need to run with elevated permissions.\n";
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Move new binary into place
        std::filesystem::copy_file(new_binary, self_path,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            std::cerr << "Error: Could not install new binary: " << ec.message() << "\n";
            // Restore backup
            std::filesystem::rename(backup_path, self_path);
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Set executable permissions on Unix
#ifndef _WIN32
        std::filesystem::permissions(self_path,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add, ec);
#endif

        // Clean up
        std::filesystem::remove(backup_path, ec);
        std::filesystem::remove_all(tmp_dir, ec);

        std::cout << "Updated successfully: v" << UNICONV_VERSION
                  << " -> v" << latest_version << "\n";
        return 0;
    }

    std::filesystem::path UpdateCommand::get_self_path()
    {
#if defined(__linux__)
        // /proc/self/exe
        std::error_code ec;
        auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec)
            return path;
        return {};
#elif defined(__APPLE__)
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
        {
            return std::filesystem::canonical(buf);
        }
        return {};
#elif defined(_WIN32)
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            return std::filesystem::path(buf);
        }
        return {};
#else
        return {};
#endif
    }

    bool UpdateCommand::extract_archive(const std::filesystem::path &archive,
                                        const std::filesystem::path &dest_dir)
    {
#ifdef _WIN32
        // Use PowerShell to extract zip
        std::string cmd = "powershell -NoProfile -Command \"Expand-Archive -Force -Path '"
            + archive.string() + "' -DestinationPath '" + dest_dir.string() + "'\"";
        return std::system(cmd.c_str()) == 0;
#else
        // Use tar to extract .tar.gz
        std::string cmd = "tar xzf '" + archive.string() +
                          "' -C '" + dest_dir.string() + "'";
        return std::system(cmd.c_str()) == 0;
#endif
    }

} // namespace uniconv::cli::commands
