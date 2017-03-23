
#include "../capsule.h"
#include "win_capture.h"

namespace capsule {
namespace d3d10 {

// inspired by libobs
struct State {
};

static struct State state = {};

void Capture (void *swap_ptr, void *backbuffer_ptr) {
  Log("d3d10::Capture: stub!");
}

void Free () {
  Log("d3d10::Free: stub!");
}

} // namespace capsule
} // namespace d3d10
