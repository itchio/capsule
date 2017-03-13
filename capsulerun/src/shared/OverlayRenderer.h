#pragma once

#include <capsulerun_args.h>
#include <cairo.h>

class OverlayRenderer {
  public:
    OverlayRenderer(capsule_args_t *args_in);
    ~OverlayRenderer() {};

  private:
    capsule_args_t *args;
};
