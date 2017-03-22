#pragma once

#include "../audio_receiver.h"
#include "../encoder.h"

// IMMDeviceEnumerator, IMMDevice
#include <mmDeviceapi.h>
// IAudioClient, IAudioCaptureClient
#include <audioclient.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <mutex>

namespace capsule {
namespace audio {

// REFERENCE_TIME time units per second and per millisecond
static const long long kReftimesPerSec = 10000000LL;
static const long long kReftimesPerMillisec = 10000LL;

class WasapiReceiver : public AudioReceiver {
  public:
    WasapiReceiver();
    virtual ~WasapiReceiver() override;

    virtual int ReceiveFormat(encoder::AudioFormat *afmt) override;
    virtual void *ReceiveFrames(int *frames_received) override;
    virtual void Stop() override;

  private:
    encoder::AudioFormat afmt_;
    IMMDeviceEnumerator *enumerator_ = nullptr;
    IMMDevice *device_ = nullptr;
    IAudioClient *audio_client_ = nullptr;
    IAudioCaptureClient *capture_client_ = nullptr;
    WAVEFORMATEX *pwfx_ = nullptr;
    int num_frames_received_ = 0;

    bool stopped_ = false;    
    std::mutex stopped_mutex_;
};

} // namespace capsule
} // namespace audio
