
#define _GNU_SOURCE
#ifdef _WIN32
#include <detours.h>
#endif

#include "libfake.h"

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

#define libfake_log(...) {\
  fprintf(stderr, "[libfake] "); \
  fprintf(stderr, __VA_ARGS__); }

static void assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  libfake_log("Assertion failed: %s\n", msg);
  /* const char *err = SDL_GetError(); */
  /* libfake_log("[main] Last SDL GetError: %s\n", err); */
  exit(1);
}

#ifdef CAPSULE_WINDOWS
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>

typedef void (*glSwapBuffersType)(void*);
glSwapBuffersType _glSwapBuffers;

void __stdcall _fakeSwapBuffers (void *hdc) {
  libfake_log("In fakeSwapBuffers\n");
  libfake_captureFrame();
  // assert("Installed hook", Mhook_Unhook(_glSwapBuffers));
  _glSwapBuffers(hdc);
  // assert("Installed hook", Mhook_SetHook(_glSwapBuffers, _fakeSwapBuffers));
}

void LIBFAKE_DLL libfake_hello () {
  libfake_log("Hello from libfake!\n");
  HMODULE mh = GetModuleHandle("opengl32.dll");
  libfake_log("OpenGL handle: %p\n", mh);
  if (mh) {
    _glSwapBuffers = (glSwapBuffersType) GetProcAddress(mh, "wglSwapBuffers");
    libfake_log("SwapBuffers handle: %p\n", _glSwapBuffers);

    if (_glSwapBuffers) {
      libfake_log("Attempting to install glSwapBuffers hook\n");
      // assert("Installed hook", Mhook_SetHook(_glSwapBuffers, _fakeSwapBuffers));
    }
  }

  HMODULE m8 = GetModuleHandle("d3d8.dll");
  libfake_log("Direct3D8 handle: %p\n", m8);
  HMODULE m9 = GetModuleHandle("d3d9.dll");
  libfake_log("Direct3D9 handle: %p\n", m9);
}

void __stdcall DllMain(void *hinstDLL, int reason, void *reserved) {
  libfake_log("DllMain called! reason = %d\n", reason);
}
#endif

#ifdef CAPSULE_LINUX
void glXSwapBuffers (void *a, void *b);

typedef int (*glXQueryExtensionType)(void*, void*, void*);
glXQueryExtensionType _realglXQueryExtension;

typedef void (*glXSwapBuffersType)(void*, void*);
glXSwapBuffersType _realglXSwapBuffers;

typedef void* (*glXGetProcAddressARBType)(const char*);
glXGetProcAddressARBType _realglXGetProcAddressARB;
#endif

#define FRAME_WIDTH 512
#define FRAME_HEIGHT 512
char *frameData;
int frameNumber = 0;

typedef void* (*dlopen_type)(const char*, int);
dlopen_type real_dlopen;
void *gl_handle;

void ensure_real_dlopen() {
#ifdef CAPSULE_LINUX
  if (!real_dlopen) {
    libfake_log("Getting real dlopen\n");
    real_dlopen = dlsym(RTLD_NEXT, "dlopen");
  }
#endif
}

void load_opengl (const char *openglPath) {
  libfake_log("Loading real opengl from %s\n", openglPath);
#ifdef CAPSULE_LINUX
  ensure_real_dlopen();
  gl_handle = real_dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#else
  gl_handle = dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#endif
  assert("Loaded real OpenGL lib", !!gl_handle);
  libfake_log("Loaded opengl!\n");

#ifdef CAPSULE_LINUX
  libfake_log("Getting glXQueryExtension adress\n");
  _realglXQueryExtension = dlsym(gl_handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);
  libfake_log("Got glXQueryExtension adress: %p\n", _realglXQueryExtension);

  libfake_log("Getting glXSwapBuffers adress\n");
  _realglXSwapBuffers = dlsym(gl_handle, "glXSwapBuffers");
  assert("Got glXSwapBuffers", !!_realglXSwapBuffers);
  libfake_log("Got glXSwapBuffers adress: %p\n", _realglXSwapBuffers);

  libfake_log("Getting glXGetProcAddressARB address\n");
  _realglXGetProcAddressARB = dlsym(gl_handle, "glXGetProcAddressARB");
  assert("Got glXGetProcAddressARB", !!_realglXGetProcAddressARB);
  libfake_log("Got glXGetProcAddressARB adress: %p\n", _realglXGetProcAddressARB);
#endif
}

void ensure_opengl () {
  if (!gl_handle) {
    load_opengl(DEFAULT_OPENGL);
  }
}

#ifdef CAPSULE_LINUX
void* glXGetProcAddressARB (const char *name) {
  if (strcmp(name, "glXSwapBuffers") == 0) {
    libfake_log("In glXGetProcAddressARB: %s\n", name);
    libfake_log("Returning fake glXSwapBuffers\n");
    return &glXSwapBuffers;
  }

  /* libfake_log("In glXGetProcAddressARB: %s\n", name); */

  ensure_opengl();
  return _realglXGetProcAddressARB(name);
}
#endif

#ifdef CAPSULE_LINUX
void* dlopen (const char * filename, int flag) {
  ensure_real_dlopen();

  if (filename != NULL && strstr(filename, "libGL.so.1")) {
    load_opengl(filename);

    if (!strcmp(filename, "libGL.so.1")) {
      libfake_log("Faking libGL for %s\n", filename);
      return real_dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
    } else {
      libfake_log("Looks like a real libGL? %s\n", filename);
      return real_dlopen(filename, flag);
    }
  } else {
    pid_t pid = getpid();
    libfake_log("pid %d, dlopen(%s, %d)\n", pid, filename, flag);
    void *res = real_dlopen(filename, flag);
    libfake_log("pid %d, dlopen(%s, %d): %p\n", pid, filename, flag, res);
    return res;
  }
}
#endif

#define GL_RGB 6407
#define GL_UNSIGNED_BYTE 5121
#define GL_VIEWPORT 2978

typedef void (*glReadPixelsType)(int, int, int, int, int, int, void*);
glReadPixelsType _realglReadPixels;

typedef void* (*glGetIntegervType)(int, int*);
glGetIntegervType _realglGetIntegerv;

FILE *outFile;

void libfake_captureFrame () {
  int width = FRAME_WIDTH;
  int height = FRAME_HEIGHT;
  int components = 3;
  int format = GL_RGB;

  if (!_realglGetIntegerv) {
    ensure_opengl();
    _realglGetIntegerv = (glGetIntegervType) dlsym(gl_handle, "glGetIntegerv");
    assert("Got glGetIntegerv address", !!_realglGetIntegerv);
  }

  int viewport[4];
  _realglGetIntegerv(GL_VIEWPORT, viewport);

  if (viewport[2] > 0 && viewport[3] > 0) {
    width = viewport[2];
    height = viewport[3];
  }

  if (frameNumber % 60 == 0) {
    libfake_log("Saved %d frames. Current resolution = %dx%d\n", frameNumber, width, height);
  }

  size_t frameDataSize = width * height * components;
  if (!frameData) {
    frameData = (char*) malloc(frameDataSize);
  }

  if (!_realglReadPixels) {
    ensure_opengl();
    _realglReadPixels = (glReadPixelsType) dlsym(gl_handle, "glReadPixels");
    assert("Got glReadPixels address", !!_realglReadPixels);
  }
  _realglReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, frameData);

  if (!outFile) {
    outFile = fopen("capsule.rawvideo", "wb");
    assert("Opened output file", !!outFile);
  }

  fwrite(frameData, 1, frameDataSize, outFile);
  fflush(outFile);

  frameNumber++;
}

#ifdef CAPSULE_LINUX
void glXSwapBuffers (void *a, void *b) {
  libfake_log("About to capture frame..\n");
  libfake_captureFrame();
  libfake_log("About to call real swap buffers..\n");
  return _realglXSwapBuffers(a, b);
}

int glXQueryExtension (void *a, void *b, void *c) {
  return _realglXQueryExtension(a, b, c);
}
#endif

void __attribute__((constructor)) libfake_load() {
  pid_t pid = getpid();
  libfake_log("Initializing (pid %d)...\n", pid);

#ifdef CAPSULE_LINUX
  libfake_log("LD_LIBRARY_PATH: %s\n", getenv("LD_LIBRARY_PATH"));
#endif
}

void __attribute__((destructor)) libfake_unload() {
  libfake_log("Winding down...\n");
}

