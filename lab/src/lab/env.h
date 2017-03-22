#pragma once

#include <vector>
#include <string>

namespace lab {
namespace env {

char **MergeBlocks (char **a, char **b);

std::string Get(std::string name);
bool Set(std::string name, std::string value);

} // namespace env
} // namespace lab
