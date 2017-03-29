
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

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
