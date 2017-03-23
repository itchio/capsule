#pragma once

#include <lab/platform.h>

#if defined(LAB_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#define dlopen(a, b) LoadLibraryA((a))
#define dlsym GetProcAddress
#define RTLD_NOW 0
#define RTLD_LOCAL 0
#define LibHandle HMODULE

#else // LAB_WINDOWS

#include <dlfcn.h>
#define LibHandle void*

#endif // !LAB_WINDOWS
