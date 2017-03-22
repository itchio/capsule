#pragma once

#include <vector>
#include <string>

#include "platform.h"

namespace lab {
namespace env {

char **MergeBlocks (char **a, char **b);

std::string Get(std::string name);
bool Set(std::string name, std::string value);

#if defined(LAB_WINDOWS)
std::string Expand(std::string input);
#endif // LAB_WINDOWS

} // namespace env
} // namespace lab
