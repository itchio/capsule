
#include <stdio.h>

static void con() __attribute__((constructor));
static void des() __attribute__((destructor));

void con() {
    printf("I'm a constructor\n");
}

void des() {
    printf("I'm a destructor\n");
}

double cos (double x) {
  return 0.69;
}
