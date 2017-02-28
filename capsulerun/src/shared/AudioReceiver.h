#pragma once

#include <capsulerun.h>

class AudioReceiver {
  public:
    virtual ~AudioReceiver();

    virtual int receiveAudioFormat(audio_format_t *afmt);
    virtual void *receiveAudioFrames(int *num_frames);
    virtual void stop();
};
