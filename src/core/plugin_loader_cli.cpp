#include "plugin_loader_cli.h"
#include <algorithm>
#include <array>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#endif

namespace uniconv::core {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Simple subprocess execution with timeout support
#ifndef _WIN32
struct SubprocessResult {
    int exit_code = -1;
    std::string stdout_data;
    std::string stderr_data;
    bool timed_out = false;
};

SubprocessResult run_subprocess(const std::string& command,
                                const std::vector<std::string>& args,
                                std::chrono::seconds timeout) {
    SubprocessResult result;

    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.stderr_data = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.stderr_data = "Failed to fork process";
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
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

    auto start_time = std::chrono::steady_clock::now();
    auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();

    std::string stdout_buf, stderr_buf;
    bool done = false;

    while (!done) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto remaining_ms = timeout_ms - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        if (remaining_ms <= 0) {
            // Timeout - kill the process
            kill(pid, SIGKILL);
            result.timed_out = true;
            break;
        }

        int poll_timeout = remaining_ms > 1000 ? 1000 : static_cast<int>(remaining_ms);
        int poll_result = poll(fds.data(), fds.size(), poll_timeout);

        if (poll_result < 0) {
            break;
        }

        // Read stdout
        if (fds[0].revents & POLLIN) {
            char buf[4096];
            ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
            if (n > 0) {
                stdout_buf.append(buf, static_cast<size_t>(n));
            }
        }

        // Read stderr
        if (fds[1].revents & POLLIN) {
            char buf[4096];
            ssize_t n = read(stderr_pipe[0], buf, sizeof(buf));
            if (n > 0) {
                stderr_buf.append(buf, static_cast<size_t>(n));
            }
        }

        // Check if both pipes are closed
        if ((fds[0].revents & POLLHUP) && (fds[1].revents & POLLHUP)) {
            done = true;
        }
    }

    // Read any remaining data
    char buf[4096];
    ssize_t n;
    while ((n = read(stdout_pipe[0], buf, sizeof(buf))) > 0) {
        stdout_buf.append(buf, static_cast<size_t>(n));
    }
    while ((n = read(stderr_pipe[0], buf, sizeof(buf))) > 0) {
        stderr_buf.append(buf, static_cast<size_t>(n));
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    // Wait for child
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    result.stdout_data = std::move(stdout_buf);
    result.stderr_data = std::move(stderr_buf);

    return result;
}
#else
// Windows implementation
SubprocessResult run_subprocess(const std::string& command,
                                const std::vector<std::string>& args,
                                std::chrono::seconds timeout) {
    SubprocessResult result;

    // Build command line
    std::ostringstream cmdline;
    cmdline << "\"" << command << "\"";
    for (const auto& arg : args) {
        cmdline << " \"" << arg << "\"";
    }
    std::string cmdline_str = cmdline.str();

    // Create pipes
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdout_read, stdout_write;
    HANDLE stderr_read, stderr_write;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
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

    if (!CreateProcessA(NULL, cmdline_str.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        result.stderr_data = "Failed to create process";
        CloseHandle(stdout_read); CloseHandle(stdout_write);
        CloseHandle(stderr_read); CloseHandle(stderr_write);
        return result;
    }

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    auto timeout_ms = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);

    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timed_out = true;
    }

    // Read output
    char buf[4096];
    DWORD bytes_read;
    while (ReadFile(stdout_read, buf, sizeof(buf), &bytes_read, NULL) && bytes_read > 0) {
        result.stdout_data.append(buf, bytes_read);
    }
    while (ReadFile(stderr_read, buf, sizeof(buf), &bytes_read, NULL) && bytes_read > 0) {
        result.stderr_data.append(buf, bytes_read);
    }

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
    : manifest_(std::move(manifest)) {
}

PluginInfo CLIPlugin::info() const {
    return manifest_.to_plugin_info();
}

bool CLIPlugin::supports_target(const std::string& target) const {
    auto lower = to_lower(target);
    for (const auto& t : manifest_.targets) {
        if (to_lower(t) == lower) {
            return true;
        }
    }
    return false;
}

bool CLIPlugin::supports_input(const std::string& format) const {
    // If no input formats specified, accept all
    if (manifest_.input_formats.empty()) {
        return true;
    }

    auto lower = to_lower(format);
    for (const auto& f : manifest_.input_formats) {
        if (to_lower(f) == lower) {
            return true;
        }
    }
    return false;
}

std::filesystem::path CLIPlugin::resolve_executable() const {
    // If executable is absolute path, use as-is
    std::filesystem::path exe_path(manifest_.executable);
    if (exe_path.is_absolute()) {
        return exe_path;
    }

    // Check if it's relative to plugin directory
    auto relative_path = manifest_.plugin_dir / manifest_.executable;
    if (std::filesystem::exists(relative_path)) {
        return relative_path;
    }

    // Otherwise, assume it's in PATH
    return manifest_.executable;
}

std::vector<std::string> CLIPlugin::build_arguments(const ETLRequest& request) const {
    std::vector<std::string> args;

    // Universal arguments (all plugins receive these)
    args.push_back("--input");
    args.push_back(request.source.string());

    args.push_back("--target");
    args.push_back(request.target);

    if (request.core_options.output) {
        args.push_back("--output");
        args.push_back(request.core_options.output->string());
    }

    if (request.core_options.force) {
        args.push_back("--force");
    }

    if (request.core_options.dry_run) {
        args.push_back("--dry-run");
    }

    // Plugin-specific options (declared in manifest, passed after --)
    // This includes domain-specific options like --quality, --width for image plugins
    if (!request.plugin_options.empty()) {
        args.push_back("--");
        for (const auto& opt : request.plugin_options) {
            args.push_back(opt);
        }
    }

    return args;
}

CLIPlugin::ExecuteResult CLIPlugin::run_process(
    const std::filesystem::path& executable,
    const std::vector<std::string>& args
) const {
    auto subprocess_result = run_subprocess(executable.string(), args, timeout_);

    ExecuteResult result;
    result.exit_code = subprocess_result.exit_code;
    result.stdout_output = std::move(subprocess_result.stdout_data);
    result.stderr_output = std::move(subprocess_result.stderr_data);
    result.timed_out = subprocess_result.timed_out;
    return result;
}

ETLResult CLIPlugin::parse_result(const ETLRequest& request, const ExecuteResult& exec_result) const {
    // Handle timeout
    if (exec_result.timed_out) {
        return ETLResult::failure(
            request.etl, request.target, request.source,
            "Plugin execution timed out"
        );
    }

    // Handle non-zero exit code without JSON
    if (exec_result.exit_code != 0 && exec_result.stdout_output.empty()) {
        std::string error = "Plugin exited with code " + std::to_string(exec_result.exit_code);
        if (!exec_result.stderr_output.empty()) {
            error += ": " + exec_result.stderr_output;
        }
        return ETLResult::failure(request.etl, request.target, request.source, error);
    }

    // Try to parse JSON output
    try {
        auto j = nlohmann::json::parse(exec_result.stdout_output);

        ETLResult result;
        result.etl = request.etl;
        result.target = request.target;
        result.plugin_used = manifest_.group;
        result.input = request.source;

        // Check success field
        bool success = j.value("success", exec_result.exit_code == 0);
        result.status = success ? ResultStatus::Success : ResultStatus::Error;

        // Output path
        if (j.contains("output")) {
            result.output = j.at("output").get<std::string>();
        }

        // Output size
        if (j.contains("output_size")) {
            result.output_size = j.at("output_size").get<size_t>();
        } else if (result.output && std::filesystem::exists(*result.output)) {
            result.output_size = std::filesystem::file_size(*result.output);
        }

        // Error message
        if (j.contains("error")) {
            result.error = j.at("error").get<std::string>();
        }

        // Extra data
        if (j.contains("extra")) {
            result.extra = j.at("extra");
        }

        return result;

    } catch (const nlohmann::json::exception& e) {
        // JSON parse failed
        std::string error = "Failed to parse plugin output as JSON: ";
        error += e.what();
        if (!exec_result.stderr_output.empty()) {
            error += "\nstderr: " + exec_result.stderr_output;
        }
        return ETLResult::failure(request.etl, request.target, request.source, error);
    }
}

ETLResult CLIPlugin::execute(const ETLRequest& request) {
    // Resolve executable path
    auto exe_path = resolve_executable();

    // Check if executable exists (if it's an absolute path)
    if (exe_path.is_absolute() && !std::filesystem::exists(exe_path)) {
        return ETLResult::failure(
            request.etl, request.target, request.source,
            "Plugin executable not found: " + exe_path.string()
        );
    }

    // Build arguments
    auto args = build_arguments(request);

    // Execute
    auto exec_result = run_process(exe_path, args);

    // Parse and return result
    auto result = parse_result(request, exec_result);
    result.input_size = std::filesystem::file_size(request.source);
    return result;
}

// CLIPluginLoader implementation

std::unique_ptr<plugins::IPlugin> CLIPluginLoader::load(const PluginManifest& manifest) {
    if (!is_cli_plugin(manifest)) {
        return nullptr;
    }
    return std::make_unique<CLIPlugin>(manifest);
}

bool CLIPluginLoader::is_cli_plugin(const PluginManifest& manifest) {
    return manifest.interface == PluginInterface::CLI && !manifest.executable.empty();
}

} // namespace uniconv::core
