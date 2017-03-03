
#include <capsule.h>

// strstr, strcmp
#include <string.h>

#include "gl-decs.h"

// inspired by libobs
struct gl_data {
  uint32_t                cx;
  uint32_t                cy;
  GLuint                  fbo;

  int                     cur_tex;
  int                     copy_wait;
  GLuint                  pbos[NUM_BUFFERS];
  GLuint                  textures[NUM_BUFFERS];
  bool                    texture_ready[NUM_BUFFERS];
  bool                    texture_mapped[NUM_BUFFERS];

  // old (pre-async GPU download)
  char                    *frame_data;
};

static struct gl_data data = {};

LIBHANDLE gl_handle;

static inline bool gl_error(const char *func, const char *str) {
	GLenum error = _glGetError();
	if (error != 0) {
		capsule_log("%s: %s: %lu", func, str, error);
		return true;
	}

	return false;
}

void CAPSULE_STDCALL load_opengl (const char *);

void CAPSULE_STDCALL ensure_opengl() {
	if (!gl_handle) {
		load_opengl(DEFAULT_OPENGL);
	}
}

#define GLSYM(sym) { \
  _ ## sym = (sym ## _t) dlsym(gl_handle, #sym); \
  capsule_assert("Got #sym address", !!_ ## sym); \
}

void CAPSULE_STDCALL ensure_own_opengl() {
  ensure_opengl();
  GLSYM(glGetError)
  GLSYM(glGetIntegerv)
  GLSYM(glReadPixels)
}

#ifdef CAPSULE_LINUX
void glXSwapBuffers (void *a, void *b);

typedef int (*glXQueryExtension_t)(void*, void*, void*);
glXQueryExtension_t _glXQueryExtension;

typedef void (*glXSwapBuffers_t)(void*, void*);
glXSwapBuffers_t _glXSwapBuffers;

typedef void* (*glXGetProcAddressARB_t)(const char*);
glXGetProcAddressARB_t _glXGetProcAddressARB;
#endif

typedef void* (*dlopen_type)(const char*, int);
dlopen_type _dlopen;

void CAPSULE_STDCALL ensure_real_dlopen() {
#ifdef CAPSULE_LINUX
  if (!_dlopen) {
    capsule_log("Getting real dlopen");
    _dlopen = (dlopen_type) dlsym(RTLD_NEXT, "dlopen");
  }
#endif
}

void CAPSULE_STDCALL load_opengl (const char *openglPath) {
  capsule_log("Loading real opengl from %s", openglPath);
#if defined(CAPSULE_WINDOWS)
  gl_handle = dlopen(L"OPENGL32.DLL", (RTLD_NOW|RTLD_LOCAL));
#elif defined(CAPSULE_LINUX)
  ensure_real_dlopen();
  gl_handle = _dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#else
  gl_handle = dlopen(openglPath, (RTLD_NOW|RTLD_LOCAL));
#endif
  capsule_assert("Loaded real OpenGL lib", !!gl_handle);
  capsule_log("Loaded opengl!");

#ifdef CAPSULE_LINUX
  capsule_log("Getting glXQueryExtension adress");
  _glXQueryExtension = (glXQueryExtension_t) dlsym(gl_handle, "glXQueryExtension");
  capsule_assert("Got glXQueryExtension", !!_glXQueryExtension);
  capsule_log("Got glXQueryExtension adress: %p", _glXQueryExtension);

  capsule_log("Getting glXSwapBuffers adress");
  _glXSwapBuffers = (glXSwapBuffers_t) dlsym(gl_handle, "glXSwapBuffers");
  capsule_assert("Got glXSwapBuffers", !!_glXSwapBuffers);
  capsule_log("Got glXSwapBuffers adress: %p", _glXSwapBuffers);

  capsule_log("Getting glXGetProcAddressARB address");
  _glXGetProcAddressARB = (glXGetProcAddressARB_t) dlsym(gl_handle, "glXGetProcAddressARB");
  capsule_assert("Got glXGetProcAddressARB", !!_glXGetProcAddressARB);
  capsule_log("Got glXGetProcAddressARB adress: %p", _glXGetProcAddressARB);
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

  // capsule_log("In glXGetProcAddressARB: %s", name);

  ensure_opengl();
  return _glXGetProcAddressARB(name);
}
}
#endif

#ifdef CAPSULE_LINUX
void* dlopen (const char * filename, int flag) {
  ensure_real_dlopen();

  if (filename != NULL && strstr(filename, "libGL.so.1")) {
    load_opengl(filename);

    if (!strcmp(filename, "libGL.so.1")) {
      _dlopen(filename, flag);
      capsule_log("Faking libGL for %s", filename);
      return _dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
    } else {
      capsule_log("Looks like a real libGL? %s", filename);
      return _dlopen(filename, flag);
    }
  } else {
    pid_t pid = getpid();
    capsule_log("pid %d, dlopen(%s, %d)", pid, filename, flag);
    void *res = _dlopen(filename, flag);
    capsule_log("pid %d, dlopen(%s, %d): %p", pid, filename, flag, res);
    return res;
  }
}
#endif

#ifdef CAPSULE_LINUX
extern "C" {
  void glXSwapBuffers (void *a, void *b) {
    capdata.saw_opengl = true;
    gl_capture(0, 0);
    return _glXSwapBuffers(a, b);
  }

  int glXQueryExtension (void *a, void *b, void *c) {
    return _glXQueryExtension(a, b, c);
  }
}
#endif

static inline bool gl_init_fbo(void) {
	_glGenFramebuffers(1, &data.fbo);
	return !gl_error("gl_init_fbo", "failed to initialize FBO");
}

static inline bool gl_shmem_init_data(size_t idx, size_t size) {
	_glBindBuffer(GL_PIXEL_PACK_BUFFER, data.pbos[idx]);
	if (gl_error("gl_shmem_init_data", "failed to bind pbo")) {
		return false;
	}

	_glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);
	if (gl_error("gl_shmem_init_data", "failed to set pbo data")) {
		return false;
	}

	_glBindTexture(GL_TEXTURE_2D, data.textures[idx]);
	if (gl_error("gl_shmem_init_data", "failed to set bind texture")) {
		return false;
	}

	_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.cx, data.cy,
			0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	if (gl_error("gl_shmem_init_data", "failed to set texture data")) {
		return false;
	}

	return true;
}

static inline bool gl_shmem_init_buffers(void) {
	uint32_t size = data.cx * data.cy * 4;
	int last_pbo; // FIXME: GLint
	int last_tex; // FIXME: GLint

	_glGenBuffers(NUM_BUFFERS, data.pbos);
	if (gl_error("gl_shmem_init_buffers", "failed to generate buffers")) {
		return false;
	}

	_glGenTextures(NUM_BUFFERS, data.textures);
	if (gl_error("gl_shmem_init_buffers", "failed to generate textures")) {
		return false;
	}

	_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &last_pbo);
	if (gl_error("gl_shmem_init_buffers",
				"failed to save pixel pack buffer")) {
		return false;
	}

	_glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
	if (gl_error("gl_shmem_init_buffers", "failed to save texture")) {
		return false;
	}

	for (size_t i = 0; i < NUM_BUFFERS; i++) {
		if (!gl_shmem_init_data(i, size)) {
			return false;
		}
	}

	_glBindBuffer(GL_PIXEL_PACK_BUFFER, last_pbo);
	_glBindTexture(GL_TEXTURE_2D, last_tex);
	return true;
}

static bool gl_shmem_init() {
	if (!gl_shmem_init_buffers()) {
		return false;
	}
	if (!gl_init_fbo()) {
		return false;
	}

	capsule_log("gl memory capture successful");
	return true;
}

static void gl_init (int width, int height) {
  ensure_own_opengl();

  if (width == 0 || height == 0) {
    int viewport[4];
    _glGetIntegerv(GL_VIEWPORT, viewport);

    if (viewport[2] > 0 && viewport[3] > 0) {
      width = viewport[2];
      height = viewport[3];
    } else {
      capsule_log("gl_init: 0x0 viewport");
      // TODO: how do we handle that error?
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

  int components = 4; // BGRA
  size_t pitch = width * components;
  size_t frame_data_size = pitch * height;
  data.frame_data = (char*) malloc(frame_data_size);

  data.cx = width;
  data.cy = height;
}

static void gl_free() {
  if (data.frame_data) {
    free(data.frame_data);
  }

  memset(&data, 0, sizeof(data));
}

/**
 * Capture one OpenGL frame
 */
void CAPSULE_STDCALL gl_capture (int width, int height) {
  static bool first_frame = true;

  if (!capsule_capture_ready()) {
    if (!capsule_capture_active()) {
      first_frame = true;
      gl_free();
    }

    return;
  }

  auto timestamp = capsule_frame_timestamp();

  if (first_frame) {
    gl_init(width, height);
  }

  /* reset error flag */
	_glGetError();

  _glReadPixels(0, 0, data.cx, data.cy, GL_BGRA, GL_UNSIGNED_BYTE, data.frame_data);
	if (gl_error("gl_capture", "failed to read pixels")) {
		return;
	}

  const size_t components = 4;  
  const size_t pitch = data.cx * components;
  if (first_frame) {
    capsule_write_video_format(data.cx, data.cy, CAPSULE_PIX_FMT_BGRA, 1 /* vflip */, pitch);
    first_frame = false;
  }
  capsule_write_video_frame(timestamp, data.frame_data, data.cy * pitch);
}
