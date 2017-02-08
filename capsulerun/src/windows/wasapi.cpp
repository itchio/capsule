
#include <stdio.h>
#include <stdlib.h>

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

#include "capsulerun.h"

using namespace std;

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

void wasapi_mess_around () {
  HRESULT hr;

  IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;

  CoInitialize(NULL);

  hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
  if (hr != S_OK) {
    printf("Could not create device enumerator instance\n");
    exit(1);
  }

  hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
  if (hr != S_OK) {
    printf("Could not get default audio endpoint\n");
    exit(1);
  }

  IPropertyStore *pProps = NULL;
  hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
  if (hr != S_OK) {
    printf("Could not get open endpoint property store\n");
    exit(1);
  }

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

  hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
  if (hr != S_OK) {
    printf("Could not activate audio device");
    exit(1);
  }

  WAVEFORMATEX *pwfx = NULL;
  hr = pAudioClient->GetMixFormat(&pwfx);
  if (hr != S_OK) {
    printf("Could not get mix format");
    exit(1);
  }

  printf("======================\n");
  printf("Audio format:\n");
  printf("Channels: %d\n", pwfx->nChannels);
  printf("Samples per sec: %d\n", pwfx->nSamplesPerSec);
  printf("Bits per sample: %d\n", pwfx->wBitsPerSample);
  printf("======================\n");

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

  BYTE *pData;
  UINT32 numFramesAvailable;
  DWORD flags;

  FILE *output_file = fopen("audio.raw", "wb");

  size_t bytesPerFrame = pwfx->wBitsPerSample / 8 * pwfx->nChannels;

  int rounds = 20;

  while (rounds > 0) {
    Sleep(200);

    while (true) {
      hr = pCaptureClient->GetBuffer(
        &pData,
        &numFramesAvailable,
        &flags,
        NULL,
        NULL
      );
      if (hr != S_OK) {
        if (hr == AUDCLNT_S_BUFFER_EMPTY) {
          // good!
          break;
        }

        printf("Could not get buffer: error %d (%x)", hr, hr);
        exit(1);
      }

      // printf("Got %d audio frames\n", numFramesAvailable);

      // TODO: this is wrong
      size_t bufferSize = numFramesAvailable * bytesPerFrame;
      fwrite(pData, 1, bufferSize, output_file);

      pCaptureClient->ReleaseBuffer(numFramesAvailable);
    }

    rounds--;

    // printf("Going around!\n");
  }

  hr = pAudioClient->Stop();
  if (hr != S_OK) {
    printf("Could not stop capture");
    exit(1);
  }

  fclose(output_file);

  printf("wasapi mess around: done!\n");
}
