#pragma once

#if defined(CAPSULE_WINDOWS)
#define DEFAULT_OPENGL "OPENGL32.DLL"
#elif defined(CAPSULE_MACOS)
#define DEFAULT_OPENGL "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#elif defined(CAPSULE_LINUX)
#define DEFAULT_OPENGL "libGL.so.1"
#endif
