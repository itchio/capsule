
//////////////////////
// <WASAPI>
//////////////////////

// IMMDeviceEnumerator, IMMDevice
#include <mmDeviceapi.h>
// IAudioClient, IAudioCaptureClient
#include <audioclient.h>
// PKEY_Device_FriendlyName
#include <Functiondiscoverykeys_devpkey.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

//////////////////////
// </WASAPI>
//////////////////////

#include "../src/shared/io.h"

typedef struct encoder_private_s {
  capsule_io_t *io;

  IAudioClient *pAudioClient;
  IAudioCaptureClient *pCaptureClient;
  WAVEFORMATEX *pwfx;
  int num_frames_received;
} encoder_private_t;

void wasapi_start (encoder_private_t *p);
int wasapi_receive_audio_format (encoder_private_t *p, audio_format_t *afmt);
void *wasapi_receive_audio_frames (encoder_private_t *p, int *num_frames);

int capsule_hotkey_init(encoder_private_t *p);
