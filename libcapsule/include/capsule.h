
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

#define CapsuleLog(...) {\
  if (!logfile) { \
    logfile = CapsuleOpenLog(); \
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

FILE *CapsuleOpenLog();

#ifdef CAPSULE_WINDOWS
wchar_t *CapsuleLogPath();
#else
char *CapsuleLogPath();
#endif // CAPSULE_WINDOWS

#ifdef CAPSULE_WINDOWS
DWORD CAPSULE_DLL CapsuleWindowsInit();
void CapsuleInstallWindowsHooks();
void CapsuleInstallProcessHooks();
void CapsuleInstallOpenglHooks();
void CapsuleInstallDxgiHooks();
void CapsuleInstallD3d9Hooks();
void CapsuleInstallDdrawHooks();

bool DcCaptureInit();

#endif // CAPSULE_WINDOWS

struct capture_data_settings {
  int fps;
  int size_divider;
  bool gpu_color_conv;
};

struct capture_data {
  bool saw_opengl;
#if defined(CAPSULE_WINDOWS)
  bool saw_dxgi;
  bool saw_d3d9;
#endif // CAPSULE_WINDOWS
  bool active;
  capture_data_settings settings;
};
extern struct capture_data capdata;

bool CAPSULE_STDCALL CapsuleCaptureReady();
bool CAPSULE_STDCALL CapsuleCaptureActive();
bool CAPSULE_STDCALL CapsuleCaptureTryStart(struct capture_data_settings* settings);
bool CAPSULE_STDCALL CapsuleCaptureTryStop();
int64_t CAPSULE_STDCALL CapsuleFrameTimestamp();

void CAPSULE_STDCALL CapsuleIoInit();
void CAPSULE_STDCALL CapsuleWriteVideoFormat(int width, int height, int format, int vflip, int pitch);
void CAPSULE_STDCALL CapsuleWriteVideoFrame(int64_t timestamp, char *frame_data, size_t frame_data_size);

// OpenGL-specific
void CAPSULE_STDCALL GlCapture(int width, int height);

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
#define WIN32_WIN32_LEAN_AND_MEAN
#include <windows.h>

extern CNktHookLib cHookMgr;
#endif // CAPSULE_WINDOWS

static void CAPSULE_STDCALL CapsuleAssert(const char *msg, int cond) {
  if (cond) {
    return;
  }
  CapsuleLog("Assertion failed: %s", msg);
  exit(1);
}
