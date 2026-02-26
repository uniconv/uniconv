#pragma once

#ifdef _WIN32

#include <string>
#include <windows.h>

namespace uniconv::utils
{

    // Read available data from a pipe without blocking.
    // Returns true if any data was read.
    inline bool read_pipe_nonblocking(HANDLE pipe, std::string &out)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL))
            return false;

        if (available == 0)
            return false;

        char buf[4096];
        while (available > 0)
        {
            DWORD to_read = (available < sizeof(buf)) ? available : sizeof(buf);
            DWORD bytes_read = 0;
            if (!ReadFile(pipe, buf, to_read, &bytes_read, NULL) || bytes_read == 0)
                break;
            out.append(buf, bytes_read);
            available -= bytes_read;
        }
        return true;
    }

    // Drain stdout/stderr pipes while waiting for a process to finish.
    // Polls every 100ms so pipe buffers never fill up (prevents deadlock).
    // Either or both pipe handles may be NULL to skip that stream.
    // timeout_ms = 0 means INFINITE.
    inline bool drain_and_wait(HANDLE process,
                               HANDLE h_stdout,
                               HANDLE h_stderr,
                               std::string &out_stdout,
                               std::string &out_stderr,
                               DWORD timeout_ms = 0)
    {
        const DWORD poll_interval = 100;
        DWORD elapsed = 0;

        while (true)
        {
            // Drain available data from both pipes
            if (h_stdout)
                read_pipe_nonblocking(h_stdout, out_stdout);
            if (h_stderr)
                read_pipe_nonblocking(h_stderr, out_stderr);

            // Check if process has exited
            DWORD wait_result = WaitForSingleObject(process, poll_interval);
            if (wait_result == WAIT_OBJECT_0)
                break; // Process exited

            elapsed += poll_interval;
            if (timeout_ms != 0 && elapsed >= timeout_ms)
            {
                // Timeout — terminate the process
                TerminateProcess(process, 1);
                WaitForSingleObject(process, 1000);
                break;
            }
        }

        // Final drain — read any remaining data after process exit
        if (h_stdout)
        {
            while (read_pipe_nonblocking(h_stdout, out_stdout))
            {
            }
        }
        if (h_stderr)
        {
            while (read_pipe_nonblocking(h_stderr, out_stderr))
            {
            }
        }

        return true;
    }

} // namespace uniconv::utils

#endif // _WIN32
