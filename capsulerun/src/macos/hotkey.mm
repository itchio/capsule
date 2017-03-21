
#include <capsulerun.h>

#include "DDHotKeyCenter.h"
#import <Carbon/Carbon.h>

#include "../shared/MainLoop.h"

namespace capsule {

void RunApp () {
  [[NSApplication sharedApplication] run];
}

namespace hotkey {

int Init(MainLoop *ml) {
  CapsuleLog("CapsuleHotkeyInit: registering hotkey");

  CapsuleLog("CapsuleHotkeyInit: kVK_F9 is %d", kVK_F9);
  DDHotKey *hotKey = [[DDHotKeyCenter sharedHotKeyCenter] registerHotKeyWithKeyCode:kVK_F9 modifierFlags:0 task:^(NSEvent *ev) {
    ml->CaptureFlip();
  }];

  if (hotKey == nil) {
    CapsuleLog("Hotkey registration failed.");
    return 1;
  }

  return 0;
}

} // namespace hotkey
} // namespace capsule
