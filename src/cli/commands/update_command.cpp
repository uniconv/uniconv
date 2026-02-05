#include "update_command.h"
#include "utils/http_utils.h"
#include "utils/version_utils.h"
#include <uniconv/version.h>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace uniconv::cli::commands
{

    UpdateCommand::UpdateCommand(std::shared_ptr<core::output::IOutput> output)
        : output_(std::move(output))
    {
    }

    int UpdateCommand::execute(const ParsedArgs &args)
    {
        std::string current_version = UNICONV_VERSION;

        output_->info("Current version: v" + current_version);
        output_->info("Checking for updates...");

        auto latest_version = fetch_latest_version();
        if (latest_version.empty())
        {
            std::string msg = "Failed to check for latest version";
            if (!last_error_.empty())
                msg += ": " + last_error_;
            output_->error(msg);
            return 1;
        }

        output_->info("Latest version:  v" + latest_version);

        int cmp = utils::compare_versions(latest_version, current_version);
        if (cmp <= 0)
        {
            output_->success("Already up to date.");
            return 0;
        }

        if (args.update_check_only)
        {
            output_->info("Update available: v" + current_version +
                          " -> v" + latest_version);
            output_->info("Run 'uniconv update' to install.");
            return 0;
        }

        return perform_update(latest_version);
    }

    std::string UpdateCommand::fetch_latest_version()
    {
        // Use the GitHub releases/latest redirect to get the version tag.
        // This avoids the API rate limit (60 req/hour unauthenticated).
        // The URL redirects from /releases/latest to /releases/tag/vX.Y.Z
        auto effective_url = utils::get_redirect_url(
            "https://github.com/uniconv/uniconv/releases/latest",
            std::chrono::seconds{15});

        if (!effective_url)
        {
            last_error_ = "Failed to fetch release info";
            return {};
        }

        // Parse version from URL: https://github.com/uniconv/uniconv/releases/tag/v0.3.0
        const std::string tag_marker = "/releases/tag/";
        auto tag_pos = effective_url->find(tag_marker);
        if (tag_pos == std::string::npos)
        {
            last_error_ = "No releases found";
            return {};
        }

        std::string tag = effective_url->substr(tag_pos + tag_marker.size());

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

        output_->info("Downloading " + asset_name + "...");

        // Download asset and checksums
        if (!utils::download_file(asset_url, archive_path))
        {
            output_->error("Failed to download " + asset_url);
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        if (!utils::download_file(checksums_url, checksums_path))
        {
            output_->warning("Could not download checksums, skipping verification");
        }
        else
        {
            // Verify checksum
            auto computed = utils::sha256_file(archive_path);
            if (!computed)
            {
                output_->error("Failed to compute checksum");
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
                            output_->error("Checksum mismatch!");
                            output_->info("  Expected: " + expected);
                            output_->info("  Got:      " + *computed);
                            std::filesystem::remove_all(tmp_dir);
                            return 1;
                        }
                    }
                    break;
                }
            }

            if (verified)
            {
                output_->info("Checksum verified.");
            }
            else
            {
                output_->warning("Asset not found in checksums.txt, skipping verification");
            }
        }

        // Extract archive
        auto extract_dir = tmp_dir / "extracted";
        std::filesystem::create_directories(extract_dir);

        output_->info("Extracting...");
        if (!extract_archive(archive_path, extract_dir))
        {
            output_->error("Failed to extract archive");
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
            output_->error("Binary not found in archive");
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Get current executable path
        auto self_path = get_self_path();
        if (self_path.empty())
        {
            output_->error("Could not determine current executable path");
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Replace the current binary
        output_->info("Installing to " + self_path.string() + "...");

        auto backup_path = self_path;
        backup_path += ".bak";

        std::error_code ec;

        // Backup current binary
        std::filesystem::rename(self_path, backup_path, ec);
        if (ec)
        {
            output_->error("Could not back up current binary: " + ec.message());
            output_->info("You may need to run with elevated permissions.");
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Move new binary into place
        std::filesystem::copy_file(new_binary, self_path,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            output_->error("Could not install new binary: " + ec.message());
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

        output_->success("Updated successfully: v" + std::string(UNICONV_VERSION) +
                         " -> v" + latest_version);
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
