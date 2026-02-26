#include "registry_client.h"
#include "utils/http_utils.h"
#include "utils/version_utils.h"
#include <uniconv/version.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace uniconv::core
{

    RegistryClient::RegistryClient(const std::string &registry_url,
                                   const std::filesystem::path &cache_dir)
        : registry_url_(registry_url), cache_dir_(cache_dir)
    {
        // Remove trailing slash from URL
        while (!registry_url_.empty() && registry_url_.back() == '/')
            registry_url_.pop_back();
    }

    std::optional<RegistryIndex> RegistryClient::fetch_index(bool force_refresh)
    {
        // Try cache first
        if (!force_refresh && is_cache_fresh())
        {
            auto cached = load_cached_index();
            if (cached)
                return cached;
        }

        // Fetch from registry
        auto url = registry_url_ + "/index.json";
        auto response = utils::http_get(url);

        if (!response.success)
            return std::nullopt;

        try
        {
            auto j = nlohmann::json::parse(response.body);
            auto index = RegistryIndex::from_json(j);

            // Save to cache
            save_cached_index(index);

            return index;
        }
        catch (const std::exception &)
        {
            return std::nullopt;
        }
    }

    std::optional<RegistryCollections> RegistryClient::fetch_collections()
    {
        auto url = registry_url_ + "/collections.json";
        auto response = utils::http_get(url);

        if (!response.success)
            return std::nullopt;

        try
        {
            auto j = nlohmann::json::parse(response.body);
            return RegistryCollections::from_json(j);
        }
        catch (const std::exception &)
        {
            return std::nullopt;
        }
    }

    std::optional<RegistryCollection> RegistryClient::find_collection(const std::string &name)
    {
        auto collections = fetch_collections();
        if (!collections)
            return std::nullopt;

        for (const auto &c : collections->collections)
        {
            if (c.name == name)
                return c;
        }

        return std::nullopt;
    }

    std::optional<RegistryPluginEntry> RegistryClient::fetch_plugin(const std::string &name)
    {
        auto url = registry_url_ + "/plugins/" + name + "/manifest.json";
        auto response = utils::http_get(url);

        if (!response.success)
            return std::nullopt;

        try
        {
            auto j = nlohmann::json::parse(response.body);
            return RegistryPluginEntry::from_json(j);
        }
        catch (const std::exception &)
        {
            return std::nullopt;
        }
    }

    std::vector<RegistryIndexEntry> RegistryClient::search(const std::string &query)
    {
        auto index = fetch_index();
        if (!index)
            return {};

        std::vector<RegistryIndexEntry> results;
        auto lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        for (const auto &plugin : index->plugins)
        {
            // Match against name, description, keywords
            if (contains_ci(plugin.name, lower_query) ||
                contains_ci(plugin.description, lower_query))
            {
                results.push_back(plugin);
                continue;
            }

            // Check keywords
            for (const auto &kw : plugin.keywords)
            {
                if (contains_ci(kw, lower_query))
                {
                    results.push_back(plugin);
                    break;
                }
            }
        }

        return results;
    }

    std::optional<RegistryRelease> RegistryClient::resolve_release(
        const RegistryPluginEntry &entry,
        const std::optional<std::string> &requested_version)
    {
        if (entry.releases.empty())
            return std::nullopt;

        std::string current_version = UNICONV_VERSION;

        // If specific version requested, find exact match
        if (requested_version)
        {
            for (const auto &rel : entry.releases)
            {
                if (rel.version == *requested_version)
                {
                    // Check compatibility
                    if (!rel.uniconv_compat.empty() &&
                        !utils::satisfies_constraint(current_version, rel.uniconv_compat))
                    {
                        continue;
                    }
                    return rel;
                }
            }
            return std::nullopt;
        }

        // Find latest compatible release
        // Releases are assumed ordered newest-first in the registry
        for (const auto &rel : entry.releases)
        {
            if (rel.uniconv_compat.empty() ||
                utils::satisfies_constraint(current_version, rel.uniconv_compat))
            {
                return rel;
            }
        }

        return std::nullopt;
    }

    std::optional<RegistryArtifact> RegistryClient::resolve_artifact(
        const RegistryRelease &release)
    {
        auto platform = utils::get_platform_string();

        // Try platform-specific first
        auto it = release.artifacts.find(platform);
        if (it != release.artifacts.end())
            return it->second;

        // Fall back to "any"
        it = release.artifacts.find("any");
        if (it != release.artifacts.end())
            return it->second;

        return std::nullopt;
    }

    std::optional<std::filesystem::path> RegistryClient::download_and_extract(
        const RegistryArtifact &artifact,
        const std::filesystem::path &dest_dir)
    {
        // Create temp directory for download
        auto temp_dir = cache_dir_ / "tmp";
        std::filesystem::create_directories(temp_dir);

        auto temp_file = temp_dir / "plugin-download.tar.gz";

        // Download
        if (!utils::download_file(artifact.url, temp_file))
        {
            std::cerr << "Download failed: " << artifact.url << "\n";
            std::filesystem::remove_all(temp_dir);
            return std::nullopt;
        }

        // Verify SHA-256
        auto computed_hash = utils::sha256_file(temp_file);
        if (!computed_hash)
        {
            std::cerr << "SHA-256 computation failed for downloaded file\n";
            std::filesystem::remove_all(temp_dir);
            return std::nullopt;
        }
        if (*computed_hash != artifact.sha256)
        {
            std::cerr << "SHA-256 mismatch:\n"
                      << "  expected: " << artifact.sha256 << "\n"
                      << "  got:      " << *computed_hash << "\n";
            std::filesystem::remove_all(temp_dir);
            return std::nullopt;
        }

        // Remove existing destination if it exists
        if (std::filesystem::exists(dest_dir))
        {
            std::filesystem::remove_all(dest_dir);
        }
        std::filesystem::create_directories(dest_dir);

        // Extract tarball
        // tar xzf <file> -C <dest> --strip-components=1
        // --strip-components=1 removes the top-level directory from the archive
        auto extract_dir = temp_dir / "extract";
        std::filesystem::create_directories(extract_dir);

        std::string tar_cmd = "tar";
        std::vector<std::string> tar_args = {
            "xzf", temp_file.string(),
            "-C", extract_dir.string()};

        // Use the http_utils subprocess pattern isn't directly available here,
        // so we use system() as a simple approach
#ifdef _WIN32
        // Use forward slashes to avoid MSYS2/GNU tar misinterpreting
        // colons in Windows paths as remote host specifiers
        auto to_forward_slash = [](std::string s) {
            for (auto& c : s) { if (c == '\\') c = '/'; }
            return s;
        };
        std::string full_cmd = "tar --force-local -xzf \""
            + to_forward_slash(temp_file.string()) + "\" -C \""
            + to_forward_slash(extract_dir.string()) + "\"";
#else
        std::string full_cmd = "tar xzf " + temp_file.string() + " -C " + extract_dir.string();
#endif
        int ret = std::system(full_cmd.c_str());

        if (ret != 0)
        {
            std::cerr << "Failed to extract archive\n";
            std::filesystem::remove_all(temp_dir);
            return std::nullopt;
        }

        // Find the extracted content
        // If there's a single directory inside extract_dir, use its contents
        std::filesystem::path source_dir = extract_dir;
        int dir_count = 0;
        std::filesystem::path single_dir;
        for (const auto &entry : std::filesystem::directory_iterator(extract_dir))
        {
            if (entry.is_directory())
            {
                single_dir = entry.path();
                ++dir_count;
            }
        }
        if (dir_count == 1)
            source_dir = single_dir;

        // Copy contents to destination
        for (const auto &entry : std::filesystem::directory_iterator(source_dir))
        {
            auto dest_path = dest_dir / entry.path().filename();
            std::filesystem::copy(entry.path(), dest_path,
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::overwrite_existing);
        }

        // Cleanup temp
        std::filesystem::remove_all(temp_dir);

        return dest_dir;
    }

    bool RegistryClient::is_cache_fresh() const
    {
        auto cache_file = cache_dir_ / "index.json";
        if (!std::filesystem::exists(cache_file))
            return false;

        auto last_write = std::filesystem::last_write_time(cache_file);
        auto now = std::filesystem::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - last_write);

        return age.count() < kCacheTTLMinutes;
    }

    std::optional<RegistryIndex> RegistryClient::load_cached_index() const
    {
        auto cache_file = cache_dir_ / "index.json";
        if (!std::filesystem::exists(cache_file))
            return std::nullopt;

        try
        {
            std::ifstream file(cache_file);
            if (!file)
                return std::nullopt;

            nlohmann::json j;
            file >> j;
            return RegistryIndex::from_json(j);
        }
        catch (const std::exception &)
        {
            return std::nullopt;
        }
    }

    void RegistryClient::save_cached_index(const RegistryIndex &index) const
    {
        std::filesystem::create_directories(cache_dir_);
        auto cache_file = cache_dir_ / "index.json";

        try
        {
            std::ofstream file(cache_file);
            if (file)
            {
                file << std::setw(2) << index.to_json() << std::endl;
            }
        }
        catch (const std::exception &)
        {
            // Cache write failure is non-fatal
        }
    }

    bool RegistryClient::contains_ci(const std::string &haystack, const std::string &needle)
    {
        if (needle.empty())
            return true;

        auto lower_haystack = haystack;
        std::transform(lower_haystack.begin(), lower_haystack.end(), lower_haystack.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        return lower_haystack.find(needle) != std::string::npos;
    }

} // namespace uniconv::core
