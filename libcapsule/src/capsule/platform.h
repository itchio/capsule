#pragma once

#if defined(_WIN32)
#define CAPSULE_WINDOWS
#define CAPSULE_PLATFORM "Windows"
#elif defined(__APPLE__)
#define CAPSULE_MACOS
#define CAPSULE_PLATFORM "macOS"
#elif defined(__linux__) || defined(__unix__)
#define CAPSULE_LINUX
#define CAPSULE_PLATFORM "GNU/Linux"
#else
#error Unsupported platform
#endif

#if defined(CAPSULE_WINDOWS)
#define CAPSULE_STDCALL __stdcall
#define CAPSULE_STDCALL __stdcall
#else
#define CAPSULE_STDCALL
#endif
