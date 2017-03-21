
#include <capsulerun.h>

#include "DDHotKeyCenter.h"
#import <Carbon/Carbon.h>

#include "../shared/MainLoop.h"

#include <thread>

using namespace std;

void CapsuleRunApp () {
  [[NSApplication sharedApplication] run];
}

int CapsuleHotkeyInit(MainLoop *ml) {
  CapsuleLog("capsule_hotkey_init: registering hotkey");

  CapsuleLog("capsule_hotkey_init: kVK_F9 is %d", kVK_F9);
  DDHotKey *hotKey = [[DDHotKeyCenter sharedHotKeyCenter] registerHotKeyWithKeyCode:kVK_F9 modifierFlags:0 task:^(NSEvent *ev) {
    ml->CaptureFlip();
  }];

  if (hotKey == nil) {
    CapsuleLog("Hotkey registration failed.");
    return 1;
  }

  return 0;
}

