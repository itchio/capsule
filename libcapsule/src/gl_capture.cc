
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

#define DebugLog(...) capsule::Log("gl: " __VA_ARGS__)
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
  GLuint                  overlay_pbo;

  int 			  avoid_apple_gl;
};

static State state = {0};

LibHandle handle;

static inline bool ErrorEx(const char *func, const char *str, GLenum error) {
  if (error != 0) {
    Log("%s: %s: %lu", func, str, (unsigned long) error);
    return true;
  }

  return false;
}

static inline bool Error(const char *func, const char *str) {
  return ErrorEx(func, str, _glGetError());
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
  GLSYM(glGetString)

  GLSYM(glGenTextures)
  GLSYM(glBindTexture)
  GLSYM(glTexParameteri)
  GLSYM(glTexImage2D)
  GLSYM(glTexSubImage2D)
  GLSYM(glGetTexImage)
  GLSYM(glDeleteTextures)

  GLSYM(glGenVertexArrays)
  GLSYM(glBindVertexArray)

#if defined(LAB_MACOS)
  GLSYM(glGenVertexArraysAPPLE)
  GLSYM(glBindVertexArrayAPPLE)
#endif

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
  GLSYM(glGetShaderiv)
  GLSYM(glGetShaderInfoLog)
  GLSYM(glCreateProgram)
  GLSYM(glAttachShader)
  GLSYM(glLinkProgram)
  GLSYM(glValidateProgram)
  GLSYM(glGetProgramiv)
  GLSYM(glGetProgramInfoLog)
  GLSYM(glUseProgram)
  GLSYM(glGetAttribLocation)
  GLSYM(glBindFragDataLocation);
  GLSYM(glEnableVertexAttribArray)
  GLSYM(glVertexAttribPointer)
  GLSYM(glGetUniformLocation)
  GLSYM(glUniform1i)

  GLSYM(glDrawArrays)
  GLSYM(glClearColor)
  GLSYM(glClear)

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

static bool InitOverlayTexture(void) {
  state.overlay_width = 256;
  state.overlay_height = 128;
  size_t pixels_size = state.overlay_width * 4 * state.overlay_height;
  state.overlay_pixels = (unsigned char*) malloc(pixels_size);
  for (size_t i = 0; i < pixels_size; i++) {
    state.overlay_pixels[i] = 255;
  }

#define GLCHECK(msg) if (Error("InitOverlayVbo", msg)) { break; }

  GLint last_tex = 0;
  GLint last_unpack_pbo = 0;

  // save the state we change
  auto success = false;
  do {
    _glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
    GLCHECK("get last tex");

    _glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &last_unpack_pbo);
    GLCHECK("get last unpack pbo");

    success = true;
  } while (false);

  if (!success) {
    return false;
  }

  success = false;
  do {
    _glGenBuffers(1, &state.overlay_pbo);
    GLCHECK("gen pbo");

    _glBindBuffer(GL_PIXEL_UNPACK_BUFFER, state.overlay_pbo);
    GLCHECK("bind pbo");

    _glBufferData(GL_PIXEL_UNPACK_BUFFER, state.overlay_width * state.overlay_height * 4, nullptr, GL_STREAM_DRAW);
    GLCHECK("bind pbo");

    _glGenTextures(1, &state.overlay_tex);
    GLCHECK("gen texture");

    _glBindTexture(GL_TEXTURE_2D, state.overlay_tex);
    GLCHECK("bind texture");

    _glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    GLCHECK("set texture base level");

    _glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GLCHECK("set texture max level");

    _glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
    state.overlay_width, state.overlay_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLCHECK("tex image 2d");

    success = true;
  } while(false);

#undef GLCHECK

  _glBindTexture(GL_TEXTURE_2D, last_tex);
  _glBindBuffer(GL_PIXEL_UNPACK_BUFFER, last_unpack_pbo);

  return success;
}

static inline void safeGlGenVertexArrays(GLsizei n, GLuint *buffers) {
#if defined(LAB_MACOS)
  if (!state.avoid_apple_gl) {
    _glGenVertexArrays(n, buffers);
  } else {
    _glGenVertexArraysAPPLE(n, buffers);
    GLenum error = _glGetError();
    if (error != 0) {
      Log("Avoiding Apple GL: %d for glGenVertexArraysAPPLE", error);
      state.avoid_apple_gl = 1;
      safeGlGenVertexArrays(n, buffers);
    }
  }
#else
  _glGenVertexArrays(n, buffers);
#endif // LAB_MACOS
}

static inline void safeGlBindVertexArray(GLuint buffer) {
#if defined(LAB_MACOS)
  if (state.avoid_apple_gl) {
    _glBindVertexArray(buffer);
  } else {
    _glBindVertexArrayAPPLE(buffer);
    GLenum error = _glGetError();
    if (error != 0) {
      Log("Avoiding Apple GL: %d for glBindVertexArrayAPPLE", error);
      state.avoid_apple_gl = 1;
      safeGlBindVertexArray(buffer);
    }
  }
#else
  _glBindVertexArray(buffer);
#endif // LAB_MACOS
}

static bool InitOverlayVbo(void) {
  Log("OpenGL vendor: %s", _glGetString(GL_VENDOR));
  Log("OpenGL renderer: %s", _glGetString(GL_RENDERER));
  Log("OpenGL version: %s", _glGetString(GL_VERSION));
  Log("OpenGL shading language version: %s", _glGetString(GL_SHADING_LANGUAGE_VERSION));

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

#define GLCHECK(msg) if (Error("InitOverlayVbo", msg)) { break; }
#define GLSHADERCHECK(sh) { \
  GLint shader_status = 0; \
  _glGetShaderiv(sh, GL_COMPILE_STATUS, &shader_status); \
  GLCHECK("get shader iv compile"); \
  if (shader_status == GL_FALSE) { \
    Log("gl: shader compilation failed"); \
    GLint log_size = 0; \
    _glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &log_size); \
    GLCHECK("get shader iv log"); \
    auto log = new char[log_size]; \
    _glGetShaderInfoLog(sh, log_size, nullptr, log); \
    GLCHECK("get shader info log"); \
    Log("gl: shader compilation log (size %d):\n%s", log_size, log); \
    delete[] log; \
    break; \
  } \
}
#define GLPROGRAMCHECK(pr) { \
  GLint program_status = 0; \
  _glGetProgramiv(pr, GL_LINK_STATUS, &program_status); \
  GLCHECK("get program iv link"); \
  if (program_status == GL_FALSE) { \
    Log("gl: program link failed"); \
    GLint log_size = 0; \
    _glGetProgramiv(pr, GL_INFO_LOG_LENGTH, &log_size); \
    GLCHECK("get program iv log"); \
    auto log = new char[log_size]; \
    _glGetProgramInfoLog(pr, log_size, nullptr, log); \
    GLCHECK("get program info log"); \
    Log("gl: program link log:\n%s", log); \
    delete[] log; \
    break; \
  } \
  _glValidateProgram(pr); \
  GLCHECK("validate program"); \
  _glGetProgramiv(pr, GL_VALIDATE_STATUS, &program_status); \
  GLCHECK("get program iv validate"); \
  if (program_status == GL_FALSE) { \
    Log("gl: program validate failed"); \
    GLint log_size = 0; \
    _glGetProgramiv(pr, GL_INFO_LOG_LENGTH, &log_size); \
    GLCHECK("get program iv log length"); \
    auto log = new char[log_size]; \
    _glGetProgramInfoLog(pr, log_size, nullptr, log); \
    GLCHECK("get program info log"); \
    Log("gl: program validate log:\n%s", log); \
    delete[] log; \
    break; \
  } \
}

  GLint last_vao = 0;
  GLint last_vbo = 0;
  GLint last_program = 0;

  // save the state we change
  auto success = false;
  do {
    _glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vao);
    GLCHECK("get last vao");

    _glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vbo);
    GLCHECK("get last vbo");

    _glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLCHECK("get last program");

    success = true;
  } while(false);
  if (!success) {
    return false;
  }

  success = false;
  do {
    DebugLog("Creating overlay vao...");
    safeGlGenVertexArrays(1, &state.overlay_vao);
    GLCHECK("vao gen");
    safeGlBindVertexArray(state.overlay_vao);
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
    GLSHADERCHECK(state.overlay_vertex_shader);
    DebugLog("Vertex shader compiled!");

    DebugLog("Creating overlay fragment shader...");
    state.overlay_fragment_shader = _glCreateShader(GL_FRAGMENT_SHADER);
    GLCHECK("fshader create");
    _glShaderSource(state.overlay_fragment_shader, 1, &kFragmentSource, nullptr);
    GLCHECK("fshader source");
    _glCompileShader(state.overlay_fragment_shader);
    GLCHECK("fshader compile");
    GLSHADERCHECK(state.overlay_fragment_shader);
    DebugLog("Fragment shader compiled!");

    DebugLog("Creating shader program...");
    state.overlay_shader_program = _glCreateProgram();
    GLCHECK("program create");
    _glAttachShader(state.overlay_shader_program, state.overlay_vertex_shader);
    GLCHECK("vshader attach");
    _glAttachShader(state.overlay_shader_program, state.overlay_fragment_shader);
    GLCHECK("fshader attach");
    _glBindFragDataLocation(state.overlay_shader_program, 0, "outColor");
    GLCHECK("bind frag data location");
    _glLinkProgram(state.overlay_shader_program);
    GLCHECK("program link");
    GLPROGRAMCHECK(state.overlay_shader_program);
    DebugLog("Program linked & validated!");
    _glUseProgram(state.overlay_shader_program);
    GLCHECK("program use");

    DebugLog("Specifying vertex data layout");

    GLint pos_attrib = _glGetAttribLocation(state.overlay_shader_program, "position");
    GLCHECK("pos attrib: get location");
    _glEnableVertexAttribArray(pos_attrib);
    GLCHECK("pos attrib: enable");
    _glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    GLCHECK("pos attrib: pointer");

    GLint tex_attrib = _glGetAttribLocation(state.overlay_shader_program, "texcoord");
    GLCHECK("tex attrib: get location");
    _glEnableVertexAttribArray(tex_attrib);
    GLCHECK("tex attrib: enable");
    _glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    GLCHECK("tex attrib: pointer");

    GLint tex_loc = _glGetUniformLocation(state.overlay_shader_program, "diffuse");
    GLCHECK("tex loc");
    _glUniform1i(tex_loc, 0);
    GLCHECK("tex uniform");

    success = true;
  } while (false);

#undef GLCHECK
#undef GLSHADERCHECK
#undef GLPROGRAMCHECK

  safeGlBindVertexArray(last_vao);
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

  if (!InitOverlayTexture() || !InitOverlayVbo()) {
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

static inline bool UpdateOverlayTexture() {

#define GLCHECK(msg) if (Error("UpdateOverlayTexture", msg)) { return false; }

  size_t pixels_size = state.overlay_width * 4 * state.overlay_height;
  for (int y = 0; y < state.overlay_height; y++) {
    for (int x = 0; x < state.overlay_width; x++) {
      int i = (y * state.overlay_width + x) * 4;
      state.overlay_pixels[i]     = (state.overlay_pixels[i]     + 1) % 256;
      state.overlay_pixels[i + 1] = (state.overlay_pixels[i + 1] + 1) % 256;
      state.overlay_pixels[i + 2] = (state.overlay_pixels[i + 2] + 1) % 256;
      // state.overlay_pixels[i + 3] = (state.overlay_pixels[i + 3] + 1) % 256;
    }
  }

  _glBindBuffer(GL_PIXEL_UNPACK_BUFFER, state.overlay_pbo);
  GLCHECK("bind buffer");

  _glBufferData(GL_PIXEL_UNPACK_BUFFER, pixels_size, 0, GL_STREAM_DRAW);
  GLCHECK("buffer data");

  void *mapped = (void *) _glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  GLCHECK("map buffer");

  if (mapped) {
    memcpy(mapped, state.overlay_pixels, pixels_size);
    _glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    GLCHECK("unmap buffer");
  } else {
    Log("UpdateOverlayTexture: failed to map texture pbo");
  }

  _glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
    state.overlay_width, state.overlay_height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  GLCHECK("tex sub image 2d");

#undef GLCHECK

  return true;
}

void DrawOverlay() {

  DebugLog("Drawing overlay!");

#define GLCHECK(msg) if (Error("DrawOverlay", msg)) { break; }

  GLint last_tex = 0;
  GLint last_vao = 0;
  GLint last_vbo = 0;
  GLint last_unpack_pbo = 0;
  GLint last_program = 0;

  // save the state we change
  auto success = false;
  do {
    _glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
    GLCHECK("get last tex");

    _glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vao);
    GLCHECK("get last vao");

    _glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vbo);
    GLCHECK("get last vbo");

    _glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &last_unpack_pbo);
    GLCHECK("get last unpack pbo");

    _glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLCHECK("get last program");

    success = true;
  } while(false);
  if (!success) {
    return;
  }

  success = false;
  do {
    _glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    GLCHECK("clear");

    _glBindTexture(GL_TEXTURE_2D, state.overlay_tex);
    GLCHECK("bind texture");

    if (!UpdateOverlayTexture()) {
      break;
    }

    safeGlBindVertexArray(state.overlay_vao);
    GLCHECK("bind vao");

    _glUseProgram(state.overlay_shader_program);
    GLCHECK("use program");

    _glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLCHECK("draw arrays");

    success = true;
  } while(false);

  _glBindTexture(GL_TEXTURE_2D, last_tex);

  safeGlBindVertexArray(last_vao);

  _glBindBuffer(GL_ARRAY_BUFFER, last_vbo);
  _glUseProgram(last_program);
  _glBindBuffer(GL_PIXEL_UNPACK_BUFFER, last_unpack_pbo);

#undef GLCHECK

  if (success) {
    DebugLog("Done drawing overlay!");
  } else {
    Log("Drawing overlay failed!");
  }
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

  DrawOverlay();
}

} // namespace gl
} // namespace capture

/////////////////////////////////
// GL functions start
/////////////////////////////////
glGetError_t _glGetError;
glGetIntegerv_t _glGetIntegerv;
glGetString_t _glGetString;

glGenTextures_t _glGenTextures;
glBindTexture_t _glBindTexture;
glTexParameteri_t _glTexParameteri;
glTexImage2D_t _glTexImage2D;
glTexSubImage2D_t _glTexSubImage2D;
glGetTexImage_t _glGetTexImage;
glDeleteTextures_t _glDeleteTextures;

glGenVertexArrays_t _glGenVertexArrays;
glBindVertexArray_t _glBindVertexArray;

#if defined(LAB_MACOS)
glGenVertexArraysAPPLE_t _glGenVertexArraysAPPLE;
glBindVertexArrayAPPLE_t _glBindVertexArrayAPPLE;
#endif

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
glGetShaderiv_t _glGetShaderiv;
glGetShaderInfoLog_t _glGetShaderInfoLog;
glCreateProgram_t _glCreateProgram;
glAttachShader_t _glAttachShader;
glLinkProgram_t _glLinkProgram;
glValidateProgram_t _glValidateProgram;
glGetProgramiv_t _glGetProgramiv;
glGetProgramInfoLog_t _glGetProgramInfoLog;
glUseProgram_t _glUseProgram;
glGetAttribLocation_t _glGetAttribLocation;
glBindFragDataLocation_t _glBindFragDataLocation;
glEnableVertexAttribArray_t _glEnableVertexAttribArray;
glVertexAttribPointer_t _glVertexAttribPointer;
glGetUniformLocation_t _glGetUniformLocation;
glUniform1i_t _glUniform1i;

glDrawArrays_t _glDrawArrays;
glClearColor_t _glClearColor;
glClear_t _glClear;
/////////////////////////////////
// GL functions end
/////////////////////////////////
