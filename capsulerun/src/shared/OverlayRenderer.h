#pragma once

#include <capsulerun_args.h>
#include <cairo.h>

#include <string>

#include <Shm.h>

class OverlayRenderer {
  public:
    OverlayRenderer(capsule_args_t *args_in);
    ~OverlayRenderer() {};

  private:
    int width;
    int height;
    int components;
    capsule_args_t *args;

    Shm *shm;
    std::string shm_path;
    size_t shm_size;
};
