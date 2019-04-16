#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef int (*wglSwapBuffers_t)(void *arg1);

int main() {
    HANDLE lib = LoadLibraryA("opengl32.dll");
    wglSwapBuffers_t wglSwapBuffers_p = (void *) GetProcAddress(lib, "wglSwapBuffers");
    wglSwapBuffers_p((void *) 0xDEADBEEF);
    return 0;
}

