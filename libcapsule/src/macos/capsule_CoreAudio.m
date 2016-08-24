
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
      inAddress->mScope == devlist_address.mScope // &&
      // inAddress->mSelector == devlist_address.mSelector
      ) {

    AudioDeviceID* devs = (AudioDeviceID*) outData;
    UInt32 numDevs = *ioDataSize / sizeof(AudioDeviceID);

    capsule_log("Querying device list, got %d results", (unsigned int) numDevs);
    if (inAddress->mSelector != devlist_address.mSelector) {
      capsule_log("Selector differed: %d instead of %d", inAddress->mSelector, devlist_address.mSelector);
      capsule_log("kAudioHardwarePropertyDevices: %d", kAudioHardwarePropertyDevices);
      capsule_log("kAudioObjectPropertyScopeGlobal: %d", kAudioObjectPropertyScopeGlobal);
      capsule_log("kAudioObjectPropertyElementMaster: %d", kAudioObjectPropertyElementMaster);

      capsule_log("kAudioHardwarePropertyDevices: %d", kAudioHardwarePropertyDevices);
      capsule_log("kAudioHardwarePropertyDefaultInputDevice: %d", kAudioHardwarePropertyDefaultInputDevice);
      capsule_log("kAudioHardwarePropertyDefaultOutputDevice: %d", kAudioHardwarePropertyDefaultOutputDevice);
      capsule_log("kAudioHardwarePropertyDefaultSystemOutputDevice: %d", kAudioHardwarePropertyDefaultSystemOutputDevice);
      capsule_log("kAudioHardwarePropertyTranslateUIDToDevice: %d", kAudioHardwarePropertyTranslateUIDToDevice);
      capsule_log("kAudioHardwarePropertyMixStereoToMono);: %d", kAudioHardwarePropertyMixStereoToMono);
      capsule_log("kAudioHardwarePropertyPlugInList: %d", kAudioHardwarePropertyPlugInList);
      capsule_log("kAudioHardwarePropertyTranslateBundleIDToPlugIn: %d", kAudioHardwarePropertyTranslateBundleIDToPlugIn);
      capsule_log("kAudioHardwarePropertyTransportManagerList: %d", kAudioHardwarePropertyTransportManagerList);
      capsule_log("kAudioHardwarePropertyTranslateBundleIDToTransportManager: %d", kAudioHardwarePropertyTranslateBundleIDToTransportManager);
      capsule_log("kAudioHardwarePropertyBoxList: %d", kAudioHardwarePropertyBoxList);
      capsule_log("kAudioHardwarePropertyTranslateUIDToBox: %d", kAudioHardwarePropertyTranslateUIDToBox);
      capsule_log("kAudioHardwarePropertyProcessIsMaster: %d", kAudioHardwarePropertyProcessIsMaster);
      capsule_log("kAudioHardwarePropertyIsInitingOrExiting: %d", kAudioHardwarePropertyIsInitingOrExiting);
      capsule_log("kAudioHardwarePropertyUserIDChanged: %d", kAudioHardwarePropertyUserIDChanged);
      capsule_log("kAudioHardwarePropertyProcessIsAudible: %d", kAudioHardwarePropertyProcessIsAudible);
      capsule_log("kAudioHardwarePropertySleepingIsAllowed: %d", kAudioHardwarePropertySleepingIsAllowed);
      capsule_log("kAudioHardwarePropertyUnloadingIsAllowed: %d", kAudioHardwarePropertyUnloadingIsAllowed);
      capsule_log("kAudioHardwarePropertyHogModeIsAllowed: %d", kAudioHardwarePropertyHogModeIsAllowed);
      capsule_log("kAudioHardwarePropertyUserSessionIsActiveOrHeadless: %d", kAudioHardwarePropertyUserSessionIsActiveOrHeadless);
      capsule_log("kAudioHardwarePropertyServiceRestarted: %d", kAudioHardwarePropertyServiceRestarted);
      capsule_log("kAudioHardwarePropertyPowerHint: %d", kAudioHardwarePropertyPowerHint);
    }

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
      capsule_log("Querying device %d, id = %x", i, dev)
      AudioObjectGetPropertyData(dev, &nameaddr, 0, NULL, &size, &cfstr);

      if (!cfstr) {
        capsule_log("No output name for device %d", i);
        continue;
      }

      capsule_log("Getting maximum size")
      CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);
      capsule_log("len = %d", len)

      ptr = (char *) malloc(len + 1);
      if (!CFStringGetCString(cfstr, ptr, len + 1, kCFStringEncodingUTF8)) {
        capsule_log("Could not convert to CString");
      }
      CFRelease(cfstr);
      capsule_log("Converted name", ptr)
      capsule_log("strlen = %d", strlen(ptr))

      capsule_log("Device #%d name: %s", i, ptr)

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

bool capsule_setAudioOutput (char *targetDevice) {
  AudioObjectPropertyAddress  propertyAddress;
  AudioObjectID               *deviceIDs;
  UInt32                      propertySize;
  NSInteger                   numDevices;

  propertyAddress.mSelector = kAudioHardwarePropertyDevices;
  propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
  propertyAddress.mElement = kAudioObjectPropertyElementMaster;

  // enumerate all current/valid devices
  if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize) == noErr) {
    numDevices = propertySize / sizeof(AudioDeviceID);
    deviceIDs = (AudioDeviceID *)calloc(numDevices, sizeof(AudioDeviceID));

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, deviceIDs) == noErr) {
      AudioObjectPropertyAddress      deviceAddress;
      char                            deviceName[64];
      char                            manufacturerName[64];

      for (NSInteger idx=0; idx<numDevices; idx++) {
        propertySize = sizeof(deviceName);
        deviceAddress.mSelector = kAudioDevicePropertyDeviceName;
        deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
        deviceAddress.mElement = kAudioObjectPropertyElementMaster;

        // get the deets!
        if (AudioObjectGetPropertyData(deviceIDs[idx], &deviceAddress, 0, NULL, &propertySize, deviceName) == noErr) {
          propertySize = sizeof(manufacturerName);
          deviceAddress.mSelector = kAudioDevicePropertyDeviceManufacturer;
          deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
          deviceAddress.mElement = kAudioObjectPropertyElementMaster;
          if (AudioObjectGetPropertyData(deviceIDs[idx], &deviceAddress, 0, NULL, &propertySize, manufacturerName) == noErr) {
            CFStringRef     uidString;

            propertySize = sizeof(uidString);
            deviceAddress.mSelector = kAudioDevicePropertyDeviceUID;
            deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
            deviceAddress.mElement = kAudioObjectPropertyElementMaster;
            if (AudioObjectGetPropertyData(deviceIDs[idx], &deviceAddress, 0, NULL, &propertySize, &uidString) == noErr) {
              // comment this out if you don't want to log everything

              NSLog(@"device %s by %s id %@", deviceName, manufacturerName, uidString);
              CFRelease(uidString);
            }

            if (strcmp(targetDevice, deviceName) == 0){
              propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
              propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
              propertyAddress.mElement = kAudioObjectPropertyElementMaster;

              return AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, sizeof(AudioDeviceID), &deviceIDs[idx]) == noErr;
            }
          }
        }
      }
    }

    free(deviceIDs);
  }
  return false;
}


