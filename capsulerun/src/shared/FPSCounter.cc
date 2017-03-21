
#include "FPSCounter.h"

using namespace std;

FPSCounter::FPSCounter () {
  first_ts = chrono::steady_clock::now();
  last_ts = 0;
  for (int i = 0; i < NUM_FPS_SAMPLES; i++) {
    samples[i] = 0.0f;
  }
  cur_sample = 0;
}

bool FPSCounter::tick () {
  auto ts = timestamp();
  auto delta = ts - last_ts;
  last_ts = ts;
  return tick_delta(delta);
}

bool FPSCounter::tick_delta (int64_t delta) {
  if (delta == 0) {
    // don't divide by 0
    delta = 1;
  }

  float fps = 1000000.0f / (float) delta;
  samples[cur_sample] = fps;
  cur_sample = (cur_sample + 1) % NUM_FPS_SAMPLES;

  return (cur_sample == 0);
}

float FPSCounter::fps () {
  float fps = 0.0f;
  for (int i = 0; i < NUM_FPS_SAMPLES; i++) {
    fps += samples[i];
  }
  return fps / (float) NUM_FPS_SAMPLES;
}

int64_t FPSCounter::timestamp () {
  auto frame_timestamp = chrono::steady_clock::now() - first_ts;
  auto micro_timestamp = chrono::duration_cast<chrono::microseconds>(frame_timestamp);
  return (int64_t) micro_timestamp.count();
}

