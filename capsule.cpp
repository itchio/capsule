
#include "capsule.h"

#ifdef _WIN32
#include <NktHookLib.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if defined(CAPSULE_WINDOWS)
#define dlopen(a, b) LoadLibrary((a))
#define dlsym GetProcAddress
#define RTLD_NOW 0
#define RTLD_LOCAL 0
#define LIBHANDLE HMODULE
#else

#include <dlfcn.h>
#define LIBHANDLE void*
#endif

FILE *logfile;

static void CAPSULE_STDCALL assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  capsule_log("Assertion failed: %s\n", msg);
  /* const char *err = SDL_GetError(); */
  /* capsule_log("[main] Last SDL GetError: %s\n", err); */
  exit(1);
}

LIBHANDLE gl_handle;

typedef void(CAPSULE_STDCALL *glReadPixelsType)(int, int, int, int, int, int, void*);
glReadPixelsType _realglReadPixels;

typedef void* (CAPSULE_STDCALL *glGetIntegervType)(int, int*);
glGetIntegervType _realglGetIntegerv;


void CAPSULE_STDCALL load_opengl (const char *);

void CAPSULE_STDCALL ensure_opengl() {
	if (!gl_handle) {
		load_opengl(DEFAULT_OPENGL);
	}
}

void CAPSULE_STDCALL ensure_own_opengl() {
	if (!_realglGetIntegerv) {
		ensure_opengl();
		_realglGetIntegerv = (glGetIntegervType)dlsym(gl_handle, "glGetIntegerv");
		assert("Got glGetIntegerv address", !!_realglGetIntegerv);
	}

	if (!_realglReadPixels) {
		ensure_opengl();
		_realglReadPixels = (glReadPixelsType)dlsym(gl_handle, "glReadPixels");
		assert("Got glReadPixels address", !!_realglReadPixels);
	}
}

#ifdef CAPSULE_WINDOWS
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>

typedef void (CAPSULE_STDCALL *glSwapBuffersType)(void*);
glSwapBuffersType _glSwapBuffers;

glSwapBuffersType fnSwapBuffers;

void CAPSULE_STDCALL _fakeSwapBuffers (void *hdc) {
  capsule_log("In fakeSwapBuffers\n");
  capsule_captureFrame();
  fnSwapBuffers(hdc);
}

CNktHookLib cHookMgr;

CAPSULE_DLL void capsule_hello () {
  capsule_log("Hello from capsule!\n");
  HMODULE mh = GetModuleHandle("opengl32.dll");
  capsule_log("OpenGL handle: %p\n", mh);
  if (mh) {
	ensure_own_opengl();

    _glSwapBuffers = (glSwapBuffersType) GetProcAddress(mh, "wglSwapBuffers");
    capsule_log("SwapBuffers handle: %p\n", _glSwapBuffers);

    if (_glSwapBuffers) {
      capsule_log("Attempting to install glSwapBuffers hook\n");
	  
	  cHookMgr.SetEnableDebugOutput(TRUE);

	  HINSTANCE hOpengl32Dll = NktHookLibHelpers::GetModuleBaseAddress(L"opengl32.dll");
	  if (hOpengl32Dll == NULL) {
		  ::MessageBoxW(0, L"Error: Cannot get handle of opengl.dll", L"HookTest", MB_OK | MB_ICONERROR);
		  exit(1);
	  }
	  LPVOID fnOrigSwapBuffers = NktHookLibHelpers::GetProcedureAddress(hOpengl32Dll, "wglSwapBuffers");
	  if (fnOrigSwapBuffers == NULL) {
		  ::MessageBoxW(0, L"Error: Cannot get address of wglSwapBuffers", L"HookTest", MB_OK | MB_ICONERROR);
		  exit(1);
	  }
	  capsule_log("Ours = %p, theirs = %p\n", _glSwapBuffers, fnOrigSwapBuffers);

	  SIZE_T hookId;
	  int dwOsErr = cHookMgr.Hook(&hookId, (PVOID*) &fnSwapBuffers, fnOrigSwapBuffers, _fakeSwapBuffers, 0);

	  capsule_log("Well I think we're doing fine! err = %d\n", dwOsErr);

      // assert("Installed hook", Mhook_SetHook(_glSwapBuffers, _fakeSwapBuffers));
      // try {
      //   detour_glSwapBuffers = new MologieDetours::Detour<glSwapBuffersType>(_glSwapBuffers, &_fakeSwapBuffers);
      // } catch (MologieDetours::DetourException &e) {
      //   capsule_log("While installing detour: %s\n", e.what());
      // }
    }
  }

  HMODULE m8 = GetModuleHandle("d3d8.dll");
  capsule_log("Direct3D8 handle: %p\n", m8);

  if (m8) {
    capsule_dx8_sniff();
  }

  HMODULE m9 = GetModuleHandle("d3d9.dll");
  capsule_log("Direct3D9 handle: %p\n", m9);

  HMODULE m10 = GetModuleHandle("d3d10.dll");
  capsule_log("Direct3D10 handle: %p\n", m10);

  HMODULE m11 = GetModuleHandle("d3d11.dll");
  capsule_log("Direct3D11 handle: %p\n", m11);
}

BOOL CAPSULE_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  capsule_log("DllMain called! reason = %d\n", reason);
  if (reason == 1) {
	  capsule_hello();
  }
  return TRUE;
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

void CAPSULE_STDCALL ensure_real_dlopen() {
#ifdef CAPSULE_LINUX
  if (!real_dlopen) {
    capsule_log("Getting real dlopen\n");
    real_dlopen = (dlopen_type)dlsym(RTLD_NEXT, "dlopen");
  }
#endif
}

void CAPSULE_STDCALL load_opengl (const char *openglPath) {
  capsule_log("Loading real opengl from %s\n", openglPath);
#ifdef CAPSULE_LINUX
  ensure_real_dlopen();
  gl_handle = real_dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#else
  gl_handle = dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#endif
  assert("Loaded real OpenGL lib", !!gl_handle);
  capsule_log("Loaded opengl!\n");

#ifdef CAPSULE_LINUX
  capsule_log("Getting glXQueryExtension adress\n");
  _realglXQueryExtension = (glXQueryExtensionType)dlsym(gl_handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);
  capsule_log("Got glXQueryExtension adress: %p\n", _realglXQueryExtension);

  capsule_log("Getting glXSwapBuffers adress\n");
  _realglXSwapBuffers = (glXSwapBuffersType)dlsym(gl_handle, "glXSwapBuffers");
  assert("Got glXSwapBuffers", !!_realglXSwapBuffers);
  capsule_log("Got glXSwapBuffers adress: %p\n", _realglXSwapBuffers);

  capsule_log("Getting glXGetProcAddressARB address\n");
  _realglXGetProcAddressARB = (glXGetProcAddressARBType)dlsym(gl_handle, "glXGetProcAddressARB");
  assert("Got glXGetProcAddressARB", !!_realglXGetProcAddressARB);
  capsule_log("Got glXGetProcAddressARB adress: %p\n", _realglXGetProcAddressARB);
#endif
}

#ifdef CAPSULE_LINUX
extern "C" {
void* glXGetProcAddressARB (const char *name) {
  if (strcmp(name, "glXSwapBuffers") == 0) {
    capsule_log("In glXGetProcAddressARB: %s\n", name);
    capsule_log("Returning fake glXSwapBuffers\n");
    return (void*) &glXSwapBuffers;
  }

  capsule_log("In glXGetProcAddressARB: %s\n", name);

  ensure_opengl();
  return _realglXGetProcAddressARB(name);
}
}
#endif

#ifdef CAPSULE_LINUX
void* dlopen (const char * filename, int flag) {
  ensure_real_dlopen();

  if (filename != NULL && strstr(filename, "libGL.so.1")) {
    load_opengl(filename);

    if (!strcmp(filename, "libGL.so.1")) {
      capsule_log("Faking libGL for %s\n", filename);
      return real_dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
    } else {
      capsule_log("Looks like a real libGL? %s\n", filename);
      return real_dlopen(filename, flag);
    }
  } else {
    pid_t pid = getpid();
    capsule_log("pid %d, dlopen(%s, %d)\n", pid, filename, flag);
    void *res = real_dlopen(filename, flag);
    capsule_log("pid %d, dlopen(%s, %d): %p\n", pid, filename, flag, res);
    return res;
  }
}
#endif

#define GL_RGB 6407
#define GL_UNSIGNED_BYTE 5121
#define GL_VIEWPORT 2978

FILE *outFile;

void CAPSULE_STDCALL capsule_captureFrame () {
  frameNumber++;
  if (frameNumber < 120) {
    return;
  }

  int width = FRAME_WIDTH;
  int height = FRAME_HEIGHT;
  int components = 3;
  int format = GL_RGB;

  ensure_own_opengl();

  int viewport[4];
  _realglGetIntegerv(GL_VIEWPORT, viewport);

  if (viewport[2] > 0 && viewport[3] > 0) {
    width = viewport[2];
    height = viewport[3];
  }

  if (frameNumber % 60 == 0) {
    capsule_log("Saved %d frames. Current resolution = %dx%d\n", frameNumber, width, height);
  }

  size_t frameDataSize = width * height * components;
  if (!frameData) {
    frameData = (char*) malloc(frameDataSize);
  }
  _realglReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, frameData);

  if (!outFile) {
    outFile = fopen("capsule.rawvideo", "wb");
    assert("Opened output file", !!outFile);
  }

  fwrite(frameData, 1, frameDataSize, outFile);
  fflush(outFile);
}

#ifdef CAPSULE_LINUX
extern "C" {
void glXSwapBuffers (void *a, void *b) {
  capsule_log("About to capture frame..\n");
  capsule_captureFrame();
  capsule_log("About to call real swap buffers..\n");
  return _realglXSwapBuffers(a, b);
}

int glXQueryExtension (void *a, void *b, void *c) {
  return _realglXQueryExtension(a, b, c);
}
}
#endif

#ifndef CAPSULE_WINDOWS
void __attribute__((constructor)) capsule_load() {
  pid_t pid = getpid();
  capsule_log("Initializing (pid %d)...\n", pid);

#ifdef CAPSULE_LINUX
  capsule_log("LD_LIBRARY_PATH: %s\n", getenv("LD_LIBRARY_PATH"));
#endif
}

void __attribute__((destructor)) capsule_unload() {
  capsule_log("Winding down...\n");
}
#endif

