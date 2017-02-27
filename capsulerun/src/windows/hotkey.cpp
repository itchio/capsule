
#include <capsulerun.h>

#include <thread>

using namespace std;

static void capsule_hotkey_poll(struct encoder_private_s *p) {
  BOOL success = RegisterHotKey(NULL, 1, MOD_NOREPEAT, VK_F9);

  if (!success) {
    DWORD err = GetLastError();
    capsule_log("Could not register hotkey: %d (%x)", err, err);
    return;
  }

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0) != 0) {
    if (msg.message = WM_HOTKEY) {
      capsule_log("capsule_hotkey_poll: Starting capture!");
      wasapi_start(p);
      capsule_io_capture_start(p->io);
    }
  }
}

int capsule_hotkey_init(struct encoder_private_s *p) {
  // we must register from the same thread we use to poll,
  // since we don't register it for a specific hwnd.
  // That's how Win32 message queues work!
  thread poll_thread(capsule_hotkey_poll, p);
  poll_thread.detach();

  return 0;
}
