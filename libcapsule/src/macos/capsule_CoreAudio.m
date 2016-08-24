
#include "capsule.h"
#include "capsule_macos_utils.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioToolbox.h>

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

    AudioDeviceID* devs = (AudioDeviceID*) outData;
    UInt32 numDevs = *ioDataSize / sizeof(AudioDeviceID);

    capsule_log("Querying %s, got %d results",
        (inAddress->mSelector == kAudioHardwarePropertyDevices ? "all devices" :
         (inAddress->mSelector == kAudioHardwarePropertyDefaultOutputDevice ? "default output" :
          (inAddress->mSelector == kAudioHardwarePropertyDefaultSystemOutputDevice ? "default system output" : "???"))),
        (unsigned int) numDevs);
    /* if (inAddress->mSelector != devlist_address.mSelector) { */
    /*   capsule_log("Selector differed: %d instead of %d", inAddress->mSelector, devlist_address.mSelector); */
    /*   capsule_log("kAudioHardwarePropertyDevices: %d", kAudioHardwarePropertyDevices); */
    /*   capsule_log("kAudioObjectPropertyScopeGlobal: %d", kAudioObjectPropertyScopeGlobal); */
    /*   capsule_log("kAudioObjectPropertyElementMaster: %d", kAudioObjectPropertyElementMaster); */

    /*   capsule_log("kAudioHardwarePropertyDevices: %d", kAudioHardwarePropertyDevices); */
    /*   capsule_log("kAudioHardwarePropertyDefaultInputDevice: %d", kAudioHardwarePropertyDefaultInputDevice); */
    /*   capsule_log("kAudioHardwarePropertyDefaultOutputDevice: %d", kAudioHardwarePropertyDefaultOutputDevice); */
    /*   capsule_log("kAudioHardwarePropertyDefaultSystemOutputDevice: %d", kAudioHardwarePropertyDefaultSystemOutputDevice); */
    /*   capsule_log("kAudioHardwarePropertyTranslateUIDToDevice: %d", kAudioHardwarePropertyTranslateUIDToDevice); */
    /*   capsule_log("kAudioHardwarePropertyMixStereoToMono);: %d", kAudioHardwarePropertyMixStereoToMono); */
    /*   capsule_log("kAudioHardwarePropertyPlugInList: %d", kAudioHardwarePropertyPlugInList); */
    /*   capsule_log("kAudioHardwarePropertyTranslateBundleIDToPlugIn: %d", kAudioHardwarePropertyTranslateBundleIDToPlugIn); */
    /*   capsule_log("kAudioHardwarePropertyTransportManagerList: %d", kAudioHardwarePropertyTransportManagerList); */
    /*   capsule_log("kAudioHardwarePropertyTranslateBundleIDToTransportManager: %d", kAudioHardwarePropertyTranslateBundleIDToTransportManager); */
    /*   capsule_log("kAudioHardwarePropertyBoxList: %d", kAudioHardwarePropertyBoxList); */
    /*   capsule_log("kAudioHardwarePropertyTranslateUIDToBox: %d", kAudioHardwarePropertyTranslateUIDToBox); */
    /*   capsule_log("kAudioHardwarePropertyProcessIsMaster: %d", kAudioHardwarePropertyProcessIsMaster); */
    /*   capsule_log("kAudioHardwarePropertyIsInitingOrExiting: %d", kAudioHardwarePropertyIsInitingOrExiting); */
    /*   capsule_log("kAudioHardwarePropertyUserIDChanged: %d", kAudioHardwarePropertyUserIDChanged); */
    /*   capsule_log("kAudioHardwarePropertyProcessIsAudible: %d", kAudioHardwarePropertyProcessIsAudible); */
    /*   capsule_log("kAudioHardwarePropertySleepingIsAllowed: %d", kAudioHardwarePropertySleepingIsAllowed); */
    /*   capsule_log("kAudioHardwarePropertyUnloadingIsAllowed: %d", kAudioHardwarePropertyUnloadingIsAllowed); */
    /*   capsule_log("kAudioHardwarePropertyHogModeIsAllowed: %d", kAudioHardwarePropertyHogModeIsAllowed); */
    /*   capsule_log("kAudioHardwarePropertyUserSessionIsActiveOrHeadless: %d", kAudioHardwarePropertyUserSessionIsActiveOrHeadless); */
    /*   capsule_log("kAudioHardwarePropertyServiceRestarted: %d", kAudioHardwarePropertyServiceRestarted); */
    /*   capsule_log("kAudioHardwarePropertyPowerHint: %d", kAudioHardwarePropertyPowerHint); */
    /* } */

    for (UInt32 i = 0; i < numDevs; i++) {
      UInt32 size;
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
        capsule_log("No output name for device %d", i);
        continue;
      }

      CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);

      ptr = (char *) malloc(len + 1);
      if (!CFStringGetCString(cfstr, ptr, len + 1, kCFStringEncodingUTF8)) {
        capsule_log("Could not convert to CString");
      }
      CFRelease(cfstr);

      capsule_log("Device #%d: %s", i, ptr)

      if (strcmp("Soundflower (2ch)", ptr) == 0) {
        capsule_log("Found the soundflower!")
      }
      if (strcmp("Built-in Output", ptr) == 0) {
        capsule_log("Found the built-in output!")
      }

      free(ptr);
    }
  } else {
    capsule_log("Called AudioObjectGetPropertyData with unknown parameters");
    capsule_log("mElement is %u instead of %u", inAddress->mElement, devlist_address.mElement);
    capsule_log("mScope is %u instead of %u", inAddress->mScope, devlist_address.mScope);
    capsule_log("mSelector is %u instead of %u", inAddress->mSelector, devlist_address.mSelector);
  }

  return status;
}

DYLD_INTERPOSE(capsule_AudioObjectGetPropertyData, AudioObjectGetPropertyData)


