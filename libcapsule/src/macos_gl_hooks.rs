#![cfg(target_os = "macos")]

use lazy_static::lazy_static;
use libc::{c_char, c_int, c_void};
use libc_print::libc_println;
use log::*;
use std::ffi::{CStr, CString};
use std::sync::Once;

///////////////////////////////////////////
// libdl definition & link instructions
///////////////////////////////////////////

pub const RTLD_LAZY: c_int = 0x1;
pub const RTLD_NOLOAD: c_int = 0x10;
pub const RTLD_NEXT: *const c_void = (-1 as isize) as *const c_void;

extern "C" {
    pub fn dlsym(handle: *const c_void, symbol: *const c_char) -> *const c_void;
}

///////////////////////////////////////////
// lazily-opened libGL handle
///////////////////////////////////////////

static gl_dylib_path: &str = "/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL";

lazy_static! {
    static ref gl_dylib_path_cstr: CString = { CString::new(gl_dylib_path).unwrap() };
}

struct LibHandle {
    addr: *const c_void,
}
unsafe impl std::marker::Sync for LibHandle {}

lazy_static! {
    static ref gl_handle: LibHandle = {
        info!("in gl_handle initialization");
        let handle = dlopen__next(gl_dylib_path_cstr.as_ptr(), RTLD_LAZY);
        if handle.is_null() {
            panic!("could not dlopen libGL.dylib")
        }
        LibHandle { addr: handle }
    };
}

///////////////////////////////////////////
// dlopen hook
///////////////////////////////////////////

hook_dyld! {
    ("ld", "library") => fn dlopen(filename: *const c_char, flags: c_int) -> *const c_void {
        let res = dlopen__next(filename, flags);
        {
            let name = if filename.is_null() {
                String::from("NULL")
            } else {
                CStr::from_ptr(filename).to_string_lossy().into_owned()
            };
            info!("dlopen({:?}, {}) = {:x}", name, flags, res as usize);
        }
        hook_if_needed();
        res
    }
}

///////////////////////////////////////////
// CGLFlushDrawable
///////////////////////////////////////////
hook_dyld! {
    ("OpenGL", "framework") => fn CGLFlushDrawable(ctx: *const c_void) -> *const c_void {
        if super::SETTINGS.in_test && ctx == 0xDEADBEEF as *const c_void {
            libc_println!("caught dead beef");
            std::process::exit(0);
        }

        info!("Swapping buffers!");
        CGLFlushDrawable__next(ctx)
    }
}

unsafe fn is_using_opengl() -> bool {
    let handle = dlopen__next(gl_dylib_path_cstr.as_ptr(), RTLD_NOLOAD | RTLD_LAZY);
    !handle.is_null()
}

static HOOK_SWAPBUFFERS_ONCE: Once = Once::new();

unsafe fn hook_if_needed() {
    if is_using_opengl() {
        HOOK_SWAPBUFFERS_ONCE.call_once(|| {
            info!("libGL usage detected, hooking OpenGL");
            lazy_static::initialize(&CGLFlushDrawable__hook);
        })
    }
}

pub fn initialize() {
    unsafe {
        lazy_static::initialize(&dlopen__hook);
        hook_if_needed()
    }
}
