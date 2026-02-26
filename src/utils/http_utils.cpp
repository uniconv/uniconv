#include "http_utils.h"
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include "utils/win_subprocess.h"
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace uniconv::utils
{

    namespace
    {

#ifndef _WIN32
        struct SubprocessResult
        {
            int exit_code = -1;
            std::string stdout_data;
            std::string stderr_data;
        };

        SubprocessResult run_command(const std::string &command,
                                     const std::vector<std::string> &args,
                                     std::chrono::seconds timeout)
        {
            SubprocessResult result;

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
                close(stdout_pipe[0]);
                close(stderr_pipe[0]);

                dup2(stdout_pipe[1], STDOUT_FILENO);
                dup2(stderr_pipe[1], STDERR_FILENO);

                close(stdout_pipe[1]);
                close(stderr_pipe[1]);

                std::vector<char *> argv;
                argv.push_back(const_cast<char *>(command.c_str()));
                for (const auto &arg : args)
                {
                    argv.push_back(const_cast<char *>(arg.c_str()));
                }
                argv.push_back(nullptr);

                execvp(command.c_str(), argv.data());
                _exit(127);
            }

            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            std::array<pollfd, 2> fds{};
            fds[0].fd = stdout_pipe[0];
            fds[0].events = POLLIN;
            fds[1].fd = stderr_pipe[0];
            fds[1].events = POLLIN;

            auto start_time = std::chrono::steady_clock::now();
            auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();

            bool done = false;
            while (!done)
            {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto remaining_ms = timeout_ms - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

                if (remaining_ms <= 0)
                {
                    kill(pid, SIGKILL);
                    break;
                }

                int poll_timeout = remaining_ms > 1000 ? 1000 : static_cast<int>(remaining_ms);
                int poll_result = poll(fds.data(), fds.size(), poll_timeout);

                if (poll_result < 0)
                    break;

                if (fds[0].revents & POLLIN)
                {
                    char buf[4096];
                    ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
                    if (n > 0)
                        result.stdout_data.append(buf, static_cast<size_t>(n));
                }

                if (fds[1].revents & POLLIN)
                {
                    char buf[4096];
                    ssize_t n = read(stderr_pipe[0], buf, sizeof(buf));
                    if (n > 0)
                        result.stderr_data.append(buf, static_cast<size_t>(n));
                }

                if ((fds[0].revents & POLLHUP) && (fds[1].revents & POLLHUP))
                    done = true;
            }

            char buf[4096];
            ssize_t n;
            while ((n = read(stdout_pipe[0], buf, sizeof(buf))) > 0)
                result.stdout_data.append(buf, static_cast<size_t>(n));
            while ((n = read(stderr_pipe[0], buf, sizeof(buf))) > 0)
                result.stderr_data.append(buf, static_cast<size_t>(n));

            close(stdout_pipe[0]);
            close(stderr_pipe[0]);

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status))
                result.exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                result.exit_code = 128 + WTERMSIG(status);

            return result;
        }
#else
        struct SubprocessResult
        {
            int exit_code = -1;
            std::string stdout_data;
            std::string stderr_data;
        };

        SubprocessResult run_command(const std::string &command,
                                     const std::vector<std::string> &args,
                                     std::chrono::seconds timeout)
        {
            SubprocessResult result;

            std::ostringstream cmdline;
            cmdline << "\"" << command << "\"";
            for (const auto &arg : args)
            {
                cmdline << " \"" << arg << "\"";
            }
            std::string cmdline_str = cmdline.str();

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

            if (!CreateProcessA(NULL, cmdline_str.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
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

            auto timeout_ms = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
            drain_and_wait(pi.hProcess, stdout_read, stderr_read,
                           result.stdout_data, result.stderr_data, timeout_ms);

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

    HttpResponse http_get(const std::string &url, std::chrono::seconds timeout)
    {
        HttpResponse response;

        auto timeout_str = std::to_string(timeout.count());
        // Use -w to append HTTP status code after response body
        std::vector<std::string> args = {
            "-sS", "-L", "--max-time", timeout_str,
            "-H", "User-Agent: uniconv",
            "-w", "\n%{http_code}",
            url};

        auto result = run_command("curl", args, timeout + std::chrono::seconds{5});

        if (result.exit_code == 0)
        {
            // Parse the HTTP status code from the end of stdout
            auto &output = result.stdout_data;
            auto last_newline = output.rfind('\n');
            if (last_newline != std::string::npos && last_newline < output.size() - 1)
            {
                std::string status_str = output.substr(last_newline + 1);
                response.status_code = std::stoi(status_str);
                response.body = output.substr(0, last_newline);
            }
            else
            {
                response.status_code = 200;
                response.body = std::move(output);
            }

            response.success = (response.status_code >= 200 && response.status_code < 300);
            if (!response.success)
            {
                // Check for rate limit message in response body
                if (response.body.find("rate limit") != std::string::npos)
                {
                    response.error = "GitHub API rate limit exceeded. Try again later.";
                }
                else
                {
                    response.error = "HTTP " + std::to_string(response.status_code);
                }
            }
        }
        else
        {
            response.success = false;
            response.status_code = result.exit_code;
            response.error = result.stderr_data.empty()
                                 ? "curl exited with code " + std::to_string(result.exit_code)
                                 : result.stderr_data;
        }

        return response;
    }

    bool download_file(const std::string &url,
                       const std::filesystem::path &dest,
                       std::chrono::seconds timeout)
    {
        // Ensure parent directory exists
        if (dest.has_parent_path())
        {
            std::filesystem::create_directories(dest.parent_path());
        }

        auto timeout_str = std::to_string(timeout.count());
        std::vector<std::string> args = {
            "-sS", "-f", "-L", "--max-time", timeout_str,
            "-o", dest.string(), url};

        auto result = run_command("curl", args, timeout + std::chrono::seconds{5});
        return result.exit_code == 0;
    }

    std::optional<std::string> sha256_file(const std::filesystem::path &path)
    {
        if (!std::filesystem::exists(path))
            return std::nullopt;

#ifdef __APPLE__
        std::string cmd = "shasum";
        std::vector<std::string> args = {"-a", "256", path.string()};
#elif defined(_WIN32)
        std::string cmd = "certutil";
        std::vector<std::string> args = {"-hashfile", path.string(), "SHA256"};
#else
        std::string cmd = "sha256sum";
        std::vector<std::string> args = {path.string()};
#endif

        auto result = run_command(cmd, args, std::chrono::seconds{60});

        if (result.exit_code != 0)
            return std::nullopt;

#ifdef _WIN32
        // certutil output format:
        //   SHA256 hash of <file>:
        //   <hex hash with possible spaces>
        //   CertUtil: -hashfile command completed successfully.
        // Extract the second line and remove spaces
        std::istringstream iss(result.stdout_data);
        std::string line;
        std::getline(iss, line); // skip first line
        if (!std::getline(iss, line))
            return std::nullopt;
        // Remove spaces and whitespace
        std::string hash;
        for (char c : line)
        {
            if (c != ' ' && c != '\r' && c != '\n')
                hash += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (hash.size() < 64)
            return std::nullopt;
        return hash;
#else
        // Output format: "<hash>  <filename>\n"
        auto space_pos = result.stdout_data.find(' ');
        if (space_pos == std::string::npos || space_pos < 64)
            return std::nullopt;

        return result.stdout_data.substr(0, space_pos);
#endif
    }

    std::string get_platform_string()
    {
        std::string os;
        std::string arch;

#if defined(__linux__)
        os = "linux";
#elif defined(__APPLE__)
        os = "darwin";
#elif defined(_WIN32)
        os = "windows";
#else
        os = "unknown";
#endif

#if defined(__x86_64__) || defined(_M_X64)
        arch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        arch = "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
        arch = "x86";
#elif defined(__arm__) || defined(_M_ARM)
        arch = "arm";
#else
        arch = "unknown";
#endif

        return os + "-" + arch;
    }

    std::optional<std::string> get_redirect_url(const std::string &url,
                                                 std::chrono::seconds timeout)
    {
        auto timeout_str = std::to_string(timeout.count());
        // Use -o /dev/null (or NUL on Windows) to discard body, -w to get effective URL after redirects
        std::vector<std::string> args = {
            "-sS", "-L", "--max-time", timeout_str,
#ifdef _WIN32
            "-o", "NUL",
#else
            "-o", "/dev/null",
#endif
            "-w", "%{url_effective}",
            url};

        auto result = run_command("curl", args, timeout + std::chrono::seconds{5});

        if (result.exit_code != 0 || result.stdout_data.empty())
            return std::nullopt;

        return result.stdout_data;
    }

} // namespace uniconv::utils
