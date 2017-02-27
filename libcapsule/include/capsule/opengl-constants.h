#pragma once

#if defined(CAPSULE_WINDOWS)
#define DEFAULT_OPENGL "OPENGL32.DLL"
#elif defined(CAPSULE_MACOS)
#define DEFAULT_OPENGL "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#elif defined(CAPSULE_LINUX)
#define DEFAULT_OPENGL "libGL.so.1"
#endif

#define GL_RGB 6407
#define GL_BGRA 32993
#define GL_UNSIGNED_BYTE 5121
#define GL_VIEWPORT 2978
#define GL_RENDERBUFFER 36161
#define GL_RENDERBUFFER_WIDTH 36162
#define GL_RENDERBUFFER_HEIGHT 36163
