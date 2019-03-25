#![cfg(target_os = "linux")]
#![link(name = "dl")]

use detour::RawDetour;
use libc::{c_char, c_int, c_void};
use std::ffi::CString;
use std::sync::Once;
use std::time::SystemTime;

///////////////////////////////////////////
// libdl definition & link instructions
///////////////////////////////////////////

#[allow(unused)]
pub const RTLD_LAZY: c_int = 0x00001;
#[allow(unused)]
pub const RTLD_NOW: c_int = 0x00002;
#[allow(unused)]
pub const RTLD_NOLOAD: c_int = 0x0004;
#[allow(unused)]
pub const RTLD_DEFAULT: *const c_void = 0x00000 as *const c_void;
#[allow(unused)]
pub const RTLD_NEXT: *const c_void = (-1 as isize) as *const c_void;

extern "C" {
    pub fn dlsym(handle: *const c_void, symbol: *const c_char) -> *const c_void;
}

///////////////////////////////////////////
// lazily-opened libGL handle
///////////////////////////////////////////

struct LibHandle {
    addr: *const c_void,
}
unsafe impl std::marker::Sync for LibHandle {}

lazy_static! {
    static ref libGLHandle: LibHandle = {
        let handle = dlopen__next(cstr!("libGL.so"), RTLD_LAZY);
        if handle.is_null() {
            panic!("could not dlopen libGL.so")
        }
        LibHandle { addr: handle }
    };
}

///////////////////////////////////////////
// glXGetProcAddressARB wrapper
///////////////////////////////////////////

lazy_static! {
    static ref glXGetProcAddressARB: unsafe extern "C" fn(name: *const c_char) -> *const c_void = unsafe {
        let addr = dlsym(libGLHandle.addr, cstr!("glXGetProcAddressARB"));
        if addr.is_null() {
            panic!("libGL.so does not contain glXGetProcAddressARB")
        }
        std::mem::transmute(addr)
    };
}

unsafe fn getProcAddressARB(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = glXGetProcAddressARB(name.as_ptr());
    libc_println!("glXGetProcAddressARB({}) = {:x}", rustName, addr as isize);
    addr as *const ()
}

lazy_static! {
    static ref startTime: SystemTime = SystemTime::now();
}

///////////////////////////////////////////
// dlopen hook
///////////////////////////////////////////

hook_extern! {
    fn dlopen(filename: *const c_char, flags: c_int) -> *const c_void {
        let res = dlopen__next(filename, flags);
        hookLibGLIfNeeded();
        res
    }
}

///////////////////////////////////////////
// glXSwapBuffers hook
///////////////////////////////////////////

hook_dynamic! {
    getProcAddressARB => {
        fn glXSwapBuffers(display: *const c_void, drawable: *const c_void) -> () {
            libc_println!(
                "[{:08}] swapping buffers! (display=0x{:X}, drawable=0x{:X})",
                startTime.elapsed().unwrap().as_millis(),
                display as isize,
                drawable as isize
            );
            glXSwapBuffers__next(display, drawable)
        }
        fn glXQueryVersion(display: *const c_void, major: *mut c_int, minor: *mut c_int) -> c_int {
            // useless hook, just here to demonstrate we can do multiple hooks if needed
            glXQueryVersion__next(display, major, minor)
        }
    }
}

/// Returns true if libGL.so was loaded in this process,
/// either by ld-linux on startup (if linked with -lGL),
/// or by dlopen() afterwards.
unsafe fn hasLibGL() -> bool {
    // RTLD_NOLOAD lets us check if a library is already loaded.
    // we never need to dlclose() it, because we don't own it.
    !dlopen__next(cstr!("libGL.so"), RTLD_NOLOAD | RTLD_LAZY).is_null()
}

static HOOK_LIBGL_ONCE: Once = Once::new();

/// If libGL.so was loaded, and only once in the lifetime
/// of this process, hook libGL functions for libcapsule usage.
unsafe fn hookLibGLIfNeeded() {
    if hasLibGL() {
        HOOK_LIBGL_ONCE.call_once(|| {
            libc_println!("libGL usage detected, hooking libGL");
            lazy_static::initialize(&glXSwapBuffers__hook);
        })
    }
}

pub fn initialize() {
    unsafe {
        lazy_static::initialize(&dlopen__hook);
        hookLibGLIfNeeded()
    }
}
