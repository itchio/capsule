#![allow(non_upper_case_globals, non_snake_case)]

#[macro_use]
extern crate ctor;
#[macro_use]
extern crate lazy_static;
#[macro_use]
extern crate libc_print;
extern crate detour;

use detour::RawDetour;

#[cfg(target_os = "macos")]
static CURRENT_PLATFORM: &str = "macOS";
#[cfg(target_os = "linux")]
static CURRENT_PLATFORM: &str = "Linux";
#[cfg(target_os = "windows")]
static CURRENT_PLATFORM: &str = "Windows";

#[ctor]
fn ctor() {
    match std::env::var("CAPSULE_TEST") {
        Ok(_) => {
            lazy_static::initialize(&puts__hook);
        }
        Err(_) => {
            libc_println!("thanks for flying capsule on {}", CURRENT_PLATFORM);
        }
    }
}

extern "C" {
    pub fn puts(ptr: *const u8) -> ();
}

unsafe extern "C" fn puts__hooked(ptr: *const u8) {
    if ptr == (0xDEADBEEF as *const u8) {
        libc_println!("caught dead beef");
        return;
    }
    puts__next(ptr)
}

lazy_static! {
    static ref puts__hook: std::sync::Mutex<RawDetour> = unsafe {
        let mut detour = RawDetour::new(puts as *const (), puts__hooked as *const ()).unwrap();
        detour.enable().unwrap();
        std::sync::Mutex::new(detour)
    };
    static ref puts__next: unsafe extern "C" fn(ptr: *const u8) -> () =
        unsafe { std::mem::transmute(puts__hook.lock().unwrap().trampoline()) };
}
