
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

#include "encoder.h"

namespace capsule {
namespace audio {

class AudioReceiver {
  public:
    virtual ~AudioReceiver() {};

    virtual int ReceiveFormat(encoder::AudioFormat *afmt) = 0;
    virtual void *ReceiveFrames(int64_t *frames_received) = 0;
    virtual void FramesCommitted(int64_t, int64_t) {
      // muffin
    };
    virtual void Stop() = 0;
};

} // namespace audio
} // namespace capsule
