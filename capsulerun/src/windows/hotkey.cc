
#include <capsulerun.h>

#include "../src/shared/main_loop.h"

#include <thread>

static void CapsuleHotkeyPoll(MainLoop *ml) {
  BOOL success = RegisterHotKey(NULL, 1, MOD_NOREPEAT, VK_F9);

  if (!success) {
    DWORD err = GetLastError();
    CapsuleLog("Could not register hotkey: %d (%x)", err, err);
    return;
  }

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0) != 0) {
    if (msg.message = WM_HOTKEY) {
      CapsuleLog("capsule_hotkey_poll: Starting capture!");
      ml->CaptureFlip();
    }
  }
}

int CapsuleHotkeyInit(MainLoop *ml) {
  // we must register from the same thread we use to poll,
  // since we don't register it for a specific hwnd.
  // That's how Win32 message queues work!
  std::thread poll_thread(capsule_hotkey_poll, ml);
  poll_thread.detach();

  return 0;
}
