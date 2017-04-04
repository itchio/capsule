
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "gl_capture.h"

#include <string.h> // memset

#include <lab/strings.h>

#include "dynlib.h"
#include "io.h"
#include "capture.h"

#include "gl_shaders.h"

#define DebugLog(...) capsule::Log(__VA_ARGS__)
// #define DebugLog(...)

namespace capsule {
namespace gl {

#if defined(LAB_WINDOWS)
const char *kDefaultOpengl = "OPENGL32.DLL";
#elif defined(LAB_MACOS)
const char *kDefaultOpengl = "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib";
#elif defined(LAB_LINUX)
const char *kDefaultOpengl = "libGL.so.1";
#endif

struct State {
  int                     cx;
  int                     cy;
  int64_t                 pitch;
  GLuint                  fbo;

  int                     cur_tex;
  int                     copy_wait;
  GLuint                  pbos[capture::kNumBuffers];
  GLuint                  textures[capture::kNumBuffers];
  bool                    texture_ready[capture::kNumBuffers];
  bool                    texture_mapped[capture::kNumBuffers];
  int64_t                 timestamps[capture::kNumBuffers];

  int                     overlay_width;  
  int                     overlay_height;  
  unsigned char           *overlay_pixels;
  GLuint                  overlay_vao;
  GLuint                  overlay_vbo;
  GLuint                  overlay_tex;
  GLuint                  overlay_fragment_shader;
  GLuint                  overlay_vertex_shader;
  GLuint                  overlay_shader_program;
};

static State state = {};

LibHandle handle;

static inline bool Error(const char *func, const char *str) {
	GLenum error = _glGetError();
	if (error != 0) {
		Log("%s: %s: %lu", func, str, (unsigned long) error);
		return true;
	}

	return false;
}

bool EnsureOpengl() {
  if (!handle) {
    Log("Loading default OpenGL %s", kDefaultOpengl);
		if (!LoadOpengl(kDefaultOpengl)) {
      Log("Could not load OpenGL from %s", kDefaultOpengl);
      return false;
    }
  }

  return true;
}

bool InitFunctions() {
  if (!EnsureOpengl()) {
    return false;
  }

  GLSYM(glGetError)
  GLSYM(glGetIntegerv)

  GLSYM(glGenTextures)
  GLSYM(glBindTexture)
  GLSYM(glTexImage2D)
  GLSYM(glGetTexImage)
  GLSYM(glDeleteTextures)

  GLSYM(glGenVertexArrays)
  GLSYM(glBindVertexArray)

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

  GLSYM(glCreateShader)
  GLSYM(glShaderSource)
  GLSYM(glCompileShader)
  GLSYM(glCreateProgram)
  GLSYM(glAttachShader)
  GLSYM(glLinkProgram)
  GLSYM(glUseProgram)
  GLSYM(glGetAttribLocation)
  GLSYM(glEnableVertexAttribArray)
  GLSYM(glVertexAttribPointer)

  return true;
}

static void Free() {
  for (size_t i = 0; i < capture::kNumBuffers; i++) {
    if (state.pbos[i]) {
      if (state.texture_mapped[i]) {
        _glBindBuffer(GL_PIXEL_PACK_BUFFER, state.pbos[i]);
        _glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        _glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      }

      _glDeleteBuffers(1, &state.pbos[i]);
    }

    if (state.textures[i])
      _glDeleteTextures(1, &state.textures[i]);
  }

  if (state.fbo) {
		_glDeleteFramebuffers(1, &state.fbo);
  }

	Error("Free", "GL error occurred on free");

  memset(&state, 0, sizeof(state));

  Log("----------------------- gl capture freed ----------------------");
}

static inline bool InitFbo(void) {
	_glGenFramebuffers(1, &state.fbo);
	return !Error("InitFbo", "failed to initialize FBO");
}

static inline bool ShmemInitData(size_t idx, size_t size) {
	_glBindBuffer(GL_PIXEL_PACK_BUFFER, state.pbos[idx]);
	if (Error("ShmemInitData", "failed to bind pbo")) {
		return false;
	}

	_glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);
	if (Error("ShmemInitData", "failed to set pbo data")) {
		return false;
	}

	_glBindTexture(GL_TEXTURE_2D, state.textures[idx]);
	if (Error("ShmemInitData", "failed to set bind texture")) {
		return false;
	}

	_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, state.cx, state.cy,
			0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	if (Error("ShmemInitData", "failed to set texture data")) {
		return false;
	}

	return true;
}

static bool InitOverlayVbo(void) {
  // gl coordinate system: (0, 0) = bottom-left
  float cx = (float) state.cx;
  float cy = (float) state.cy;
  float width = (float) state.overlay_width;
  float height = (float) state.overlay_height;
  float x = cx - width;
  float y = 0;

  float l = x / cx * 2.0f - 1.0f;
  float r = (x + width) / cx * 2.0f - 1.0f;
  float b = y / cy * 2.0f - 1.0f;
  float t = (y + height) / cy * 2.0f - 1.0f;

  Log("Overlay vbo: left = %.2f, right = %.2f, top = %.2f, bottom = %.2f", l, r, t, b);

  const GLfloat verts[] = {
    // pos    texcoord
    l, t,     0.0f, 0.0f,
    l, b,     0.0f, 1.0f,
    r, t,     1.0f, 0.0f,
    r, b,     1.0f, 1.0f
  };

  GLint last_vao;
  GLint last_vbo;
  GLint last_program;

  // save the state we change
  {
    _glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vao);
    if (Error("InitOverlayVbo", "failed to get last vao")) {
      return false;
    }

    _glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vbo);
    if (Error("InitOverlayVbo", "failed to get last vbo")) {
      return false;
    }

    _glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    if (Error("InitOverlayVbo", "failed to get last program")) {
      return false;
    }
  }

#define GLCHECK(msg) if (Error("InitOverlayVbo", msg)) { break; }

  auto success = false;
  do {
    DebugLog("Creating overlay vao...");
    _glGenVertexArrays(1, &state.overlay_vao);
    GLCHECK("vao gen");
    _glBindVertexArray(state.overlay_vao);
    GLCHECK("vao bind");

    DebugLog("Creating overlay vbo...");
    _glGenBuffers(1, &state.overlay_vbo);
    GLCHECK("vbo gen");
    _glBindBuffer(GL_ARRAY_BUFFER, state.overlay_vbo);
    GLCHECK("vbo bind");

    _glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    GLCHECK("vbo upload");

    DebugLog("Creating overlay vertex shader...");
    state.overlay_vertex_shader = _glCreateShader(GL_VERTEX_SHADER);
    GLCHECK("vshader create");
    _glShaderSource(state.overlay_vertex_shader, 1, &kVertexSource, nullptr);
    GLCHECK("vshader source");
    _glCompileShader(state.overlay_vertex_shader);
    GLCHECK("vshader compile");

    DebugLog("Creating overlay fragment shader...");
    state.overlay_fragment_shader = _glCreateShader(GL_FRAGMENT_SHADER);
    GLCHECK("fshader create");
    _glShaderSource(state.overlay_fragment_shader, 1, &kFragmentSource, nullptr);
    GLCHECK("fshader source");
    _glCompileShader(state.overlay_fragment_shader);
    GLCHECK("fshader compile");

    DebugLog("Creating shader program...");
    state.overlay_shader_program = _glCreateProgram();
    GLCHECK("program create");
    _glAttachShader(state.overlay_shader_program, state.overlay_vertex_shader);
    GLCHECK("vshader attach");
    _glAttachShader(state.overlay_shader_program, state.overlay_fragment_shader);
    GLCHECK("fshader attach");
    _glLinkProgram(state.overlay_shader_program);
    GLCHECK("program link");
    _glUseProgram(state.overlay_shader_program);
    GLCHECK("program use");

    success = true;
  } while (false);

#undef GLCHECK

  _glBindVertexArray(last_vao);
  _glBindBuffer(GL_ARRAY_BUFFER, last_vbo);
  _glUseProgram(last_program);

  return success;
}

static inline bool ShmemInitBuffers(void) {
	size_t size = state.cx * state.cy * 4;
	GLint last_pbo;
	GLint last_tex;

	_glGenBuffers(capture::kNumBuffers, state.pbos);
	if (Error("ShmemInitBuffers", "failed to generate buffers")) {
		return false;
	}

	_glGenTextures(capture::kNumBuffers, state.textures);
	if (Error("ShmemInitBuffers", "failed to generate textures")) {
		return false;
	}

	_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &last_pbo);
	if (Error("ShmemInitBuffers",
				"failed to save pixel pack buffer")) {
		return false;
	}

	_glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
	if (Error("ShmemInitBuffers", "failed to save texture")) {
		return false;
	}

	for (size_t i = 0; i < capture::kNumBuffers; i++) {
		if (!ShmemInitData(i, size)) {
			return false;
		}
	}

	_glBindBuffer(GL_PIXEL_PACK_BUFFER, last_pbo);
	_glBindTexture(GL_TEXTURE_2D, last_tex);
	return true;
}

static bool ShmemInit() {
	if (!ShmemInitBuffers()) {
		return false;
	}
	if (!InitFbo()) {
		return false;
	}

	Log("gl memory capture successful");
	return true;
}

static inline void FixWidthHeight(int &width, int &height) {
  if (width == 0 || height == 0) {
    int viewport[4];
    _glGetIntegerv(GL_VIEWPORT, viewport);

    if (viewport[2] > 0 && viewport[3] > 0) {
      width = viewport[2];
      height = viewport[3];
    } else {
      Log("gl_init: 0x0 viewport");
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
}

static bool Init (int width, int height) {
  FixWidthHeight(width, height);

  const int components = 4; // BGRA
  const size_t pitch = width * components;

  state.cx = width;
  state.cy = height;
  state.pitch = pitch;

  if (!InitOverlayVbo()) {
    Free();
    return false;
  }

  bool success = ShmemInit();
  if (!success) {
    Free();
    return false;
  }

  return true;
}

static void CopyBackbuffer(GLuint dst) {
	_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.fbo);
	if (Error("gl_copy_backbuffer", "failed to bind FBO")) {
		return;
	}

	_glBindTexture(GL_TEXTURE_2D, dst);
	if (Error("gl_copy_backbuffer", "failed to bind texture")) {
		return;
	}

	_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, dst, 0);
	if (Error("gl_copy_backbuffer", "failed to set frame buffer")) {
		return;
	}

	_glReadBuffer(GL_BACK);

	_glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (Error("gl_copy_backbuffer", "failed to set draw buffer")) {
		return;
	}

	_glBlitFramebuffer(0, 0, state.cx, state.cy,
			0, 0, state.cx, state.cy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	Error("gl_copy_backbuffer", "failed to blit");
}

static inline void ShmemCaptureQueueCopy(void) {
	for (int i = 0; i < capture::kNumBuffers; i++) {
		if (state.texture_ready[i]) {
			GLvoid *buffer;
      auto timestamp = state.timestamps[i];

			state.texture_ready[i] = false;

			_glBindBuffer(GL_PIXEL_PACK_BUFFER, state.pbos[i]);
			if (Error("gl_shmem_capture_queue_copy", "failed to bind pbo")) {
				return;
			}

			buffer = _glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (buffer) {
				state.texture_mapped[i] = true;
        io::WriteVideoFrame(timestamp, (char*) buffer, state.cy * state.pitch);
			}
			break;
		}
	}
}

static inline void ShmemCaptureStage(GLuint dst_pbo, GLuint src_tex) {
	_glBindTexture(GL_TEXTURE_2D, src_tex);
	if (Error("ShmemCaptureStage", "failed to bind src_tex")) {
		return;
	}

	_glBindBuffer(GL_PIXEL_PACK_BUFFER, dst_pbo);
	if (Error("ShmemCaptureStage", "failed to bind dst_pbo")) {
		return;
	}

	_glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	if (Error("ShmemCaptureStage", "failed to read src_tex")) {
		return;
	}
}

void ShmemCapture () {
  int next_tex;
  GLint last_fbo;
  GLint last_tex;

  auto timestamp = capture::FrameTimestamp();

  // save last fbo & texture to restore them after capture
  {
    _glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fbo);
    if (Error("ShmemCapture", "failed to get last fbo")) {
      return;
    }

    _glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
    if (Error("ShmemCapture", "failed to get last texture")) {
      return;
    }
  }

  // try to map & send all the textures that are ready
  ShmemCaptureQueueCopy();

  next_tex = (state.cur_tex + 1) % capture::kNumBuffers;

  state.timestamps[next_tex] = timestamp;
  CopyBackbuffer(state.textures[next_tex]);

  if (state.copy_wait < capture::kNumBuffers - 1) {
    state.copy_wait++;
  } else {
    GLuint src = state.textures[next_tex];
    GLuint dst = state.pbos[next_tex];

    // TODO: look into - obs has some locking here
    _glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    state.texture_mapped[next_tex] = false;

    ShmemCaptureStage(dst, src);
    state.texture_ready[next_tex] = true;
  }

  _glBindTexture(GL_TEXTURE_2D, last_tex);
  _glBindFramebuffer(GL_DRAW_FRAMEBUFFER, last_fbo);
}

/**
 * Capture one OpenGL frame
 */
void Capture(int width, int height) {
  capture::SawBackend(capture::kBackendGL);

  static bool functions_initialized = false;
  static bool critical_failure = false;
  static bool first_frame = true;

  if (critical_failure) {
    return;
  }

  if (!functions_initialized) {
    functions_initialized = InitFunctions();
    if (!functions_initialized) {
      critical_failure = true;
      return;
    }
  }

  // reset error flag
	_glGetError();

  if (!capture::Ready()) {
    if (!capture::Active() && !first_frame) {
      first_frame = true;
      Free();
    }

    return;
  }

  if (!state.cx) {
    if (!Init(width, height)) {
      Log("GL: initialization failed, stopping capture");
      io::WriteCaptureStop();
      return;
    }
  }

  if (state.cx) {
    if (first_frame) {
      io::WriteVideoFormat(
        state.cx,
        state.cy,
        messages::PixFmt_BGRA,
        true /* vflip */,
        state.pitch
      );
      first_frame = false;
    }

    ShmemCapture();
  }
}

} // namespace gl
} // namespace capture

/////////////////////////////////
// GL functions start
/////////////////////////////////
glGetError_t _glGetError;
glGetIntegerv_t _glGetIntegerv;

glGenTextures_t _glGenTextures;
glBindTexture_t _glBindTexture;
glTexImage2D_t _glTexImage2D;
glGetTexImage_t _glGetTexImage;
glDeleteTextures_t _glDeleteTextures;

glGenVertexArrays_t _glGenVertexArrays;
glBindVertexArray_t _glBindVertexArray;

glGenBuffers_t _glGenBuffers;
glBindBuffer_t _glBindBuffer;
glReadBuffer_t _glReadBuffer;
glDrawBuffer_t _glDrawBuffer;
glBufferData_t _glBufferData;
glMapBuffer_t _glMapBuffer;
glUnmapBuffer_t _glUnmapBuffer;
glDeleteBuffers_t _glDeleteBuffers;

glGenFramebuffers_t _glGenFramebuffers;
glBindFramebuffer_t _glBindFramebuffer;
glBlitFramebuffer_t _glBlitFramebuffer;
glFramebufferTexture2D_t _glFramebufferTexture2D;
glDeleteFramebuffers_t _glDeleteFramebuffers;

glCreateShader_t _glCreateShader;
glShaderSource_t _glShaderSource;
glCompileShader_t _glCompileShader;
glCreateProgram_t _glCreateProgram;
glAttachShader_t _glAttachShader;
glLinkProgram_t _glLinkProgram;
glUseProgram_t _glUseProgram;
glGetAttribLocation_t _glGetAttribLocation;
glEnableVertexAttribArray_t _glEnableVertexAttribArray;
glVertexAttribPointer_t _glVertexAttribPointer;
/////////////////////////////////
// GL functions end
/////////////////////////////////
