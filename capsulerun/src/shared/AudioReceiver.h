#pragma once

#include <capsulerun.h>

class AudioReceiver {
  public:
    virtual ~AudioReceiver() {};

    virtual int receiveFormat(audio_format_t *afmt) = 0;
    virtual void *receiveFrames(int *frames_received) = 0;
    virtual void stop() = 0;
};
