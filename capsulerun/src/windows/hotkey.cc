
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "../hotkey.h"
#include "../logging.h"

#include <thread>

namespace capsule {
namespace hotkey {

static void Poll (MainLoop *ml) {
  BOOL success = RegisterHotKey(NULL, 1, MOD_NOREPEAT, VK_F9);

  if (!success) {
    DWORD err = GetLastError();
    Log("Could not register hotkey: %d (%x)", err, err);
    return;
  }

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0) != 0) {
    if (msg.message == WM_HOTKEY) {
      ml->CaptureFlip();
    }
  }
}

void Init (MainLoop *ml) {
  // we must register from the same thread we use to poll,
  // since we don't register it for a specific hwnd.
  // That's how Win32 message queues work!
  std::thread poll_thread(Poll, ml);
  poll_thread.detach();
}

} // namespace hotkey
} // namespace capsule
