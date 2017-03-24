
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


#include "pulse_receiver.h"

#include "../macros.h"

#include <chrono>

#include <string.h> // memset, memcpy

namespace capsule {
namespace audio {

PulseReceiver::PulseReceiver() {
  memset(&afmt_, 0, sizeof(afmt_));

  if (!pulse::Load()) {
    CapsuleLog("PulseReceiver: could not load pulse library");
    return;
  }

  auto dev = pulse::GetDefaultSinkMonitor();
  if (!dev) {
    CapsuleLog("PulseReceiver: could not determine default sink");
    return;
  }

  CapsuleLog("PulseReceiver: will record sink %s", dev);

  static const pa_sample_spec ss = {
      .format = PA_SAMPLE_FLOAT32LE,
      .rate = 44100,
      .channels = 2
  };

  int pa_err = 0;
  ctx_ = pulse::SimpleNew(NULL,             // server
                          "capsule",        // name
                          PA_STREAM_RECORD, // direction
                          dev,              // device
                          "record",         // stream name
                          &ss,              // sample spec
                          NULL,             // channel map
                          NULL,             // buffer attributes
                          &pa_err           // error
                          );

  free(dev);

  if (!ctx_) {
    CapsuleLog(
        "PulseReceiver: Could not create pulseaudio context, error %d (%x)",
        pa_err, pa_err);
    return;
  }

  afmt_.channels = ss.channels;
  afmt_.samplerate = ss.rate;
  afmt_.samplewidth = 32;
  buffer_size_ = kAudioNbSamples * afmt_.channels * (afmt_.samplewidth / 8);
  in_buffer_ = reinterpret_cast<uint8_t *>(calloc(1, buffer_size_));
  buffers_ = reinterpret_cast<uint8_t *>(calloc(kAudioNbBuffers, buffer_size_));

  for (int i = 0; i < kAudioNbBuffers; i++) {
    buffer_state_[i] = kBufferStateAvailable;
  }

  commit_index_ = 0;
  process_index_ = 0;

  // start reading
  pa_thread_ = new std::thread(&PulseReceiver::ReadLoop, this);
}

void PulseReceiver::ReadLoop() {
  while (true) {
    if (!ReadFromPa()) {
      return;
    }
  }
}

bool PulseReceiver::ReadFromPa() {
  {
    std::lock_guard<std::mutex> lock(pa_mutex_);

    if (!ctx_) {
      // we're closed!
      return false;
    }

    int pa_err;
    int ret = pulse::SimpleRead(ctx_, in_buffer_, buffer_size_, &pa_err);
    if (ret < 0) {
      fprintf(stderr, "Could not read from pulseaudio, error %d (%x)\n", pa_err,
              pa_err);
      return false;
    }
  }

  // cool, so we got a buffer, now let's find room for it.
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (buffer_state_[commit_index_] != kBufferStateAvailable) {
      // no room for it, just skip those then
      if (!overrun_) {
        overrun_ = true;
        CapsuleLog("PulseReceiver: buffer overrun (captured audio but "
                   "nowhere to place it)\n");
      }
      // keep trying tho
      return true;
    }

    overrun_ = false;

    uint8_t *target = buffers_ + (commit_index_ * buffer_size_);
    memcpy(target, in_buffer_, buffer_size_);
    buffer_state_[commit_index_] = kBufferStateCommitted;
    commit_index_ = (commit_index_ + 1) % kAudioNbBuffers;
  }

  return true;
}

int PulseReceiver::ReceiveFormat(encoder::AudioFormat *afmt) {
  memcpy(afmt, &afmt_, sizeof(*afmt));
  return 0;
}

void *PulseReceiver::ReceiveFrames(int *frames_received) {
  std::lock_guard<std::mutex> lock(buffer_mutex_);

  if (buffer_state_[process_index_] != kBufferStateCommitted) {
    // nothing to receive
    *frames_received = 0;
    return NULL;
  } else {
    // ooh there's a buffer ready

    // if the previous one was processing, that means we've processed it.
    auto prev_process_index =
        (process_index_ == 0) ? (kAudioNbBuffers - 1) : (process_index_ - 1);
    if (buffer_state_[prev_process_index] == kBufferStateProcessing) {
      buffer_state_[prev_process_index] = kBufferStateAvailable;
    }

    uint8_t *source = buffers_ + (process_index_ * buffer_size_);
    *frames_received = kAudioNbSamples;
    buffer_state_[process_index_] = kBufferStateProcessing;
    process_index_ = (process_index_ + 1) % kAudioNbBuffers;
    return source;
  }
}

void PulseReceiver::Stop() {
  {
    std::lock_guard<std::mutex> lock(pa_mutex_);
    if (ctx_) {
      pulse::SimpleFree(ctx_);
    }
    ctx_ = nullptr;
  }

  pa_thread_->join();
}

PulseReceiver::~PulseReceiver() {
  if (in_buffer_) {
    free(in_buffer_);
  }
  if (buffers_) {
    free(buffers_);
  }
}

} // namespace audio
} // namespace capsule
