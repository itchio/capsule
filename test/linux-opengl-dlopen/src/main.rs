#![allow(non_snake_case)]

use const_cstr::*;
use libc::*;
use std::mem::transmute;

const RTLD_LAZY: c_int = 0x1;

extern "C" {
    fn dlopen(name: *const c_char, flags: c_int) -> *const c_void;
    fn dlsym(handle: *const c_void, name: *const c_char) -> *const c_void;
}

fn main() {
    unsafe {
        let handle = dlopen(const_cstr!("libGL.so.1").as_ptr(), RTLD_LAZY);
        let glXSwapBuffers: unsafe extern "C" fn(dpy: *const c_void, drawable: c_ulong) =
            { transmute(dlsym(handle, const_cstr!("glXSwapBuffers").as_ptr())) };
        glXSwapBuffers(0xDEADBEEF as *const c_void, 0);
    }
}
