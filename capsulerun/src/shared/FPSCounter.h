#pragma once

#include <stdint.h>
#include <chrono>

#define NUM_FPS_SAMPLES 10

/**
 * Computes a sliding mean of FPS, with microsecond timestamps.
 */
class FPSCounter {
  public:
    FPSCounter();

    bool tick_delta(int64_t delta);
    bool tick();
    float fps();

  private:
    int64_t timestamp();
    float samples[NUM_FPS_SAMPLES];
    int cur_sample;

    std::chrono::time_point<std::chrono::steady_clock> first_ts;
    int64_t last_ts;
};
