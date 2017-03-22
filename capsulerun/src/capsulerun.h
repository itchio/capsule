
#pragma once

#include <capsule/constants.h>
#include "macros.h"
#include "args.h"

namespace capsule {

int Main(MainArgs *args);

#if defined(LAB_MACOS)
void RunApp();
#endif // LAB_MACOS

} // namespace capsule
