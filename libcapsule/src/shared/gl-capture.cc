
#include <capsule.h>

// strstr, strcmp
#include <string.h>

#include "gl-decs.h"

// inspired by libobs
struct gl_data {
  int                     cx;
  int                     cy;
  int64_t                 pitch;
  GLuint                  fbo;

  int                     cur_tex;
  int                     copy_wait;
  GLuint                  pbos[NUM_BUFFERS];
  GLuint                  textures[NUM_BUFFERS];
  bool                    texture_ready[NUM_BUFFERS];
  bool                    texture_mapped[NUM_BUFFERS];
  int64_t                 timestamps[NUM_BUFFERS];
};

static struct gl_data data = {};

LIBHANDLE gl_handle;

static inline bool gl_error(const char *func, const char *str) {
	GLenum error = _glGetError();
	if (error != 0) {
		capsule_log("%s: %s: %lu", func, str, (unsigned long) error);
		return true;
	}

	return false;
}

bool CAPSULE_STDCALL load_opengl (const char *opengl_path);

void CAPSULE_STDCALL ensure_opengl() {
	if (!gl_handle) {
		load_opengl(DEFAULT_OPENGL);
	}
}

#define GLSYM(sym) { \
  if (! _ ## sym && terryglGetProcAddress) { \
    _ ## sym = (sym ## _t) terryglGetProcAddress(#sym);\
  } \
  if (! _ ## sym) { \
    _ ## sym = (sym ## _t) dlsym(gl_handle, #sym); \
  } \
  if (! _ ## sym) { \
    capsule_log("failed to glsym %s", #sym); \
    return false; \
  } \
}

bool CAPSULE_STDCALL init_gl_functions() {
  ensure_opengl();
  GLSYM(glGetError)
  GLSYM(glGetIntegerv)

  GLSYM(glGenTextures)
  GLSYM(glBindTexture)
  GLSYM(glTexImage2D)
  GLSYM(glGetTexImage)
  GLSYM(glDeleteTextures)

  GLSYM(glGenBuffers)
  GLSYM(glBindBuffer)
  GLSYM(glReadBuffer)
  GLSYM(glDrawBuffer)
  GLSYM(glBufferData)
  GLSYM(glMapBuffer)
  GLSYM(glUnmapBuffer)
  GLSYM(glDeleteBuffers)

  GLSYM(glGenFramebuffers)
  GLSYM(glBindFramebuffer)
  GLSYM(glBlitFramebuffer)
  GLSYM(glFramebufferTexture2D)
  GLSYM(glDeleteFramebuffers)

  return true;
}

static void gl_free() {
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    if (data.pbos[i]) {
      if (data.texture_mapped[i]) {
        _glBindBuffer(GL_PIXEL_PACK_BUFFER, data.pbos[i]);
        _glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        _glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      }

      _glDeleteBuffers(1, &data.pbos[i]);
    }

    if (data.textures[i])
      _glDeleteTextures(1, &data.textures[i]);
  }

  if (data.fbo) {
		_glDeleteFramebuffers(1, &data.fbo);
  }

	gl_error("gl_free", "GL error occurred on free");

  memset(&data, 0, sizeof(data));

  capsule_log("----------------------- gl capture freed ----------------------");
}

typedef void* (*dlopen_type)(const char*, int);
dlopen_type _dlopen;

void CAPSULE_STDCALL ensure_real_dlopen() {
#ifdef CAPSULE_LINUX
  if (!_dlopen) {
    // on linux, since we intercept dlopen, we need to
    // get back the actual dlopen, so that we can, y'know,
    // open libraries.
    capsule_log("Getting real dlopen");
    _dlopen = (dlopen_type) dlsym(RTLD_NEXT, "dlopen");
  }
#endif
}

bool CAPSULE_STDCALL load_opengl (const char *opengl_path) {
#if defined(CAPSULE_LINUX)
  ensure_real_dlopen();
  gl_handle = _dlopen(opengl_path, (RTLD_NOW|RTLD_LOCAL));
#else
  gl_handle = dlopen(opengl_path, (RTLD_NOW|RTLD_LOCAL));
#endif

  if (!gl_handle) {
    capsule_log("could not load real opengl library from %s", opengl_path);
    return false;
  }

#if defined(CAPSULE_LINUX)
  GLSYM(glXQueryExtension)
  GLSYM(glXSwapBuffers)
  GLSYM(glXGetProcAddressARB)
#elif defined(CAPSULE_WINDOWS)
  GLSYM(wglGetProcAddress)
#elif defined(CAPSULE_MACOS)
  GLSYM(glGetProcAddress)
#endif

  return true;
}

#ifdef CAPSULE_LINUX
extern "C" {

// this intercepts glXGetProcAddressARB for linux games
// that link statically against libGL
void* glXGetProcAddressARB (const char *name) {
  if (strcmp(name, "glXSwapBuffers") == 0) {
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
	size_t size = data.cx * data.cy * 4;
	GLint last_pbo;
	GLint last_tex;

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

static bool gl_init (int width, int height) {
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

  const int components = 4; // BGRA
  const size_t pitch = width * components;

  data.cx = width;
  data.cy = height;
  data.pitch = pitch;

  bool success = gl_shmem_init();

  if (!success) {
    gl_free();
    return false;
  }

  return true;
}

static void gl_copy_backbuffer(GLuint dst) {
	_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, data.fbo);
	if (gl_error("gl_copy_backbuffer", "failed to bind FBO")) {
		return;
	}

	_glBindTexture(GL_TEXTURE_2D, dst);
	if (gl_error("gl_copy_backbuffer", "failed to bind texture")) {
		return;
	}

	_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, dst, 0);
	if (gl_error("gl_copy_backbuffer", "failed to set frame buffer")) {
		return;
	}

	_glReadBuffer(GL_BACK);

	_glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (gl_error("gl_copy_backbuffer", "failed to set draw buffer")) {
		return;
	}

	_glBlitFramebuffer(0, 0, data.cx, data.cy,
			0, 0, data.cx, data.cy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	gl_error("gl_copy_backbuffer", "failed to blit");
}

static inline void gl_shmem_capture_queue_copy(void) {
	for (int i = 0; i < NUM_BUFFERS; i++) {
		if (data.texture_ready[i]) {
			GLvoid *buffer;
      auto timestamp = data.timestamps[i];

			data.texture_ready[i] = false;

			_glBindBuffer(GL_PIXEL_PACK_BUFFER, data.pbos[i]);
			if (gl_error("gl_shmem_capture_queue_copy", "failed to bind pbo")) {
				return;
			}

			buffer = _glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (buffer) {
				data.texture_mapped[i] = true;
        capsule_write_video_frame(timestamp, (char*) buffer, data.cy * data.pitch);
			}
			break;
		}
	}
}

static inline void gl_shmem_capture_stage(GLuint dst_pbo, GLuint src_tex) {
	_glBindTexture(GL_TEXTURE_2D, src_tex);
	if (gl_error("gl_shmem_capture_stage", "failed to bind src_tex")) {
		return;
	}

	_glBindBuffer(GL_PIXEL_PACK_BUFFER, dst_pbo);
	if (gl_error("gl_shmem_capture_stage", "failed to bind dst_pbo")) {
		return;
	}

	_glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	if (gl_error("gl_shmem_capture_stage", "failed to read src_tex")) {
		return;
	}
}

void gl_shmem_capture () {
  int next_tex;
  GLint last_fbo;
  GLint last_tex;

  auto timestamp = capsule_frame_timestamp();

  // save last fbo & texture to restore them after capture
  {
    _glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fbo);
    if (gl_error("gl_shmem_capture", "failed to get last fbo")) {
      return;
    }

    _glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
    if (gl_error("gl_shmem_capture", "failed to get last texture")) {
      return;
    }
  }

  // try to map & send all the textures that are ready
  gl_shmem_capture_queue_copy();

  next_tex = (data.cur_tex + 1) % NUM_BUFFERS;

  data.timestamps[next_tex] = timestamp;
  gl_copy_backbuffer(data.textures[next_tex]);

  if (data.copy_wait < NUM_BUFFERS - 1) {
    data.copy_wait++;
  } else {
    GLuint src = data.textures[next_tex];
    GLuint dst = data.pbos[next_tex];

    // TODO: look into - obs has some locking here
    _glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    data.texture_mapped[next_tex] = false;

    gl_shmem_capture_stage(dst, src);
    data.texture_ready[next_tex] = true;
  }

  _glBindTexture(GL_TEXTURE_2D, last_tex);
  _glBindFramebuffer(GL_DRAW_FRAMEBUFFER, last_fbo);
}

/**
 * Capture one OpenGL frame
 */
void CAPSULE_STDCALL gl_capture (int width, int height) {

  static bool functions_initialized = false;
  static bool critical_failure = false;
  static bool first_frame = true;

  if (critical_failure) {
    return;
  }

  if (!functions_initialized) {
    functions_initialized = init_gl_functions();
    if (!functions_initialized) {
      critical_failure = true;
      return;
    }
  }

  // reset error flag
	_glGetError();

  if (!capsule_capture_ready()) {
    if (!capsule_capture_active() && !first_frame) {
      first_frame = true;
      gl_free();
    }

    return;
  }

  if (!data.cx) {
    gl_init(width, height);
  }

  if (data.cx) {
    if (first_frame) {
      capsule_write_video_format(
        data.cx,
        data.cy,
        CAPSULE_PIX_FMT_BGRA,
        1 /* vflip */,
        data.pitch
      );
      first_frame = false;
    }

    gl_shmem_capture();
  }
}

