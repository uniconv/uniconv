#include "dependency_installer.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace uniconv::core {

namespace {

#ifndef _WIN32
static DependencyInstaller::CommandResult
run_subprocess_impl(const std::string& command, const std::vector<std::string>& args,
                    const std::filesystem::path& working_dir) {
    DependencyInstaller::CommandResult result;

    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.stderr_output = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.stderr_output = "Failed to fork process";
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
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

        // Change working directory if specified
        if (!working_dir.empty()) {
            if (chdir(working_dir.c_str()) != 0) {
                _exit(127);
            }
        }

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

    // Read all output
    std::string stdout_buf, stderr_buf;
    char buf[4096];
    ssize_t n;

    // Use poll to read from both pipes
    std::array<pollfd, 2> fds{};
    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN;

    int open_fds = 2;
    while (open_fds > 0) {
        int poll_result = poll(fds.data(), fds.size(), -1);
        if (poll_result < 0) {
            break;
        }

        if (fds[0].revents & POLLIN) {
            n = read(stdout_pipe[0], buf, sizeof(buf));
            if (n > 0) {
                stdout_buf.append(buf, static_cast<size_t>(n));
            }
        }
        if (fds[0].revents & POLLHUP) {
            fds[0].fd = -1;
            --open_fds;
        }

        if (fds[1].revents & POLLIN) {
            n = read(stderr_pipe[0], buf, sizeof(buf));
            if (n > 0) {
                stderr_buf.append(buf, static_cast<size_t>(n));
            }
        }
        if (fds[1].revents & POLLHUP) {
            fds[1].fd = -1;
            --open_fds;
        }
    }

    // Read any remaining data
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

    result.stdout_output = std::move(stdout_buf);
    result.stderr_output = std::move(stderr_buf);

    return result;
}
#else
// Windows implementation
static DependencyInstaller::CommandResult
run_subprocess_impl(const std::string& command, const std::vector<std::string>& args,
                    const std::filesystem::path& working_dir) {
    DependencyInstaller::CommandResult result;

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
        result.stderr_output = "Failed to create pipes";
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

    const char* cwd = working_dir.empty() ? NULL : working_dir.string().c_str();

    if (!CreateProcessA(NULL, cmdline_str.data(), NULL, NULL, TRUE, 0, NULL,
                        cwd, &si, &pi)) {
        result.stderr_output = "Failed to create process";
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);
        return result;
    }

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    // Wait for process to complete (no timeout for installation)
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Read output
    char buf[4096];
    DWORD bytes_read;
    while (ReadFile(stdout_read, buf, sizeof(buf), &bytes_read, NULL) &&
           bytes_read > 0) {
        result.stdout_output.append(buf, bytes_read);
    }
    while (ReadFile(stderr_read, buf, sizeof(buf), &bytes_read, NULL) &&
           bytes_read > 0) {
        result.stderr_output.append(buf, bytes_read);
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

// DepEnvironment implementation

bool DepEnvironment::load() {
    auto path = deps_file();
    if (!std::filesystem::exists(path)) {
        return false;
    }

    try {
        std::ifstream f(path);
        auto j = nlohmann::json::parse(f);

        plugin_name = j.value("plugin_name", plugin_name);
        dependencies.clear();

        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            for (const auto& dep_json : j["dependencies"]) {
                dependencies.push_back(InstalledDependency::from_json(dep_json));
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool DepEnvironment::save() const {
    try {
        std::filesystem::create_directories(env_dir);

        std::ofstream f(deps_file());
        f << to_json().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

// DependencyInstaller implementation

DependencyInstaller::DependencyInstaller(const std::filesystem::path& deps_base_dir)
    : deps_base_dir_(deps_base_dir) {
    std::filesystem::create_directories(deps_base_dir_);
}

std::string DependencyInstaller::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

DependencyInstaller::CommandResult
DependencyInstaller::run_command(const std::string& command,
                                  const std::vector<std::string>& args,
                                  const std::filesystem::path& working_dir) {
    return run_subprocess_impl(command, args, working_dir);
}

DepEnvironment DependencyInstaller::get_or_create_env(const std::string& plugin_name) {
    DepEnvironment env;
    env.plugin_name = plugin_name;
    env.env_dir = deps_base_dir_ / plugin_name;

    // Try to load existing
    env.load();

    return env;
}

std::optional<DepEnvironment>
DependencyInstaller::get_env(const std::string& plugin_name) const {
    auto env_dir = deps_base_dir_ / plugin_name;
    if (!std::filesystem::exists(env_dir)) {
        return std::nullopt;
    }

    DepEnvironment env;
    env.plugin_name = plugin_name;
    env.env_dir = env_dir;
    env.load();

    return env;
}

bool DependencyInstaller::remove_env(const std::string& plugin_name) {
    auto env_dir = deps_base_dir_ / plugin_name;
    if (!std::filesystem::exists(env_dir)) {
        return true;
    }

    try {
        std::filesystem::remove_all(env_dir);
        return true;
    } catch (...) {
        return false;
    }
}

bool DependencyInstaller::create_python_venv(const std::filesystem::path& venv_dir) {
    // Run: python3 -m venv <path>
    auto result = run_command("python3", {"-m", "venv", venv_dir.string()});

    if (result.exit_code != 0) {
        // Try with just "python" on Windows
#ifdef _WIN32
        result = run_command("python", {"-m", "venv", venv_dir.string()});
#endif
    }

    return result.exit_code == 0;
}

DepInstallResult DependencyInstaller::install_python_package(
    const DepEnvironment& env, const Dependency& dep) {
    DepInstallResult result;

    // Build package spec: name or name==version or name>=version
    std::string package_spec = dep.name;
    if (dep.version) {
        // Check if version has an operator
        const auto& ver = *dep.version;
        if (ver.find(">=") != std::string::npos ||
            ver.find("<=") != std::string::npos ||
            ver.find("==") != std::string::npos ||
            ver.find("~=") != std::string::npos ||
            ver.find("!=") != std::string::npos) {
            package_spec = dep.name + ver;
        } else {
            // Assume exact version
            package_spec = dep.name + "==" + ver;
        }
    }

    auto pip = env.pip_bin().string();
    auto cmd_result = run_command(pip, {"install", package_spec});

    if (cmd_result.exit_code == 0) {
        result.success = true;
        result.installed.push_back(dep.name);
        result.message = "Installed " + package_spec;
    } else {
        result.success = false;
        result.failed.push_back(dep.name);
        result.message = "Failed to install " + package_spec + ": " +
                         cmd_result.stderr_output;
    }

    return result;
}

bool DependencyInstaller::setup_node_env(const std::filesystem::path& node_dir) {
    try {
        std::filesystem::create_directories(node_dir / "node_modules");
        return true;
    } catch (...) {
        return false;
    }
}

DepInstallResult DependencyInstaller::install_node_package(
    const DepEnvironment& env, const Dependency& dep) {
    DepInstallResult result;

    // Build package spec: name or name@version
    std::string package_spec = dep.name;
    if (dep.version) {
        package_spec = dep.name + "@" + *dep.version;
    }

    // Run: npm install --prefix <dir> <package>
    auto cmd_result = run_command(
        "npm",
        {"install", "--prefix", env.node_dir().string(), package_spec});

    if (cmd_result.exit_code == 0) {
        result.success = true;
        result.installed.push_back(dep.name);
        result.message = "Installed " + package_spec;
    } else {
        result.success = false;
        result.failed.push_back(dep.name);
        result.message = "Failed to install " + package_spec + ": " +
                         cmd_result.stderr_output;
    }

    return result;
}

DepInstallResult DependencyInstaller::install_all(
    const PluginManifest& manifest, DepProgressCallback progress) {
    DepInstallResult overall;
    overall.success = true;

    if (manifest.dependencies.empty()) {
        overall.message = "No dependencies to install";
        return overall;
    }

    // Separate dependencies by type
    std::vector<Dependency> python_deps;
    std::vector<Dependency> node_deps;
    std::vector<Dependency> system_deps;

    for (const auto& dep : manifest.dependencies) {
        if (dep.type == "python") {
            python_deps.push_back(dep);
        } else if (dep.type == "node") {
            node_deps.push_back(dep);
        } else {
            // system or unknown
            system_deps.push_back(dep);
        }
    }

    // Get or create environment
    auto env = get_or_create_env(manifest.name);

    // Skip system deps (just warn)
    for (const auto& dep : system_deps) {
        overall.skipped.push_back(dep.name);
        if (progress) {
            progress("Skipping system dependency: " + dep.name +
                     " (requires manual installation)");
        }
    }

    // Install Python dependencies
    if (!python_deps.empty()) {
        // Create venv if needed
        if (!env.has_python_env()) {
            if (progress) {
                progress("Creating Python virtual environment...");
            }

            if (!create_python_venv(env.python_dir())) {
                overall.success = false;
                overall.message = "Failed to create Python virtual environment";
                for (const auto& dep : python_deps) {
                    overall.failed.push_back(dep.name);
                }
                return overall;
            }
        }

        // Install each package
        for (const auto& dep : python_deps) {
            if (progress) {
                progress("Installing Python package: " + dep.name);
            }

            auto pkg_result = install_python_package(env, dep);

            if (pkg_result.success) {
                overall.installed.insert(overall.installed.end(),
                                         pkg_result.installed.begin(),
                                         pkg_result.installed.end());

                // Record in environment
                InstalledDependency installed;
                installed.name = dep.name;
                installed.type = "python";
                installed.version = dep.version.value_or("");
                installed.installed_at = current_timestamp();
                env.dependencies.push_back(installed);
            } else {
                overall.success = false;
                overall.failed.insert(overall.failed.end(),
                                      pkg_result.failed.begin(),
                                      pkg_result.failed.end());
                if (overall.message.empty()) {
                    overall.message = pkg_result.message;
                }
            }
        }
    }

    // Install Node dependencies
    if (!node_deps.empty()) {
        // Set up node env if needed
        if (!env.has_node_env()) {
            if (progress) {
                progress("Setting up Node.js environment...");
            }

            if (!setup_node_env(env.node_dir())) {
                overall.success = false;
                overall.message = "Failed to set up Node.js environment";
                for (const auto& dep : node_deps) {
                    overall.failed.push_back(dep.name);
                }
                return overall;
            }
        }

        // Install each package
        for (const auto& dep : node_deps) {
            if (progress) {
                progress("Installing Node package: " + dep.name);
            }

            auto pkg_result = install_node_package(env, dep);

            if (pkg_result.success) {
                overall.installed.insert(overall.installed.end(),
                                         pkg_result.installed.begin(),
                                         pkg_result.installed.end());

                // Record in environment
                InstalledDependency installed;
                installed.name = dep.name;
                installed.type = "node";
                installed.version = dep.version.value_or("");
                installed.installed_at = current_timestamp();
                env.dependencies.push_back(installed);
            } else {
                overall.success = false;
                overall.failed.insert(overall.failed.end(),
                                      pkg_result.failed.begin(),
                                      pkg_result.failed.end());
                if (overall.message.empty()) {
                    overall.message = pkg_result.message;
                }
            }
        }
    }

    // Save environment state
    env.save();

    // Build summary message
    if (overall.success) {
        overall.message = "Installed " + std::to_string(overall.installed.size()) +
                          " package(s)";
        if (!overall.skipped.empty()) {
            overall.message += ", skipped " +
                               std::to_string(overall.skipped.size()) +
                               " system dependency(ies)";
        }
    }

    return overall;
}

DependencyInstaller::DepCheckResult
DependencyInstaller::check_deps(const PluginManifest& manifest) const {
    DepCheckResult result;
    result.satisfied = true;

    auto env = get_env(manifest.name);
    if (!env) {
        // No environment - check if there are any deps that need one
        for (const auto& dep : manifest.dependencies) {
            if (dep.type == "python" || dep.type == "node") {
                result.satisfied = false;
                result.missing.push_back(dep.name);
            }
        }
        return result;
    }

    // Check each dependency
    for (const auto& dep : manifest.dependencies) {
        if (dep.type == "system") {
            // Skip system deps in check
            continue;
        }

        bool found = false;
        for (const auto& installed : env->dependencies) {
            if (installed.name == dep.name && installed.type == dep.type) {
                found = true;
                result.present.push_back(dep.name);
                break;
            }
        }

        if (!found) {
            result.satisfied = false;
            result.missing.push_back(dep.name);
        }
    }

    return result;
}

std::vector<std::string> DependencyInstaller::clean_orphaned(
    const std::vector<std::string>& installed_plugin_names) {
    std::vector<std::string> removed;

    if (!std::filesystem::exists(deps_base_dir_)) {
        return removed;
    }

    for (const auto& entry : std::filesystem::directory_iterator(deps_base_dir_)) {
        if (!entry.is_directory()) {
            continue;
        }

        auto dir_name = entry.path().filename().string();

        // Check if this plugin is still installed
        bool is_installed = false;
        for (const auto& name : installed_plugin_names) {
            if (name == dir_name) {
                is_installed = true;
                break;
            }
        }

        if (!is_installed) {
            // Remove orphaned environment
            if (remove_env(dir_name)) {
                removed.push_back(dir_name);
            }
        }
    }

    return removed;
}

} // namespace uniconv::core
