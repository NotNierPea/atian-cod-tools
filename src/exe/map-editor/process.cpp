#include "editor.hpp"
#include <TlHelp32.h>

namespace editor {
    Process::~Process() {
        Kill();
        Close();
    }

    bool Process::Start(const std::wstring& cmdline, const fs::path& cwd) {
        Close();
        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE writePipe{};
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            Log(LogLevel::ERR, "can't create a pipe");
            return false;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi{};
        std::wstring cmd{ cmdline };
        std::wstring dir{ cwd.wstring() };
        BOOL ok{ CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
            dir.empty() ? nullptr : dir.c_str(), &si, &pi) };
        CloseHandle(writePipe);
        if (!ok) {
            Logf(LogLevel::ERR, "can't start the process (error %lu)", GetLastError());
            CloseHandle(readPipe);
            readPipe = nullptr;
            return false;
        }
        CloseHandle(pi.hThread);
        process = pi.hProcess;
        exitCode = 0;

        // its output, a line at a time
        reader = std::thread([pipe = readPipe]() {
            std::string pending{};
            char buff[4096];
            DWORD read{};
            while (ReadFile(pipe, buff, sizeof(buff), &read, nullptr) && read) {
                pending.append(buff, read);
                size_t nl;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    std::string line{ pending.substr(0, nl) };
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    Log(LogLevel::PROCESS, line);
                    pending.erase(0, nl + 1);
                }
            }
            if (!pending.empty()) {
                Log(LogLevel::PROCESS, pending);
            }
        });
        return true;
    }

    bool Process::Running() {
        if (!process) {
            return false;
        }
        if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
            return true;
        }
        DWORD code{};
        GetExitCodeProcess(process, &code);
        exitCode = (int)code;
        Close();
        return false;
    }

    void Process::Kill() {
        if (process) {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, 2000);
        }
    }

    void Process::Close() {
        if (process) {
            CloseHandle(process);
            process = nullptr;
        }
        if (reader.joinable()) {
            reader.join();
        }
        if (readPipe) {
            CloseHandle(readPipe);
            readPipe = nullptr;
        }
    }

    bool IsProcessRunning(const wchar_t* exeName) {
        HANDLE snap{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
        if (snap == INVALID_HANDLE_VALUE) {
            return false;
        }
        PROCESSENTRY32W pe{ sizeof(pe) };
        bool found{};
        for (BOOL ok{ Process32FirstW(snap, &pe) }; ok; ok = Process32NextW(snap, &pe)) {
            if (!_wcsicmp(pe.szExeFile, exeName)) {
                found = true;
                break;
            }
        }
        CloseHandle(snap);
        return found;
    }
}
