#pragma once

#include "audio_receiver.h"
#include "video_receiver.h"

#include <thread>

namespace capsule {

class Session {
  public: 
    Session(capsule_args_t *args, video::VideoReceiver *video, audio::AudioReceiver *audio) :
      args_(args),
      video_(video),
      audio_(audio) {};
    ~Session();
    void Start();
    void Stop();
    void Join();

    struct encoder_params_s encoder_params_;

    // these need to be public for the C callbacks (to avoid
    // another level of indirection)
    video::VideoReceiver *video_;
    audio::AudioReceiver *audio_;

  private:
    std::thread *encoder_thread_;
    capsule_args_t *args_;
};

} // namespace capsule
