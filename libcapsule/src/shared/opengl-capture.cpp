
#include <capsule.h>

// strstr, strcmp
#include <string.h>

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
		capsule_assert("Got glGetIntegerv address", !!_realglGetIntegerv);
	}

	if (!_realglReadPixels) {
		ensure_opengl();
		_realglReadPixels = (glReadPixelsType)dlsym(gl_handle, "glReadPixels");
		capsule_assert("Got glReadPixels address", !!_realglReadPixels);
	}
}

#ifdef CAPSULE_LINUX
void glXSwapBuffers (void *a, void *b);

typedef int (*glXQueryExtensionType)(void*, void*, void*);
glXQueryExtensionType _realglXQueryExtension;

typedef void (*glXSwapBuffersType)(void*, void*);
glXSwapBuffersType _realglXSwapBuffers;

typedef void* (*glXGetProcAddressARBType)(const char*);
glXGetProcAddressARBType _realglXGetProcAddressARB;
#endif

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
  capsule_assert("Loaded real OpenGL lib", !!gl_handle);
  capsule_log("Loaded opengl!");

#ifdef CAPSULE_LINUX
  capsule_log("Getting glXQueryExtension adress");
  _realglXQueryExtension = (glXQueryExtensionType)dlsym(gl_handle, "glXQueryExtension");
  capsule_assert("Got glXQueryExtension", !!_realglXQueryExtension);
  capsule_log("Got glXQueryExtension adress: %p", _realglXQueryExtension);

  capsule_log("Getting glXSwapBuffers adress");
  _realglXSwapBuffers = (glXSwapBuffersType)dlsym(gl_handle, "glXSwapBuffers");
  capsule_assert("Got glXSwapBuffers", !!_realglXSwapBuffers);
  capsule_log("Got glXSwapBuffers adress: %p", _realglXSwapBuffers);

  capsule_log("Getting glXGetProcAddressARB address");
  _realglXGetProcAddressARB = (glXGetProcAddressARBType)dlsym(gl_handle, "glXGetProcAddressARB");
  capsule_assert("Got glXGetProcAddressARB", !!_realglXGetProcAddressARB);
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

#ifdef CAPSULE_LINUX
extern "C" {
  void glXSwapBuffers (void *a, void *b) {
    capdata.saw_opengl = true;
    opengl_capture(0, 0);
    return _realglXSwapBuffers(a, b);
  }

  int glXQueryExtension (void *a, void *b, void *c) {
    return _realglXQueryExtension(a, b, c);
  }
}
#endif

/**
 * Capture one OpenGL frame
 */
void CAPSULE_STDCALL opengl_capture (int width, int height) {
  static char *frame_data = nullptr;
  static bool first_frame = true;

  if (!capsule_capture_ready()) {
    if (!capsule_capture_active()) {
      first_frame = true;
      free(frame_data);
      frame_data = nullptr;
    }

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

  size_t pitch = width * components;
  size_t frame_data_size = pitch * height;
  if (!frame_data) {
    frame_data = (char*) malloc(frame_data_size);
  }
  _realglReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, frame_data);

  if (first_frame) {
    capsule_write_video_format(width, height, CAPSULE_PIX_FMT_BGRA, 1 /* vflip */, pitch);
    first_frame = false;
  }
  capsule_write_video_frame(timestamp, frame_data, frame_data_size);
}