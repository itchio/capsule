
#include "session.h"

#include "video_receiver.h"
#include "audio_receiver.h"
#include "encoder.h"

#include <capsulerun.h>

namespace capsule {

static int ReceiveVideoFormat(Session *s, encoder::VideoFormat *vfmt) {
  return s->video_->ReceiveFormat(vfmt);
}

static int ReceiveVideoFrame(Session *s, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return s->video_->ReceiveFrame(buffer, buffer_size, timestamp);
}

static int ReceiveAudioFormat(Session *s, encoder::AudioFormat *afmt) {
  return s->audio_->ReceiveFormat(afmt);
}

static void *ReceiveAudioFrames(Session *s, int *frames_received) {
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
  CapsuleLog("Waiting for encoder thread...");
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
