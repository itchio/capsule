
#pragma once

#include <stdint.h>
#include <chrono>

namespace capsule {

const static int kNumFpsSamples = 10;

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
    float samples_[kNumFpsSamples];
    int cur_sample_;

    std::chrono::time_point<std::chrono::steady_clock> first_ts_;
    int64_t last_ts_;
};

}
