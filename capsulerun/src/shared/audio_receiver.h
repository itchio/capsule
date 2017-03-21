#pragma once

#include <capsulerun.h>

class AudioReceiver {
  public:
    virtual ~AudioReceiver() {};

    virtual int ReceiveFormat(audio_format_t *afmt) = 0;
    virtual void *ReceiveFrames(int *frames_received) = 0;
    virtual void Stop() = 0;
};
