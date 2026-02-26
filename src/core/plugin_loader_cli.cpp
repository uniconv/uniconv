#include "plugin_loader_cli.h"
#include "dependency_installer.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include "utils/win_subprocess.h"
#else
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
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

// Simple subprocess execution
        struct SubprocessResult
        {
            int exit_code = -1;
            std::string stdout_data;
            std::string stderr_data;
        };

#ifndef _WIN32
        SubprocessResult run_subprocess(const std::string &command,
                                        const std::vector<std::string> &args,
                                        const std::map<std::string, std::string> &env = {})
        {
            SubprocessResult result;

            // Create pipes for stdout and stderr
            int stdout_pipe[2];
            int stderr_pipe[2];

            if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
            {
                result.stderr_data = "Failed to create pipes";
                return result;
            }

            pid_t pid = fork();

            if (pid < 0)
            {
                result.stderr_data = "Failed to fork process";
                close(stdout_pipe[0]);
                close(stdout_pipe[1]);
                close(stderr_pipe[0]);
                close(stderr_pipe[1]);
                return result;
            }

            if (pid == 0)
            {
                // Child process
                close(stdout_pipe[0]);
                close(stderr_pipe[0]);

                dup2(stdout_pipe[1], STDOUT_FILENO);
                dup2(stderr_pipe[1], STDERR_FILENO);

                close(stdout_pipe[1]);
                close(stderr_pipe[1]);

                // Set environment variables
                for (const auto &[key, value] : env)
                {
                    setenv(key.c_str(), value.c_str(), 1);
                }

                // Build argv
                std::vector<char *> argv;
                argv.push_back(const_cast<char *>(command.c_str()));
                for (const auto &arg : args)
                {
                    argv.push_back(const_cast<char *>(arg.c_str()));
                }
                argv.push_back(nullptr);

                execvp(command.c_str(), argv.data());

                // If execvp returns, it failed
                _exit(127);
            }

            // Parent process
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            // Set up polling
            std::array<pollfd, 2> fds{};
            fds[0].fd = stdout_pipe[0];
            fds[0].events = POLLIN;
            fds[1].fd = stderr_pipe[0];
            fds[1].events = POLLIN;

            std::string stdout_buf, stderr_buf;
            bool done = false;

            while (!done)
            {
                int poll_result = poll(fds.data(), fds.size(), 1000);

                if (poll_result < 0)
                {
                    break;
                }

                // Read stdout
                if (fds[0].revents & POLLIN)
                {
                    char buf[4096];
                    ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
                    if (n > 0)
                    {
                        stdout_buf.append(buf, static_cast<size_t>(n));
                    }
                }

                // Read stderr
                if (fds[1].revents & POLLIN)
                {
                    char buf[4096];
                    ssize_t n = read(stderr_pipe[0], buf, sizeof(buf));
                    if (n > 0)
                    {
                        stderr_buf.append(buf, static_cast<size_t>(n));
                    }
                }

                // Check if both pipes are closed
                if ((fds[0].revents & POLLHUP) && (fds[1].revents & POLLHUP))
                {
                    done = true;
                }
            }

            // Read any remaining data
            char buf[4096];
            ssize_t n;
            while ((n = read(stdout_pipe[0], buf, sizeof(buf))) > 0)
            {
                stdout_buf.append(buf, static_cast<size_t>(n));
            }
            while ((n = read(stderr_pipe[0], buf, sizeof(buf))) > 0)
            {
                stderr_buf.append(buf, static_cast<size_t>(n));
            }

            close(stdout_pipe[0]);
            close(stderr_pipe[0]);

            // Wait for child
            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status))
            {
                result.exit_code = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status))
            {
                result.exit_code = 128 + WTERMSIG(status);
            }

            result.stdout_data = std::move(stdout_buf);
            result.stderr_data = std::move(stderr_buf);

            return result;
        }
#else
        // Windows implementation
        SubprocessResult run_subprocess(const std::string &command,
                                        const std::vector<std::string> &args,
                                        const std::map<std::string, std::string> &env = {})
        {
            SubprocessResult result;

            // Build command line
            std::ostringstream cmdline;
            cmdline << "\"" << command << "\"";
            for (const auto &arg : args)
            {
                cmdline << " \"" << arg << "\"";
            }
            std::string cmdline_str = cmdline.str();

            // Build environment block if provided
            std::string env_block;
            LPVOID env_ptr = NULL;
            if (!env.empty())
            {
                // Get current environment and merge with new vars
                for (const auto &[key, value] : env)
                {
                    env_block += key + "=" + value + '\0';
                }
                // Add existing environment variables
                char *current_env = GetEnvironmentStringsA();
                if (current_env)
                {
                    char *p = current_env;
                    while (*p)
                    {
                        std::string var(p);
                        // Check if this var is overridden
                        auto eq_pos = var.find('=');
                        if (eq_pos != std::string::npos)
                        {
                            std::string key = var.substr(0, eq_pos);
                            if (env.find(key) == env.end())
                            {
                                env_block += var + '\0';
                            }
                        }
                        p += var.length() + 1;
                    }
                    FreeEnvironmentStringsA(current_env);
                }
                env_block += '\0'; // Double null terminator
                env_ptr = const_cast<char*>(env_block.c_str());
            }

            // Create pipes
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = NULL;

            HANDLE stdout_read, stdout_write;
            HANDLE stderr_read, stderr_write;

            if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
                !CreatePipe(&stderr_read, &stderr_write, &sa, 0))
            {
                result.stderr_data = "Failed to create pipes";
                return result;
            }

            SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            si.hStdOutput = stdout_write;
            si.hStdError = stderr_write;
            si.dwFlags |= STARTF_USESTDHANDLES;

            PROCESS_INFORMATION pi = {};

            if (!CreateProcessA(NULL, cmdline_str.data(), NULL, NULL, TRUE, 0, env_ptr, NULL, &si, &pi))
            {
                result.stderr_data = "Failed to create process";
                CloseHandle(stdout_read);
                CloseHandle(stdout_write);
                CloseHandle(stderr_read);
                CloseHandle(stderr_write);
                return result;
            }

            CloseHandle(stdout_write);
            CloseHandle(stderr_write);

            utils::drain_and_wait(pi.hProcess, stdout_read, stderr_read,
                                  result.stdout_data, result.stderr_data);

            DWORD exit_code;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            result.exit_code = static_cast<int>(exit_code);

            CloseHandle(stdout_read);
            CloseHandle(stderr_read);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return result;
        }
#endif

    } // anonymous namespace

    // CLIPlugin implementation

    CLIPlugin::CLIPlugin(PluginManifest manifest)
        : manifest_(std::move(manifest))
    {
    }

    PluginInfo CLIPlugin::info() const
    {
        return manifest_.to_plugin_info();
    }

    bool CLIPlugin::supports_target(const std::string &target) const
    {
        auto lower = to_lower(target);
        for (const auto &[t, _] : manifest_.targets)
        {
            if (to_lower(t) == lower)
            {
                return true;
            }
        }
        return false;
    }

    bool CLIPlugin::supports_input(const std::string &format) const
    {
        // nullopt (field omitted) → accept all
        if (!manifest_.accepts.has_value())
        {
            return true;
        }

        const auto &formats = *manifest_.accepts;

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

    std::filesystem::path CLIPlugin::resolve_executable() const
    {
        // If executable is absolute path, use as-is
        std::filesystem::path exe_path(manifest_.executable);
        if (exe_path.is_absolute())
        {
            return exe_path;
        }

        // Check if it's relative to plugin directory
        auto relative_path = manifest_.plugin_dir / manifest_.executable;
        if (std::filesystem::exists(relative_path))
        {
            return relative_path;
        }

        // Otherwise, assume it's in PATH
        return manifest_.executable;
    }

    std::vector<std::string> CLIPlugin::build_arguments(const Request &request) const
    {
        std::vector<std::string> args;

        // Universal arguments (all plugins receive these)
        // Skip --input for generator mode (empty source)
        if (!request.source.empty())
        {
            args.push_back("--input");
            args.push_back(request.source.string());
        }

        args.push_back("--target");
        args.push_back(request.target);

        if (request.input_format)
        {
            args.push_back("--input-format");
            args.push_back(*request.input_format);
        }

        if (request.core_options.output)
        {
            args.push_back("--output");
            args.push_back(request.core_options.output->string());
        }

        if (request.core_options.force)
        {
            args.push_back("--force");
        }

        if (request.core_options.dry_run)
        {
            args.push_back("--dry-run");
        }

        // Plugin-specific options (declared in manifest)
        // This includes domain-specific options like --quality, --width for image plugins
        for (const auto &opt : request.plugin_options)
        {
            args.push_back(opt);
        }

        return args;
    }

    std::map<std::string, std::string> CLIPlugin::build_environment() const
    {
        std::map<std::string, std::string> env;

        if (!dep_env_dir_) {
            return env;
        }

        auto env_dir = *dep_env_dir_;
        if (!std::filesystem::exists(env_dir)) {
            return env;
        }

        // Build PATH additions
        std::string path_additions;

#ifdef _WIN32
        auto python_bin = env_dir / "python" / "Scripts";
        auto node_bin = env_dir / "node" / "node_modules" / ".bin";
        const char path_sep = ';';
#else
        auto python_bin = env_dir / "python" / "bin";
        auto node_bin = env_dir / "node" / "node_modules" / ".bin";
        const char path_sep = ':';
#endif

        if (std::filesystem::exists(python_bin)) {
            path_additions = python_bin.string();
        }
        if (std::filesystem::exists(node_bin)) {
            if (!path_additions.empty()) {
                path_additions += path_sep;
            }
            path_additions += node_bin.string();
        }

        // Prepend to existing PATH
        if (!path_additions.empty()) {
            const char* current_path = std::getenv("PATH");
            if (current_path) {
                path_additions += path_sep;
                path_additions += current_path;
            }
            env["PATH"] = path_additions;
        }

        // Set Python virtualenv variables
        auto python_venv = env_dir / "python";
        if (std::filesystem::exists(python_venv)) {
            env["VIRTUAL_ENV"] = python_venv.string();

            // Set PYTHONPATH to site-packages
#ifdef _WIN32
            auto site_packages = python_venv / "Lib" / "site-packages";
#else
            // Find the python version directory
            auto lib_dir = python_venv / "lib";
            std::filesystem::path site_packages;
            if (std::filesystem::exists(lib_dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(lib_dir)) {
                    if (entry.is_directory() &&
                        entry.path().filename().string().find("python") == 0) {
                        site_packages = entry.path() / "site-packages";
                        break;
                    }
                }
            }
#endif
            if (std::filesystem::exists(site_packages)) {
                env["PYTHONPATH"] = site_packages.string();
            }
        }

        // Set NODE_PATH
        auto node_modules = env_dir / "node" / "node_modules";
        if (std::filesystem::exists(node_modules)) {
            env["NODE_PATH"] = node_modules.string();
        }

        return env;
    }

    void CLIPlugin::set_dep_environment(std::optional<DepEnvironment> env)
    {
        if (env) {
            dep_env_dir_ = env->env_dir;
        } else {
            dep_env_dir_ = std::nullopt;
        }
    }

    CLIPlugin::ExecuteResult CLIPlugin::run_process(
        const std::filesystem::path &executable,
        const std::vector<std::string> &args,
        const std::map<std::string, std::string> &extra_env) const
    {
        auto command = executable.string();
        auto final_args = args;

        // Merge build_environment with extra_env
        auto env = build_environment();
        for (const auto& [key, value] : extra_env) {
            env[key] = value;
        }

#ifdef _WIN32
        // Windows lacks shebang support; map script extensions to interpreters
        auto ext = to_lower(executable.extension().string());
        std::string interpreter;
        if (ext == ".py")
            interpreter = "python";
        else if (ext == ".js")
            interpreter = "node";
        else if (ext == ".rb")
            interpreter = "ruby";

        if (!interpreter.empty())
        {
            final_args.insert(final_args.begin(), command);
            command = interpreter;
        }
#endif

        auto subprocess_result = run_subprocess(command, final_args, env);

        ExecuteResult result;
        result.exit_code = subprocess_result.exit_code;
        result.stdout_output = std::move(subprocess_result.stdout_data);
        result.stderr_output = std::move(subprocess_result.stderr_data);
        return result;
    }

    Result CLIPlugin::parse_result(const Request &request, const ExecuteResult &exec_result) const
    {
        // Handle non-zero exit code without JSON
        if (exec_result.exit_code != 0 && exec_result.stdout_output.empty())
        {
            std::string error = "Plugin exited with code " + std::to_string(exec_result.exit_code);
            if (!exec_result.stderr_output.empty())
            {
                error += ": " + exec_result.stderr_output;
            }
            return Result::failure(request.target, request.source, error);
        }

        // Try to parse JSON output
        try
        {
            auto j = nlohmann::json::parse(exec_result.stdout_output);

            Result result;
            result.target = request.target;
            result.plugin_used = manifest_.scope;
            result.input = request.source;

            // Check success field
            bool success = j.value("success", exec_result.exit_code == 0);
            result.status = success ? ResultStatus::Success : ResultStatus::Error;

            // Output path (single output)
            if (j.contains("output"))
            {
                result.output = j.at("output").get<std::string>();
            }

            // Scatter outputs (multiple outputs from 1→N plugin)
            if (j.contains("outputs") && j.at("outputs").is_array())
            {
                for (const auto &o : j.at("outputs"))
                {
                    result.outputs.push_back(o.get<std::string>());
                }
            }

            // Output size
            if (j.contains("output_size"))
            {
                result.output_size = j.at("output_size").get<size_t>();
            }
            else if (result.output && std::filesystem::exists(*result.output) &&
                     !std::filesystem::is_directory(*result.output))
            {
                result.output_size = std::filesystem::file_size(*result.output);
            }

            // Error message
            if (j.contains("error"))
            {
                result.error = j.at("error").get<std::string>();
            }

            // Extra data
            if (j.contains("extra"))
            {
                result.extra = j.at("extra");
            }

            return result;
        }
        catch (const nlohmann::json::exception &e)
        {
            // JSON parse failed
            std::string error = "Failed to parse plugin output as JSON: ";
            error += e.what();
            if (!exec_result.stderr_output.empty())
            {
                error += "\nstderr: " + exec_result.stderr_output;
            }
            return Result::failure(request.target, request.source, error);
        }
    }

    Result CLIPlugin::execute(const Request &request)
    {
        // Resolve executable path
        auto exe_path = resolve_executable();

        // Check if executable exists (if it's an absolute path)
        if (exe_path.is_absolute() && !std::filesystem::exists(exe_path))
        {
            return Result::failure(
                request.target, request.source,
                "Plugin executable not found: " + exe_path.string());
        }

        // Build arguments
        auto args = build_arguments(request);

        // Execute
        auto exec_result = run_process(exe_path, args);

        // Parse and return result
        auto result = parse_result(request, exec_result);
        result.input_size = (request.source.empty() || std::filesystem::is_directory(request.source))
            ? 0 : std::filesystem::file_size(request.source);
        return result;
    }

    // CLIPluginLoader implementation

    std::unique_ptr<plugins::IPlugin> CLIPluginLoader::load(const PluginManifest &manifest)
    {
        if (!is_cli_plugin(manifest))
        {
            return nullptr;
        }
        return std::make_unique<CLIPlugin>(manifest);
    }

    bool CLIPluginLoader::is_cli_plugin(const PluginManifest &manifest)
    {
        return manifest.iface == PluginInterface::CLI && !manifest.executable.empty();
    }

} // namespace uniconv::core
