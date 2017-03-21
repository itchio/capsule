#pragma once

#include "../shared/AudioReceiver.h"

// IMMDeviceEnumerator, IMMDevice
#include <mmDeviceapi.h>
// IAudioClient, IAudioCaptureClient
#include <audioclient.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <mutex>

class WasapiReceiver : public AudioReceiver {
  public:
    WasapiReceiver();
    virtual ~WasapiReceiver();

    virtual int ReceiveFormat(audio_format_t *afmt);
    virtual void *ReceiveFrames(int *frames_received);
    virtual void Stop();

  private:
    audio_format_t afmt;
    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioClient *audio_client = nullptr;
    IAudioCaptureClient *capture_client = nullptr;
    WAVEFORMATEX *pwfx = nullptr;
    int num_frames_received = 0;

    bool stopped = false;    
    std::mutex stopped_mutex;
};
