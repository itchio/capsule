
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

#include "../audio_receiver.h"

#include "pulse_dynamic.h"

#include <thread>
#include <mutex>

#include "../encoder.h"

namespace capsule {
namespace audio {

static const int kAudioNbBuffers = 64;
static const int kAudioNbSamples = 128;

enum BufferState {
  kBufferStateAvailable = 0,
  kBufferStateCommitted,
  kBufferStateProcessing,
};

class PulseReceiver : public AudioReceiver {
  public:
    PulseReceiver();
    virtual ~PulseReceiver() override;

    virtual int ReceiveFormat(encoder::AudioFormat *afmt) override;
    virtual void *ReceiveFrames(int *frames_received) override;
    virtual void Stop() override;

  private:
    void ReadLoop();
    bool ReadFromPa();

    encoder::AudioFormat afmt_;
    pa_simple *ctx_;

    uint8_t *in_buffer_;

    uint8_t *buffers_;
    size_t buffer_size_;
    int buffer_state_[kAudioNbBuffers];
    int commit_index_ = 0;
    int process_index_ = 0;

    std::thread *pa_thread_;
    std::mutex buffer_mutex_;
    std::mutex pa_mutex_;

    bool overrun_ = false;
};

} // namespace audio
} // namespace capsule
