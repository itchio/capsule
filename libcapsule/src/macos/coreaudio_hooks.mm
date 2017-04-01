
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

#include "coreaudio_hooks.h"

#include <lab/strings.h>

#include "interpose.h"
#include "../logging.h"
#include "../ensure.h"
#include "../capture.h"
#include "../io.h"

namespace capsule {
namespace coreaudio {

struct CoreAudioState {
  bool seen;
} state = {0};

void Saw() {
  if (!state.seen) {
    capsule::Log("CoreAudio: saw!");
    state.seen = true;

    auto channels = 2;
    auto rate = 44100;

    // FIXME: query instead of blindly assuming format
    capture::HasAudioIntercept(messages::SampleFmt_F32LE, rate, channels);
  }
}

static OSStatus RenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                            UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {
  Saw();

  auto proxy_data = reinterpret_cast<CallbackProxyData*>(inRefCon);

  // capsule::Log("CoreAudio: rendering a buffer with %d bytes", ioData->mBuffers[0].mDataByteSize);
  auto ret = proxy_data->actual_callback(
    proxy_data->actual_userdata, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData
  );

  if (capsule::capture::Active()) {
    const int64_t channels = 2;
    const int64_t sample_size = 32 / 8;
    const int64_t frame_size = sample_size * channels;
    auto buf = ioData->mBuffers[0];
    const int64_t num_frames = buf.mDataByteSize / frame_size;
    capsule::io::WriteAudioFrames((char*) buf.mData, num_frames);
  }

  return ret;
}

// interposed AudioToolbox function
OSStatus AudioUnitSetProperty(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, const void *inData, UInt32 inDataSize) {
  capsule::Log("CoreAudio: setting property %d of unit %p", inID, inUnit);
  if (inID == kAudioUnitProperty_SetRenderCallback) {
    capsule::Log("CoreAudio: subverting callback");
    auto proxy_data = reinterpret_cast<CallbackProxyData *>(calloc(sizeof(CallbackProxyData), 1));
    auto passed_cb = reinterpret_cast<const AURenderCallbackStruct *>(inData);
    proxy_data->actual_callback = passed_cb->inputProc;
    proxy_data->actual_userdata = passed_cb->inputProcRefCon;

    AURenderCallbackStruct hooked_cb;
    hooked_cb.inputProc = coreaudio::RenderCallback;
    hooked_cb.inputProcRefCon = proxy_data;

    return ::AudioUnitSetProperty(inUnit, inID, inScope, inElement, &hooked_cb, inDataSize);
  } else {
    return ::AudioUnitSetProperty(inUnit, inID, inScope, inElement, inData, inDataSize);
  }
}

} // namespace coreaudio
} // namespace capsule

DYLD_INTERPOSE(capsule::coreaudio::AudioUnitSetProperty, AudioUnitSetProperty)

