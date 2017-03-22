
#pragma once

#include <stdio.h>

#include "platform.h"
#include "strings.h"

#include <string>

namespace lab {
namespace io {

FILE* Fopen(std::string path, std::string mode);

} // namespace io
} // namespace lab
