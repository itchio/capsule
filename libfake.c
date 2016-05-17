
#define _GNU_SOURCE

#if defined(_WIN32)
#define LIBSDL2_FILENAME "SDL2.dll"
#elif defined(__APPLE__)
#define LIBSDL2_FILENAME "libSDL2.dylib"
#elif defined(__linux__) or defined(__unix__)
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

void SDL_GL_SwapWindow (void* win) {
  printf("[libfake] In SDL_GL_SwapWindow\n");
  if (!Original_SDL_GL_SwapWindow) {
    void *handle = dlopen(LIBSDL2_FILENAME, RTLD_LOCAL | RTLD_NOW);

    Original_SDL_GL_SwapWindow = dlsym(handle, "SDL_GL_SwapWindow");
    printf("[libfake] org sdl init = %p\n", Original_SDL_GL_SwapWindow);

    dlclose(handle);
  }

  printf("[libfake] Calling original SDL_GL_SwapWindow\n");
  Original_SDL_GL_SwapWindow(win);
  printf("[libfake] Original SDL_GL_SwapWindow returend.\n");
}

void __attribute__((constructor)) libfake_load() {
  printf("[libfake] All systems go.\n");
}

void __attribute__((destructor)) libfake_unload() {
  printf("[libfake] Winding down...\n");
}

