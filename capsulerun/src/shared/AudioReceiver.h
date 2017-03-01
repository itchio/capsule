#pragma once

#include <capsulerun.h>

class AudioReceiver {
  public:
    virtual ~AudioReceiver() {};

    virtual int receive_format(audio_format_t *afmt) = 0;
    virtual void *receive_frames(int *frames_received) = 0;
    virtual void stop() = 0;
};
