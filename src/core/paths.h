#pragma once

#include <filesystem>

namespace neo::paths {

std::filesystem::path moduleDirectory();
std::filesystem::path appDataDirectory();
std::filesystem::path configFile();
std::filesystem::path logFile();
std::filesystem::path assetDirectory();
std::filesystem::path fontFile(const char* name);

} // namespace neo::paths
