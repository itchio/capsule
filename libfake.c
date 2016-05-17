
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

typedef int (*SDL_Init_Func)(uint32_t);
static SDL_Init_Func Original_SDL_Init;

typedef void (*SDL_GL_SwapWindow_Func)(void *);
static SDL_GL_SwapWindow_Func Original_SDL_GL_SwapWindow;

static void assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  fprintf(stderr, "[main] Assertion failed: %s\n", msg);
  const char *err = SDL_GetError();
  fprintf(stderr, "[main] Last SDL GetError: %s\n", err);
  exit(1);
}

int SDL_Init (uint32_t flags) {
  printf("[libfake] in SDL_Init\n");
  if (!Original_SDL_Init) {
    void *handle = dlopen(LIBSDL2_FILENAME, RTLD_LOCAL | RTLD_NOW);

    Original_SDL_Init = dlsym(handle, "SDL_Init");
    printf("[libfake] org sdl init = %p\n", Original_SDL_Init);

    void *our_sdl_init = dlsym(RTLD_DEFAULT, "SDL_Init");
    printf("[libfake] dls sdl init = %p\n", our_sdl_init);
    printf("[libfake] ptr sdl init = %p\n", &SDL_Init);

    dlclose(handle);

    if (Original_SDL_Init == &SDL_Init) {
      printf("[libfake] can't find original SDL init! bailing out\n");
      exit(1);
    }
  }

  printf("[libfake] Calling original SDL_Init with %x\n", flags);
  int retval = Original_SDL_Init(flags);
  printf("[libfake] Original SDL_Init return value: %d\n", retval);
  return retval;
}

/* void SDL_GL_SwapWindow (void* win) { */
/*   printf("[libfake] In SDL_GL_SwapWindow\n"); */
/*   if (!Original_SDL_GL_SwapWindow) { */
/*     void *handle = dlopen(LIBSDL2_FILENAME, RTLD_LOCAL | RTLD_NOW); */

/*     Original_SDL_GL_SwapWindow = dlsym(handle, "SDL_GL_SwapWindow"); */
/*     printf("[libfake] org sdl init = %p\n", Original_SDL_GL_SwapWindow); */

/*     dlclose(handle); */
/*   } */

/*   printf("[libfake] Calling original SDL_GL_SwapWindow\n"); */
/*   Original_SDL_GL_SwapWindow(win); */
/*   printf("[libfake] Original SDL_GL_SwapWindow returend.\n"); */
/* } */

void glFinish () {
  printf("[libfake] In glFinish\n");
}

typedef int (*glXQueryExtensionType)(void*, void*, void*);
glXQueryExtensionType _realglXQueryExtension;

int glXQueryExtension (void *d, int *a, int *b) {
  printf("[libfake] In glXQueryExtension with %p, %p, %p\n", d, a, b);
  printf("[libfake] In glXQueryExtension with %d, %d\n", *a, *b);
  int retval = _realglXQueryExtension(d, a, b);
  printf("[libfake] retval: %d\n", retval);
  return retval;
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

void glXSwapBuffers () {
  printf("[libfake] In glXSwapBuffers\n");
}

void glXGetProcAddress () {
  printf("[libfake] In glXGetProcAddress\n");
}

void __attribute__((constructor)) libfake_load() {
  printf("[libfake] Initializing...\n");

  printf("[libfake] Loading real OpenGL lib...\n");
  void *handle = dlopen("libGL.so", (RTLD_NOW|RTLD_GLOBAL));
  assert("Loaded real OpenGL lib", !!handle);

  printf("[libfake] Getting glXQueryExtension adress\n");
  _realglXQueryExtension = dlsym(handle, "glXQueryExtension");
  assert("Got glXQueryExtension", !!_realglXQueryExtension);
  printf("[libfake] glXQueryExtension their address: %p\n", _realglXQueryExtension);
  printf("[libfake] glXQueryExtension ourss address: %p\n", &glXQueryExtension);

  // dlclose(handle);

  printf("[libfake] All systems go.\n");
}

void __attribute__((destructor)) libfake_unload() {
  printf("[libfake] Winding down...\n");
}

