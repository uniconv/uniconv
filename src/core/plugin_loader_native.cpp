#include "plugin_loader_native.h"
#include <uniconv/plugin_api.h>
#include <algorithm>
#include <cstring>
#include <map>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace uniconv::core {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Option context for passing to native plugins
struct OptionContext {
    std::map<std::string, std::string> core_options;
    std::map<std::string, std::string> plugin_options;
};

// Option getter callbacks
const char* get_core_option_impl(const char* key, void* ctx) {
    auto* context = static_cast<OptionContext*>(ctx);
    auto it = context->core_options.find(key);
    if (it != context->core_options.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

const char* get_plugin_option_impl(const char* key, void* ctx) {
    auto* context = static_cast<OptionContext*>(ctx);
    auto it = context->plugin_options.find(key);
    if (it != context->plugin_options.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

} // anonymous namespace

// NativePlugin implementation

NativePlugin::NativePlugin(PluginManifest manifest, void* handle,
                           void* info_func, void* execute_func, void* free_result_func)
    : manifest_(std::move(manifest))
    , handle_(handle)
    , info_func_(info_func)
    , execute_func_(execute_func)
    , free_result_func_(free_result_func) {
}

NativePlugin::~NativePlugin() {
    unload();
}

void NativePlugin::unload() {
    if (handle_) {
        NativePluginLoader::unload_library(handle_);
        handle_ = nullptr;
    }
    info_func_ = nullptr;
    execute_func_ = nullptr;
    free_result_func_ = nullptr;
}

PluginInfo NativePlugin::info() const {
    if (info_cached_) {
        return cached_info_;
    }

    // Call the plugin's info function
    auto info_fn = reinterpret_cast<UniconvPluginInfoFunc>(info_func_);
    UniconvPluginInfo* native_info = info_fn();

    if (!native_info) {
        // Return info from manifest as fallback
        return manifest_.to_plugin_info();
    }

    // Convert to PluginInfo
    cached_info_.id = std::string(native_info->group) + "." +
                      etl_type_to_string(static_cast<ETLType>(native_info->etl));
    cached_info_.group = native_info->group ? native_info->group : manifest_.group;
    cached_info_.etl = static_cast<ETLType>(native_info->etl);
    cached_info_.version = native_info->version ? native_info->version : manifest_.version;
    cached_info_.description = native_info->description ? native_info->description : manifest_.description;
    cached_info_.builtin = false;

    // Copy targets
    if (native_info->targets) {
        for (const char** t = native_info->targets; *t != nullptr; ++t) {
            cached_info_.targets.emplace_back(*t);
        }
    }

    // Copy input formats
    if (native_info->input_formats) {
        for (const char** f = native_info->input_formats; *f != nullptr; ++f) {
            cached_info_.input_formats.emplace_back(*f);
        }
    }

    info_cached_ = true;
    return cached_info_;
}

bool NativePlugin::supports_target(const std::string& target) const {
    auto plugin_info = info();
    auto lower = to_lower(target);
    for (const auto& t : plugin_info.targets) {
        if (to_lower(t) == lower) {
            return true;
        }
    }
    return false;
}

bool NativePlugin::supports_input(const std::string& format) const {
    auto plugin_info = info();

    // If no input formats specified, accept all
    if (plugin_info.input_formats.empty()) {
        return true;
    }

    auto lower = to_lower(format);
    for (const auto& f : plugin_info.input_formats) {
        if (to_lower(f) == lower) {
            return true;
        }
    }
    return false;
}

ETLResult NativePlugin::execute(const ETLRequest& request) {
    if (!execute_func_) {
        return ETLResult::failure(
            request.etl, request.target, request.source,
            "Plugin execute function not loaded"
        );
    }

    // Build option context
    OptionContext opt_ctx;

    // Parse plugin options (key=value pairs or --key value or flags)
    for (const auto& opt : request.plugin_options) {
        auto eq_pos = opt.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = opt.substr(0, eq_pos);
            std::string value = opt.substr(eq_pos + 1);
            // Remove leading -- or -
            if (key.starts_with("--")) key = key.substr(2);
            else if (key.starts_with("-")) key = key.substr(1);
            opt_ctx.plugin_options[key] = value;
        } else {
            // Boolean flag
            std::string key = opt;
            if (key.starts_with("--")) key = key.substr(2);
            else if (key.starts_with("-")) key = key.substr(1);
            opt_ctx.plugin_options[key] = "true";
        }
    }

    // Build native request
    UniconvRequest native_req{};
    native_req.etl = static_cast<UniconvETLType>(request.etl);
    native_req.source = request.source.string().c_str();
    native_req.target = request.target.c_str();

    std::string output_str;
    if (request.core_options.output) {
        output_str = request.core_options.output->string();
        native_req.output = output_str.c_str();
    }

    native_req.force = request.core_options.force ? 1 : 0;
    native_req.dry_run = request.core_options.dry_run ? 1 : 0;
    native_req.get_core_option = get_core_option_impl;
    native_req.get_plugin_option = get_plugin_option_impl;
    native_req.options_ctx = &opt_ctx;

    // Execute
    auto execute_fn = reinterpret_cast<UniconvPluginExecuteFunc>(execute_func_);
    UniconvResult* native_result = execute_fn(&native_req);

    if (!native_result) {
        return ETLResult::failure(
            request.etl, request.target, request.source,
            "Plugin returned null result"
        );
    }

    // Convert result
    ETLResult result;
    result.etl = request.etl;
    result.target = request.target;
    result.plugin_used = manifest_.group;
    result.input = request.source;
    result.input_size = std::filesystem::file_size(request.source);

    switch (native_result->status) {
        case UNICONV_SUCCESS:
            result.status = ResultStatus::Success;
            break;
        case UNICONV_SKIPPED:
            result.status = ResultStatus::Skipped;
            break;
        default:
            result.status = ResultStatus::Error;
            break;
    }

    if (native_result->output) {
        result.output = native_result->output;
    }
    result.output_size = native_result->output_size;

    if (native_result->error) {
        result.error = native_result->error;
    }

    if (native_result->extra_json) {
        try {
            result.extra = nlohmann::json::parse(native_result->extra_json);
        } catch (...) {
            // Ignore JSON parse errors
        }
    }

    // Free the native result
    if (free_result_func_) {
        auto free_fn = reinterpret_cast<UniconvPluginFreeResultFunc>(free_result_func_);
        free_fn(native_result);
    }

    return result;
}

// NativePluginLoader implementation

std::unique_ptr<plugins::IPlugin> NativePluginLoader::load(const PluginManifest& manifest) {
    if (!is_native_plugin(manifest)) {
        return nullptr;
    }

    // Resolve library path
    std::filesystem::path lib_path;
    if (std::filesystem::path(manifest.library).is_absolute()) {
        lib_path = manifest.library;
    } else {
        lib_path = manifest.plugin_dir / manifest.library;
    }

    // Check if library exists
    if (!std::filesystem::exists(lib_path)) {
        // Try adding platform extension
        std::string lib_with_ext = lib_path.string() + library_extension();
        if (std::filesystem::exists(lib_with_ext)) {
            lib_path = lib_with_ext;
        } else {
            return nullptr;
        }
    }

    // Load the library
    void* handle = load_library(lib_path);
    if (!handle) {
        return nullptr;
    }

    // Get required symbols
    void* info_func = get_symbol(handle, UNICONV_PLUGIN_INFO_FUNC);
    void* execute_func = get_symbol(handle, UNICONV_PLUGIN_EXECUTE_FUNC);
    void* free_result_func = get_symbol(handle, UNICONV_PLUGIN_FREE_RESULT_FUNC);

    if (!info_func || !execute_func) {
        unload_library(handle);
        return nullptr;
    }

    // Verify API version
    auto info_fn = reinterpret_cast<UniconvPluginInfoFunc>(info_func);
    UniconvPluginInfo* plugin_info = info_fn();

    if (plugin_info && plugin_info->api_version != 0 &&
        plugin_info->api_version != UNICONV_API_VERSION) {
        unload_library(handle);
        return nullptr;
    }

    return std::unique_ptr<plugins::IPlugin>(
        new NativePlugin(manifest, handle, info_func, execute_func, free_result_func)
    );
}

bool NativePluginLoader::is_native_plugin(const PluginManifest& manifest) {
    return manifest.interface == PluginInterface::Native && !manifest.library.empty();
}

const char* NativePluginLoader::library_extension() {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

void* NativePluginLoader::load_library(const std::filesystem::path& path) {
#ifdef _WIN32
    return LoadLibraryA(path.string().c_str());
#else
    return dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* NativePluginLoader::get_symbol(void* handle, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

void NativePluginLoader::unload_library(void* handle) {
    if (handle) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
    }
}

std::string NativePluginLoader::get_load_error() {
#ifdef _WIN32
    DWORD error = GetLastError();
    char* msg = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   nullptr, error, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
    std::string result = msg ? msg : "Unknown error";
    LocalFree(msg);
    return result;
#else
    const char* err = dlerror();
    return err ? err : "Unknown error";
#endif
}

} // namespace uniconv::core
