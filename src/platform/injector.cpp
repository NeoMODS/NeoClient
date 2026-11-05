#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

struct Options {
    DWORD pid = 0;
    std::wstring processName;
    std::wstring dllPath;
    bool list = false;
};

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return value;
}

void printUsage() {
    std::wcout << L"NeoInjector - NeoClient loader\n\n"
               << L"Usage:\n"
               << L"  NeoInjector.exe                 Find the Minecraft window and inject\n"
               << L"  NeoInjector.exe --pid <pid>     Inject into a specific process\n"
               << L"  NeoInjector.exe --name <exe>    Inject into the first matching process\n"
               << L"  NeoInjector.exe --dll <path>    Use a custom NeoClient.dll\n"
               << L"  NeoInjector.exe --list          List candidate processes\n\n";
}

std::filesystem::path executableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
}

DWORD findMinecraftWindowProcess() {
    struct Search {
        DWORD pid = 0;
    } search;

    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        auto& search = *reinterpret_cast<Search*>(parameter);
        if (!IsWindowVisible(window)) {
            return TRUE;
        }

        wchar_t title[256]{};
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (std::wcsstr(title, L"Minecraft") == nullptr) {
            return TRUE;
        }

        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (pid != 0) {
            search.pid = pid;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));

    return search.pid;
}

std::vector<PROCESSENTRY32W> listProcesses() {
    std::vector<PROCESSENTRY32W> result;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            result.push_back(entry);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

DWORD findProcessByName(const std::wstring& name) {
    const std::wstring needle = lower(name);
    for (const auto& entry : listProcesses()) {
        if (lower(entry.szExeFile) == needle) {
            return entry.th32ProcessID;
        }
    }
    return 0;
}

bool isMinecraftLike(const std::wstring& name) {
    const std::wstring value = lower(name);
    return value == L"javaw.exe" || value == L"java.exe" || value == L"minecraft.exe";
}

bool enableDebugPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &privileges.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }

    const BOOL result = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
    CloseHandle(token);
    return result == TRUE;
}

bool inject(DWORD pid, const std::filesystem::path& dll) {
    if (pid == 0) {
        std::wcerr << L"[!] Could not find a target process.\n";
        return false;
    }

    std::error_code ec;
    const std::wstring fullPath = std::filesystem::absolute(dll, ec).wstring();
    if (ec || !std::filesystem::exists(fullPath)) {
        std::wcerr << L"[!] DLL not found: " << fullPath << L"\n";
        return false;
    }

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_SUSPEND_RESUME,
        FALSE, pid);
    if (!process) {
        std::wcerr << L"[!] OpenProcess failed (" << GetLastError() << L"). Run as administrator.\n";
        return false;
    }

    BOOL targetWow64 = FALSE;
    IsWow64Process(process, &targetWow64);
    BOOL selfWow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &selfWow64);
    if (targetWow64 != selfWow64) {
        std::wcerr << L"[!] Architecture mismatch. Build the injector for the same architecture as Minecraft.\n";
        CloseHandle(process);
        return false;
    }

    const SIZE_T bytes = (fullPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        std::wcerr << L"[!] VirtualAllocEx failed (" << GetLastError() << L").\n";
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, remote, fullPath.c_str(), bytes, nullptr)) {
        std::wcerr << L"[!] WriteProcessMemory failed (" << GetLastError() << L").\n";
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32, "LoadLibraryW"));
    if (!loadLibrary) {
        std::wcerr << L"[!] Could not resolve LoadLibraryW.\n";
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr);
    if (!thread) {
        std::wcerr << L"[!] CreateRemoteThread failed (" << GetLastError() << L").\n";
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    std::wcout << L"[+] Injecting into PID " << pid << L"...\n";
    WaitForSingleObject(thread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);

    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    if (exitCode == 0) {
        std::wcerr << L"[!] LoadLibraryW returned 0. Check that the DLL architecture matches.\n";
        return false;
    }

    std::wcout << L"[+] NeoClient loaded. Press Right Shift or Insert in game.\n";
    return true;
}

Options parseOptions(int argc, wchar_t** argv) {
    Options options;
    options.dllPath = (executableDirectory() / L"NeoClient.dll").wstring();

    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        const auto next = [&](std::wstring& target) -> bool {
            if (i + 1 >= argc) {
                return false;
            }
            target = argv[++i];
            return true;
        };

        if (argument == L"--pid") {
            std::wstring value;
            if (next(value)) {
                options.pid = static_cast<DWORD>(std::wcstoul(value.c_str(), nullptr, 10));
            }
        } else if (argument == L"--name") {
            next(options.processName);
        } else if (argument == L"--dll") {
            next(options.dllPath);
        } else if (argument == L"--list") {
            options.list = true;
        } else if (argument == L"--help" || argument == L"-h" || argument == L"/?") {
            printUsage();
            ExitProcess(0);
        } else {
            std::wcerr << L"[!] Unknown argument: " << argument << L"\n";
            printUsage();
            ExitProcess(2);
        }
    }

    return options;
}

} // namespace

int main() {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return 1;
    }

    Options options = parseOptions(argc, argv);
    LocalFree(argv);

    if (options.list) {
        std::wcout << L"Candidate processes:\n";
        for (const auto& entry : listProcesses()) {
            if (isMinecraftLike(entry.szExeFile)) {
                std::wcout << L"  " << entry.th32ProcessID << L"  " << entry.szExeFile << L"\n";
            }
        }
        return 0;
    }

    enableDebugPrivilege();

    DWORD pid = options.pid;
    if (pid == 0 && !options.processName.empty()) {
        pid = findProcessByName(options.processName);
    }
    if (pid == 0) {
        pid = findMinecraftWindowProcess();
    }
    if (pid == 0) {
        // Fall back to the only javaw.exe running, which covers most setups.
        DWORD candidate = 0;
        int matches = 0;
        for (const auto& entry : listProcesses()) {
            if (isMinecraftLike(entry.szExeFile)) {
                candidate = entry.th32ProcessID;
                ++matches;
            }
        }
        if (matches == 1) {
            pid = candidate;
        }
    }

    return inject(pid, options.dllPath) ? 0 : 1;
}
