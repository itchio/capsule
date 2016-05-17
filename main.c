
#include <stdio.h>
#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
  printf("[main] Calling SDL_Init\n");
  SDL_Init(SDL_INIT_VIDEO);
  printf("[main] Returned from SDL_Init\n");

  printf("[main] Asking for OpenGL 3.2 context\n");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  printf("[main] Sleeping for a second\n");
  SDL_Delay(1000);

  printf("[main] Quitting\n");
  SDL_Quit();

  return 0;
}
