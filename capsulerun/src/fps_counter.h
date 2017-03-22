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

    bool TickDelta(int64_t delta);
    bool Tick();
    float Fps();

  private:
    int64_t Timestamp();
    float samples_[NUM_FPS_SAMPLES];
    int cur_sample_;

    std::chrono::time_point<std::chrono::steady_clock> first_ts_;
    int64_t last_ts_;
};
