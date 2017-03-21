
#include "fps_counter.h"

using namespace std;

FPSCounter::FPSCounter () {
  first_ts_ = chrono::steady_clock::now();
  last_ts_ = 0;
  for (int i = 0; i < NUM_FPS_SAMPLES; i++) {
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
  cur_sample_ = (cur_sample_ + 1) % NUM_FPS_SAMPLES;

  return (cur_sample_ == 0);
}

float FPSCounter::Fps () {
  float fps = 0.0f;
  for (int i = 0; i < NUM_FPS_SAMPLES; i++) {
    fps += samples_[i];
  }
  return fps / (float) NUM_FPS_SAMPLES;
}

int64_t FPSCounter::Timestamp () {
  auto frame_timestamp = chrono::steady_clock::now() - first_ts_;
  auto micro_timestamp = chrono::duration_cast<chrono::microseconds>(frame_timestamp);
  return (int64_t) micro_timestamp.count();
}

