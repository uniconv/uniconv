#include "plugin_command.h"
#include "utils/version_utils.h"
#include <iostream>
#include <iomanip>

namespace uniconv::cli::commands
{

    PluginCommand::PluginCommand(
        std::shared_ptr<core::PluginManager> plugin_manager,
        std::shared_ptr<core::ConfigManager> config_manager)
        : plugin_manager_(std::move(plugin_manager)),
          config_manager_(std::move(config_manager)),
          installed_(core::ConfigManager::get_default_config_dir())
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

        std::cerr << "Unknown plugin action: " << action << "\n";
        std::cerr << "Available actions: list, install, remove, info, search, update\n";
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

        // Also get built-in plugins from plugin manager
        auto builtin = plugin_manager_->list_plugins();

        if (args.core_options.json_output)
        {
            nlohmann::json j = nlohmann::json::array();

            // Add built-in plugins
            for (const auto &p : builtin)
            {
                auto pj = p.to_json();
                pj["source"] = "built-in";
                j.push_back(pj);
            }

            // Add discovered external plugins
            for (const auto &m : manifests)
            {
                auto info = m.to_plugin_info();
                auto pj = info.to_json();
                pj["path"] = m.plugin_dir.string();
                pj["interface"] = core::plugin_interface_to_string(m.interface);
                pj["source"] = installed_.is_registry_installed(m.name) ? "registry" : "local";
                j.push_back(pj);
            }

            std::cout << j.dump(2) << std::endl;
            return 0;
        }

        // Table header
        std::cout << std::left
                  << std::setw(25) << "NAME"
                  << std::setw(30) << "TARGETS"
                  << std::setw(10) << "VERSION"
                  << std::setw(12) << "SOURCE"
                  << "\n";
        std::cout << std::string(77, '-') << "\n";

        // Built-in plugins
        for (const auto &p : builtin)
        {
            std::string targets;
            for (size_t i = 0; i < p.targets.size() && i < 5; ++i)
            {
                if (i > 0)
                    targets += ",";
                targets += p.targets[i];
            }
            if (p.targets.size() > 5)
                targets += ",...";

            std::cout << std::left
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
            for (size_t i = 0; i < m.targets.size() && i < 5; ++i)
            {
                if (i > 0)
                    targets += ",";
                targets += m.targets[i];
            }
            if (m.targets.size() > 5)
                targets += ",...";

            std::string source = installed_.is_registry_installed(m.name) ? "registry" : "local";

            std::cout << std::left
                      << std::setw(25) << m.id()
                      << std::setw(30) << targets
                      << std::setw(10) << m.version
                      << std::setw(12) << source
                      << "\n";
        }

        if (builtin.empty() && manifests.empty())
        {
            std::cout << "(no plugins installed)\n";
        }

        return 0;
    }

    int PluginCommand::list_registry(const ParsedArgs &args)
    {
        auto client = make_registry_client();

        auto index = client->fetch_index();
        if (!index)
        {
            std::cerr << "Error: Could not fetch registry index\n";
            return 1;
        }

        auto collections_result = client->fetch_collections();

        if (args.core_options.json_output)
        {
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

            std::cout << j.dump(2) << std::endl;
            return 0;
        }

        // Text mode: plugins section
        std::cout << "PLUGINS\n";
        std::cout << std::left
                  << std::setw(25) << "NAME"
                  << std::setw(10) << "VERSION"
                  << std::setw(12) << "INTERFACE"
                  << "DESCRIPTION\n";
        std::cout << std::string(80, '-') << "\n";

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

            std::cout << std::left
                      << std::setw(25) << name_col
                      << std::setw(10) << entry.latest
                      << std::setw(12) << entry.interface
                      << desc << "\n";
        }

        std::cout << "\n"
                  << index->plugins.size() << " plugin(s) available\n";

        // Collections section
        if (collections_result && !collections_result->collections.empty())
        {
            std::cout << "\nCOLLECTIONS\n";
            std::cout << std::left
                      << std::setw(25) << "NAME"
                      << std::setw(35) << "PLUGINS"
                      << "DESCRIPTION\n";
            std::cout << std::string(80, '-') << "\n";

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

                std::cout << std::left
                          << std::setw(25) << c.name
                          << std::setw(35) << plugins_str
                          << desc << "\n";
            }

            std::cout << "\n"
                      << collections_result->collections.size() << " collection(s) available\n";
        }

        return 0;
    }

    int PluginCommand::install(const ParsedArgs &args)
    {
        if (args.subcommand_args.size() < 2)
        {
            std::cerr << "Usage: uniconv plugin install <name[@version]> | <path> | +<collection> | collection:<name>\n";
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
                    std::cerr << "Error: Expected a directory or plugin.json file\n";
                    return 1;
                }
            }

            // Load manifest to get plugin name
            auto manifest = discovery_.load_manifest(source_path);
            if (!manifest)
            {
                std::cerr << "Error: Could not load plugin manifest from: " << source_path << "\n";
                std::cerr << "Make sure the directory contains a valid plugin.json file\n";
                return 1;
            }

            // Destination in user plugins directory
            auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
            auto dest_path = user_plugins / manifest->name;

            // Check if already installed
            if (std::filesystem::exists(dest_path))
            {
                if (!args.core_options.force)
                {
                    std::cerr << "Error: Plugin already installed at: " << dest_path << "\n";
                    std::cerr << "Use --force to overwrite\n";
                    return 1;
                }
                std::filesystem::remove_all(dest_path);
            }

            // Copy plugin
            if (!copy_plugin(source_path, dest_path))
            {
                std::cerr << "Error: Failed to copy plugin to: " << dest_path << "\n";
                return 1;
            }

            // Check dependencies
            if (!manifest->dependencies.empty())
            {
                auto dep_results = dep_checker_.check_all(manifest->dependencies);
                core::DependencyChecker::print_warnings(dep_results);
            }

            if (!args.core_options.quiet)
            {
                std::cout << "Installed plugin: " << manifest->name << " (" << manifest->version << ")\n";
                std::cout << "Location: " << dest_path << "\n";
            }

            if (args.core_options.json_output)
            {
                nlohmann::json j;
                j["success"] = true;
                j["plugin"] = manifest->name;
                j["version"] = manifest->version;
                j["path"] = dest_path.string();
                j["source"] = "local";
                std::cout << j.dump(2) << std::endl;
            }

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

        if (!args.core_options.quiet)
        {
            std::cout << "Searching registry for " << name;
            if (version)
                std::cout << "@" << *version;
            std::cout << "...\n";
        }

        // Fetch plugin entry
        auto entry = client->fetch_plugin(name);
        if (!entry)
        {
            std::cerr << "Error: Plugin not found in registry: " << name << "\n";
            std::cerr << "Use 'uniconv plugin search' to find available plugins\n";
            return 1;
        }

        // Resolve release
        auto release = client->resolve_release(*entry, version);
        if (!release)
        {
            std::cerr << "Error: No compatible release found for " << name;
            if (version)
                std::cerr << "@" << *version;
            std::cerr << "\n";
            return 1;
        }

        // Resolve artifact
        auto artifact = client->resolve_artifact(*release);
        if (!artifact)
        {
            std::cerr << "Error: No artifact available for current platform\n";
            return 1;
        }

        // Check if already installed
        auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
        auto dest_dir = user_plugins / name;

        if (std::filesystem::exists(dest_dir) && !args.core_options.force)
        {
            auto existing = installed_.get(name);
            if (existing && existing->version == release->version)
            {
                std::cerr << "Plugin " << name << "@" << release->version << " is already installed\n";
                std::cerr << "Use --force to reinstall\n";
                return 1;
            }
        }

        if (!args.core_options.quiet)
        {
            std::cout << "Downloading " << name << "@" << release->version << "...\n";
        }

        // Download and extract
        auto result = client->download_and_extract(*artifact, dest_dir);
        if (!result)
        {
            std::cerr << "Error: Failed to download or extract plugin\n";
            std::cerr << "This may be caused by a network error or invalid artifact\n";
            return 1;
        }

        // Record installation
        installed_.record_install(name, release->version);
        installed_.save();

        // Check dependencies
        if (!release->dependencies.empty())
        {
            auto dep_results = dep_checker_.check_all(release->dependencies);
            core::DependencyChecker::print_warnings(dep_results);
        }

        if (!args.core_options.quiet)
        {
            std::cout << "Installed " << name << "@" << release->version
                      << " to " << dest_dir << "\n";
        }

        if (args.core_options.json_output)
        {
            nlohmann::json j;
            j["success"] = true;
            j["plugin"] = name;
            j["version"] = release->version;
            j["path"] = dest_dir.string();
            j["source"] = "registry";
            std::cout << j.dump(2) << std::endl;
        }

        return 0;
    }

    int PluginCommand::install_collection(const std::string &collection_name,
                                          const ParsedArgs &args)
    {
        auto client = make_registry_client();

        if (!args.core_options.quiet)
        {
            std::cout << "Fetching collection '" << collection_name << "'...\n";
        }

        auto collection = client->find_collection(collection_name);
        if (!collection)
        {
            std::cerr << "Error: Collection not found: " << collection_name << "\n";
            return 1;
        }

        if (!args.core_options.quiet)
        {
            std::cout << "Collection '" << collection_name << "': "
                      << collection->description << "\n";
            std::cout << "Installing " << collection->plugins.size() << " plugin(s)...\n\n";
        }

        int installed = 0;
        int failed = 0;

        for (const auto &plugin_name : collection->plugins)
        {
            if (!args.core_options.quiet)
            {
                std::cout << "--- " << plugin_name << " ---\n";
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

            if (!args.core_options.quiet)
            {
                std::cout << "\n";
            }
        }

        if (!args.core_options.quiet)
        {
            std::cout << installed << " plugin(s) installed";
            if (failed > 0)
                std::cout << ", " << failed << " failed";
            std::cout << "\n";
        }

        if (args.core_options.json_output)
        {
            nlohmann::json j;
            j["success"] = (failed == 0);
            j["collection"] = collection_name;
            j["installed"] = installed;
            j["failed"] = failed;

            j["plugins"] = nlohmann::json::array();
            for (const auto &p : collection->plugins)
            {
                j["plugins"].push_back(p);
            }

            std::cout << j.dump(2) << std::endl;
        }

        return failed > 0 ? 1 : 0;
    }

    int PluginCommand::remove(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Usage: uniconv plugin remove <name>\n";
            return 1;
        }

        const auto &name = args.subcommand;

        // Find plugin
        auto plugin_dir = find_plugin_dir(name);
        if (!plugin_dir)
        {
            std::cerr << "Error: Plugin not found: " << name << "\n";
            return 1;
        }

        // Don't allow removing built-in plugins
        auto user_plugins = core::PluginDiscovery::get_user_plugin_dir();
        if (!plugin_dir->string().starts_with(user_plugins.string()))
        {
            std::cerr << "Error: Cannot remove non-user plugins\n";
            std::cerr << "Plugin location: " << *plugin_dir << "\n";
            return 1;
        }

        // Remove
        try
        {
            std::filesystem::remove_all(*plugin_dir);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: Failed to remove plugin: " << e.what() << "\n";
            return 1;
        }

        // Update installed.json
        installed_.record_remove(name);
        installed_.save();

        if (!args.core_options.quiet)
        {
            std::cout << "Removed plugin: " << name << "\n";
        }

        if (args.core_options.json_output)
        {
            nlohmann::json j;
            j["success"] = true;
            j["plugin"] = name;
            std::cout << j.dump(2) << std::endl;
        }

        return 0;
    }

    int PluginCommand::info(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Usage: uniconv plugin info <name>\n";
            return 1;
        }

        const auto &name = args.subcommand;

        // First check built-in plugins
        auto builtin = plugin_manager_->list_plugins();
        for (const auto &p : builtin)
        {
            if (p.id == name || p.scope == name)
            {
                if (args.core_options.json_output)
                {
                    auto j = p.to_json();
                    j["source"] = "built-in";
                    std::cout << j.dump(2) << std::endl;
                }
                else
                {
                    std::cout << "Name:        " << p.id << "\n";
                    std::cout << "Scope:       " << p.scope << "\n";
                    std::cout << "Version:     " << p.version << "\n";
                    std::cout << "Description: " << p.description << "\n";
                    std::cout << "Source:      built-in\n";
                    std::cout << "Targets:     ";
                    for (size_t i = 0; i < p.targets.size(); ++i)
                    {
                        if (i > 0)
                            std::cout << ", ";
                        std::cout << p.targets[i];
                    }
                    std::cout << "\n";
                    std::cout << "Inputs:      ";
                    for (size_t i = 0; i < p.input_formats.size(); ++i)
                    {
                        if (i > 0)
                            std::cout << ", ";
                        std::cout << p.input_formats[i];
                    }
                    std::cout << "\n";
                }
                return 0;
            }
        }

        // Find external plugin
        auto plugin_dir = find_plugin_dir(name);
        if (!plugin_dir)
        {
            std::cerr << "Error: Plugin not found: " << name << "\n";
            return 1;
        }

        auto manifest = discovery_.load_manifest(*plugin_dir);
        if (!manifest)
        {
            std::cerr << "Error: Could not load manifest for: " << name << "\n";
            return 1;
        }

        std::string source = installed_.is_registry_installed(manifest->name) ? "registry" : "local";

        if (args.core_options.json_output)
        {
            auto j = manifest->to_json();
            j["path"] = plugin_dir->string();
            j["source"] = source;
            std::cout << j.dump(2) << std::endl;
        }
        else
        {
            std::cout << "Name:        " << manifest->name << "\n";
            std::cout << "Scope:       " << manifest->scope << "\n";
            std::cout << "Version:     " << manifest->version << "\n";
            std::cout << "Description: " << manifest->description << "\n";
            std::cout << "Type:        " << core::plugin_interface_to_string(manifest->interface) << "\n";
            std::cout << "Source:      " << source << "\n";
            std::cout << "Path:        " << *plugin_dir << "\n";

            if (manifest->interface == core::PluginInterface::CLI)
            {
                std::cout << "Executable:  " << manifest->executable << "\n";
            }
            else
            {
                std::cout << "Library:     " << manifest->library << "\n";
            }

            std::cout << "Targets:     ";
            for (size_t i = 0; i < manifest->targets.size(); ++i)
            {
                if (i > 0)
                    std::cout << ", ";
                std::cout << manifest->targets[i];
            }
            std::cout << "\n";

            std::cout << "Inputs:      ";
            for (size_t i = 0; i < manifest->input_formats.size(); ++i)
            {
                if (i > 0)
                    std::cout << ", ";
                std::cout << manifest->input_formats[i];
            }
            std::cout << "\n";

            if (!manifest->options.empty())
            {
                std::cout << "\nOptions:\n";
                for (const auto &opt : manifest->options)
                {
                    std::cout << "  " << opt.name;
                    if (!opt.type.empty())
                        std::cout << " (" << opt.type << ")";
                    if (!opt.default_value.empty())
                        std::cout << " [default: " << opt.default_value << "]";
                    std::cout << "\n";
                    if (!opt.description.empty())
                    {
                        std::cout << "      " << opt.description << "\n";
                    }
                }
            }

            if (!manifest->dependencies.empty())
            {
                std::cout << "\nDependencies:\n";
                for (const auto &dep : manifest->dependencies)
                {
                    std::cout << "  [" << dep.type << "] " << dep.name;
                    if (dep.version)
                        std::cout << " " << *dep.version;
                    std::cout << "\n";
                }
            }
        }

        return 0;
    }

    int PluginCommand::search(const ParsedArgs &args)
    {
        if (args.subcommand.empty())
        {
            std::cerr << "Usage: uniconv plugin search <query>\n";
            return 1;
        }

        auto client = make_registry_client();
        auto results = client->search(args.subcommand);

        if (results.empty())
        {
            if (!args.core_options.quiet)
            {
                std::cout << "No plugins found matching: " << args.subcommand << "\n";
            }

            if (args.core_options.json_output)
            {
                std::cout << nlohmann::json::array().dump(2) << std::endl;
            }
            return 0;
        }

        if (args.core_options.json_output)
        {
            nlohmann::json j = nlohmann::json::array();
            for (const auto &entry : results)
            {
                j.push_back(entry.to_json());
            }
            std::cout << j.dump(2) << std::endl;
            return 0;
        }

        std::cout << std::left
                  << std::setw(25) << "NAME"
                  << std::setw(10) << "VERSION"
                  << std::setw(15) << "AUTHOR"
                  << "DESCRIPTION\n";
        std::cout << std::string(80, '-') << "\n";

        for (const auto &entry : results)
        {
            // Truncate description to fit
            std::string desc = entry.description;
            if (desc.size() > 40)
                desc = desc.substr(0, 37) + "...";

            std::cout << std::left
                      << std::setw(25) << entry.name
                      << std::setw(10) << entry.latest
                      << std::setw(15) << entry.author
                      << desc << "\n";
        }

        std::cout << "\n"
                  << results.size() << " plugin(s) found\n";

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
            if (!installed_.is_registry_installed(args.subcommand))
            {
                std::cerr << "Error: " << args.subcommand << " was not installed from registry\n";
                return 1;
            }
            to_update.push_back(args.subcommand);
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
            if (!args.core_options.quiet)
            {
                std::cout << "No registry-installed plugins to update\n";
            }
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
                if (!args.core_options.quiet)
                {
                    std::cerr << "Warning: Could not fetch registry info for " << name << "\n";
                }
                ++failed;
                continue;
            }

            auto release = client->resolve_release(*entry);
            if (!release)
            {
                if (!args.core_options.quiet)
                {
                    std::cerr << "Warning: No compatible release found for " << name << "\n";
                }
                ++failed;
                continue;
            }

            // Compare versions
            int cmp = utils::compare_versions(release->version, record->version);
            if (cmp <= 0)
            {
                if (!args.core_options.quiet)
                {
                    std::cout << name << " is up to date (" << record->version << ")\n";
                }
                continue;
            }

            if (!args.core_options.quiet)
            {
                std::cout << "Updating " << name << " " << record->version
                          << " -> " << release->version << "...\n";
            }

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

        if (!args.core_options.quiet)
        {
            std::cout << "\n"
                      << updated << " plugin(s) updated";
            if (failed > 0)
                std::cout << ", " << failed << " failed";
            std::cout << "\n";
        }

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
