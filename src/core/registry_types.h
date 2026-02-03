#pragma once

#include "plugin_manifest.h"
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace uniconv::core
{

    // Artifact for a specific platform
    struct RegistryArtifact
    {
        std::string url;
        std::string sha256;

        nlohmann::json to_json() const
        {
            return {{"url", url}, {"sha256", sha256}};
        }

        static RegistryArtifact from_json(const nlohmann::json &j)
        {
            RegistryArtifact a;
            a.url = j.at("url").get<std::string>();
            a.sha256 = j.at("sha256").get<std::string>();
            return a;
        }
    };

    // A single release of a registry plugin
    struct RegistryRelease
    {
        std::string version;
        std::string uniconv_compat; // e.g., ">=0.3.0"
        std::string interface;      // "cli" or "native"
        std::vector<Dependency> dependencies;
        std::map<std::string, RegistryArtifact> artifacts; // platform -> artifact

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["version"] = version;
            j["uniconv_compat"] = uniconv_compat;
            j["interface"] = interface;

            if (!dependencies.empty())
            {
                j["dependencies"] = nlohmann::json::array();
                for (const auto &dep : dependencies)
                {
                    j["dependencies"].push_back(dep.to_json());
                }
            }

            j["artifact"] = nlohmann::json::object();
            for (const auto &[platform, artifact] : artifacts)
            {
                j["artifact"][platform] = artifact.to_json();
            }

            return j;
        }

        static RegistryRelease from_json(const nlohmann::json &j)
        {
            RegistryRelease r;
            r.version = j.at("version").get<std::string>();
            r.uniconv_compat = j.value("uniconv_compat", "");
            r.interface = j.value("interface", "cli");

            if (j.contains("dependencies") && j.at("dependencies").is_array())
            {
                for (const auto &dep_json : j.at("dependencies"))
                {
                    r.dependencies.push_back(Dependency::from_json(dep_json));
                }
            }

            if (j.contains("artifact") && j.at("artifact").is_object())
            {
                for (auto &[platform, artifact_json] : j.at("artifact").items())
                {
                    r.artifacts[platform] = RegistryArtifact::from_json(artifact_json);
                }
            }

            return r;
        }
    };

    // Full registry plugin entry (from plugins/<name>/manifest.json in registry)
    struct RegistryPluginEntry
    {
        std::string name;
        std::string description;
        std::string author;
        std::string license;
        std::string repository;
        std::vector<std::string> keywords;
        std::vector<RegistryRelease> releases;

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["description"] = description;
            j["author"] = author;
            j["license"] = license;
            j["repository"] = repository;
            j["keywords"] = keywords;

            j["releases"] = nlohmann::json::array();
            for (const auto &rel : releases)
            {
                j["releases"].push_back(rel.to_json());
            }

            return j;
        }

        static RegistryPluginEntry from_json(const nlohmann::json &j)
        {
            RegistryPluginEntry e;
            e.name = j.at("name").get<std::string>();
            e.description = j.value("description", "");
            e.author = j.value("author", "");
            e.license = j.value("license", "");
            e.repository = j.value("repository", "");

            if (j.contains("keywords"))
            {
                e.keywords = j.at("keywords").get<std::vector<std::string>>();
            }

            if (j.contains("releases") && j.at("releases").is_array())
            {
                for (const auto &rel_json : j.at("releases"))
                {
                    e.releases.push_back(RegistryRelease::from_json(rel_json));
                }
            }

            return e;
        }
    };

    // Entry in the index.json summary
    struct RegistryIndexEntry
    {
        std::string name;
        std::string description;
        std::vector<std::string> keywords;
        std::string latest;
        std::string author;
        std::string interface;

        nlohmann::json to_json() const
        {
            return {
                {"name", name},
                {"description", description},
                {"keywords", keywords},
                {"latest", latest},
                {"author", author},
                {"interface", interface}};
        }

        static RegistryIndexEntry from_json(const nlohmann::json &j)
        {
            RegistryIndexEntry e;
            e.name = j.at("name").get<std::string>();
            e.description = j.value("description", "");
            e.latest = j.value("latest", "");
            e.author = j.value("author", "");
            e.interface = j.value("interface", "");

            if (j.contains("keywords"))
            {
                e.keywords = j.at("keywords").get<std::vector<std::string>>();
            }

            return e;
        }
    };

    // The full index.json
    struct RegistryIndex
    {
        int version = 1;
        std::string updated_at;
        std::vector<RegistryIndexEntry> plugins;

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["version"] = version;
            j["updated_at"] = updated_at;

            j["plugins"] = nlohmann::json::array();
            for (const auto &p : plugins)
            {
                j["plugins"].push_back(p.to_json());
            }

            return j;
        }

        static RegistryIndex from_json(const nlohmann::json &j)
        {
            RegistryIndex idx;
            idx.version = j.value("version", 1);
            idx.updated_at = j.value("updated_at", "");

            if (j.contains("plugins") && j.at("plugins").is_array())
            {
                for (const auto &p_json : j.at("plugins"))
                {
                    idx.plugins.push_back(RegistryIndexEntry::from_json(p_json));
                }
            }

            return idx;
        }
    };

    // Installed plugin record (for installed.json)
    struct InstalledPluginRecord
    {
        std::string version;
        std::string installed_at;
        std::string source; // "registry" or "local"

        nlohmann::json to_json() const
        {
            return {
                {"version", version},
                {"installed_at", installed_at},
                {"source", source}};
        }

        static InstalledPluginRecord from_json(const nlohmann::json &j)
        {
            InstalledPluginRecord r;
            r.version = j.at("version").get<std::string>();
            r.installed_at = j.value("installed_at", "");
            r.source = j.value("source", "registry");
            return r;
        }
    };

} // namespace uniconv::core
