#pragma once

#include "../shared/AudioReceiver.h"

// IAudioClient, IAudioCaptureClient
#include <audioclient.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#include <mutex>

class WasapiReceiver : public AudioReceiver {
  public:
    WasapiReceiver();
    virtual ~WasapiReceiver();

    virtual int receive_format(audio_format_t *afmt);
    virtual void *receive_frames(int *frames_received);
    virtual void stop();

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
