#pragma once

namespace lab {

extern const char *kPlatform;

#if defined(_WIN32)
#define LAB_WINDOWS
#elif defined(__APPLE__)
#define LAB_MACOS
#elif defined(__linux__) || defined(__unix__)
#define LAB_LINUX
#else
#error Unsupported platform
#endif

#if defined(LAB_WINDOWS)
#define LAB_STDCALL __stdcall
#define LAB_STDCALL __stdcall
#else
#define LAB_STDCALL
#endif

}
