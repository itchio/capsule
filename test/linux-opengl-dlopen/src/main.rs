#![allow(non_snake_case)]

#[macro_use]
extern crate dlopen_derive;

use dlopen::symbor::{Library, SymBorApi, Symbol};
use libc::{c_ulong, c_void};

#[derive(SymBorApi)]
struct LibGL<'a> {
    pub glXSwapBuffers: Symbol<'a, unsafe extern "C" fn(dpy: *const c_void, drawable: c_ulong)>,
}

fn main() {
    let lib = Library::open("libGL.so.1").unwrap();
    let api = unsafe { LibGL::load(&lib) }.unwrap();
    unsafe {
        (api.glXSwapBuffers)(0xDEADBEEF as *const c_void, 0);
    }
}
