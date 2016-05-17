
#define _GNU_SOURCE

#if defined(_WIN32)
#define LIBSDL2_FILENAME "SDL2.dll"
#elif defined(__APPLE__)
#define LIBSDL2_FILENAME "libSDL2.dylib"
#elif defined(__linux__) || defined(__unix__)
#define LIBSDL2_FILENAME "libSDL2.so"
#else
#error Unsupported platform
#endif


#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

extern const char *SDL_GetError();

static void assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  fprintf(stderr, "[main] Assertion failed: %s\n", msg);
  const char *err = SDL_GetError();
  fprintf(stderr, "[main] Last SDL GetError: %s\n", err);
  exit(1);
}

void glFinish () {
  printf("[libfake] In glFinish\n");
}

typedef int (*glXQueryExtensionType)(void*, void*, void*);
glXQueryExtensionType _realglXQueryExtension;

int glXQueryExtension (void *d, int *a, int *b) {
  printf("[libfake] in glXQueryExtension\n");
  return _realglXQueryExtension(d, a, b);
}

void glXChooseVisual () {
  printf("[libfake] In glXChooseVisual\n");
}

void glXCreateContext () {
  printf("[libfake] In glXCreateContext\n");
}

void glXDestroyContext () {
  printf("[libfake] In glXDestroyContext\n");
}

void glXMakeCurrent () {
  printf("[libfake] In glXMakeCurrent\n");
}

typedef void (*glXSwapBuffersType)(void*, void*);
glXSwapBuffersType _realglXSwapBuffers;

void glXSwapBuffers (void *a, void *b) {
  printf("[libfake] In glXSwapBuffers\n");
  return _realglXSwapBuffers(a, b);
}

void glXGetProcAddress () {
  printf("[libfake] In glXGetProcAddress\n");
}

typedef void* (*dlopen_type)(const char*, int);
dlopen_type real_dlopen;

void ensure_real_dlopen() {
  if (!real_dlopen) {
    printf("[libfake] Getting real dlopen\n");
    real_dlopen = dlsym(RTLD_NEXT, "dlopen");
    printf("[libfake] Real dlopen = %p\n", real_dlopen);
  }
}

void* dlopen (const char * filename, int flag) {
  printf("[libfake] In dlopen(%s, %d)\n", filename, flag);
  ensure_real_dlopen();

  if (!strcmp("libGL.so.1", filename)) {
    printf("[libfake] Loading OpenGL\n");
    return real_dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
  } else {
    return real_dlopen(filename, flag);
  }
}

void __attribute__((constructor)) libfake_load() {
  printf("[libfake] Initializing...\n");

  printf("[libfake] Loading real OpenGL lib...\n");
  ensure_real_dlopen();
  void *handle = real_dlopen("libGL.so.1", (RTLD_NOW|RTLD_GLOBAL));
  assert("Loaded real OpenGL lib", !!handle);

  printf("[libfake] Getting glXQueryExtension adress\n");
  _realglXQueryExtension = dlsym(handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);

  printf("[libfake] glXQueryExtension they address: %p\n", _realglXQueryExtension);
  printf("[libfake] glXQueryExtension ours address: %p\n", &glXQueryExtension);

  printf("[libfake] Getting glXSwapBuffers adress\n");
  _realglXSwapBuffers = dlsym(handle, "glXSwapBuffers");
  assert("Got glXSwapBuffers", !!_realglXSwapBuffers);

  printf("[libfake] glXSwapBuffers they address: %p\n", _realglXSwapBuffers);
  printf("[libfake] glXSwapBuffers ours address: %p\n", &glXSwapBuffers);

  // dlclose(handle);

  printf("[libfake] All systems go.\n");
}

void __attribute__((destructor)) libfake_unload() {
  printf("[libfake] Winding down...\n");
}

