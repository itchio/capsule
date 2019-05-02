use comm::*;
use log::*;
use std::ptr;
use winapi::um::winuser::{GetMessageW, RegisterHotKey, MOD_NOREPEAT, MSG, VK_F9};

fn poll() {
    unsafe {
        let result = RegisterHotKey(ptr::null_mut(), 1, MOD_NOREPEAT as u32, VK_F9 as u32);
        if result != 1 {
            warn!(
                "Could not register hotkey: {}",
                wincap::get_last_error_string()
            );
            return;
        }

        let mut msg: MSG = std::mem::zeroed();
        while GetMessageW(&mut msg, ptr::null_mut(), 0, 0) != 0 {
            get_hub().hotkey_pressed();
        }
    }
}

pub fn initialize() {
    std::thread::spawn(poll);
}
