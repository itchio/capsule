#pragma once

#include "../shared/AudioReceiver.h"

#include "pulse-dynamic.h"

#include <thread>
#include <mutex>

namespace capsule {
namespace audio {

const int kAudioNbBuffers = 16;
const int kAudioNbSamples = 128;

// TODO: that's amore^Wan enum
const int kBufferStateProcessed = 0;
const int kBufferStateCommitted = 1;
const int kBufferStateProcessing = 2;

class PulseReceiver : public AudioReceiver {
  public:
    PulseReceiver();
    virtual ~PulseReceiver();

    virtual int ReceiveFormat(audio_format_t *afmt);
    virtual void *ReceiveFrames(int *frames_received);
    virtual void Stop();

    bool ReadFromPa();

  private:
    audio_format_t afmt_;
    pa_simple *ctx_;

    uint8_t *in_buffer_;

    uint8_t *buffers_;
    size_t buffer_size_;
    int buffer_state_[kAudioNbBuffers];
    int commit_index_ = 0;
    int process_index_ = 0;

    std::thread *pa_thread_;
    std::mutex buffer_mutex_;
    std::mutex pa_mutex_;

    bool overrun_ = false;
};

}
}
