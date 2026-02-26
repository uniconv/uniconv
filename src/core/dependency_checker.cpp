#include "dependency_checker.h"
#include "utils/version_utils.h"
#include <array>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include "utils/win_subprocess.h"
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace uniconv::core
{

    namespace
    {

#ifndef _WIN32
        struct CmdResult
        {
            int exit_code = -1;
            std::string stdout_data;
        };

        CmdResult exec_command(const std::string &command,
                               const std::vector<std::string> &args)
        {
            CmdResult result;

            int stdout_pipe[2];
            if (pipe(stdout_pipe) != 0)
                return result;

            pid_t pid = fork();
            if (pid < 0)
            {
                close(stdout_pipe[0]);
                close(stdout_pipe[1]);
                return result;
            }

            if (pid == 0)
            {
                close(stdout_pipe[0]);
                dup2(stdout_pipe[1], STDOUT_FILENO);
                // Redirect stderr to /dev/null
                int devnull = open("/dev/null", O_WRONLY);
                if (devnull >= 0)
                {
                    dup2(devnull, STDERR_FILENO);
                    close(devnull);
                }
                close(stdout_pipe[1]);

                std::vector<char *> argv;
                argv.push_back(const_cast<char *>(command.c_str()));
                for (const auto &arg : args)
                    argv.push_back(const_cast<char *>(arg.c_str()));
                argv.push_back(nullptr);

                execvp(command.c_str(), argv.data());
                _exit(127);
            }

            close(stdout_pipe[1]);

            char buf[4096];
            ssize_t n;
            while ((n = read(stdout_pipe[0], buf, sizeof(buf))) > 0)
                result.stdout_data.append(buf, static_cast<size_t>(n));

            close(stdout_pipe[0]);

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status))
                result.exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                result.exit_code = 128 + WTERMSIG(status);

            return result;
        }
#else
        struct CmdResult
        {
            int exit_code = -1;
            std::string stdout_data;
        };

        CmdResult exec_command(const std::string &command,
                               const std::vector<std::string> &args)
        {
            CmdResult result;

            std::ostringstream cmdline;
            cmdline << "\"" << command << "\"";
            for (const auto &arg : args)
                cmdline << " \"" << arg << "\"";
            std::string cmdline_str = cmdline.str();

            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = NULL;

            HANDLE stdout_read, stdout_write;
            if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
                return result;

            SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

            // Create a NUL handle to suppress stderr (like /dev/null on Unix)
            HANDLE nul_handle = CreateFileA("NUL", GENERIC_WRITE, 0, &sa,
                                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            si.hStdOutput = stdout_write;
            si.hStdError = nul_handle ? nul_handle : GetStdHandle(STD_ERROR_HANDLE);
            si.dwFlags |= STARTF_USESTDHANDLES;

            PROCESS_INFORMATION pi = {};

            if (!CreateProcessA(NULL, cmdline_str.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
            {
                CloseHandle(stdout_read);
                CloseHandle(stdout_write);
                if (nul_handle) CloseHandle(nul_handle);
                return result;
            }

            CloseHandle(stdout_write);
            if (nul_handle) CloseHandle(nul_handle);

            std::string unused_stderr;
            utils::drain_and_wait(pi.hProcess, stdout_read, NULL,
                                  result.stdout_data, unused_stderr, 10000);

            DWORD exit_code;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            result.exit_code = static_cast<int>(exit_code);

            CloseHandle(stdout_read);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return result;
        }
#endif

        // Extract version string from output like "Python 3.11.5" or "3.11.5"
        std::string extract_version(const std::string &output)
        {
            // Look for a pattern like X.Y.Z or X.Y
            for (size_t i = 0; i < output.size(); ++i)
            {
                if (std::isdigit(static_cast<unsigned char>(output[i])))
                {
                    size_t start = i;
                    while (i < output.size() && (std::isdigit(static_cast<unsigned char>(output[i])) || output[i] == '.'))
                        ++i;
                    std::string candidate = output.substr(start, i - start);
                    // Must contain at least one dot
                    if (candidate.find('.') != std::string::npos)
                        return candidate;
                }
            }
            return "";
        }

    } // anonymous namespace

    int DependencyChecker::run_check_command(const std::string &command,
                                             const std::vector<std::string> &args)
    {
        auto result = exec_command(command, args);
        return result.exit_code;
    }

    std::string DependencyChecker::run_capture_command(const std::string &command,
                                                       const std::vector<std::string> &args)
    {
        auto result = exec_command(command, args);
        if (result.exit_code == 0)
            return result.stdout_data;
        return "";
    }

    DependencyCheckResult DependencyChecker::check(const Dependency &dep) const
    {
        // If custom check command is provided, use it
        if (dep.check)
            return check_custom(dep);

        // Dispatch by type
        if (dep.type == "system")
            return check_system(dep);
        if (dep.type == "python")
            return check_python(dep);
        if (dep.type == "node")
            return check_node(dep);

        // Unknown type
        DependencyCheckResult result;
        result.satisfied = false;
        result.message = "Unknown dependency type: " + dep.type;
        return result;
    }

    std::vector<std::pair<Dependency, DependencyCheckResult>>
    DependencyChecker::check_all(const std::vector<Dependency> &deps) const
    {
        std::vector<std::pair<Dependency, DependencyCheckResult>> results;
        results.reserve(deps.size());

        for (const auto &dep : deps)
        {
            results.emplace_back(dep, check(dep));
        }

        return results;
    }

    std::vector<std::pair<Dependency, DependencyCheckResult>>
    DependencyChecker::check_all(const std::vector<Dependency> &deps,
                                  const std::filesystem::path &python_bin) const
    {
        std::vector<std::pair<Dependency, DependencyCheckResult>> results;
        results.reserve(deps.size());

        // Use venv python for Python deps if the binary exists
#ifdef _WIN32
        std::string python_cmd = "python";
#else
        std::string python_cmd = "python3";
#endif
        if (!python_bin.empty() && std::filesystem::exists(python_bin))
        {
            python_cmd = python_bin.string();
        }

        for (const auto &dep : deps)
        {
            if (dep.type == "python")
            {
                results.emplace_back(dep, check_python(dep, python_cmd));
            }
            else
            {
                results.emplace_back(dep, check(dep));
            }
        }

        return results;
    }

    void DependencyChecker::print_warnings(
        const std::vector<std::pair<Dependency, DependencyCheckResult>> &results)
    {
        bool has_missing = false;
        for (const auto &[dep, result] : results)
        {
            if (!result.satisfied)
            {
                if (!has_missing)
                {
                    std::cerr << "\nMissing dependencies:\n";
                    has_missing = true;
                }
                std::cerr << "  [" << dep.type << "] " << dep.name;
                if (dep.version)
                    std::cerr << " " << *dep.version;
                if (!result.install_hint.empty())
                    std::cerr << " -- " << result.install_hint;
                std::cerr << "\n";
            }
        }

        if (has_missing)
        {
            std::cerr << "\nPlugin may not work until dependencies are resolved.\n";
        }
    }

    DependencyCheckResult DependencyChecker::check_custom(const Dependency &dep) const
    {
        DependencyCheckResult result;

        if (dep.check->empty())
        {
            result.satisfied = false;
            result.message = "Empty check command";
            return result;
        }

        // Run through shell to support operators like ||, &&, pipes, etc.
#ifdef _WIN32
        int exit_code = run_check_command("cmd", {"/c", *dep.check});
#else
        int exit_code = run_check_command("sh", {"-c", *dep.check});
#endif

        if (exit_code == 0)
        {
            result.satisfied = true;
            result.message = dep.name + " found";
        }
        else
        {
            result.satisfied = false;
            result.message = dep.name + " not found (check: " + *dep.check + ")";
        }

        return result;
    }

    DependencyCheckResult DependencyChecker::check_system(const Dependency &dep) const
    {
        DependencyCheckResult result;

#ifdef _WIN32
        // On Windows, "python3" doesn't exist; map to "python"
        std::string check_name = (dep.name == "python3") ? "python" : dep.name;
#else
        const std::string& check_name = dep.name;
#endif

        // Check if binary exists via 'which' (or 'where' on Windows)
#ifdef _WIN32
        int exit_code = run_check_command("where", {check_name});
#else
        int exit_code = run_check_command("which", {check_name});
#endif

        if (exit_code != 0)
        {
            result.satisfied = false;
            result.message = dep.name + " not found in PATH";
#ifdef __APPLE__
            result.install_hint = "brew install " + dep.name;
#elif defined(_WIN32)
            result.install_hint = "winget install " + dep.name;
#else
            result.install_hint = "apt install " + dep.name;
#endif
            return result;
        }

        // If version constraint, try to check version
        if (dep.version)
        {
            auto output = run_capture_command(check_name, {"--version"});
            auto found_version = extract_version(output);

            if (!found_version.empty() &&
                !utils::satisfies_constraint(found_version, *dep.version))
            {
                result.satisfied = false;
                result.message = dep.name + " " + found_version +
                                 " does not satisfy " + *dep.version;
                return result;
            }
        }

        result.satisfied = true;
        result.message = dep.name + " found";
        return result;
    }

    DependencyCheckResult DependencyChecker::check_python(const Dependency &dep,
                                                           const std::string &python_cmd) const
    {
        DependencyCheckResult result;

        auto output = run_capture_command(python_cmd, {"-m", "pip", "show", dep.name});

        if (output.empty())
        {
            result.satisfied = false;
            result.message = "Python package " + dep.name + " not installed";
#ifdef _WIN32
            result.install_hint = "python -m pip install ";
#else
            result.install_hint = "python3 -m pip install ";
#endif
            if (dep.version)
                result.install_hint += "'" + dep.name + *dep.version + "'";
            else
                result.install_hint += dep.name;
            return result;
        }

        // Check version if constraint provided
        if (dep.version)
        {
            // Parse "Version: X.Y.Z" from pip show output
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line))
            {
                if (line.substr(0, 9) == "Version: ")
                {
                    auto found_version = line.substr(9);
                    // Trim whitespace
                    while (!found_version.empty() && std::isspace(static_cast<unsigned char>(found_version.back())))
                        found_version.pop_back();

                    if (!utils::satisfies_constraint(found_version, *dep.version))
                    {
                        result.satisfied = false;
                        result.message = dep.name + " " + found_version +
                                         " does not satisfy " + *dep.version;
#ifdef _WIN32
                        result.install_hint = "python -m pip install '" + dep.name + *dep.version + "'";
#else
                        result.install_hint = "python3 -m pip install '" + dep.name + *dep.version + "'";
#endif
                        return result;
                    }
                    break;
                }
            }
        }

        result.satisfied = true;
        result.message = "Python package " + dep.name + " found";
        return result;
    }

    DependencyCheckResult DependencyChecker::check_node(const Dependency &dep) const
    {
        DependencyCheckResult result;

        auto output = run_capture_command("npm", {"ls", "-g", "--depth=0", dep.name});

        if (output.empty() || output.find(dep.name) == std::string::npos)
        {
            result.satisfied = false;
            result.message = "Node package " + dep.name + " not installed globally";
            result.install_hint = "npm install -g " + dep.name;
            if (dep.version)
                result.install_hint += "@" + dep.version->substr(dep.version->find_first_of("0123456789"));
            return result;
        }

        result.satisfied = true;
        result.message = "Node package " + dep.name + " found";
        return result;
    }

} // namespace uniconv::core
