
#include "capsule_macos_utils.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioToolbox.h>

Component capsule_FindNextComponent (Component start, ComponentDescription *desc) {
  if (desc->componentType == kAudioUnitType_Output) {
    NSLog(@"Looking for audio component, desc =");
    NSLog(@"type: %d", (unsigned int) desc->componentType);
    NSLog(@"manufacturer: %d", (unsigned int) desc->componentManufacturer);
    NSLog(@"subType: %d", (unsigned int) desc->componentSubType);

    ComponentDescription lookDesc;
    bzero(&lookDesc, sizeof(lookDesc));
    lookDesc.componentType = desc->componentType;

    Component lookComp = NULL;
    ComponentDescription compDesc;
    NSString *componentName;

    while (true) {
      lookComp = FindNextComponent(lookComp, &lookDesc);
      if (!lookComp) {
        break;
      }

      NSLog(@"Found audio component");
      bzero(&compDesc, sizeof(compDesc));

      Handle nameH = NewHandle (0);
      Handle infoH = NewHandle (0);
      GetComponentInfo(lookComp, &compDesc, nameH, infoH, NULL);
      NSLog(@"---------------------------------");
      NSLog(@"type: %d", (unsigned int) compDesc.componentType);
      NSLog(@"manufacturer: %d", (unsigned int) compDesc.componentManufacturer);
      NSLog(@"subType: %d", (unsigned int) compDesc.componentSubType);
      NSLog(@"name: %s", *nameH + 1);
      NSLog(@"info: %s", *infoH + 1);

      if (compDesc.componentSubType == 1634230636) {
        return lookComp;
      }
    }
  }

  return FindNextComponent(start, desc);
}

DYLD_INTERPOSE(capsule_FindNextComponent, FindNextComponent)
