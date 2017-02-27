
#include <capsulerun.h>

#include "DDHotKeyCenter.h"
#import <Carbon/Carbon.h>

#include <thread>

using namespace std;

void capsule_run_app () {
  [[NSApplication sharedApplication] run];
}

int capsule_hotkey_init(encoder_private_t *p) {
  capsule_log("capsule_hotkey_init: registering hotkey");

  capsule_log("capsule_hotkey_init: kVK_F9 is %d", kVK_F9);
  DDHotKey *hotKey = [[DDHotKeyCenter sharedHotKeyCenter] registerHotKeyWithKeyCode:kVK_F9 modifierFlags:0 task:^(NSEvent *ev) {
    capsule_log("Hotkey pressed!");
    capsule_io_capture_start(p->io);
  }];

  if (hotKey == nil) {
    capsule_log("Hotkey registration failed.");
    return 1;
  }

  return 0;
}
