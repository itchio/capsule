#pragma once

#include "../shared/AudioReceiver.h"

#include "pulse-dynamic.h"

#include <thread>
#include <mutex>

#define AUDIO_NB_BUFFERS 16
#define AUDIO_NB_SAMPLES 128

#define AUDIO_BUFFER_PROCESSED  0
#define AUDIO_BUFFER_COMMITTED  1
#define AUDIO_BUFFER_PROCESSING 2

class PulseReceiver : public AudioReceiver {
  public:
    PulseReceiver();
    virtual ~PulseReceiver();

    virtual int receive_format(audio_format_t *afmt);
    virtual void *receive_frames(int *frames_received);
    virtual void stop();

    bool read_from_pa();

  private:
    audio_format_t afmt;
    pa_simple *ctx;

    uint8_t *in_buffer;

    uint8_t *buffers;
    size_t buffer_size;
    int buffer_state[AUDIO_NB_BUFFERS];
    int commit_index = 0;
    int process_index = 0;

    std::thread *pa_thread;    
    std::mutex buffer_mutex;
    std::mutex pa_mutex;

    bool overrun = false;
};
