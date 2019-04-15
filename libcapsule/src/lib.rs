#![allow(non_upper_case_globals, non_snake_case)]

#[cfg(target_os = "windows")]
#[macro_use]
extern crate wstr;
#[cfg(target_os = "windows")]
#[macro_use]
extern crate cstr_macro;
#[cfg(target_os = "windows")]
#[macro_use]
extern crate wincap;

#[macro_use]
extern crate ctor;
#[macro_use]
extern crate libc_print;

#[macro_use(lazy_static)]
extern crate lazy_static;

use libc::c_char;

#[macro_use]
mod hook;
mod gl;
mod linux_gl_hooks;
mod windows_gl_hooks;

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
            #[cfg(target_os = "linux")]
            {
                linux_gl_hooks::initialize()
            }
            #[cfg(target_os = "windows")]
            {
                windows_gl_hooks::initialize()
            }
        }
    }
}

hook_extern! {
    fn puts(ptr: *const c_char) -> () {
        if ptr == 0xDEADBEEF as *const c_char {
            libc_println!("caught dead beef");
            return;
        }
        puts__next(ptr)
    }
}
