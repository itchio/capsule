
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <thread>

namespace capsule {
namespace hotkey {

Display *capsule_x11_dpy;
Window capsule_x11_root;

static void Poll (MainLoop *ml) {
    XEvent ev;

    while(1) {
        XNextEvent(capsule_x11_dpy, &ev);

        switch (ev.type) {
            case KeyPress:
                ml->CaptureFlip();
            default:
                break;
        }
    }
}

void Init (MainLoop *ml) {
    // XSetErrorHandler(capsule_x11_error_handler);
    capsule_x11_dpy = XOpenDisplay(0);
    capsule_x11_root = DefaultRootWindow(capsule_x11_dpy);

    int num_modifiers = 4;
    unsigned int modifier_list[] = {
        0,
        Mod2Mask,
        Mod3Mask,
        Mod2Mask | Mod3Mask,
    };

    int keycode = XKeysymToKeycode(capsule_x11_dpy, XK_F9);
    Window grab_window = capsule_x11_root;
    Bool owner_events = False;
    int pointer_mode = GrabModeAsync;
    int keyboard_mode = GrabModeAsync;

    for (int i = 0; i < num_modifiers; i++) {
        XGrabKey(
            capsule_x11_dpy,
            keycode,
            modifier_list[i],
            grab_window,
            owner_events,
            pointer_mode,
            keyboard_mode
        );
    }
    XSelectInput(capsule_x11_dpy, capsule_x11_root, KeyPressMask);

    std::thread poll_thread(Poll, ml);
    poll_thread.detach();
}

} // namespace hotkey
} // namespace capsule
