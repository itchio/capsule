#pragma once

#include <capsulerun.h>

namespace capsule {
namespace audio {

class AudioReceiver {
  public:
    virtual ~AudioReceiver() {};

    virtual int ReceiveFormat(audio_format_t *afmt) = 0;
    virtual void *ReceiveFrames(int *frames_received) = 0;
    virtual void Stop() = 0;
};

} // namespace audio
} // namespace capsule
