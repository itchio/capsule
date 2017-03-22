
#pragma once

#include <lab/platform.h>

#include "capsule/constants.h"
#include "capsule/logging.h"
#include "capsule/dynlib.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(LAB_WINDOWS)
#define getpid(a) 0
#define pid_t int
#else // LAB_WINDOWS
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#endif // !LAB_WINDOWS

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LAB_WINDOWS

DWORD __declspec(dllexport) CapsuleWindowsInit();
void CapsuleInstallWindowsHooks();
void CapsuleInstallProcessHooks();
void CapsuleInstallOpenglHooks();
void CapsuleInstallDxgiHooks();
void CapsuleInstallD3d9Hooks();

bool DcCaptureInit();

#endif // LAB_WINDOWS

struct capture_data_settings {
  int fps;
  int size_divider;
  bool gpu_color_conv;
};

struct capture_data {
  bool saw_opengl;
#if defined(LAB_WINDOWS)
  bool saw_dxgi;
  bool saw_d3d9;
#endif // LAB_WINDOWS
  bool active;
  capture_data_settings settings;
};
extern struct capture_data capdata;

// OpenGL-specific
void LAB_STDCALL GlCapture(int width, int height);

void* glXGetProcAddressARB(const char*);
void glXSwapBuffers(void *a, void *b);
int glXQueryExtension(void *a, void *b, void *c);

#ifdef __cplusplus
}
#endif

#ifdef LAB_WINDOWS
// Deviare-InProc, for hooking
#include <NktHookLib.h>

// LoadLibrary, etc.
#define WIN32_WIN32_LEAN_AND_MEAN
#include <windows.h>

extern CNktHookLib cHookMgr;
#endif // LAB_WINDOWS

static void LAB_STDCALL CapsuleAssert(const char *msg, int cond) {
  if (cond) {
    return;
  }
  CapsuleLog("Assertion failed: %s", msg);
  exit(1);
}

namespace capsule {

bool LAB_STDCALL CaptureReady();
bool LAB_STDCALL CaptureActive();
bool LAB_STDCALL CaptureTryStart(struct capture_data_settings* settings);
bool LAB_STDCALL CaptureTryStop();
int64_t LAB_STDCALL FrameTimestamp();

namespace io {

void LAB_STDCALL Init();
void LAB_STDCALL WriteVideoFormat(int width, int height, int format, bool vflip, int64_t pitch);
void LAB_STDCALL WriteVideoFrame(int64_t timestamp, char *frame_data, size_t frame_data_size);

} // namespace io
} // namespace capsule
