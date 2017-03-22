
#pragma once

namespace capsule {

enum PixFmt {
  kPixFmtUnknown = 0,
  kPixFmtRgba = 40069,    // R8,  G8,  B8,  A8
  kPixFmtBgra = 40070,    // B8,  G8,  R8,  A8
  kPixFmtRgb10A2 = 40071, // R10, G10, B10, A2

  kPixFmtYuv444P = 60021, // planar Y4 U4 B4
};

// numbers of buffers used for async GPU download
static const int kNumBuffers = 3;

} // namespace capsule

#include "opengl-constants.h"
