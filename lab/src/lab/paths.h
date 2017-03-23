
#pragma once

#include <string>

namespace lab {
namespace paths {

extern const char *kSeparator;

std::string SelfPath();
std::string DirName(const std::string path);
std::string Join(const std::string a, const std::string b);
std::string PipePath(std::string name);

} // namespace paths
} // namespace lab
