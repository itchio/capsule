
#include <capsulerun.h>

#include "DDHotKeyCenter.h"
#import <Carbon/Carbon.h>

#include "../shared/main_loop.h"

namespace capsule {

void RunApp () {
  [[NSApplication sharedApplication] run];
}

namespace hotkey {

int Init(MainLoop *ml) {
  CapsuleLog("capsule::hotkey::Init: registering hotkey");

  CapsuleLog("capsule::hotkey::Init: kVK_F9 is %d", kVK_F9);
  DDHotKey *hotKey = [[DDHotKeyCenter sharedHotKeyCenter] registerHotKeyWithKeyCode:kVK_F9 modifierFlags:0 task:^(NSEvent *ev) {
    ml->CaptureFlip();
  }];

  if (hotKey == nil) {
    CapsuleLog("capsule::hotkey::init: registration failed");
    return 1;
  }

  return 0;
}

} // namespace hotkey
} // namespace capsule
