#![allow(non_snake_case)]

use std::ffi::CString;
use std::mem::transmute;

#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const i8) -> *const i8;
    fn GetProcAddress(handle: *const i8, name: *const i8) -> *const i8;
}

fn main() {
    unsafe {
        let handle = LoadLibraryA(CString::new("opengl32.dll").unwrap().as_ptr());
        let wglSwapBuffers: unsafe extern "C" fn(hdc: *const i8) = {
            transmute(GetProcAddress(handle, CString::new("wglSwapBuffers").unwrap().as_ptr()))
        };
        wglSwapBuffers(0xDEADBEEF as *const i8);
    }
}
