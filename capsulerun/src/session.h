#pragma once

#include "audio_receiver.h"
#include "video_receiver.h"

#include <thread>

namespace capsule {

class Session {
  public: 
    Session(MainArgs *args, video::VideoReceiver *video, audio::AudioReceiver *audio) :
      args_(args),
      video_(video),
      audio_(audio) {};
    ~Session();
    void Start();
    void Stop();
    void Join();

    encoder::Params encoder_params_;

  private:
    std::thread *encoder_thread_;
    MainArgs *args_;

  public:
    // these need to be public for the C callbacks (to avoid
    // another level of indirection)
    video::VideoReceiver *video_;
    audio::AudioReceiver *audio_;
};

} // namespace capsule
