#![allow(non_snake_case)]

#[macro_use]
extern crate dlopen_derive;

use dlopen::symbor::{Library, SymBorApi, Symbol};
use libc::c_void;

#[derive(SymBorApi)]
struct LibGL<'a> {
    pub CGLFlushDrawable: Symbol<'a, unsafe extern "C" fn(ctx: *const c_void)>,
}

fn main() {
    let lib =
        Library::open("/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL").unwrap();
    let api = unsafe { LibGL::load(&lib) }.unwrap();
    unsafe {
        (api.CGLFlushDrawable)(0xDEADBEEF as *const c_void);
    }
}
