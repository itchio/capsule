
#include <stdio.h>
#include <SDL2/SDL.h>

#define GLEW_STATIC
#include <GL/glew.h>

void assert (const char *msg, void *cond) {
  if (cond) {
    return;
  }
  fprintf(stderr, "[main] Assertion failed: %s\n", msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  printf("[main] Calling SDL_Init\n");
  SDL_Init(SDL_INIT_VIDEO);
  printf("[main] Returned from SDL_Init\n");

  printf("[main] Asking for OpenGL 3.2 context\n");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_Window* window = SDL_CreateWindow("opengl-inject-poc", 100, 100, 800, 600, SDL_WINDOW_OPENGL);
  assert("Window created correctly", window);

  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert("OpenGL context created correctly", context);

  printf("[main] Initializing glew...\n");
  glewExperimental = GL_TRUE;
  glewInit();

  printf("[main] Making a vertex buffer...\n");
  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  printf("[main] Vertex buffer: %u\n", vertexBuffer);

  printf("[main] Sleeping for a second\n");
  SDL_Delay(1000);

  printf("[main] Deleting OpenGL context\n");
  SDL_GL_DeleteContext(context);

  printf("[main] Quitting\n");
  SDL_Quit();

  return 0;
}
