
#pragma once

#include "capsule/platform.h"
#include "capsule/constants.h"
#include "capsule/logging.h"
#include "capsule/dynlib.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(CAPSULE_LINUX) || defined(CAPSULE_OSX)
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#else // CAPSULE_LINUX or CAPSULE_OSX
#define getpid(a) 0
#define pid_t int
#endif // not CAPSULE_LINUX nor CAPSULE_OSX

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
CAPSULE_DLL void capsule_install_windows_hooks();
void capsule_install_opengl_hooks();
void capsule_install_dxgi_hooks();
#endif // CAPSULE_WINDOWS

bool CAPSULE_STDCALL capsule_capture_ready();
int64_t CAPSULE_STDCALL capsule_frame_timestamp();

void CAPSULE_STDCALL capsule_write_video_format(int width, int height, int format, int vflip, int pitch);
void CAPSULE_STDCALL capsule_write_video_frame(int64_t timestamp, char *frameData, size_t frameDataSize);

// OpenGL-specific
void CAPSULE_STDCALL opengl_capture(int width, int height);

void* glXGetProcAddressARB(const char*);
void glXSwapBuffers(void *a, void *b);
int glXQueryExtension(void *a, void *b, void *c);

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
#endif // CAPSULE_WINDOWS

static void CAPSULE_STDCALL assert(const char *msg, int cond) {
  if (cond) {
    return;
  }
  capsule_log("Assertion failed: %s", msg);
  exit(1);
}
