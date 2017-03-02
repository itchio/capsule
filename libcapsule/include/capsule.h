
#pragma once

#include "capsule/platform.h"
#include "capsule/constants.h"
#include "capsule/logging.h"
#include "capsule/dynlib.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(CAPSULE_WINDOWS)
#define getpid(a) 0
#define pid_t int
#else // CAPSULE_WINDOWS
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#endif // !CAPSULE_WINDOWS

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
DWORD CAPSULE_DLL capsule_windows_init();
void capsule_install_windows_hooks();
void capsule_install_process_hooks();
void capsule_install_opengl_hooks();
void capsule_install_dxgi_hooks();
void capsule_install_d3d9_hooks();
void capsule_install_ddraw_hooks();

bool dc_capture_init();

#endif // CAPSULE_WINDOWS

struct capture_data {
  bool saw_opengl;
#if defined(CAPSULE_WINDOWS)
  bool saw_dxgi;
  bool saw_d3d9;
#endif // CAPSULE_WINDOWS
  bool active;
};
extern struct capture_data capdata;

bool CAPSULE_STDCALL capsule_capture_ready();
bool CAPSULE_STDCALL capsule_capture_active();
bool CAPSULE_STDCALL capsule_capture_try_start();
bool CAPSULE_STDCALL capsule_capture_try_stop();
int64_t CAPSULE_STDCALL capsule_frame_timestamp();

void CAPSULE_STDCALL capsule_io_init();
void CAPSULE_STDCALL capsule_write_video_format(int width, int height, int format, int vflip, int pitch);
void CAPSULE_STDCALL capsule_write_video_frame(int64_t timestamp, char *frame_data, size_t frame_data_size);

// OpenGL-specific
void CAPSULE_STDCALL gl_capture(int width, int height);

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

static void CAPSULE_STDCALL capsule_assert(const char *msg, int cond) {
  if (cond) {
    return;
  }
  capsule_log("Assertion failed: %s", msg);
  exit(1);
}
