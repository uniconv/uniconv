#pragma once

#include "plugin_manifest.h"
#include "plugins/plugin_interface.h"
#include <filesystem>
#include <memory>

namespace uniconv::core {

// Native Plugin wrapper - adapts shared library to IPlugin interface
class NativePlugin : public plugins::IPlugin {
public:
    ~NativePlugin() override;

    // IPlugin interface
    PluginInfo info() const override;
    bool supports_target(const std::string& target) const override;
    bool supports_input(const std::string& format) const override;
    ETLResult execute(const ETLRequest& request) override;

    // Get the manifest
    const PluginManifest& manifest() const { return manifest_; }

    // Check if plugin is loaded
    bool is_loaded() const { return handle_ != nullptr; }

private:
    friend class NativePluginLoader;

    // Private constructor - use NativePluginLoader::load()
    NativePlugin(PluginManifest manifest, void* handle,
                 void* info_func, void* execute_func, void* free_result_func);

    PluginManifest manifest_;
    void* handle_ = nullptr;

    // Function pointers (stored as void* for header simplicity)
    void* info_func_ = nullptr;
    void* execute_func_ = nullptr;
    void* free_result_func_ = nullptr;

    // Cached plugin info from the loaded library
    mutable PluginInfo cached_info_;
    mutable bool info_cached_ = false;

    void unload();
};

// Native Plugin Loader - loads shared libraries as plugins
class NativePluginLoader {
public:
    // Load a native plugin from a manifest
    // Returns nullptr if loading fails
    static std::unique_ptr<plugins::IPlugin> load(const PluginManifest& manifest);

    // Check if a manifest describes a native plugin
    static bool is_native_plugin(const PluginManifest& manifest);

    // Get the expected library extension for the current platform
    static const char* library_extension();

    // Unload a library handle (public for NativePlugin access)
    static void unload_library(void* handle);

private:
    friend class NativePlugin;

    // Platform-specific library loading
    static void* load_library(const std::filesystem::path& path);
    static void* get_symbol(void* handle, const char* name);
    static std::string get_load_error();
};

} // namespace uniconv::core
