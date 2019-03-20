
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

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
    virtual void *ReceiveFrames(int64_t *frames_received) override;
    virtual void Stop() override;

  private:
    encoder::AudioFormat afmt_ = {0};
    IMMDeviceEnumerator *enumerator_ = nullptr;
    IMMDevice *device_ = nullptr;
    IAudioClient *audio_client_ = nullptr;
    IAudioCaptureClient *capture_client_ = nullptr;
    WAVEFORMATEX *pwfx_ = nullptr;
    int num_frames_received_ = 0;

    bool stopped_ = true;
    std::mutex stopped_mutex_;
};

} // namespace capsule
} // namespace audio
