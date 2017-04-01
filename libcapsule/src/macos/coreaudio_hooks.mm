
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

namespace capsule {
namespace coreaudio {

static OSStatus RenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                            UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {
  static FILE *raw = nullptr;
  static int64_t count = 0;

  auto proxy_data = reinterpret_cast<CallbackProxyData*>(inRefCon);

  capsule::Log("CoreAudio: rendering a buffer with %d bytes", ioData->mBuffers[0].mDataByteSize);
  auto ret = proxy_data->actual_callback(
    proxy_data->actual_userdata, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData
  );

  {
    if (!raw) {
      raw = fopen("capsule.rawaudio", "wb");
      capsule::Ensure("opened raw output", !!raw);
    }
    fwrite(ioData->mBuffers[0].mData, 1, ioData->mBuffers[0].mDataByteSize, raw);
    if (count++ > 15) {
      fflush(raw);
    }
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

