
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
