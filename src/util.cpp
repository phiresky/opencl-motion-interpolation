#include <string>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace util
{
std::string _exe_dir;
void set_exe_path(const std::string &path)
{
    _exe_dir = std::filesystem::path(path).parent_path();
}

std::string loadFileToString(const std::string &path)
{
    std::ifstream t(path);
    if (!t)
    {
        std::cout << "file " << path << " not found" << std::endl;
        throw std::invalid_argument("not found");
    }
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

} // namespace util