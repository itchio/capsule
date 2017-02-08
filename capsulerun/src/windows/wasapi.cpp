
#include <stdio.h>
#include <stdlib.h>

#include "capsulerun.h"

using namespace std;

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

void wasapi_start (encoder_private_t *p) {
  HRESULT hr;

  IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;

  CoInitialize(NULL);

  printf("Creating enumerator instance\n");

  hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
  if (hr != S_OK) {
    printf("Could not create device enumerator instance\n");
    exit(1);
  }

  printf("Getting default audio endpoint\n");

  hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
  if (hr != S_OK) {
    printf("Could not get default audio endpoint\n");
    exit(1);
  }

  printf("Opening property store\n");

  IPropertyStore *pProps = NULL;
  hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
  if (hr != S_OK) {
    printf("Could not get open endpoint property store\n");
    exit(1);
  }

  printf("Getting audio endpoint friendly name\n");

  PROPVARIANT varName;
  // Initialize container for property value.
  PropVariantInit(&varName);

  // Get the endpoint's friendly-name property.
  hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
  if (hr != S_OK) {
    printf("Could not get device friendly name\n");
    exit(1);
  }

  // Print endpoint friendly name and endpoint ID.
  printf("Endpoint friendly name: \"%S\"\n", varName.pwszVal);

  IAudioClient *pAudioClient = NULL;
  IAudioCaptureClient *pCaptureClient = NULL;

  printf("Activating device\n");

  hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
  if (hr != S_OK) {
    printf("Could not activate audio device");
    exit(1);
  }

  printf("Getting mix format\n");

  WAVEFORMATEX *pwfx = NULL;
  hr = pAudioClient->GetMixFormat(&pwfx);
  if (hr != S_OK) {
    printf("Could not get mix format");
    exit(1);
  }

  REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
  hr = pAudioClient->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK,
      hnsRequestedDuration,
      0,
      pwfx,
      NULL);
  if (hr != S_OK) {
    printf("Could not initialize audio client");
    exit(1);
  }

  UINT32 bufferFrameCount;

  // Get the size of the allocated buffer.
  hr = pAudioClient->GetBufferSize(&bufferFrameCount);
  if (hr != S_OK) {
    printf("Could not get buffer size");
    exit(1);
  }

  printf("Buffer frame count: %d\n", bufferFrameCount);

  hr = pAudioClient->GetService(
      IID_IAudioCaptureClient,
      (void **)&pCaptureClient);
  if (hr != S_OK) {
    printf("Could not get capture client");
    exit(1);
  }

  hr = pAudioClient->Start();
  if (hr != S_OK) {
    printf("Could not start capture");
    exit(1);
  }

  p->pAudioClient = pAudioClient;
  p->pCaptureClient = pCaptureClient;
  p->pwfx = pwfx;
}

int wasapi_receive_audio_format (encoder_private_t *p, audio_format_t *afmt) {
  afmt->channels = p->pwfx->nChannels;
  afmt->samplerate = p->pwfx->nSamplesPerSec;
  afmt->samplewidth = p->pwfx->wBitsPerSample;
  return 0;
}

void *wasapi_receive_audio_frames (encoder_private_t *p, int *num_frames) {
  BYTE *pData;
  DWORD flags;
  HRESULT hr;
  UINT32 numFramesAvailable;

  if (p->num_frames_received > 0) {
    hr = p->pCaptureClient->ReleaseBuffer(p->num_frames_received);
    if (hr != S_OK) {
      printf("Could not release buffer: error %d (%x)\n", hr, hr);
      exit(1);
    }
  }

  hr = p->pCaptureClient->GetBuffer(
    &pData,
    &numFramesAvailable,
    &flags,
    NULL,
    NULL
  );
  if (hr != S_OK) {
    if (hr == AUDCLNT_S_BUFFER_EMPTY) {
      // good!
      *num_frames = 0;
      p->num_frames_received = 0;
      return NULL;
    }

    printf("Could not get buffer: error %d (%x)\n", hr, hr);
    exit(1);
  }

  *num_frames = numFramesAvailable;
  p->num_frames_received = numFramesAvailable;

  return pData;
}

void wasapi_stop (encoder_private_t *p) {
  HRESULT hr;

  hr = p->pAudioClient->Stop();
  if (hr != S_OK) {
    printf("Could not stop capture");
    exit(1);
  }
}
