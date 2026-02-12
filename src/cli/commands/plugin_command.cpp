#include "plugin_command.h"
#include "utils/version_utils.h"
#include <functional>
#include <iomanip>
#include <sstream>

namespace uniconv::cli::commands
{

    PluginCommand::PluginCommand(
        std::shared_ptr<core::PluginManager> plugin_manager,
        std::shared_ptr<core::ConfigManager> config_manager,
        std::shared_ptr<core::output::IOutput> output)
        : plugin_manager_(std::move(plugin_manager)),
          config_manager_(std::move(config_manager)),
          output_(std::move(output)),
          installed_(core::ConfigManager::get_default_config_dir()),
          dep_installer_(core::ConfigManager::get_default_config_dir() / "deps")
    {
        installed_.load();
    }

    int PluginCommand::execute(const ParsedArgs &args)
    {
        if (args.subcommand_args.empty())
        {
            return list(args);
        }

        const auto &action = args.subcommand_args[0];

        if (action == "list")
        {
            return list(args);
        }
        else if (action == "install")
        {
            return install(args);
        }
        else if (action == "remove")
        {
            return remove(args);
        }
        else if (action == "info")
        {
            return info(args);
        }
        else if (action == "search")
        {
            return search(args);
        }
        else if (action == "update")
        {
            return update(args);
        }

        output_->error("Unknown plugin action: " + action);
        output_->info("Available actions: list, install, remove, info, search, update");
        return 1;
    }

    int PluginCommand::list(const ParsedArgs &args)
    {
        if (args.list_registry)
        {
            return list_registry(args);
        }

        // Discover all plugins
        auto manifests = discovery_.discover_all();

        // Get built-in plugins only (not manifest-discovered ones)
        std::vector<core::PluginInfo> builtin;
        for (const auto &p : plugin_manager_->list_plugins())
        {
            if (p.builtin)
                builtin.push_back(p);
        }

        nlohmann::json j = nlohmann::json::array();

        // Add built-in plugins
        for (const auto &p : builtin)
        {
            auto pj = p.to_json();
            pj["interface"] = "built-in";
            pj["source"] = "built-in";
            j.push_back(pj);
        }

        // Add discovered external plugins
        for (const auto &m : manifests)
        {
            auto info = m.to_plugin_info();
            auto pj = info.to_json();
            pj["path"] = m.plugin_dir.string();
            pj["interface"] = core::plugin_interface_to_string(m.iface);
            pj["source"] = installed_.is_registry_installed(m.name) ? "registry" : "local";
            j.push_back(pj);
        }

        // Build text representation
        std::ostringstream text;
        text << std::left
             << std::setw(25) << "NAME"
             << std::setw(30) << "TARGETS"
             << std::setw(10) << "VERSION"
             << std::setw(12) << "SOURCE"
             << "\n";
        text << std::string(77, '-') << "\n";

        // Built-in plugins
        for (const auto &p : builtin)
        {
            std::string targets;
            size_t ti = 0;
            for (const auto &[t, _] : p.targets)
            {
                if (ti >= 5) break;
                if (ti > 0)
                    targets += ",";
                targets += t;
                ++ti;
            }
            if (p.targets.size() > 5)
                targets += ",...";
            // Truncate targets to fit column
            if (targets.size() > 27)
                targets = targets.substr(0, 24) + "...";

            text << std::left
                 << std::setw(25) << p.id
                 << std::setw(30) << targets
                 << std::setw(10) << p.version
                 << std::setw(12) << "built-in"
                 << "\n";
        }

        // External plugins
        for (const auto &m : manifests)
        {
            std::string targets;
            size_t ti = 0;
            for (const auto &[t, _] : m.targets)
            {
                if (ti >= 5) break;
                if (ti > 0)
                    targets += ",";
                targets += t;
                ++ti;
            }
            if (m.targets.size() > 5)
                targets += ",...";
            // Truncate targets to fit column
            if (targets.size() > 27)
                targets = targets.substr(0, 24) + "...";

            std::string source = installed_.is_registry_installed(m.name) ? "registry" : "local";

            // Show scope@name for third-party plugins, just name for official (uniconv)
            std::string display_name = m.id();

            text << std::left
                 << std::setw(25) << display_name
                 << std::setw(30) << targets
                 << std::setw(10) << m.version
                 << std::setw(12) << source
                 << "\n";
        }

        if (builtin.empty() && manifests.empty())
        {
            text << "(no plugins installed)\n";
        }

        output_->data(j, text.str());
        return 0;
    }

    int PluginCommand::list_registry(const ParsedArgs & /*args*/)
    {
        auto client = make_registry_client();

        auto index = client->fetch_index(true);
        if (!index)
        {
            output_->error("Could not fetch registry index");
            return 1;
        }

        auto collections_result = client->fetch_collections();

        nlohmann::json j;
        j["plugins"] = nlohmann::json::array();
        for (const auto &entry : index->plugins)
        {
            auto pj = entry.to_json();
            pj["installed"] = installed_.is_registry_installed(entry.name);
            j["plugins"].push_back(pj);
        }

        j["collections"] = nlohmann::json::array();
        if (collections_result)
        {
            for (const auto &c : collections_result->collections)
            {
                j["collections"].push_back(c.to_json());
            }
        }

        // Build text representation
        std::ostringstream text;
        text << "PLUGINS\n";
        text << std::left
             << std::setw(25) << "NAME"
             << std::setw(10) << "VERSION"
             << std::setw(12) << "INTERFACE"
             << "DESCRIPTION\n";
        text << std::string(80, '-') << "\n";

        for (const auto &entry : index->plugins)
        {
            std::string name_col = entry.name;
            if (installed_.is_registry_installed(entry.name))
            {
                name_col += " [installed]";
            }

            std::string desc = entry.description;
            if (desc.size() > 35)
                desc = desc.substr(0, 32) + "...";

            text << std::left
                 << std::setw(25) << name_col
                 << std::setw(10) << entry.latest
                 << std::setw(12) << entry.iface
                 << desc << "\n";
        }

        text << "\n"
             << index->plugins.size() << " plugin(s) available\n";

        // Collections section
        if (collections_result && !collections_result->collections.empty())
        {
            text << "\nCOLLECTIONS\n";
            text << std::left
                 << std::setw(25) << "NAME"
                 << std::setw(35) << "PLUGINS"
                 << "DESCRIPTION\n";
            text << std::string(80, '-') << "\n";

            for (const auto &c : collections_result->collections)
            {
                std::string plugins_str;
                for (size_t i = 0; i < c.plugins.size(); ++i)
                {
                    if (i > 0)
                        plugins_str += ",";
                    plugins_str += c.plugins[i];
                }
                if (plugins_str.size() > 32)
                    plugins_str = plugins_str.substr(0, 29) + "...";

                std::string desc = c.description;
                if (desc.size() > 30)
                    desc = desc.substr(0, 27) + "...";

                text << std::left
                     << std::setw(25) << c.name
                     << std::setw(35) << plugins_str
                     << desc << "\n";
            }

            text << "\n"
                 << collections_result->collections.size() << " collection(s) available\n";
        }

        output_->data(j, text.str());
        return 0;
    }

    int PluginCommand::install(const ParsedArgs &args)
    {
        if (args.subcommand_args.size() < 2)
        {
            output_->error("Usage: uniconv plugin install <name[@version]> | <path> | +<collection> | collection:<name>");
            return 1;
        }

        const auto &source_arg = args.subcommand_args[1];

        // Check if source is a collection (+ prefix or collection: prefix)
        if (source_arg.size() > 1 && source_arg[0] == '+')
        {
            auto collection_name = source_arg.substr(1);
            return install_collection(collection_name, args);
        }

        const std::string kCollectionPrefix = "collection:";
        if (source_arg.size() > kCollectionPrefix.size() &&
            source_arg.substr(0, kCollectionPrefix.size()) == kCollectionPrefix)
        {
            auto collection_name = source_arg.substr(kCollectionPrefix.size());
            return install_collection(collection_name, args);
        }

        // Check if source is a local path
        if (std::filesystem::exists(source_arg))
        {
            std::filesystem::path source_path = source_arg;

            // If source is a file, assume it's a manifest and use parent directory
            if (std::filesystem::is_regular_file(source_path))
            {
                if (source_path.filename() == core::PluginDiscovery::kManifestFilename)
                {
                    source_path = source_path.parent_path();
                }
                else
                {
                    output_->error("Expected a directory or plugin.json file");
                    return 1;
                }
            }

            // Load manifest to get plugin name
            auto manifest = discovery_.load_manifest(source_path);
            if (!manifest)
            {
                output_->error("Could not load plugin manifest from: " + source_path.string());
                output_->info("Make sure the directory contains a valid plugin.json file");
                return 1;
            }

            // Destination in user plugins directory: scope/name
            auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
            auto dest_path = user_plugins / manifest->scope / manifest->name;

            // Check if already installed
            if (std::filesystem::exists(dest_path))
            {
                if (!args.core_options.force)
                {
                    output_->error("Plugin already installed at: " + dest_path.string());
                    output_->info("Use --force to overwrite");
                    return 1;
                }
                std::filesystem::remove_all(dest_path);
            }

            // Copy plugin
            if (!copy_plugin(source_path, dest_path))
            {
                output_->error("Failed to copy plugin to: " + dest_path.string());
                return 1;
            }

            // Install dependencies
            bool deps_failed = false;
            std::string deps_error;
            if (!manifest->dependencies.empty())
            {
                // Check for system dependencies first
                auto dep_results = dep_checker_.check_all(manifest->dependencies);

                // Check if any system dependencies are missing
                for (const auto& [dep_info, check_result] : dep_results)
                {
                    if (dep_info.type == "system" && !check_result.satisfied)
                    {
                        deps_failed = true;
                        deps_error = "Missing system dependency: " + dep_info.name;
                        if (dep_info.install_hint)
                        {
                            deps_error += " (hint: " + *dep_info.install_hint + ")";
                        }
                        else
                        {
                            deps_error += " (see plugin documentation for install instructions)";
                        }
                    }
                }

                // If system deps are satisfied, install Python/Node dependencies
                if (!deps_failed)
                {
                    bool has_auto_deps = false;
                    for (const auto& dep : manifest->dependencies)
                    {
                        if (dep.type == "python" || dep.type == "node")
                        {
                            has_auto_deps = true;
                            break;
                        }
                    }

                    if (has_auto_deps)
                    {
                        output_->info("Installing dependencies...");

                        std::function<void(const std::string&)> progress;
                        if (!output_->is_quiet())
                        {
                            progress = [this](const std::string& msg) { output_->info("  " + msg); };
                        }

                        auto install_result = dep_installer_.install_all(*manifest, progress);

                        if (!install_result.success)
                        {
                            deps_failed = true;
                            deps_error = "Dependency installation failed: " + install_result.message;
                        }
                        else if (!install_result.installed.empty())
                        {
                            output_->info("  " + install_result.message);
                        }
                    }
                }
            }

            // If dependency installation failed, rollback
            if (deps_failed)
            {
                output_->error(deps_error);
                output_->info("Rolling back plugin installation...");

                // Remove copied plugin directory
                try
                {
                    std::filesystem::remove_all(dest_path);
                }
                catch (const std::exception& e)
                {
                    output_->warning("Failed to remove plugin directory: " + std::string(e.what()));
                }

                // Remove dependency environment
                dep_installer_.remove_env(manifest->name);

                nlohmann::json j;
                j["success"] = false;
                j["plugin"] = manifest->name;
                j["error"] = deps_error;
                output_->data(j, "");

                return 1;
            }

            nlohmann::json j;
            j["success"] = true;
            j["plugin"] = manifest->name;
            j["version"] = manifest->version;
            j["path"] = dest_path.string();
            j["source"] = "local";

            std::ostringstream text;
            text << "Installed plugin: " << manifest->name << " (" << manifest->version << ")\n";
            text << "Location: " << dest_path;

            output_->data(j, text.str());
            return 0;
        }

        // Not a local path — treat as registry install
        auto [name, version] = parse_install_arg(source_arg);
        return install_from_registry(name, version, args);
    }

    int PluginCommand::install_from_registry(const std::string &name,
                                             const std::optional<std::string> &version,
                                             const ParsedArgs &args)
    {
        auto client = make_registry_client();

        std::string search_msg = "Searching registry for " + name;
        if (version)
            search_msg += "@" + *version;
        search_msg += "...";
        output_->info(search_msg);

        // Fetch plugin entry
        auto entry = client->fetch_plugin(name);
        if (!entry)
        {
            output_->error("Plugin not found in registry: " + name);
            output_->info("Use 'uniconv plugin search' to find available plugins");
            return 1;
        }

        // Resolve release
        auto release = client->resolve_release(*entry, version);
        if (!release)
        {
            std::string err = "No compatible release found for " + name;
            if (version)
                err += "@" + *version;
            output_->error(err);
            return 1;
        }

        // Resolve artifact
        auto artifact = client->resolve_artifact(*release);
        if (!artifact)
        {
            output_->error("No artifact available for current platform");
            return 1;
        }

        // Check if already installed (via installed.json)
        auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();

        if (!args.core_options.force)
        {
            auto existing = installed_.get(name);
            if (existing && existing->version == release->version)
            {
                output_->error("Plugin " + name + "@" + release->version + " is already installed");
                output_->info("Use --force to reinstall");
                return 1;
            }
        }

        output_->info("Downloading " + name + "@" + release->version + "...");

        // Download to a staging directory first (we need the manifest to determine scope)
        auto staging_dir = user_plugins / (".staging-" + name);
        auto result = client->download_and_extract(*artifact, staging_dir);
        if (!result)
        {
            output_->error("Failed to download or extract plugin");
            output_->info("This may be caused by a network error or invalid artifact");
            std::filesystem::remove_all(staging_dir);
            return 1;
        }

        // Load manifest to determine scope and get dependency info
        auto manifest = discovery_.load_manifest(staging_dir);
        if (!manifest)
        {
            output_->error("Downloaded plugin has no valid manifest");
            std::filesystem::remove_all(staging_dir);
            return 1;
        }

        // Move to final location: scope/name
        auto dest_dir = user_plugins / manifest->scope / manifest->name;
        if (std::filesystem::exists(dest_dir))
        {
            std::filesystem::remove_all(dest_dir);
        }
        std::filesystem::create_directories(dest_dir.parent_path());
        std::filesystem::rename(staging_dir, dest_dir);

        // Re-load manifest from final location
        manifest = discovery_.load_manifest(dest_dir);

        // Install dependencies
        bool deps_failed = false;
        std::string deps_error;
        if (manifest && !manifest->dependencies.empty())
        {
            // Check for system dependencies first
            auto dep_results = dep_checker_.check_all(manifest->dependencies);

            // Check if any system dependencies are missing
            for (const auto& [dep_info, check_result] : dep_results)
            {
                if (dep_info.type == "system" && !check_result.satisfied)
                {
                    deps_failed = true;
                    deps_error = "Missing system dependency: " + dep_info.name;
                    if (dep_info.install_hint)
                    {
                        deps_error += " (hint: " + *dep_info.install_hint + ")";
                    }
                    else
                    {
                        deps_error += " (see plugin documentation for install instructions)";
                    }
                }
            }

            // If system deps are satisfied, install Python/Node dependencies
            if (!deps_failed)
            {
                bool has_auto_deps = false;
                for (const auto& dep : manifest->dependencies)
                {
                    if (dep.type == "python" || dep.type == "node")
                    {
                        has_auto_deps = true;
                        break;
                    }
                }

                if (has_auto_deps)
                {
                    output_->info("Installing dependencies...");

                    std::function<void(const std::string&)> progress;
                    if (!output_->is_quiet())
                    {
                        progress = [this](const std::string& msg) { output_->info("  " + msg); };
                    }

                    auto install_result = dep_installer_.install_all(*manifest, progress);

                    if (!install_result.success)
                    {
                        deps_failed = true;
                        deps_error = "Dependency installation failed: " + install_result.message;
                    }
                    else if (!install_result.installed.empty())
                    {
                        output_->info("  " + install_result.message);
                    }
                }
            }
        }

        // If dependency installation failed, rollback
        if (deps_failed)
        {
            output_->error(deps_error);
            output_->info("Rolling back plugin installation...");

            // Remove extracted plugin directory
            try
            {
                std::filesystem::remove_all(dest_dir);
            }
            catch (const std::exception& e)
            {
                output_->warning("Failed to remove plugin directory: " + std::string(e.what()));
            }

            // Remove dependency environment
            dep_installer_.remove_env(name);

            nlohmann::json j;
            j["success"] = false;
            j["plugin"] = name;
            j["error"] = deps_error;
            output_->data(j, "");

            return 1;
        }

        // Record installation only after successful dependency installation
        installed_.record_install(name, release->version);
        installed_.save();

        nlohmann::json j;
        j["success"] = true;
        j["plugin"] = name;
        j["version"] = release->version;
        j["path"] = dest_dir.string();
        j["source"] = "registry";

        output_->data(j, "Installed " + name + "@" + release->version + " to " + dest_dir.string());
        return 0;
    }

    int PluginCommand::install_collection(const std::string &collection_name,
                                          const ParsedArgs &args)
    {
        auto client = make_registry_client();

        output_->info("Fetching collection '" + collection_name + "'...");

        auto collection = client->find_collection(collection_name);
        if (!collection)
        {
            output_->error("Collection not found: " + collection_name);
            return 1;
        }

        output_->info("Collection '" + collection_name + "': " + collection->description);
        output_->info("Installing " + std::to_string(collection->plugins.size()) + " plugin(s)...\n");

        int installed = 0;
        int skipped = 0;
        int failed = 0;

        for (const auto &plugin_name : collection->plugins)
        {
            output_->info("--- " + plugin_name + " ---");

            // Check if already installed (skip unless --force)
            if (!args.core_options.force && installed_.is_registry_installed(plugin_name))
            {
                output_->info("Already installed, skipping");
                ++skipped;
                output_->info("");
                continue;
            }

            int result = install_from_registry(plugin_name, std::nullopt, args);
            if (result == 0)
            {
                ++installed;
            }
            else
            {
                ++failed;
            }

            output_->info("");
        }

        std::string summary = std::to_string(installed) + " plugin(s) installed";
        if (skipped > 0)
            summary += ", " + std::to_string(skipped) + " skipped";
        if (failed > 0)
            summary += ", " + std::to_string(failed) + " failed";

        nlohmann::json j;
        j["success"] = (failed == 0);
        j["collection"] = collection_name;
        j["installed"] = installed;
        j["skipped"] = skipped;
        j["failed"] = failed;
        j["plugins"] = nlohmann::json::array();
        for (const auto &p : collection->plugins)
        {
            j["plugins"].push_back(p);
        }

        output_->data(j, summary);
        return failed > 0 ? 1 : 0;
    }

    int PluginCommand::remove(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            output_->error("Usage: uniconv plugin remove <name>");
            return 1;
        }

        const auto &name = args.subcommand;

        // Find plugin
        auto plugin_dir = find_plugin_dir(name);
        if (!plugin_dir)
        {
            output_->error("Plugin not found: " + name);
            return 1;
        }

        // Don't allow removing built-in plugins
        auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
        if (!plugin_dir->string().starts_with(user_plugins.string()))
        {
            output_->error("Cannot remove non-user plugins");
            output_->info("Plugin location: " + plugin_dir->string());
            return 1;
        }

        // Remove
        try
        {
            std::filesystem::remove_all(*plugin_dir);
        }
        catch (const std::exception &e)
        {
            output_->error("Failed to remove plugin: " + std::string(e.what()));
            return 1;
        }

        // Update installed.json
        installed_.record_remove(name);
        installed_.save();

        // Remove dependency environment
        if (dep_installer_.remove_env(name))
        {
            output_->info("Removed dependency environment for: " + name);
        }

        nlohmann::json j;
        j["success"] = true;
        j["plugin"] = name;
        output_->data(j, "Removed plugin: " + name);

        return 0;
    }

    int PluginCommand::info(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            output_->error("Usage: uniconv plugin info <name>");
            return 1;
        }

        const auto &name = args.subcommand;

        // First check built-in plugins only
        for (const auto &p : plugin_manager_->list_plugins())
        {
            if (!p.builtin)
                continue;
            if (p.id == name || p.scope == name)
            {
                auto j = p.to_json();
                j["interface"] = "built-in";
                j["source"] = "built-in";

                std::ostringstream text;
                text << "Name:        " << p.id << "\n";
                text << "Scope:       " << p.scope << "\n";
                text << "Version:     " << p.version << "\n";
                text << "Description: " << p.description << "\n";
                text << "Source:      built-in\n";
                text << "Targets:     ";
                {
                    size_t ti = 0;
                    for (const auto &[t, _] : p.targets)
                    {
                        if (ti > 0)
                            text << ", ";
                        text << t;
                        ++ti;
                    }
                }
                text << "\n";
                text << "Inputs:      ";
                if (p.accepts.has_value())
                {
                    for (size_t i = 0; i < p.accepts->size(); ++i)
                    {
                        if (i > 0)
                            text << ", ";
                        text << (*p.accepts)[i];
                    }
                }
                else
                {
                    text << "(any)";
                }

                output_->data(j, text.str());
                return 0;
            }
        }

        // Find external plugin
        auto plugin_dir = find_plugin_dir(name);
        if (!plugin_dir)
        {
            output_->error("Plugin not found: " + name);
            return 1;
        }

        auto manifest = discovery_.load_manifest(*plugin_dir);
        if (!manifest)
        {
            output_->error("Could not load manifest for: " + name);
            return 1;
        }

        std::string source = installed_.is_registry_installed(manifest->name) ? "registry" : "local";

        auto j = manifest->to_json();
        j["builtin"] = false;
        j["path"] = plugin_dir->string();
        j["source"] = source;

        std::ostringstream text;
        text << "Name:        " << manifest->name << "\n";
        text << "Scope:       " << manifest->scope << "\n";
        text << "Version:     " << manifest->version << "\n";
        text << "Description: " << manifest->description << "\n";
        text << "Type:        " << core::plugin_interface_to_string(manifest->iface) << "\n";
        text << "Source:      " << source << "\n";
        text << "Path:        " << *plugin_dir << "\n";

        if (manifest->iface == core::PluginInterface::CLI)
        {
            text << "Executable:  " << manifest->executable << "\n";
        }
        else
        {
            text << "Library:     " << manifest->library << "\n";
        }

        text << "Targets:     ";
        {
            size_t ti = 0;
            for (const auto &[t, _] : manifest->targets)
            {
                if (ti > 0)
                    text << ", ";
                text << t;
                ++ti;
            }
        }
        text << "\n";

        text << "Inputs:      ";
        if (manifest->accepts.has_value())
        {
            for (size_t i = 0; i < manifest->accepts->size(); ++i)
            {
                if (i > 0)
                    text << ", ";
                text << (*manifest->accepts)[i];
            }
        }
        else
        {
            text << "(any)";
        }

        if (!manifest->options.empty())
        {
            text << "\n\nOptions:";
            for (const auto &opt : manifest->options)
            {
                text << "\n  " << opt.name;
                if (!opt.type.empty())
                    text << " (" << opt.type << ")";
                if (!opt.default_value.empty())
                    text << " [default: " << opt.default_value << "]";
                if (opt.min_value.has_value() || opt.max_value.has_value())
                {
                    text << " [range: ";
                    if (opt.min_value.has_value())
                        text << opt.min_value.value();
                    else
                        text << "..";
                    text << "-";
                    if (opt.max_value.has_value())
                        text << opt.max_value.value();
                    else
                        text << "..";
                    text << "]";
                }
                if (!opt.choices.empty())
                {
                    text << " [choices: ";
                    for (size_t i = 0; i < opt.choices.size(); ++i)
                    {
                        if (i > 0) text << ", ";
                        text << opt.choices[i];
                    }
                    text << "]";
                }
                if (!opt.description.empty())
                {
                    text << "\n      " << opt.description;
                }
            }
        }

        if (!manifest->dependencies.empty())
        {
            // Check all dependencies (system, python, node)
            // Use plugin's virtualenv python for Python dependency checks
            auto env = dep_installer_.get_env(manifest->name);
            auto dep_results = (env && env->has_python_env())
                ? dep_checker_.check_all(manifest->dependencies, env->python_bin())
                : dep_checker_.check_all(manifest->dependencies);

            std::vector<std::string> missing;
            for (const auto& [dep_info, check_result] : dep_results)
            {
                if (!check_result.satisfied)
                {
                    missing.push_back(dep_info.name);
                }
            }

            text << "\n\nDependencies:";
            if (missing.empty())
            {
                text << " (all satisfied)";
            }
            else
            {
                text << " (MISSING: " << missing.size() << ")";
            }

            for (const auto& [dep_info, check_result] : dep_results)
            {
                text << "\n  [" << dep_info.type << "] " << dep_info.name;
                if (dep_info.version)
                    text << " " << *dep_info.version;
                if (check_result.satisfied)
                {
                    text << " - OK";
                }
                else
                {
                    text << " - MISSING";
                    if (dep_info.install_hint)
                    {
                        text << "\n      hint: " << *dep_info.install_hint;
                    }
                }
            }

            j["deps_satisfied"] = missing.empty();
            j["deps_missing"] = missing;
        }

        output_->data(j, text.str());
        return 0;
    }

    int PluginCommand::search(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            output_->error("Usage: uniconv plugin search <query>");
            return 1;
        }

        auto client = make_registry_client();
        auto results = client->search(args.subcommand);

        if (results.empty())
        {
            output_->info("No plugins found matching: " + args.subcommand);
            output_->data(nlohmann::json::array(), "No plugins found matching: " + args.subcommand);
            return 0;
        }

        nlohmann::json j = nlohmann::json::array();
        for (const auto &entry : results)
        {
            j.push_back(entry.to_json());
        }

        std::ostringstream text;
        text << std::left
             << std::setw(25) << "NAME"
             << std::setw(10) << "VERSION"
             << std::setw(15) << "AUTHOR"
             << "DESCRIPTION\n";
        text << std::string(80, '-') << "\n";

        for (const auto &entry : results)
        {
            // Truncate description to fit
            std::string desc = entry.description;
            if (desc.size() > 40)
                desc = desc.substr(0, 37) + "...";

            text << std::left
                 << std::setw(25) << entry.name
                 << std::setw(10) << entry.latest
                 << std::setw(15) << entry.author
                 << desc << "\n";
        }

        text << "\n"
             << results.size() << " plugin(s) found";

        output_->data(j, text.str());
        return 0;
    }

    int PluginCommand::update(const ParsedArgs &args)
    {
        auto client = make_registry_client();
        const auto &all_installed = installed_.all();

        // If a specific name is given, update only that
        std::vector<std::string> to_update;

        if (!args.subcommand.empty())
        {
            // Check if it's a collection (+ prefix)
            if (args.subcommand.size() > 1 && args.subcommand[0] == '+')
            {
                auto collection_name = args.subcommand.substr(1);
                auto collection = client->find_collection(collection_name);
                if (!collection)
                {
                    output_->error("Collection not found: " + collection_name);
                    return 1;
                }

                output_->info("Updating collection '" + collection_name + "'...");

                // Add installed plugins from this collection to update list
                for (const auto &plugin_name : collection->plugins)
                {
                    if (installed_.is_registry_installed(plugin_name))
                    {
                        to_update.push_back(plugin_name);
                    }
                }

                if (to_update.empty())
                {
                    output_->info("No plugins from collection '" + collection_name + "' are installed");
                    return 0;
                }
            }
            else
            {
                if (!installed_.is_registry_installed(args.subcommand))
                {
                    output_->error(args.subcommand + " was not installed from registry");
                    return 1;
                }
                to_update.push_back(args.subcommand);
            }
        }
        else
        {
            // Update all registry-installed plugins
            for (const auto &[name, record] : all_installed)
            {
                if (record.source == "registry")
                    to_update.push_back(name);
            }
        }

        if (to_update.empty())
        {
            output_->info("No registry-installed plugins to update");
            return 0;
        }

        // --check: report available updates without installing
        if (args.plugin_update_check)
        {
            nlohmann::json j;
            j["updates"] = nlohmann::json::array();
            std::ostringstream text;

            for (const auto &name : to_update)
            {
                auto record = installed_.get(name);
                if (!record)
                    continue;
                auto entry = client->fetch_plugin(name);
                if (!entry)
                    continue;
                auto release = client->resolve_release(*entry);
                if (!release)
                    continue;
                if (utils::compare_versions(release->version, record->version) > 0)
                {
                    nlohmann::json uj;
                    uj["name"] = name;
                    uj["current"] = record->version;
                    uj["latest"] = release->version;
                    j["updates"].push_back(uj);
                    text << name << " " << record->version << " -> " << release->version << "\n";
                }
            }

            if (j["updates"].empty())
            {
                output_->info("All plugins are up to date");
            }
            output_->data(j, text.str());
            return 0;
        }

        int updated = 0;
        int failed = 0;

        for (const auto &name : to_update)
        {
            auto record = installed_.get(name);
            if (!record)
                continue;

            auto entry = client->fetch_plugin(name);
            if (!entry)
            {
                output_->warning("Could not fetch registry info for " + name);
                ++failed;
                continue;
            }

            auto release = client->resolve_release(*entry);
            if (!release)
            {
                output_->warning("No compatible release found for " + name);
                ++failed;
                continue;
            }

            // Compare versions
            int cmp = utils::compare_versions(release->version, record->version);
            if (cmp <= 0)
            {
                output_->info(name + " is up to date (" + record->version + ")");
                continue;
            }

            output_->info("Updating " + name + " " + record->version + " -> " + release->version + "...");

            // Create a modified args with force=true for the reinstall
            ParsedArgs install_args = args;
            install_args.core_options.force = true;

            int result = install_from_registry(name, release->version, install_args);
            if (result == 0)
            {
                ++updated;
            }
            else
            {
                ++failed;
            }
        }

        std::string summary = "\n" + std::to_string(updated) + " plugin(s) updated";
        if (failed > 0)
            summary += ", " + std::to_string(failed) + " failed";
        output_->info(summary);

        return failed > 0 ? 1 : 0;
    }

    bool PluginCommand::copy_plugin(const std::filesystem::path &source, const std::filesystem::path &dest)
    {
        try
        {
            // Create destination directory
            std::filesystem::create_directories(dest.parent_path());

            // Copy recursively
            std::filesystem::copy(source, dest, std::filesystem::copy_options::recursive);
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    std::optional<std::filesystem::path> PluginCommand::find_plugin_dir(const std::string &name)
    {
        auto manifests = discovery_.discover_all();

        for (const auto &m : manifests)
        {
            if (m.name == name || m.scope == name || m.id() == name)
            {
                return m.plugin_dir;
            }
        }

        return std::nullopt;
    }

    std::unique_ptr<core::RegistryClient> PluginCommand::make_registry_client() const
    {
        auto url = config_manager_->get_registry_url();
        auto cache_dir = config_manager_->config_dir() / "registry-cache";
        return std::make_unique<core::RegistryClient>(url, cache_dir);
    }

    std::pair<std::string, std::optional<std::string>>
    PluginCommand::parse_install_arg(const std::string &arg)
    {
        auto at_pos = arg.find('@');
        if (at_pos != std::string::npos)
        {
            return {arg.substr(0, at_pos), arg.substr(at_pos + 1)};
        }
        return {arg, std::nullopt};
    }

} // namespace uniconv::cli::commands
