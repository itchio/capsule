
#include <capsule.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// chrono::steady_clock
#include <chrono>

// this_thread
#include <thread>

#if defined(CAPSULE_WINDOWS)
#include <windows.h>
#define dlopen(a, b) LoadLibrary((a))
#define dlsym GetProcAddress
#define RTLD_NOW 0
#define RTLD_LOCAL 0
#define LIBHANDLE HMODULE
#else

#include <dlfcn.h>
#define LIBHANDLE void*
#endif

using namespace std;

FILE *logfile;

static void CAPSULE_STDCALL assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  capsule_log("Assertion failed: %s", msg);
  exit(1);
}

LIBHANDLE gl_handle;

typedef void(CAPSULE_STDCALL *glReadPixelsType)(int, int, int, int, int, int, void*);
glReadPixelsType _realglReadPixels;

typedef void* (CAPSULE_STDCALL *glGetIntegervType)(int, int*);
glGetIntegervType _realglGetIntegerv;

typedef void* (CAPSULE_STDCALL *glGetRenderbufferParameterivType)(int, int, int*);
glGetRenderbufferParameterivType _realglGetRenderbufferParameteriv;


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

#ifndef CAPSULE_WINDOWS
	if (!_realglGetRenderbufferParameteriv) {
		ensure_opengl();
		_realglGetRenderbufferParameteriv = (glGetRenderbufferParameterivType)dlsym(gl_handle, "glGetRenderbufferParameteriv");
		assert("Got glGetRenderbufferParameteriv address", !!_realglGetRenderbufferParameteriv);
	}
#endif
}

#ifdef CAPSULE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <NktHookLib.h>

typedef void (CAPSULE_STDCALL *glSwapBuffersType)(void*);
glSwapBuffersType _glSwapBuffers;

glSwapBuffersType fnSwapBuffers;

void CAPSULE_STDCALL _fakeSwapBuffers (void *hdc) {
  capsule_log("In fakeSwapBuffers");
  capsule_capture_frame(0, 0);
  fnSwapBuffers(hdc);
}

CNktHookLib cHookMgr;
static int capsule_inited = 0;

CAPSULE_DLL void capsule_install_windows_hooks () {
  if (capsule_inited) return;
  capsule_inited = 1;

  cHookMgr.SetEnableDebugOutput(TRUE);

  capsule_log("Installing capsule windows hooks");

  HINSTANCE mh;

  mh = NktHookLibHelpers::GetModuleBaseAddress(L"opengl32.dll");
  capsule_log("OpenGL handle: %p", mh);
  if (mh) {
    ensure_own_opengl();

    _glSwapBuffers = (glSwapBuffersType) NktHookLibHelpers::GetProcedureAddress(mh, "wglGetProcAddress");
    capsule_log("GetProcAddress handle: %p", _glSwapBuffers);

    _glSwapBuffers = (glSwapBuffersType) NktHookLibHelpers::GetProcedureAddress(mh, "wglSwapBuffers");
    capsule_log("SwapBuffers handle: %p", _glSwapBuffers);
  
    if (_glSwapBuffers) {
      capsule_log("Attempting to install glSwapBuffers hook");

      DWORD err;

      SIZE_T hookId;

      err = cHookMgr.Hook(&hookId, (LPVOID *) &fnSwapBuffers, _glSwapBuffers, _fakeSwapBuffers, 0);
      if (err != ERROR_SUCCESS) {
        capsule_log("Hooking derped with error %d (%x)", err, err);
      } else {
        capsule_log("Well I think we're doing fine!");
      }
    }
  }

  // HMODULE m8 = GetModuleHandle("d3d8.dll");
  // capsule_log("Direct3D8 handle: %p", m8);

  // if (m8) {
  //   capsule_d3d8_sniff();
  // }

  // HMODULE m9 = GetModuleHandle("d3d9.dll");
  // capsule_log("Direct3D9 handle: %p", m9);

  // HMODULE m10 = GetModuleHandle("d3d10.dll");
  // capsule_log("Direct3D10 handle: %p", m10);

  // HMODULE m11 = GetModuleHandle("d3d11.dll");
  // capsule_log("Direct3D11 handle: %p", m11);

  // if (m11) {
	//   capsule_d3d11_sniff();
  // }
}

BOOL CAPSULE_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      capsule_log("DllMain (PROCESS_ATTACH)", reason); break;
    case DLL_PROCESS_DETACH:
      capsule_log("DllMain (PROCESS_DETACH)", reason); break;
    case DLL_THREAD_ATTACH:
      capsule_log("DllMain (THREAD_ATTACH)", reason); break;
    case DLL_THREAD_DETACH:
      capsule_log("DllMain (THREAD_DETACH)", reason); break;
  }

  if (reason == DLL_PROCESS_ATTACH) {
    // we've just been attached to a process
	  capsule_install_windows_hooks();
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

char *frameData;
int frameNumber = 0;
chrono::time_point<chrono::steady_clock> old_ts, ts;

typedef void* (*dlopen_type)(const char*, int);
dlopen_type real_dlopen;

void CAPSULE_STDCALL ensure_real_dlopen() {
#ifdef CAPSULE_LINUX
  if (!real_dlopen) {
    capsule_log("Getting real dlopen");
    real_dlopen = (dlopen_type)dlsym(RTLD_NEXT, "dlopen");
  }
#endif
}

void CAPSULE_STDCALL load_opengl (const char *openglPath) {
  capsule_log("Loading real opengl from %s", openglPath);
#ifdef CAPSULE_LINUX
  ensure_real_dlopen();
  gl_handle = real_dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#else
  gl_handle = dlopen(L"OPENGL32.DLL", (RTLD_NOW|RTLD_LOCAL));
#endif
  assert("Loaded real OpenGL lib", !!gl_handle);
  capsule_log("Loaded opengl!");

#ifdef CAPSULE_LINUX
  capsule_log("Getting glXQueryExtension adress");
  _realglXQueryExtension = (glXQueryExtensionType)dlsym(gl_handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);
  capsule_log("Got glXQueryExtension adress: %p", _realglXQueryExtension);

  capsule_log("Getting glXSwapBuffers adress");
  _realglXSwapBuffers = (glXSwapBuffersType)dlsym(gl_handle, "glXSwapBuffers");
  assert("Got glXSwapBuffers", !!_realglXSwapBuffers);
  capsule_log("Got glXSwapBuffers adress: %p", _realglXSwapBuffers);

  capsule_log("Getting glXGetProcAddressARB address");
  _realglXGetProcAddressARB = (glXGetProcAddressARBType)dlsym(gl_handle, "glXGetProcAddressARB");
  assert("Got glXGetProcAddressARB", !!_realglXGetProcAddressARB);
  capsule_log("Got glXGetProcAddressARB adress: %p", _realglXGetProcAddressARB);
#endif
}

#ifdef CAPSULE_LINUX
extern "C" {
void* glXGetProcAddressARB (const char *name) {
  if (strcmp(name, "glXSwapBuffers") == 0) {
    capsule_log("In glXGetProcAddressARB: %s", name);
    capsule_log("Returning fake glXSwapBuffers");
    return (void*) &glXSwapBuffers;
  }

  capsule_log("In glXGetProcAddressARB: %s", name);

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
      capsule_log("Faking libGL for %s", filename);
      return real_dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
    } else {
      capsule_log("Looks like a real libGL? %s", filename);
      return real_dlopen(filename, flag);
    }
  } else {
    pid_t pid = getpid();
    capsule_log("pid %d, dlopen(%s, %d)", pid, filename, flag);
    void *res = real_dlopen(filename, flag);
    capsule_log("pid %d, dlopen(%s, %d): %p", pid, filename, flag, res);
    return res;
  }
}
#endif

#define GL_RGB 6407
#define GL_UNSIGNED_BYTE 5121
#define GL_VIEWPORT 2978
#define GL_RENDERBUFFER 36161
#define GL_RENDERBUFFER_WIDTH 36162
#define GL_RENDERBUFFER_HEIGHT 36163

FILE *outFile;

FILE *capsule_open_log () {
#ifdef CAPSULE_WINDOWS
  return _wfopen(capsule_log_path(), L"w");
#else // CAPSULE_WINDOWS
  return fopen(capsule_log_path(), "w");
#endif // CAPSULE_WINDOWS
}

#ifdef CAPSULE_WINDOWS

#define CAPSULE_LOG_PATH_SIZE 32*1024
wchar_t *_capsule_log_path;

wchar_t *capsule_log_path () {
  if (!_capsule_log_path) {
    _capsule_log_path = (wchar_t*) malloc(sizeof(wchar_t) * CAPSULE_LOG_PATH_SIZE);
    ExpandEnvironmentStrings(L"%APPDATA%\\capsule.log.txt", _capsule_log_path, CAPSULE_LOG_PATH_SIZE);
  }
  return _capsule_log_path;
}

#else // defined CAPSULE_WINDOWS

char *capsule_log_path () {
  return "/tmp/capsule.log.txt";
}

#endif


void CAPSULE_STDCALL capsule_write_frame (char *frameData, size_t frameDataSize, int width, int height) {
  if (!outFile) {
    outFile = fopen("capsule.rawvideo", "wb");
    assert("Opened output file", !!outFile);

    int64_t num = (int64_t) width;
    fwrite(&num, sizeof(int64_t), 1, outFile);

    num = (int64_t) height;
    fwrite(&num, sizeof(int64_t), 1, outFile);
  }

  fwrite(frameData, 1, frameDataSize, outFile);
  fflush(outFile);
}

void CAPSULE_STDCALL capsule_capture_frame (int width, int height) {
  frameNumber++;
  if (frameNumber < 120) {
    return;
  }

  int components = 3;
  int format = GL_RGB;

  ensure_own_opengl();

  if (width == 0 || height == 0) {
    int viewport[4];
    _realglGetIntegerv(GL_VIEWPORT, viewport);

    // capsule_log("Viewport values: %d, %d, %d, %d", viewport[0], viewport[1], viewport[2], viewport[3]);
    if (viewport[2] > 0 && viewport[3] > 0) {
      // capsule_log("Setting size from viewport");
      width = viewport[2];
      height = viewport[3];
    } else {
      capsule_log("Missing viewport values :(");
    }
  }

  // make sure width and height are multiples of 2
  if (height % 2 > 0) {
    height++;
    width += 2;
  }

  if (width % 2 > 0) {
    width++;
    height += 2;
  }

  if (frameNumber % 30 == 0) {
    capsule_log("Saved %d frames. Current resolution = %dx%d", frameNumber, width, height);
  }

  size_t frameDataSize = width * height * components;
  if (!frameData) {
    frameData = (char*) malloc(frameDataSize);
  }
  _realglReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, frameData);

  capsule_write_frame(frameData, frameDataSize, width, height);

  ts = chrono::steady_clock::now();
  auto delta = old_ts - ts;
  auto wanted_delta = chrono::microseconds(1000000 / 30);
  auto sleep_duration = wanted_delta - delta;

  // if (sleep_duration > chrono::seconds(0)) {
  //   this_thread::sleep_for(wanted_delta - delta);
  // }
  old_ts = chrono::steady_clock::now();
}

#ifdef CAPSULE_LINUX
extern "C" {
  void glXSwapBuffers (void *a, void *b) {
    // capsule_log("About to capture frame..");
    capsule_capture_frame(0, 0);
    // capsule_log("About to call real swap buffers..");
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
  capsule_log("Initializing (pid %d)...", pid);

#ifdef CAPSULE_LINUX
  capsule_log("LD_LIBRARY_PATH: %s", getenv("LD_LIBRARY_PATH"));
  capsule_log("LD_PRELOAD: %s", getenv("LD_PRELOAD"));
#endif
}

void __attribute__((destructor)) capsule_unload() {
  capsule_log("Winding down...");
}
#endif
