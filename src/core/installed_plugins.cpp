#include "installed_plugins.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace uniconv::core
{

    InstalledPlugins::InstalledPlugins(const std::filesystem::path &config_dir)
        : file_path_(config_dir / "installed.json")
    {
    }

    bool InstalledPlugins::load()
    {
        if (!std::filesystem::exists(file_path_))
            return false;

        try
        {
            std::ifstream file(file_path_);
            if (!file)
                return false;

            nlohmann::json j;
            file >> j;

            if (!j.is_object())
                return false;

            for (auto &[name, record_json] : j.items())
            {
                plugins_[name] = InstalledPluginRecord::from_json(record_json);
            }

            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool InstalledPlugins::save() const
    {
        try
        {
            // Ensure parent directory exists
            if (file_path_.has_parent_path())
            {
                std::filesystem::create_directories(file_path_.parent_path());
            }

            nlohmann::json j = nlohmann::json::object();
            for (const auto &[name, record] : plugins_)
            {
                j[name] = record.to_json();
            }

            std::ofstream file(file_path_);
            if (!file)
                return false;

            file << std::setw(2) << j << std::endl;
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    void InstalledPlugins::record_install(const std::string &name, const std::string &version)
    {
        InstalledPluginRecord record;
        record.version = version;
        record.source = "registry";

        // Generate ISO 8601 timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        record.installed_at = oss.str();

        plugins_[name] = record;
    }

    void InstalledPlugins::record_remove(const std::string &name)
    {
        plugins_.erase(name);
    }

    std::optional<InstalledPluginRecord> InstalledPlugins::get(const std::string &name) const
    {
        auto it = plugins_.find(name);
        if (it != plugins_.end())
            return it->second;
        return std::nullopt;
    }

    bool InstalledPlugins::is_registry_installed(const std::string &name) const
    {
        auto it = plugins_.find(name);
        return it != plugins_.end() && it->second.source == "registry";
    }

    bool InstalledPlugins::reconcile(const std::vector<PluginManifest> &on_disk)
    {
        std::map<std::string, const PluginManifest *> disk_map;
        for (const auto &m : on_disk)
            disk_map[m.name] = &m;

        bool changed = false;

        // Remove entries not on disk
        for (auto it = plugins_.begin(); it != plugins_.end();)
        {
            if (disk_map.find(it->first) == disk_map.end())
            {
                it = plugins_.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }

        // Update version if on-disk manifest differs
        for (auto &[name, record] : plugins_)
        {
            auto dit = disk_map.find(name);
            if (dit != disk_map.end() && dit->second->version != record.version)
            {
                record.version = dit->second->version;
                changed = true;
            }
        }

        return changed;
    }

} // namespace uniconv::core
