
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

#include "logging.h"

#include <string.h> // memset, memcpy

namespace capsule {
namespace audio {

AudioInterceptReceiver::AudioInterceptReceiver(Connection *conn, const messages::AudioSetup &as) {
  memset(&afmt_, 0, sizeof(afmt_));

  conn_ = conn;

  Log("AudioInterceptReceiver: initializing with %d channels, %d rate, sample format %d",
    as.channels(),
    as.rate(),
    as.sample_fmt()
  );

  if (as.sample_fmt() != messages::SampleFmt_F32LE) {
    Log("AudioInterceptReceiver: unsupported sample format, bailing out");
  }

  afmt_.channels = as.channels();
  afmt_.samplerate = as.rate();
  afmt_.samplewidth = 32; // assuming F32LE

  auto shm_path = as.shmem()->path()->str();  
  auto shm = new shoom::Shm(shm_path, static_cast<size_t>(as.shmem()->size()));
  int ret = shm->Open();
  if (ret != shoom::kOK) {
    Log("AudioInterceptReceiver: Could not open shared memory area: code %d", ret);
    return;
  }

  shm_ = shm;

  size_t sample_size = (afmt_.samplewidth / 8);
  frame_size_ = afmt_.channels * sample_size;

  int64_t buffered_seconds = 4;
  num_frames_ = afmt_.samplerate * buffered_seconds;

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
  Log("AudioInterceptReceiver: frames committed: %d offset, %d frames", offset, frames);

  size_t avail_frames = num_frames_ - commit_index_;
  if (avail_frames > 0 && (size_t) frames <= avail_frames) {
    Log("AudioInterceptReceiver: storing %d frames (shm size = %" PRIdS ")", frames, shm_->Size());
    char *src = (char*) shm_->Data() + (offset * frame_size_);
    char *dst = buffer_ + (commit_index_ * frame_size_);
    memcpy(dst, src, frames * frame_size_);

    commit_index_ += frames;
  } else {
    Log("AudioInterceptReceiver: no space for frames");
  }
}

void *AudioInterceptReceiver::ReceiveFrames(int *frames_received) {
  // stub
  *frames_received = 0;

  if (sent_index_ < commit_index_) {
    size_t avail_frames = commit_index_ - sent_index_;
    *frames_received = avail_frames;
    Log("AudioInterceptReceiver: receiving %" PRIdS " frames", avail_frames);

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
