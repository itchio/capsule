
#include <stdio.h>
#include <SDL2/SDL.h>

#define GLEW_STATIC
#include <GL/glew.h>

#ifdef _WIN32
#include "libfake.h"
#endif

#define SHADER_LEN 4096

static void assert (const char *msg, int cond) {
  if (cond) {
    return;
  }
  fprintf(stderr, "[main] Assertion failed: %s\n", msg);
  const char *err = SDL_GetError();
  fprintf(stderr, "[main] Last SDL GetError: %s\n", err);
  exit(1);
}

void readFile (char *target, const char *path) {
  FILE *f = fopen(path, "r");
  assert("Opened shader file successfully", !!f);

  size_t read = fread(target, 1, SHADER_LEN, f);
  assert("Read shader successfully", read > 0);

  fclose(f);
  return;
}

int main(int argc, char **argv) {
  fprintf(stderr, "[main] Calling SDL_Init\n");
  SDL_Init(SDL_INIT_VIDEO);
  fprintf(stderr, "[main] Returned from SDL_Init\n");

  fprintf(stderr, "[main] Asking for OpenGL 2.0 context\n");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_Window* window = SDL_CreateWindow("opengl-inject-poc", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
  assert("Window created correctly", !!window);

  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert("OpenGL context created correctly", !!context);

  fprintf(stderr, "[main] Initializing glew...\n");
  glewExperimental = GL_TRUE;
  glewInit();

  float vertices[] = {
    0.0f,  0.5f, // Vertex 1 (X, Y)
    0.5f, -0.5f, // Vertex 2 (X, Y)
    -0.5f, -0.5f  // Vertex 3 (X, Y)
  };

  fprintf(stderr, "[main] Making a vertex array object...\n");
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  fprintf(stderr, "[main] Making a vertex buffer...\n");
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  fprintf(stderr, "[main] Uploading vertex data to GPU...\n");
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  char* vertexSource = (char*) malloc(SHADER_LEN);
  memset(vertexSource, 0, SHADER_LEN);
  readFile(vertexSource, "shader.vert");
  /* fprintf(stderr, "vertex source: %s\n", vertexSource); */

  char* fragmentSource = (char*) malloc(SHADER_LEN);
  memset(fragmentSource, 0, SHADER_LEN);
  readFile(fragmentSource, "shader.frag");
  /* fprintf(stderr, "fragment source: %s\n", fragmentSource); */

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, (const GLchar* const*) &vertexSource, NULL);
  glCompileShader(vertexShader);

  GLint status;
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);

  if (status != GL_TRUE) {
    char buffer[512];
    glGetShaderInfoLog(vertexShader, 512, NULL, buffer);
    fprintf(stderr, "Vertex shader compile log: %s\n", buffer);

    assert("Vertex shader compiled", status == GL_TRUE);
  }

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, (const GLchar* const*) &fragmentSource, NULL);
  glCompileShader(fragmentShader);

  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);

  if (status != GL_TRUE) {
    char buffer[512];
    glGetShaderInfoLog(fragmentShader, 512, NULL, buffer);
    fprintf(stderr, "Fragment shader compile log: %s\n", buffer);

    assert("Fragment shader compiled", status == GL_TRUE);
  }

  GLuint shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);

  glBindFragDataLocation(shaderProgram, 0, "outColor");

  glLinkProgram(shaderProgram);

  glUseProgram(shaderProgram);

  GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
  glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

  glEnableVertexAttribArray(posAttrib);

  GLint uniColor = glGetUniformLocation(shaderProgram, "triangleColor");

  SDL_Event windowEvent;
  float x = 1.0;
  float dx = 0.02f;

#ifdef _WIN32
  libfake_hello();
#endif

  for (;;) {
    x += dx;
    if (x < 0.0f) {
      dx = -dx;
      x = 0.0f;
    } else if (x > 1.0f) {
      dx = -dx;
      x = 1.0f;
    }

    if (SDL_PollEvent(&windowEvent)) {
      if (windowEvent.type == SDL_QUIT) {
        fprintf(stderr, "[main] Window closed, quitting...\n");
        break;
      }
    }


    glClearColor(0.98f, 0.36f, 0.36f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindVertexArray(vao);

    glUniform3f(uniColor, 0.98f * x, 0.36f * x, 0.36f * x);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    SDL_GL_SwapWindow(window);

    SDL_Delay(16);
  }

  fprintf(stderr, "[main] Deleting OpenGL context\n");
  SDL_GL_DeleteContext(context);

  fprintf(stderr, "[main] Quitting\n");
  SDL_Quit();

  return 0;
}
