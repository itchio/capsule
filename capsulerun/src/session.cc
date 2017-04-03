
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

#include "session.h"

#include "video_receiver.h"
#include "audio_receiver.h"
#include "encoder.h"
#include "logging.h"

namespace capsule {

static int ReceiveVideoFormat(Session *s, encoder::VideoFormat *vfmt) {
  return s->video_->ReceiveFormat(vfmt);
}

static int64_t ReceiveVideoFrame(Session *s, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return s->video_->ReceiveFrame(buffer, buffer_size, timestamp);
}

static int ReceiveAudioFormat(Session *s, encoder::AudioFormat *afmt) {
  return s->audio_->ReceiveFormat(afmt);
}

static void *ReceiveAudioFrames(Session *s, int64_t *frames_received) {
  return s->audio_->ReceiveFrames(frames_received);
}

void Session::Start () {
  memset(&encoder_params_, 0, sizeof(encoder_params_));
  encoder_params_.private_data = this;
  encoder_params_.receive_video_format = reinterpret_cast<encoder::VideoFormatReceiver>(ReceiveVideoFormat);
  encoder_params_.receive_video_frame  = reinterpret_cast<encoder::VideoFrameReceiver>(ReceiveVideoFrame);

  if (audio_) {
    encoder_params_.has_audio = 1;
    encoder_params_.receive_audio_format = reinterpret_cast<encoder::AudioFormatReceiver>(ReceiveAudioFormat);
    encoder_params_.receive_audio_frames = reinterpret_cast<encoder::AudioFramesReceiver>(ReceiveAudioFrames);
  } else {
    encoder_params_.has_audio = 0;  
  }

  encoder_thread_ = new std::thread(encoder::Run, args_, &encoder_params_);
}

void Session::Stop () {
  if (video_) {
    video_->Stop();
  }

  if (audio_) {
    audio_->Stop();
  }
}

void Session::Join () {
  Log("Waiting for encoder thread...");
  encoder_thread_->join();
}

Session::~Session () {
  if (video_) {
    delete video_;
  }
  if (audio_) {
    delete audio_;
  }
  delete encoder_thread_;
}

}
