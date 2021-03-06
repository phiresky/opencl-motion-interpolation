#pragma once

#include <filesystem>
#include <string>

namespace util {
void set_exe_path(const std::string &path);

std::string loadFileToString(const std::string &path);
}  // namespace util
