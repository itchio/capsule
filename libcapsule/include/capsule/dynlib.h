#pragma once

#include "platform.h"

#if defined(CAPSULE_WINDOWS)
#include <windows.h>
#define dlopen(a, b) LoadLibrary((a))
#define dlsym GetProcAddress
#define RTLD_NOW 0
#define RTLD_LOCAL 0
#define LIBHANDLE HMODULE
#else

#include <dlfcn.h>
#define LIBHANDLE void*
#endif
