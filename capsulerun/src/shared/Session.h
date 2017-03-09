#pragma once

#include "AudioReceiver.h"
#include "VideoReceiver.h"
#include "OverlayRenderer.h"

#include <thread>

class Session {
  public: 
    Session(capsule_args_t *args, VideoReceiver *video, AudioReceiver *audio) :
      args(args),
      video(video),
      audio(audio) {};
    ~Session();
    void start();
    void stop();
    void join();

    struct encoder_params_s encoder_params;

    // these need to be public for the C callbacks (to avoid
    // another level of indirection)
    VideoReceiver *video;
    AudioReceiver *audio;

  private:
    std::thread *encoder_thread;
    capsule_args_t *args;
    OverlayRenderer *overlay_renderer;
};