
#include "../main_loop.h"

namespace capsule {
namespace hotkey {

void Init(MainLoop * /* unused */) {
  // nothing to do, hotkey is handled child-side on macOS
}

} // namespace hotkey
} // namespace capsule

