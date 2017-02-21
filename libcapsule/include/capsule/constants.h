#pragma once

#include "platform.h"

typedef enum capsule_pix_fmt_t {
  CAPSULE_PIX_FMT_RGBA        = 40069, // R8,  G8,  B8,  A8
  CAPSULE_PIX_FMT_BGRA        = 40070, // B8,  G8,  R8,  A8
  CAPSULE_PIX_FMT_RGB10_A2    = 40071, // R10, G10, B10, A2
  CAPSULE_PIX_FMT_UNKNOWN     = 49999,
} capsule_pix_fmt_t;

#include "opengl-constants.h"
