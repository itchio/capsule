
#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>

void __attribute__((constructor)) libfake_load() {
    printf("[libfake] loading...\n");
    void *original_cos = dlsym(RTLD_NEXT, "cos");
    printf("[libfake] original cos = %p\n", original_cos);
}

void __attribute__((destructor)) libfake_unload() {
    printf("[libfake] unloading...\n");
}

double cos (double x) {
  return 0.69;
}
