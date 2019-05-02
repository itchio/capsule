#![cfg(target_os = "linux")]

#[link(name = "dl")]
extern "C" {}

use crate::gl;
use const_cstr::const_cstr;
use lazy_static::lazy_static;
use libc::{c_char, c_int, c_ulong, c_void};
use libc_print::libc_println;
use log::*;
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
    pub fn dlclose(handle: *const c_void);
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
        use const_cstr::*;
        let handle = dlopen::next(const_cstr!("libGL.so.1").as_ptr(), RTLD_LAZY);
        if handle.is_null() {
            panic!("could not dlopen libGL.so.1")
        }
        LibHandle { addr: handle }
    };
}

///////////////////////////////////////////
// glXGetProcAddressARB wrapper
///////////////////////////////////////////

lazy_static! {
    static ref glXGetProcAddressARB: unsafe extern "C" fn(name: *const c_char) -> *const c_void = unsafe {
        let addr = dlsym(
            libGLHandle.addr,
            const_cstr!("glXGetProcAddressARB").as_ptr(),
        );
        if addr.is_null() {
            panic!("libGL.so.1 does not contain glXGetProcAddressARB")
        }
        std::mem::transmute(addr)
    };
}

unsafe fn get_glx_proc_address(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = glXGetProcAddressARB(name.as_ptr());
    info!("glXGetProcAddressARB({}) = {:x}", rustName, addr as isize);
    addr as *const ()
}

lazy_static! {
    static ref startTime: SystemTime = SystemTime::now();
}

///////////////////////////////////////////
// dlopen hook
///////////////////////////////////////////

hook_ld! {
    "dl" => fn dlopen(filename: *const c_char, flags: c_int) -> *const c_void {
        let res = dlopen::next(filename, flags);
        hook_if_needed();
        res
    }
}

///////////////////////////////////////////
// glXSwapBuffers hook
///////////////////////////////////////////

hook_dynamic! {
    get_glx_proc_address => {
        fn glXSwapBuffers(display: *const c_void, drawable: c_ulong) -> () {
            if super::SETTINGS.in_test && display == 0xDEADBEEF as *const c_void {
                libc_println!("caught dead beef");
                std::process::exit(0);
            }

            let cc = gl::get_capture_context(get_glx_proc_address);
            cc.capture_frame();

            glXSwapBuffers::next(display, drawable)
        }
    }
}

/// Returns true if libGL.so.1 was loaded in this process,
/// either by ld-linux on startup (if linked with -lGL),
/// or by dlopen() afterwards.
unsafe fn is_using_opengl() -> bool {
    // RTLD_NOLOAD lets us check if a library is already loaded.
    // FIXME: we actually do need to call dlclose here, apparently
    // modules are reference-counted.
    let handle = dlopen::next(const_cstr!("libGL.so.1").as_ptr(), RTLD_NOLOAD | RTLD_LAZY);
    let using = !handle.is_null();
    if using {
        dlclose(handle);
    }
    using
}

static HOOK_SWAPBUFFERS_ONCE: Once = Once::new();

/// If libGL.so.1 was loaded, and only once in the lifetime
/// of this process, hook libGL functions for libcapsule usage.
unsafe fn hook_if_needed() {
    if is_using_opengl() {
        HOOK_SWAPBUFFERS_ONCE.call_once(|| {
            info!("libGL usage detected, hooking OpenGL");
            glXSwapBuffers::enable_hook();
        })
    }
}

pub fn initialize() {
    unsafe { hook_if_needed() }
}

lazy_static! {
    static ref frame_buffer: Vec<u8> = {
        let num_bytes = (1920 * 1080 * 4) as usize;
        let mut data = Vec::<u8>::with_capacity(num_bytes);
        data.resize(num_bytes, 0);
        data
    };
}
