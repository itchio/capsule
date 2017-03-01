
#include "Session.h"

#include "VideoReceiver.h"
#include "AudioReceiver.h"

#include <capsulerun.h>

using namespace std;

int receive_video_format (Session *s, video_format_t *vfmt) {
  return s->video->receiveFormat(vfmt);
}

int receive_video_frame (Session *s, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return s->video->receiveFrame(buffer, buffer_size, timestamp);
}

void Session::start () {

  memset(&encoder_params, 0, sizeof(encoder_params));
  encoder_params.private_data = this;
  encoder_params.receive_video_format = (receive_video_format_t) receive_video_format;
  encoder_params.receive_video_frame  = (receive_video_frame_t) receive_video_frame;

  // TODO: audio
  encoder_params.has_audio = 0;  

  encoder_thread = new thread(encoder_run, &encoder_params);
}

void Session::stop () {
  if (video) {
    video->stop();
  }

  if (audio) {
    audio->stop();
  }
}

void Session::join () {
  capsule_log("Waiting for encoder thread...");
  encoder_thread->join();
}

Session::~Session () {
  if (video) {
    delete video;
  }
  if (audio) {
    delete audio;
  }
  delete encoder_thread;
}
