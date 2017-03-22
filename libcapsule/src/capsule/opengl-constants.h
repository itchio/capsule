
#pragma once

#include <lab/platform.h>

#if defined(LAB_WINDOWS)
#define DEFAULT_OPENGL "OPENGL32.DLL"
#elif defined(LAB_MACOS)
#define DEFAULT_OPENGL "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#elif defined(LAB_LINUX)
#define DEFAULT_OPENGL "libGL.so.1"
#endif
