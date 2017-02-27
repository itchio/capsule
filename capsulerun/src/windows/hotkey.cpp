
#include "capsulerun.h"

static void poll_hotkey (struct encoder_private_s *p) {
  BOOL success = RegisterHotKey(
    NULL,
    1,
    MOD_NOREPEAT,
    VK_F9
  );

  if (!success) {
    DWORD err = GetLastError();
    capsule_log("Could not register hotkey: %d (%x)", err, err);
    return;
  }

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0) != 0) {
    if (msg.message = WM_HOTKEY) {
      capsule_log("poll_hotkey: Starting capture!");
      wasapi_start(p);
      capsule_io_capture_start(p->io);
    }
  }
}