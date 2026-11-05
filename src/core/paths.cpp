#include "core/paths.h"

#include <windows.h>
#include <shlobj.h>

#include <array>

namespace neo::paths {
namespace {

std::filesystem::path getModulePath() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&getModulePath),
            &module)) {
        return {};
    }

    std::array<wchar_t, MAX_PATH> buffer{};
    DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }

    return std::filesystem::path(std::wstring(buffer.data(), length));
}

std::filesystem::path knownFolder(REFKNOWNFOLDERID folder) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(folder, KF_FLAG_DEFAULT, nullptr, &raw))) {
        return {};
    }

    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    return path;
}

} // namespace

std::filesystem::path moduleDirectory() {
    static const std::filesystem::path path = getModulePath().parent_path();
    return path;
}

std::filesystem::path appDataDirectory() {
    static const std::filesystem::path path = knownFolder(FOLDERID_RoamingAppData) / L"Neo";
    return path;
}

std::filesystem::path configFile() {
    return appDataDirectory() / L"config.json";
}

std::filesystem::path logFile() {
    return appDataDirectory() / L"neo.log";
}

std::filesystem::path assetDirectory() {
    return moduleDirectory() / L"assets";
}

std::filesystem::path fontFile(const char* name) {
    return assetDirectory() / L"fonts" / name;
}

} // namespace neo::paths
