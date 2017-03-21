#pragma once

#include "audio_receiver.h"
#include "video_receiver.h"

#include <thread>

class Session {
  public: 
    Session(capsule_args_t *args, VideoReceiver *video, AudioReceiver *audio) :
      args(args),
      video(video),
      audio(audio) {};
    ~Session();
    void Start();
    void Stop();
    void Join();

    struct encoder_params_s encoder_params;

    // these need to be public for the C callbacks (to avoid
    // another level of indirection)
    VideoReceiver *video;
    AudioReceiver *audio;

  private:
    std::thread *encoder_thread;
    capsule_args_t *args;
};