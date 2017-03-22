
#pragma once

#include "platform.h"

// int64_t etc.
#include <stdint.h>

#if defined(LAB_LINUX) || defined(LAB_MACOS)
#include <sys/types.h>
#endif // LAB_LINUX || LAB_MACOS
