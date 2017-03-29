
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

#include "audio_intercept_receiver.h"

#include "logging.h"

namespace capsule {
namespace audio {

AudioInterceptReceiver::AudioInterceptReceiver(Connection *conn, const messages::AudioSetup &as) {
  conn_ = conn;

  Log("Got audio intercept! %d channels, %d rate, sample format %d",
    as.channels(),
    as.rate(),
    as.sample_fmt()
  );
  Log("shm area is %d bytes at %s",
    as.shmem()->size(),
    as.shmem()->path()->c_str()
  );
}

AudioInterceptReceiver::~AudioInterceptReceiver() {
  // stub
}

int AudioInterceptReceiver::ReceiveFormat(encoder::AudioFormat *afmt) {
  // stub
  afmt->channels = 0;
  return -1;
}

void *AudioInterceptReceiver::ReceiveFrames(int *frames_received) {
  // stub
  *frames_received = 0;
  return nullptr;
}

void AudioInterceptReceiver::FramesCommitted(int64_t offset, int64_t frames) {
  Log("frames committed: %d offset, %d frames", offset, frames);
}

void AudioInterceptReceiver::Stop() {
  // stub
}

}
}
