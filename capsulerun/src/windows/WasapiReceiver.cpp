
// this file is pretty much textbook msdn:
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd370800(v=vs.85).aspx 

#include "WasapiReceiver.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

// PKEY_Device_FriendlyName
#include <Functiondiscoverykeys_devpkey.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

#include <stdexcept>

#include <microprofile.h>

MICROPROFILE_DEFINE(WasapiReceiveFormat, "Wasapi", "WasapiReceiveFormat", 0xff00ff38);
MICROPROFILE_DEFINE(WasapiReceiveFrames, "Wasapi", "WasapiReceiveFrames", 0xff00ff00);

using namespace std;

WasapiReceiver::WasapiReceiver() {
  memset(&afmt, 0, sizeof(audio_format_t));

  HRESULT hr;

  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    throw runtime_error("CoInitializeEx failed");
  }

  hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
  if (FAILED(hr)) {
    throw runtime_error("Could not create device enumerator instance");
  }

  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  if (FAILED(hr)) {
    throw runtime_error("Could not get default audio endpoint");
  }

  IPropertyStore *props = nullptr;
  hr = device->OpenPropertyStore(STGM_READ, &props);
  if (FAILED(hr)) {
    throw runtime_error("Could not get open endpoint property store");
  }

  PROPVARIANT var_name;
  // Initialize container for property value.
  PropVariantInit(&var_name);

  // Get the endpoint's friendly-name property.
  hr = props->GetValue(PKEY_Device_FriendlyName, &var_name);
  if (FAILED(hr)) {
    throw runtime_error("Could not get device friendly name");
  }

  // Print endpoint friendly name and endpoint ID.
  capsule_log("WasapiReceiver: Capturing from: \"%S\"", var_name.pwszVal);

  hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**) &audio_client);
  if (FAILED(hr)) {
    throw runtime_error("Could not activate audio device");
  }

  hr = audio_client->GetMixFormat(&pwfx);
  if (FAILED(hr)) {
    throw runtime_error("Could not get mix format");
  }

  afmt.channels = pwfx->nChannels;
  afmt.samplerate = pwfx->nSamplesPerSec;
  afmt.samplewidth = pwfx->wBitsPerSample;

  REFERENCE_TIME hns_requested_duration = REFTIMES_PER_SEC;
  hr = audio_client->Initialize(
      AUDCLNT_SHAREMODE_SHARED, // we don't need exclusive access
      AUDCLNT_STREAMFLAGS_LOOPBACK, // we want to capture output, not an input
      hns_requested_duration,
      0,
      pwfx,
      NULL);
  if (FAILED(hr)) {
    throw runtime_error("Could not initialize audio client");
  }

  UINT32 buffer_frame_count;

  // Get the size of the allocated buffer.
  hr = audio_client->GetBufferSize(&buffer_frame_count);
  if (hr != S_OK) {
    throw runtime_error("Could not get buffer size");
  }

  capsule_log("WasapiReceiver: Buffer frame count: %d", buffer_frame_count);

  hr = audio_client->GetService(
      IID_IAudioCaptureClient,
      (void **) &capture_client);
  if (FAILED(hr)) {
    throw runtime_error("Could not get capture client");
  }

  hr = audio_client->Start();
  if (hr != S_OK) {
    printf("Could not start capture");
    exit(1);
  }
}

int WasapiReceiver::receive_format(audio_format_t *afmt_out) {
  MICROPROFILE_SCOPE(WasapiReceiveFormat);

  memcpy(afmt_out, &afmt, sizeof(audio_format_t));
  return 0;
}

void *WasapiReceiver::receive_frames(int *frames_received) {
  MICROPROFILE_SCOPE(WasapiReceiveFrames);

  lock_guard<mutex> lock(stopped_mutex);

  if (stopped) {
    *frames_received = 0;
    return nullptr;
  }

  BYTE *buffer;
  DWORD flags;
  HRESULT hr;
  UINT32 num_frames_available;

  if (num_frames_received > 0) {
    hr = capture_client->ReleaseBuffer(num_frames_received);
    if (FAILED(hr)) {
      capsule_log("WasapiReceiver: Could not release buffer: error %d (%x)\n", hr, hr);
      stopped = true;
      *frames_received = 0;
      return nullptr;
    }
  }

  hr = capture_client->GetBuffer(
    &buffer,
    &num_frames_available,
    &flags,
    NULL,
    NULL
  );

  if (FAILED(hr)) {
    if (hr == AUDCLNT_S_BUFFER_EMPTY) {
      // good!
      *frames_received = 0;
      num_frames_received = 0;
      return nullptr;
    } else {
      capsule_log("WasapiReceiver: Could not get buffer: error %d (%x)\n", hr, hr);
      stopped = true;
      *frames_received = 0;
      return nullptr;
    }
  }

  *frames_received = num_frames_available;
  num_frames_received = num_frames_available;
  return buffer;
}

void WasapiReceiver::stop() {
  lock_guard<mutex> lock(stopped_mutex);

  HRESULT hr = audio_client->Stop();
  if (FAILED(hr)) {
    capsule_log("WasapiReceiver: Could not stop audio client: error %d (%x)", hr, hr);
  }
  stopped = true;
}

WasapiReceiver::~WasapiReceiver() {
  CoTaskMemFree(pwfx);
  SAFE_RELEASE(enumerator)
  SAFE_RELEASE(device)
  SAFE_RELEASE(audio_client)
  SAFE_RELEASE(capture_client)
}
