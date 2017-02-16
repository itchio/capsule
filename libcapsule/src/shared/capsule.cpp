
#include <capsule.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
  capsule_capture_frame(0, 0);
  fnSwapBuffers(hdc);
}

typedef HRESULT (CAPSULE_STDCALL *createDXGIFactoryType)(REFIID, void**);
createDXGIFactoryType _createDXGIFactory;
createDXGIFactoryType fnCreateDXGIFactory;

HRESULT CAPSULE_STDCALL _fakeCreateDXGIFactory (REFIID riid, void** ppFactory) {
  capsule_log("Intercepted CreateDXGIFactory!");
  return fnCreateDXGIFactory(riid, ppFactory);
}


#endif // CAPSULE_WINDOWS

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
#if defined(CAPSULE_WINDOWS)
  gl_handle = dlopen(L"OPENGL32.DLL", (RTLD_NOW|RTLD_LOCAL));
#elif defined(CAPSULE_LINUX)
  ensure_real_dlopen();
  gl_handle = real_dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#else
  gl_handle = dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
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
      real_dlopen(filename, flag);
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
#define GL_BGRA 32993
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
  return (char*) "/tmp/capsule.log.txt";
}

#endif

int ensure_outfile() {
  if (!outFile) {
#ifdef CAPSULE_WINDOWS
    const int pipe_path_len = CAPSULE_LOG_PATH_SIZE;
    wchar_t *pipe_path = (wchar_t*) malloc(sizeof(wchar_t) * pipe_path_len);
    pipe_path[0] = '\0';
    GetEnvironmentVariable(L"CAPSULE_PIPE_PATH", pipe_path, pipe_path_len);
    assert("Got pipe path", wcslen(pipe_path) > 0);

    capsule_log("Pipe path: %S", pipe_path);

    outFile = _wfopen(pipe_path, L"wb");
    free(pipe_path);
#else
    char *pipe_path = getenv("CAPSULE_PIPE_PATH");
    capsule_log("Pipe path: %s", pipe_path);
    outFile = fopen(pipe_path, "wb");
#endif
    assert("Opened output file", !!outFile);
    return 1;
  }

  return 0;
}

void CAPSULE_STDCALL capsule_write_video_format (int width, int height, int format, int vflip) {
  ensure_outfile();
  int64_t num = (int64_t) width;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) height;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) format;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) vflip;
  fwrite(&num, sizeof(int64_t), 1, outFile);
}

void CAPSULE_STDCALL capsule_write_video_frame (int64_t timestamp, char *frameData, size_t frameDataSize) {
  ensure_outfile();
  fwrite(&timestamp, 1, sizeof(int64_t), outFile);
  fwrite(frameData, 1, frameDataSize, outFile);
}

void CAPSULE_STDCALL capsule_capture_frame (int width, int height) {
  static bool first_frame = true;

  if (!capsule_capture_ready()) {
    return;
  }

  auto timestamp = capsule_frame_timestamp();

  int components = 4;
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

  size_t frameDataSize = width * height * components;
  if (!frameData) {
    frameData = (char*) malloc(frameDataSize);
  }
  _realglReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, frameData);

  if (first_frame) {
    capsule_write_video_format(width, height, CAPSULE_PIX_FMT_BGRA, 1 /* vflip */);
    first_frame = false;
  }
  capsule_write_video_frame(timestamp, frameData, frameDataSize);
}

#ifdef CAPSULE_LINUX
extern "C" {
  void glXSwapBuffers (void *a, void *b) {
    if (capsule_x11_should_capture()) {
      // capsule_log("About to capture frame..");
      capsule_capture_frame(0, 0);
    }
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
  capsule_log("CAPSULE_PIPE_PATH: %s", getenv("CAPSULE_PIPE_PATH"));
  capsule_x11_init();
#endif
}

void __attribute__((destructor)) capsule_unload() {
  capsule_log("Winding down...");
}
#endif
