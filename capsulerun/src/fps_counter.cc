
#include "fps_counter.h"

namespace capsule {

FPSCounter::FPSCounter () {
  first_ts_ = std::chrono::steady_clock::now();
  last_ts_ = 0;
  for (int i = 0; i < kNumFpsSamples; i++) {
    samples_[i] = 0.0f;
  }
  cur_sample_ = 0;
}

bool FPSCounter::Tick () {
  auto ts = Timestamp();
  auto delta = ts - last_ts_;
  last_ts_ = ts;
  return TickDelta(delta);
}

bool FPSCounter::TickDelta (int64_t delta) {
  if (delta == 0) {
    // don't divide by 0
    delta = 1;
  }

  float fps = 1000000.0f / (float) delta;
  samples_[cur_sample_] = fps;
  cur_sample_ = (cur_sample_ + 1) % kNumFpsSamples;

  return (cur_sample_ == 0);
}

float FPSCounter::Fps () {
  float fps = 0.0f;
  for (int i = 0; i < kNumFpsSamples; i++) {
    fps += samples_[i];
  }
  return fps / (float) kNumFpsSamples;
}

int64_t FPSCounter::Timestamp () {
  auto frame_timestamp = std::chrono::steady_clock::now() - first_ts_;
  auto micro_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(frame_timestamp);
  return (int64_t) micro_timestamp.count();
}

} // namespace capsule
