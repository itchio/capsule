
#include "platform.h"

namespace lab {

#if defined(LAB_WINDOWS)
const char* kPlatform = "Windows";
#elif defined(LAB_MACOS)
const char* kPlatform = "macOS";
#elif defined(LAB_LINUX)
const char* kPlatform = "Linux";
#endif // LAB_LINUX

} // namespace lab
