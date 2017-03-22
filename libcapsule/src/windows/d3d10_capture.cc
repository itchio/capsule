
#include <capsule.h>
#include "win_capture.h"

// inspired by libobs
struct d3d10_data {
};

static struct d3d10_data data = {};

void D3d10Capture (void *swap_ptr, void *backbuffer_ptr) {
  CapsuleLog("d3d10_capture: stub!");
}

void D3d10Free () {
  CapsuleLog("d3d10_free: stub!");
}
