#include "plugin_loader_native.h"
#include <uniconv/plugin_api.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace uniconv::core
{

    namespace
    {

        std::string to_lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });
            return s;
        }

        // Option context for passing to native plugins
        struct OptionContext
        {
            std::map<std::string, std::string> core_options;
            std::map<std::string, std::string> plugin_options;
        };

        // Option getter callbacks
        const char *get_core_option_impl(const char *key, void *ctx)
        {
            auto *context = static_cast<OptionContext *>(ctx);
            auto it = context->core_options.find(key);
            if (it != context->core_options.end())
            {
                return it->second.c_str();
            }
            return nullptr;
        }

        const char *get_plugin_option_impl(const char *key, void *ctx)
        {
            auto *context = static_cast<OptionContext *>(ctx);
            auto it = context->plugin_options.find(key);
            if (it != context->plugin_options.end())
            {
                return it->second.c_str();
            }
            return nullptr;
        }

    } // anonymous namespace

    // NativePlugin implementation

    NativePlugin::NativePlugin(PluginManifest manifest, void *handle,
                               void *info_func, void *execute_func, void *free_result_func)
        : manifest_(std::move(manifest)), handle_(handle), info_func_(info_func), execute_func_(execute_func), free_result_func_(free_result_func)
    {
    }

    NativePlugin::~NativePlugin()
    {
        unload();
    }

    void NativePlugin::unload()
    {
        if (handle_)
        {
            NativePluginLoader::unload_library(handle_);
            handle_ = nullptr;
        }
        info_func_ = nullptr;
        execute_func_ = nullptr;
        free_result_func_ = nullptr;
    }

    PluginInfo NativePlugin::info() const
    {
        if (info_cached_)
        {
            return cached_info_;
        }

        // Call the plugin's info function
        auto info_fn = reinterpret_cast<UniconvPluginInfoFunc>(info_func_);
        UniconvPluginInfo *native_info = info_fn();

        if (!native_info)
        {
            // Return info from manifest as fallback
            return manifest_.to_plugin_info();
        }

        // Convert to PluginInfo
        // Prefer manifest scope/name (reflects installed location in plugin store)
        cached_info_.name = manifest_.name;
        cached_info_.scope = !manifest_.scope.empty() ? manifest_.scope : (native_info->scope ? native_info->scope : "");
        cached_info_.id = cached_info_.scope;
        cached_info_.version = native_info->version ? native_info->version : manifest_.version;
        cached_info_.description = native_info->description ? native_info->description : manifest_.description;
        cached_info_.builtin = false;
        cached_info_.sink = manifest_.sink;

        // Copy targets (native plugins provide flat list, convert to map with empty extensions)
        if (native_info->targets)
        {
            for (const char **t = native_info->targets; *t != nullptr; ++t)
            {
                cached_info_.targets[*t] = {};
            }
        }

        // Map native input_formats to accepts
        if (native_info->input_formats)
        {
            std::vector<std::string> fmts;
            for (const char **f = native_info->input_formats; *f != nullptr; ++f)
            {
                fmts.emplace_back(*f);
            }
            cached_info_.accepts = std::move(fmts);
        }
        // else: stays nullopt → accept all

        info_cached_ = true;
        return cached_info_;
    }

    bool NativePlugin::supports_target(const std::string &target) const
    {
        auto plugin_info = info();
        auto lower = to_lower(target);
        for (const auto &[t, _] : plugin_info.targets)
        {
            if (to_lower(t) == lower)
            {
                return true;
            }
        }
        return false;
    }

    bool NativePlugin::supports_input(const std::string &format) const
    {
        auto plugin_info = info();

        // nullopt (field omitted) → accept all
        if (!plugin_info.accepts.has_value())
        {
            return true;
        }

        const auto &formats = *plugin_info.accepts;

        // Empty array → accept nothing
        if (formats.empty())
        {
            return false;
        }

        auto lower = to_lower(format);
        for (const auto &f : formats)
        {
            if (to_lower(f) == lower)
            {
                return true;
            }
        }
        return false;
    }

    Result NativePlugin::execute(const Request &request)
    {
        if (!execute_func_)
        {
            return Result::failure(
                request.target, request.source,
                "Plugin execute function not loaded");
        }

        // Build option context
        OptionContext opt_ctx;

        // Pass core options so plugins can query them (e.g., verbose, quiet)
        if (request.core_options.verbose)
            opt_ctx.core_options["verbose"] = "true";
        if (request.core_options.quiet)
            opt_ctx.core_options["quiet"] = "true";

        // Parse plugin options (key=value pairs or --key value or flags)
        for (size_t i = 0; i < request.plugin_options.size(); ++i)
        {
            const auto &opt = request.plugin_options[i];
            auto eq_pos = opt.find('=');
            if (eq_pos != std::string::npos)
            {
                std::string key = opt.substr(0, eq_pos);
                std::string value = opt.substr(eq_pos + 1);
                // Remove leading -- or -
                if (key.starts_with("--"))
                    key = key.substr(2);
                else if (key.starts_with("-"))
                    key = key.substr(1);
                opt_ctx.plugin_options[key] = value;
            }
            else if (opt.starts_with("-"))
            {
                std::string key = opt;
                if (key.starts_with("--"))
                    key = key.substr(2);
                else if (key.starts_with("-"))
                    key = key.substr(1);
                // Check if next token is a value (not a flag)
                if (i + 1 < request.plugin_options.size() &&
                    !request.plugin_options[i + 1].starts_with("-"))
                {
                    opt_ctx.plugin_options[key] = request.plugin_options[++i];
                }
                else
                {
                    opt_ctx.plugin_options[key] = "true";
                }
            }
        }

        // Build native request
        UniconvRequest native_req{};

        // Store strings to keep them alive during execution
        std::string source_str = request.source.string();
        std::string target_str = request.target;
        std::string output_str;

        native_req.source = source_str.c_str();
        native_req.target = target_str.c_str();

        if (request.core_options.output)
        {
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
        UniconvResult *native_result = execute_fn(&native_req);

        if (!native_result)
        {
            return Result::failure(
                request.target, request.source,
                "Plugin returned null result");
        }

        // Convert result
        Result result;
        result.target = request.target;
        result.plugin_used = manifest_.scope;
        result.input = request.source;
        result.input_size = std::filesystem::file_size(request.source);

        switch (native_result->status)
        {
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

        if (native_result->output)
        {
            result.output = native_result->output;
        }
        result.output_size = native_result->output_size;

        if (native_result->error)
        {
            result.error = native_result->error;
        }

        if (native_result->extra_json)
        {
            try
            {
                result.extra = nlohmann::json::parse(native_result->extra_json);
            }
            catch (...)
            {
                // Ignore JSON parse errors
            }
        }

        // Free the native result
        if (free_result_func_)
        {
            auto free_fn = reinterpret_cast<UniconvPluginFreeResultFunc>(free_result_func_);
            free_fn(native_result);
        }

        return result;
    }

    // NativePluginLoader implementation

    std::unique_ptr<plugins::IPlugin> NativePluginLoader::load(const PluginManifest &manifest)
    {
        if (!is_native_plugin(manifest))
        {
            return nullptr;
        }

        // Resolve library path
        std::filesystem::path lib_path;
        if (std::filesystem::path(manifest.library).is_absolute())
        {
            lib_path = manifest.library;
        }
        else
        {
            lib_path = manifest.plugin_dir / manifest.library;
        }

        // Check if library exists
        if (!std::filesystem::exists(lib_path))
        {
            // Try adding platform extension
            std::string lib_with_ext = lib_path.string() + library_extension();
            if (std::filesystem::exists(lib_with_ext))
            {
                lib_path = lib_with_ext;
            }
            else
            {
                std::cerr << "Warning: native plugin '" << manifest.name
                          << "' library not found: " << lib_path << std::endl;
                return nullptr;
            }
        }

        // Load the library
        void *handle = load_library(lib_path);
        if (!handle)
        {
            std::cerr << "Warning: native plugin '" << manifest.name
                      << "' failed to load: " << get_load_error() << std::endl;
            return nullptr;
        }

        // Call optional init function
        void *init_func = get_symbol(handle, UNICONV_PLUGIN_INIT_FUNC);
        if (init_func)
        {
            auto init_fn = reinterpret_cast<UniconvPluginInitFunc>(init_func);
            if (init_fn() != 0)
            {
                std::cerr << "Warning: native plugin '" << manifest.name
                          << "' init failed" << std::endl;
                unload_library(handle);
                return nullptr;
            }
        }

        // Get required symbols
        void *info_func = get_symbol(handle, UNICONV_PLUGIN_INFO_FUNC);
        void *execute_func = get_symbol(handle, UNICONV_PLUGIN_EXECUTE_FUNC);
        void *free_result_func = get_symbol(handle, UNICONV_PLUGIN_FREE_RESULT_FUNC);

        if (!info_func || !execute_func)
        {
            std::cerr << "Warning: native plugin '" << manifest.name
                      << "' missing required symbols" << std::endl;
            unload_library(handle);
            return nullptr;
        }

        // Verify API version
        auto info_fn = reinterpret_cast<UniconvPluginInfoFunc>(info_func);
        UniconvPluginInfo *plugin_info = info_fn();

        // Note: API version check removed - UniconvPluginInfo no longer has api_version field

        return std::unique_ptr<plugins::IPlugin>(
            new NativePlugin(manifest, handle, info_func, execute_func, free_result_func));
    }

    bool NativePluginLoader::is_native_plugin(const PluginManifest &manifest)
    {
        return manifest.iface == PluginInterface::Native && !manifest.library.empty();
    }

    const char *NativePluginLoader::library_extension()
    {
#if defined(_WIN32)
        return ".dll";
#elif defined(__APPLE__)
        return ".dylib";
#else
        return ".so";
#endif
    }

    void *NativePluginLoader::load_library(const std::filesystem::path &path)
    {
#ifdef _WIN32
        return LoadLibraryA(path.string().c_str());
#else
        return dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    }

    void *NativePluginLoader::get_symbol(void *handle, const char *name)
    {
#ifdef _WIN32
        return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
        return dlsym(handle, name);
#endif
    }

    void NativePluginLoader::unload_library(void *handle)
    {
        if (handle)
        {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
        }
    }

    std::string NativePluginLoader::get_load_error()
    {
#ifdef _WIN32
        DWORD error = GetLastError();
        char *msg = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       nullptr, error, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
        std::string result = msg ? msg : "Unknown error";
        LocalFree(msg);
        return result;
#else
        const char *err = dlerror();
        return err ? err : "Unknown error";
#endif
    }

} // namespace uniconv::core
