
#include "DDHotKeyCenter.h"
#import <Carbon/Carbon.h>

#include "../main_loop.h"

namespace capsule {

void RunApp () {
  CapsuleLog("Pretending we're an NSApplication...");
  [[NSApplication sharedApplication] run];
}

namespace hotkey {

void Init(MainLoop *ml) {
  CapsuleLog("capsule::hotkey::Init: registering hotkey");

  CapsuleLog("capsule::hotkey::Init: kVK_F9 is %d", kVK_F9);
  DDHotKey *hotKey = [[DDHotKeyCenter sharedHotKeyCenter] registerHotKeyWithKeyCode:kVK_F9 modifierFlags:0 task:^(NSEvent *ev) {
    CapsuleLog("Hotkey pressed!");
    ml->CaptureFlip();
  }];

  if (hotKey == nil) {
    CapsuleLog("capsule::hotkey::init: registration failed");
  } else {
    CapsuleLog("capsule::hotkey::init: registered hotkey successfully");
  }
}

} // namespace hotkey
} // namespace capsule

