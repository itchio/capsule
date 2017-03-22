#pragma once

#include "../audio_receiver.h"

#include "pulse_dynamic.h"

#include <thread>
#include <mutex>

#include "../encoder.h"

namespace capsule {
namespace audio {

static const int kAudioNbBuffers = 16;
static const int kAudioNbSamples = 128;

enum BufferState {
  kBufferStateAvailable = 0,
  kBufferStateCommitted,
  kBufferStateProcessing,
};

class PulseReceiver : public AudioReceiver {
  public:
    PulseReceiver();
    virtual ~PulseReceiver() override;

    virtual int ReceiveFormat(encoder::AudioFormat *afmt) override;
    virtual void *ReceiveFrames(int *frames_received) override;
    virtual void Stop() override;

    bool ReadFromPa();

  private:
    encoder::AudioFormat afmt_;
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

} // namespace audio
} // namespace capsule
