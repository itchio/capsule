
#define _GNU_SOURCE

#if defined(_WIN32)
#define LIBSDL2_FILENAME "SDL2.dll"
#define CAPSULE_WINDOWS

#elif defined(__APPLE__)
#define LIBSDL2_FILENAME "libSDL2.dylib"
#define CAPSULE_OSX

#elif defined(__linux__) || defined(__unix__)
#define LIBSDL2_FILENAME "libSDL2.so"
#define DEFAULT_OPENGL "libGL.so.1"
#define CAPSULE_LINUX

#else
#error Unsupported platform
#endif

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

#include <GL/glew.h>

/* extern const char *SDL_GetError(); */

static void assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  fprintf(stderr, "[main] Assertion failed: %s\n", msg);
  /* const char *err = SDL_GetError(); */
  /* fprintf(stderr, "[main] Last SDL GetError: %s\n", err); */
  exit(1);
}

#define FRAME_WIDTH 512
#define FRAME_HEIGHT 512
char *frameData;
int frameNumber = 0;

typedef void* (*dlopen_type)(const char*, int);
dlopen_type real_dlopen;
void *gl_handle;

void ensure_real_dlopen() {
#ifdef CAPSULE_LINUX
  if (!real_dlopen) {
    printf("[libfake] Getting real dlopen\n");
    real_dlopen = dlsym(RTLD_NEXT, "dlopen");
    printf("[libfake] Real dlopen = %p\n", real_dlopen);
    printf("[libfake] Our  dlopen = %p\n", &dlopen);
  }

  if (!gl_handle) {
    printf("[libfake] Loading libmath from %s\n", "libm.dylib");
    void *m_handle = real_dlopen("libm.dylib", (RTLD_NOW|RTLD_LOCAL));
    printf("[libfake] Succesfully loaded libm? %d\n", !!m_handle);

    printf("[libfake] Loading ourselves from %s\n", "libfake.dylib");
    void *f_handle = real_dlopen("libfake.dylib", (RTLD_NOW|RTLD_LOCAL));
    printf("[libfake] Succesfully loaded libfake? %d\n", !!f_handle);

    printf("[libfake] Loading real opengl from %s\n", DEFAULT_OPENGL);
    gl_handle = real_dlopen(DEFAULT_OPENGL, (RTLD_NOW|RTLD_LOCAL));
    assert("Loaded real OpenGL lib", !!gl_handle);
    printf("[libfake] Loaded opengl!\n");
  }
#endif
}

void* glXGetProcAddressARB (const char *name) {
  printf("[libfake] In glXGetProcAddressARB: %s\n", name);
  ensure_real_dlopen();

#ifdef CAPSULE_LINUX
  if (!strcmp("glXSwapBuffers", name)) {
    printf("[libfake] Should return hooked swapbuffers\n");
    return &fake_glXSwapBuffers;
  }
#endif

  return dlsym(gl_handle, name);
}

void* _dlopen (const char * filename, int flag) {
  printf("[libfake] In dlopen(%s, %d)\n", filename, flag);
  ensure_real_dlopen();

  if (!strcmp("libGL.so.1", filename)) {
    printf("[libfake] Faking libGL\n");
    return real_dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
  } else {
    return real_dlopen(filename, flag);
  }
}

#ifdef CAPSULE_LINUX
void fake_glXSwapBuffers (void *a, void *b) {
  printf("[libfake] In glXSwapBuffers\n");

  int width = FRAME_WIDTH;
  int height = FRAME_HEIGHT;
  int components = 3;
  GLenum format = GL_RGB;

  size_t frameDataSize = FRAME_WIDTH * FRAME_HEIGHT * components;
  if (!frameData) {
    frameData = malloc(frameDataSize);
  }
  
  /* glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, frameData); */
  
  /* char frameName[512]; */
  /* bzero(frameName, 512); */
  /* sprintf(frameName, "frames/frame%03d.raw", frameNumber); */
  /* frameNumber++; */

  /* FILE *f = fopen(frameName, "wb"); */
  /* assert("Opened", !!f); */

  /* fwrite(frameData, 1, frameDataSize, f); */
  /* fclose(f); */

  if (frameNumber > 60) {
    printf("Captured 60 frames, going down.\n");
    exit(0);
  }

  return _realglXSwapBuffers(a, b);
}

typedef int (*glXQueryExtensionType)(void*, void*, void*);
glXQueryExtensionType _realglXQueryExtension;

int glXQueryExtension (void *a, void *b, void *c) {
  printf("[libfake] in glXQueryExtension\n");
  return _realglXQueryExtension(a, b, c);
}

typedef void (*glXSwapBuffersType)(void*, void*);
glXSwapBuffersType _realglXSwapBuffers;
#endif

void __attribute__((constructor)) libfake_load() {
  printf("[libfake] Initializing...\n");

#ifdef CAPSULE_LINUX
  printf("[libfake] Loading real OpenGL lib...\n");
  ensure_real_dlopen();

  printf("[libfake] Getting glXQueryExtension adress\n");
  _realglXQueryExtension = dlsym(gl_handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);

  _realglXSwapBuffers = dlsym(gl_handle, "glXSwapBuffers");
  assert("Got glXSwapBuffers", !!_realglXSwapBuffers);
#endif

  printf("[libfake] All systems go.\n");
}

void __attribute__((destructor)) libfake_unload() {
  printf("[libfake] Winding down...\n");
}

