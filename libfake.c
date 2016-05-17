
#define _GNU_SOURCE

#if defined(_WIN32)
#define LIBSDL2_FILENAME "SDL2.dll"
#define DEFAULT_OPENGL "OPENGL32.DLL"
#define CAPSULE_WINDOWS

#elif defined(__APPLE__)
#define LIBSDL2_FILENAME "libSDL2.dylib"
#define DEFAULT_OPENGL "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
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

static void assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  fprintf(stderr, "[main] Assertion failed: %s\n", msg);
  /* const char *err = SDL_GetError(); */
  /* fprintf(stderr, "[main] Last SDL GetError: %s\n", err); */
  exit(1);
}

#ifdef CAPSULE_LINUX
void fake_glXSwapBuffers (void *a, void *b);

typedef int (*glXQueryExtensionType)(void*, void*, void*);
glXQueryExtensionType _realglXQueryExtension;

typedef void (*glXSwapBuffersType)(void*, void*);
glXSwapBuffersType _realglXSwapBuffers;
#endif

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
#endif

  if (!gl_handle) {
    printf("[libfake] Loading real opengl from %s\n", DEFAULT_OPENGL);
    gl_handle = real_dlopen(DEFAULT_OPENGL, (RTLD_NOW|RTLD_LOCAL));
    assert("Loaded real OpenGL lib", !!gl_handle);
    printf("[libfake] Loaded opengl!\n");
  }
}

void* glXGetProcAddressARB (const char *name) {
#ifdef CAPSULE_LINUX
  ensure_real_dlopen();

  if (strcmp(name, "glXSwapBuffers") == 0) {
    printf("[libfake] In glXGetProcAddressARB: %s\n", name);
    printf("[libfake] Returning fake glXSwapBuffers\n");
    return &fake_glXSwapBuffers;
  }
#endif

  return dlsym(gl_handle, name);
}

#ifdef CAPSULE_LINUX
void* dlopen (const char * filename, int flag) {
  printf("[libfake] In dlopen(%s, %d)\n", filename, flag);
  ensure_real_dlopen();

  if (strstr(filename, "libGL.so")) {
    printf("[libfake] Faking libGL\n");
    return real_dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
  } else {
    return real_dlopen(filename, flag);
  }
}
#endif

#define GL_RGB 6407
#define GL_UNSIGNED_BYTE 5121

typedef void (*glReadPixelsType)(int, int, int, int, int, int, void*);
glReadPixelsType glReadPixels;

void captureFrame () {
  int width = FRAME_WIDTH;
  int height = FRAME_HEIGHT;
  int components = 3;
  int format = GL_RGB;

  size_t frameDataSize = FRAME_WIDTH * FRAME_HEIGHT * components;
  if (!frameData) {
    frameData = malloc(frameDataSize);
  }
  
  if (!glReadPixels) {
    glReadPixels = dlsym(gl_handle, "glReadPixels");
    assert("Got glReadPixels address", !!glReadPixels);
  }
  glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, frameData);
  
  char frameName[512];
  bzero(frameName, 512);
  sprintf(frameName, "frames/frame%03d.raw", frameNumber);
  frameNumber++;

  FILE *f = fopen(frameName, "wb");
  assert("Opened", !!f);

  fwrite(frameData, 1, frameDataSize, f);
  fclose(f);
  printf("[libfake] Saving %s\n", frameName);

  if (frameNumber > 60) {
    printf("Captured 60 frames, going down.\n");
    exit(0);
  }
}

#ifdef CAPSULE_LINUX
void fake_glXSwapBuffers (void *a, void *b) {
  printf("[libfake] In glXSwapBuffers\n");
  captureFrame();
  return _realglXSwapBuffers(a, b);
}

int glXQueryExtension (void *a, void *b, void *c) {
  printf("[libfake] in glXQueryExtension\n");
  return _realglXQueryExtension(a, b, c);
}
#endif

void __attribute__((constructor)) libfake_load() {
  printf("[libfake] Initializing...\n");

#ifdef CAPSULE_LINUX
  printf("[libfake] Loading real OpenGL lib...\n");
  ensure_real_dlopen();

  printf("[libfake] Getting glXQueryExtension adress\n");
  _realglXQueryExtension = dlsym(gl_handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);
  printf("[libfake] Got glXQueryExtension adress: %p\n", _realglXQueryExtension);

  _realglXSwapBuffers = dlsym(gl_handle, "glXSwapBuffers");
  assert("Got glXSwapBuffers", !!_realglXSwapBuffers);
  printf("[libfake] Got glXSwapBuffers adress: %p\n", _realglXSwapBuffers);
#endif

  printf("[libfake] All systems go.\n");
}

void __attribute__((destructor)) libfake_unload() {
  printf("[libfake] Winding down...\n");
}

