
#include "playthrough/CAPlayThrough.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioToolbox.h>

#include <lab/strings.h>

#include "interpose.h"
#include "../logging.h"

static CAPlayThroughHost *playThroughHost;
static bool settingUpPlayThrough = false;

static const AudioObjectPropertyAddress devlist_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

OSStatus capsule_AudioObjectGetPropertyData (AudioObjectID objectID, const AudioObjectPropertyAddress *inAddress, UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *ioDataSize, void *outData) {

  OSStatus status = AudioObjectGetPropertyData(objectID, inAddress, inQualifierDataSize, inQualifierData, ioDataSize, outData);

  if (objectID == kAudioObjectSystemObject &&
      inAddress->mElement == devlist_address.mElement &&
      inAddress->mScope == devlist_address.mScope &&
      (inAddress->mSelector == kAudioHardwarePropertyDevices ||
       inAddress->mSelector == kAudioHardwarePropertyDefaultOutputDevice ||
       inAddress->mSelector == kAudioHardwarePropertyDefaultSystemOutputDevice
       )
      ) {

    AudioDeviceID soundflowerId = 0;
    AudioDeviceID builtinId = 0;
    bool found_soundflower;
    bool found_builtin;
    UInt32 size;
    OSStatus nStatus = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devlist_address, 0, NULL, &size);
    if (nStatus != kAudioHardwareNoError) {
      capsule::Log("Could not get size of device list");
      return status;
    }

    AudioDeviceID* devs = (AudioDeviceID*) alloca(size);
    if (!devs) {
      capsule::Log("Could not allocate device list");
      return status;
    }

    nStatus = AudioObjectGetPropertyData(kAudioObjectSystemObject, &devlist_address, 0, NULL, &size, devs);
    if (nStatus != kAudioHardwareNoError) {
      capsule::Log("Could not list devices");
      return status;
    }

    UInt32 numDevs = size / sizeof(AudioDeviceID);

    capsule::Log("Querying %s, got %d results",
        (inAddress->mSelector == kAudioHardwarePropertyDevices ? "all devices" :
         (inAddress->mSelector == kAudioHardwarePropertyDefaultOutputDevice ? "default output" :
          (inAddress->mSelector == kAudioHardwarePropertyDefaultSystemOutputDevice ? "default system output" : "???"))),
        (unsigned int) numDevs);

    for (UInt32 i = 0; i < numDevs; i++) {
      CFStringRef cfstr = NULL;
      char *ptr = NULL;
      const AudioObjectPropertyAddress nameaddr = {
          kAudioObjectPropertyName,
          kAudioDevicePropertyScopeOutput,
          kAudioObjectPropertyElementMaster
      };

      size = sizeof (CFStringRef);
      AudioDeviceID dev = devs[i];
      AudioObjectGetPropertyData(dev, &nameaddr, 0, NULL, &size, &cfstr);

      if (!cfstr) {
        capsule::Log("No output name for device %d", (unsigned int) i);
        continue;
      }

      CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);

      ptr = (char *) malloc(len + 1);
      if (!CFStringGetCString(cfstr, ptr, len + 1, kCFStringEncodingUTF8)) {
        capsule::Log("Could not convert to CString");
      }
      CFRelease(cfstr);

      // capsule::Log("Device #%d: %s", (unsigned int) i, ptr);

      if (lab::strings::CEquals("Soundflower (2ch)", ptr)) {
        // capsule::Log("Found the soundflower!")
        soundflowerId = dev;
        found_soundflower = true;
      }
      if (lab::strings::CEquals("Built-in Output", ptr)) {
        // capsule::Log("Found the built-in output!")
        builtinId = dev;
        found_builtin = true;
      }

      free(ptr);
    }

    if (found_builtin && found_soundflower) {
      numDevs = *ioDataSize / sizeof(AudioDeviceID);
      devs = (AudioDeviceID*) outData;
      for (UInt32 i = 0; i < numDevs; i++) {
        AudioDeviceID dev = devs[i];
        if (dev == builtinId) {
          capsule::Log("Subverting audio device %d for capture", (unsigned int) i);
          devs[i] = soundflowerId;
        }
      }

      if (!playThroughHost && !settingUpPlayThrough) {
        settingUpPlayThrough = true;
        playThroughHost = new CAPlayThroughHost(soundflowerId, builtinId);
        if (!playThroughHost) {
          capsule::Log("ERROR: playThroughHost init failed!");
          return status;
        }
        playThroughHost->Start();
        settingUpPlayThrough = false;
      }
    }
  } else {
    /* capsule::Log("Called AudioObjectGetPropertyData with unknown parameters"); */
    /* capsule::Log("mElement is %u instead of %u", inAddress->mElement, devlist_address.mElement); */
    /* capsule::Log("mScope is %u instead of %u", inAddress->mScope, devlist_address.mScope); */
    /* capsule::Log("mSelector is %u instead of %u", inAddress->mSelector, devlist_address.mSelector); */
  }

  return status;
}

DYLD_INTERPOSE(capsule_AudioObjectGetPropertyData, AudioObjectGetPropertyData)


