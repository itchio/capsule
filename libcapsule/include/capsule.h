
#pragma once

#include <stdio.h>
#include <stdint.h>

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
#define CAPSULE_STDCALL __stdcall
#else
#define CAPSULE_STDCALL
#endif

#if defined(CAPSULE_LINUX) || defined(CAPSULE_OSX)
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#else
#define getpid(a) 0
#define pid_t int
#endif

#ifdef CAPSULE_LINUX
int capsule_x11_init();
int capsule_x11_should_capture();
#endif

#ifdef CAPSULE_WINDOWS

#ifdef BUILD_CAPSULE_DLL
#define CAPSULE_DLL __declspec(dllexport)
#else // BUILD_CAPSULE_DLL
#define CAPSULE_DLL __declspec(dllimport)
#endif // BUILD_CAPSULE_DLL

#else // CAPSULE_WINDOWS
#define CAPSULE_DLL
#endif // CAPSULE_WINDOWS

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

FILE *capsule_open_log();

#ifdef CAPSULE_WINDOWS
wchar_t *capsule_log_path();
#else
char *capsule_log_path();
#endif // CAPSULE_WINDOWS

#ifdef CAPSULE_WINDOWS
CAPSULE_DLL void capsule_install_windows_hooks ();
void capsule_install_opengl_hooks();
void capsule_install_dxgi_hooks();
#endif // CAPSULE_WINDOWS

void CAPSULE_STDCALL capsule_write_resolution (int width, int height);
void CAPSULE_STDCALL capsule_write_delta (int64_t delta);
void CAPSULE_STDCALL capsule_write_frame (char *frameData, size_t frameDataSize);
void CAPSULE_STDCALL capsule_capture_frame (int width, int height);

void* glXGetProcAddressARB (const char*);
void glXSwapBuffers (void *a, void *b);
int glXQueryExtension (void *a, void *b, void *c);

#ifdef __cplusplus
}
#endif

#ifdef CAPSULE_WINDOWS
// Deviare-InProc, for hooking
#include <NktHookLib.h>

// LoadLibrary, etc.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern CNktHookLib cHookMgr;

void* capsule_get_CreateSwapChain_address(void *);
void* capsule_get_CreateSwapChainForHwnd_address(void *);
void* capsule_get_Present_address(void *);
#endif // CAPSULE_WINDOWS
