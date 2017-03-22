
#pragma once

#include <string>

namespace capsule {
namespace paths {

extern std::string kSeparator;

std::string SelfPath();
std::string DirName(const std::string& path);
std::string Join(const std::string& a, const std::string& b);

} // namespace paths
} // namespace capsule
