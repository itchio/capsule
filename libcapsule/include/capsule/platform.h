#pragma once

#if defined(_WIN32)
#define CAPSULE_WINDOWS
#elif defined(__APPLE__)
#define CAPSULE_MACOS
#elif defined(__linux__) || defined(__unix__)
#define CAPSULE_LINUX
#else
#error Unsupported platform
#endif

#if defined(CAPSULE_WINDOWS)
#define CAPSULE_STDCALL __stdcall
#define CAPSULE_STDCALL __stdcall
#else
#define CAPSULE_STDCALL
#endif
