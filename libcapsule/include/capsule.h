
#pragma once

#include <stdio.h>

#if defined(_WIN32)
#define DEFAULT_OPENGL "OPENGL32.DLL"
#define CAPSULE_WINDOWS

#elif defined(__APPLE__)
#define DEFAULT_OPENGL "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#define CAPSULE_OSX

#elif defined(__linux__) || defined(__unix__)
#define DEFAULT_OPENGL "libGL.so.1"
#define CAPSULE_LINUX

#else
#error Unsupported platform
#endif

#if defined(CAPSULE_WINDOWS)
#define CAPSULE_STDCALL __stdcall
#define CAPSULE_LOG_PATH "C:\\capsule.log.txt"
#else
#define CAPSULE_STDCALL
#define CAPSULE_LOG_PATH "/tmp/capsule.log.txt"
#endif

#if defined(CAPSULE_LINUX) || defined(CAPSULE_OSX)
#include <sys/types.h>
#include <unistd.h>
#else
#define getpid(a) 0
#define pid_t int
#endif


#ifdef CAPSULE_WINDOWS
#ifdef BUILD_CAPSULE_DLL
#define CAPSULE_DLL __declspec(dllexport)
#else
#define CAPSULE_DLL __declspec(dllimport)
#endif
#else
#define CAPSULE_DLL
#endif

extern FILE *logfile;

#define capsule_log(...) {\
  if (!logfile) { \
    logfile = capsule_open_log(); \
  } \
  fprintf(logfile, __VA_ARGS__); \
  fprintf(logfile, "\n"); \
  fflush(logfile); \
  fprintf(stderr, "[capsule] "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); }

#ifdef __cplusplus
extern "C" {
#endif

FILE *capsule_open_log ();

#ifdef CAPSULE_WINDOWS
CAPSULE_DLL void capsule_install_windows_hooks ();
void capsule_d3d8_sniff();
void capsule_d3d11_sniff();
#endif

void CAPSULE_STDCALL capsule_write_frame (char *frameData, size_t frameDataSize);
void CAPSULE_STDCALL capsule_capture_frame (int width, int height);

void* glXGetProcAddressARB (const char*);
void glXSwapBuffers (void *a, void *b);
int glXQueryExtension (void *a, void *b, void *c);

#ifdef __cplusplus
}
#endif
