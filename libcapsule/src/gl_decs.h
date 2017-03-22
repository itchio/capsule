#pragma once

#include <stddef.h>

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
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF

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
static glGetError_t _glGetError;

typedef void *(LAB_STDCALL *glGetIntegerv_t)(GLenum pname, GLint *data);
static glGetIntegerv_t _glGetIntegerv;

// textures

typedef void(LAB_STDCALL *glGenTextures_t)(GLsizei n, GLuint *buffers);
static glGenTextures_t _glGenTextures;

typedef void(LAB_STDCALL *glBindTexture_t)(GLenum target, GLuint texture);
static glBindTexture_t _glBindTexture;

typedef void(LAB_STDCALL *glTexImage2D_t)(GLenum target, GLint level,
                                              GLint internal_format,
                                              GLsizei width, GLsizei height,
                                              GLint border, GLenum format,
                                              GLenum type, const GLvoid *data);
static glTexImage2D_t _glTexImage2D;

typedef void(LAB_STDCALL *glGetTexImage_t)(GLenum target, GLint level,
                                               GLenum format, GLenum type,
                                               GLvoid *img);
static glGetTexImage_t _glGetTexImage;

typedef void(LAB_STDCALL *glDeleteTextures_t)(GLsizei n,
                                                  const GLuint *buffers);
static glDeleteTextures_t _glDeleteTextures;

// buffers

typedef void(LAB_STDCALL *glGenBuffers_t)(GLsizei n, GLuint *buffers);
static glGenBuffers_t _glGenBuffers;

typedef void(LAB_STDCALL *glBindBuffer_t)(GLenum target, GLuint buffer);
static glBindBuffer_t _glBindBuffer;

typedef void(LAB_STDCALL *glReadBuffer_t)(GLenum);
static glReadBuffer_t _glReadBuffer;

typedef void(LAB_STDCALL *glDrawBuffer_t)(GLenum mode);
static glDrawBuffer_t _glDrawBuffer;

typedef void(LAB_STDCALL *glBufferData_t)(GLenum target, GLsizeiptrARB size,
                                              const GLvoid *data, GLenum usage);
static glBufferData_t _glBufferData;

typedef GLvoid *(LAB_STDCALL *glMapBuffer_t)(GLenum target, GLenum access);
static glMapBuffer_t _glMapBuffer;

typedef GLboolean(LAB_STDCALL *glUnmapBuffer_t)(GLenum target);
static glUnmapBuffer_t _glUnmapBuffer;

typedef void(LAB_STDCALL *glDeleteBuffers_t)(GLsizei n,
                                                 const GLuint *buffers);
static glDeleteBuffers_t _glDeleteBuffers;

// framebuffers

typedef void(LAB_STDCALL *glGenFramebuffers_t)(GLsizei n, GLuint *buffers);
static glGenFramebuffers_t _glGenFramebuffers;

typedef void(LAB_STDCALL *glBindFramebuffer_t)(GLenum target,
                                                   GLuint framebuffer);
static glBindFramebuffer_t _glBindFramebuffer;

    typedef void(LAB_STDCALL *glBlitFramebuffer_t)(
        GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
        GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
static glBlitFramebuffer_t _glBlitFramebuffer;

typedef void(LAB_STDCALL *glFramebufferTexture2D_t)(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level);
static glFramebufferTexture2D_t _glFramebufferTexture2D;

typedef void(LAB_STDCALL *glDeleteFramebuffers_t)(GLsizei n,
                                                      GLuint *framebuffers);
static glDeleteFramebuffers_t _glDeleteFramebuffers;

// platform-specific

typedef void* (*terryglGetProcAddress_t)(const char*);

#if defined(LAB_LINUX)

// this intercepts swapbuffers for games statically-linked
// against libGL. Others go through dlopen, which we intercept
// as well.
void glXSwapBuffers (void *a, void *b);

typedef int (*glXQueryExtension_t)(void*, void*, void*);
static glXQueryExtension_t _glXQueryExtension;

typedef void (*glXSwapBuffers_t)(void*, void*);
static glXSwapBuffers_t _glXSwapBuffers;

typedef terryglGetProcAddress_t glXGetProcAddressARB_t;
static glXGetProcAddressARB_t _glXGetProcAddressARB;

#define terryglGetProcAddress _glXGetProcAddressARB

#elif defined(LAB_WINDOWS)

typedef terryglGetProcAddress_t wglGetProcAddress_t;
static wglGetProcAddress_t _wglGetProcAddress;

#define terryglGetProcAddress _wglGetProcAddress

#elif defined(LAB_MACOS)

typedef terryglGetProcAddress_t glGetProcAddress_t;
static glGetProcAddress_t _glGetProcAddress;

#define terryglGetProcAddress _glGetProcAddress

#endif // LAB_MACOS
