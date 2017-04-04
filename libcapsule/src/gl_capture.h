
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

#pragma once

#include <stddef.h>

#include <lab/platform.h>
#include "logging.h"

#include "gl_capture_callback.h"

typedef char GLchar;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;

#define GL_FRONT 0x0404
#define GL_BACK 0x0405

#define GL_INVALID_OPERATION 0x0502

#define GL_UNSIGNED_BYTE 0x1401

#define GL_RGB 0x1907
#define GL_RGBA 0x1908

#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1

#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601

#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_ARRAY_BUFFER 0x8892
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_CURRENT_PROGRAM 0x8B8D

#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6

#define WGL_ACCESS_READ_ONLY_NV 0x0000
#define WGL_ACCESS_READ_WRITE_NV 0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1

#define GL_VIEWPORT 2978
#define GL_RENDERBUFFER 36161
#define GL_RENDERBUFFER_WIDTH 36162
#define GL_RENDERBUFFER_HEIGHT 36163

// state getters

typedef GLenum(LAB_STDCALL *glGetError_t)();
extern glGetError_t _glGetError;

typedef void *(LAB_STDCALL *glGetIntegerv_t)(GLenum pname, GLint *data);
extern glGetIntegerv_t _glGetIntegerv;

// textures

typedef void(LAB_STDCALL *glGenTextures_t)(GLsizei n, GLuint *buffers);
extern glGenTextures_t _glGenTextures;

typedef void(LAB_STDCALL *glBindTexture_t)(GLenum target, GLuint texture);
extern glBindTexture_t _glBindTexture;

typedef void(LAB_STDCALL *glTexImage2D_t)(GLenum target, GLint level,
                                              GLint internal_format,
                                              GLsizei width, GLsizei height,
                                              GLint border, GLenum format,
                                              GLenum type, const GLvoid *data);
extern glTexImage2D_t _glTexImage2D;

typedef void(LAB_STDCALL *glGetTexImage_t)(GLenum target, GLint level,
                                               GLenum format, GLenum type,
                                               GLvoid *img);
extern glGetTexImage_t _glGetTexImage;

typedef void(LAB_STDCALL *glDeleteTextures_t)(GLsizei n,
                                                  const GLuint *buffers);
extern glDeleteTextures_t _glDeleteTextures;

// vertex arrays

typedef void(LAB_STDCALL *glGenVertexArrays_t)(GLsizei n, GLuint *buffers);
extern glGenVertexArrays_t _glGenVertexArrays;

typedef void(LAB_STDCALL *glBindVertexArray_t)(GLuint buffer);
extern glBindVertexArray_t _glBindVertexArray;

// buffers

typedef void(LAB_STDCALL *glGenBuffers_t)(GLsizei n, GLuint *buffers);
extern glGenBuffers_t _glGenBuffers;

typedef void(LAB_STDCALL *glBindBuffer_t)(GLenum target, GLuint buffer);
extern glBindBuffer_t _glBindBuffer;

typedef void(LAB_STDCALL *glReadBuffer_t)(GLenum);
extern glReadBuffer_t _glReadBuffer;

typedef void(LAB_STDCALL *glDrawBuffer_t)(GLenum mode);
extern glDrawBuffer_t _glDrawBuffer;

typedef void(LAB_STDCALL *glBufferData_t)(GLenum target, GLsizeiptrARB size,
                                              const GLvoid *data, GLenum usage);
extern glBufferData_t _glBufferData;

typedef GLvoid *(LAB_STDCALL *glMapBuffer_t)(GLenum target, GLenum access);
extern glMapBuffer_t _glMapBuffer;

typedef GLboolean(LAB_STDCALL *glUnmapBuffer_t)(GLenum target);
extern glUnmapBuffer_t _glUnmapBuffer;

typedef void(LAB_STDCALL *glDeleteBuffers_t)(GLsizei n,
                                                 const GLuint *buffers);
extern glDeleteBuffers_t _glDeleteBuffers;

// framebuffers

typedef void(LAB_STDCALL *glGenFramebuffers_t)(GLsizei n, GLuint *buffers);
extern glGenFramebuffers_t _glGenFramebuffers;

typedef void(LAB_STDCALL *glBindFramebuffer_t)(GLenum target,
                                                   GLuint framebuffer);
extern glBindFramebuffer_t _glBindFramebuffer;

    typedef void(LAB_STDCALL *glBlitFramebuffer_t)(
        GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
        GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern glBlitFramebuffer_t _glBlitFramebuffer;

typedef void(LAB_STDCALL *glFramebufferTexture2D_t)(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level);
extern glFramebufferTexture2D_t _glFramebufferTexture2D;

typedef void(LAB_STDCALL *glDeleteFramebuffers_t)(GLsizei n,
                                                      GLuint *framebuffers);
extern glDeleteFramebuffers_t _glDeleteFramebuffers;

// shaders

typedef GLuint(LAB_STDCALL *glCreateShader_t)(GLenum target);
extern glCreateShader_t _glCreateShader;

typedef void(LAB_STDCALL *glShaderSource_t)(GLuint shader, GLsizei n, const GLchar* const* source, GLint *length);
extern glShaderSource_t _glShaderSource;

typedef void(LAB_STDCALL *glCompileShader_t)(GLuint shader);
extern glCompileShader_t _glCompileShader;

typedef GLuint(LAB_STDCALL *glCreateProgram_t)();
extern glCreateProgram_t _glCreateProgram;

typedef void(LAB_STDCALL *glAttachShader_t)(GLuint program, GLuint shader);
extern glAttachShader_t _glAttachShader;

typedef void(LAB_STDCALL *glLinkProgram_t)(GLuint program);
extern glLinkProgram_t _glLinkProgram;

typedef void(LAB_STDCALL *glUseProgram_t)(GLuint program);
extern glUseProgram_t _glUseProgram;

typedef GLint(LAB_STDCALL *glGetAttribLocation_t)(GLuint program, GLchar *attribName);
extern glGetAttribLocation_t _glGetAttribLocation;

typedef void(LAB_STDCALL *glEnableVertexAttribArray_t)(GLuint index);
extern glEnableVertexAttribArray_t _glEnableVertexAttribArray;

typedef void(LAB_STDCALL *glVertexAttribPointer_t)(GLuint index, GLint size,
                                                   GLenum type,
                                                   GLboolean normalized,
                                                   GLsizei stride,
                                                   const void *pointer);
extern glVertexAttribPointer_t _glVertexAttribPointer;

namespace capsule {
namespace gl {

#define GLSYM(sym) { \
  _ ## sym = (sym ## _t) GetProcAddress(#sym);\
  if (! _ ## sym) { \
    Log("Could not find GL function %s", #sym); \
    return false; \
  } \
}

extern const char *kDefaultOpengl;

bool EnsureOpengl();

}
}
