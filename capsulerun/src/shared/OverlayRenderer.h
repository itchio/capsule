#pragma once

#include <string>

#include <cairo.h>
#include <shoom.h>

#include <capsulerun_args.h>

class OverlayRenderer {
  public:
    OverlayRenderer(capsule_args_t *args_in);
    ~OverlayRenderer() {};

  private:
    int width;
    int height;
    int components;
    capsule_args_t *args;

    shoom::Shm *shm;
    std::string shm_path;
    size_t shm_size;
};
