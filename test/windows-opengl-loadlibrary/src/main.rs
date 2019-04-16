#![allow(non_snake_case)]

#[macro_use]
extern crate dlopen_derive;

use dlopen::symbor::{Library, SymBorApi, Symbol};
use libc::{c_void};

#[derive(SymBorApi)]
struct LibGL<'a> {
    pub wglSwapBuffers: Symbol<'a, unsafe extern "C" fn(hdc: *const c_void)>,
}

fn main() {
    let lib = Library::open("opengl32.dll").unwrap();
    let api = unsafe { LibGL::load(&lib) }.unwrap();
    unsafe {
        (api.wglSwapBuffers)(0xDEADBEEF as *const c_void);
    }
}
