
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

#include <capsule/messages_generated.h>
#include <capsule/audio_math.h>

#include "logging.h"

#include <string.h> // memset, memcpy

namespace capsule {
namespace audio {

AudioInterceptReceiver::AudioInterceptReceiver(Connection *conn, const messages::AudioSetup &as) {
  memset(&afmt_, 0, sizeof(afmt_));

  conn_ = conn;

  Log("AudioInterceptReceiver: initializing with %d channels, %d rate, sample format %s",
    as.channels(),
    as.rate(),
    messages::EnumNameSampleFmt(as.format())
  );

  afmt_.channels = as.channels();
  afmt_.rate = as.rate();
  afmt_.format = as.format();

  auto shm_path = as.shmem()->path()->str();  
  auto shm = new shoom::Shm(shm_path, static_cast<size_t>(as.shmem()->size()));
  int ret = shm->Open();
  if (ret != shoom::kOK) {
    Log("AudioInterceptReceiver: Could not open shared memory area: code %d", ret);
    return;
  }

  shm_ = shm;

  size_t sample_size = (SampleWidth(afmt_.format) / 8);
  frame_size_ = afmt_.channels * sample_size;

  int64_t buffered_seconds = 4;
  num_frames_ = afmt_.rate * buffered_seconds;

  buffer_ = (char*) calloc(num_frames_, frame_size_);

  initialized_ = true;
}

AudioInterceptReceiver::~AudioInterceptReceiver() {
  if (shm_) {
    delete shm_;
  }
}

int AudioInterceptReceiver::ReceiveFormat(encoder::AudioFormat *afmt) {
  // stub
  if (!initialized_) {
    return -1;
  }

  memcpy(afmt, &afmt_, sizeof(afmt_));
  return 0;
}

void AudioInterceptReceiver::FramesCommitted(int64_t offset, int64_t frames) {
  // Log("AudioInterceptReceiver: frames committed: %d offset, %d frames", offset, frames);
  std::lock_guard<std::mutex> lock(buffer_mutex_);

  size_t remain_frames = frames;
  while (remain_frames > 0) {
    size_t write_frames = remain_frames;
    size_t avail_frames = num_frames_ - commit_index_;

    if (avail_frames == 0) {
      // wrap around!
      Log("AudioInterceptReceiver: commit wrap!");
      commit_index_ = 0;
      avail_frames = num_frames_ - commit_index_;
    }

    if (avail_frames < write_frames) {
      write_frames = avail_frames;
    }

    // Log("AudioInterceptReceiver: storing %d frames at %d", write_frames, commit_index_);
    char *src = (char*) shm_->Data() + (offset * frame_size_);
    char *dst = buffer_ + (commit_index_ * frame_size_);
    memcpy(dst, src, write_frames * frame_size_);

    commit_index_ += write_frames;
    remain_frames -= write_frames;
    offset += write_frames;
  }
}

void *AudioInterceptReceiver::ReceiveFrames(int64_t *frames_received) {
  std::lock_guard<std::mutex> lock(buffer_mutex_);

  *frames_received = 0;

  if (sent_index_ == num_frames_) {
    // wrap!
    Log("AudioInterceptReceiver: sent wrap!");
    sent_index_ = 0;
  }

  if (sent_index_ != commit_index_) {
    int64_t avail_frames;
    if (commit_index_ < sent_index_) {
      // commit_index_ has wrapped around, send everything till the end of the buffer
      Log("AudioInterceptReceiver: sent clear!");
      avail_frames = num_frames_ - sent_index_;
    } else {
      avail_frames = commit_index_ - sent_index_;
    }

    *frames_received = avail_frames;
    Log("AudioInterceptReceiver: received %" PRId64 " frames", avail_frames);

    char *ret = buffer_ + (sent_index_ * frame_size_);
    sent_index_ += avail_frames;
    return ret;
  }

  return nullptr;
}

void AudioInterceptReceiver::Stop() {
  // stub
}

}
}
