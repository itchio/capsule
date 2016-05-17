
#include <stdio.h>
#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
  printf("[main] Calling SDL_Init\n");
  SDL_Init(SDL_INIT_EVERYTHING);
  printf("[main] Returned from SDL_Init\n");

  SDL_Delay(1000);

  SDL_Quit();
  return 0;
}
