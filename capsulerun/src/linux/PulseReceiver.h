#pragma once

#include "../shared/AudioReceiver.h"

// pulseaudio
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include <mutex>

class PulseReceiver : public AudioReceiver {
  public:
    PulseReceiver();
    virtual ~PulseReceiver();

    virtual int receiveFormat(audio_format_t *afmt);
    virtual void *receiveFrames(int *frames_received);
    virtual void stop();

  private:
    audio_format_t afmt;
    pa_simple *ctx;
    uint8_t *audio_buffer;
    size_t audio_buffer_size;

    std::mutex pa_mutex;
};
