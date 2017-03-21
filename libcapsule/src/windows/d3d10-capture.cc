
#include <capsule.h>
#include "win-capture.h"

// inspired by libobs
struct d3d10_data {
};

static struct d3d10_data data = {};

void d3d10_capture (void *swap_ptr, void *backbuffer_ptr) {
  CapsuleLog("d3d10_capture: stub!");
}

void d3d10_free () {
  CapsuleLog("d3d10_free: stub!");
}
