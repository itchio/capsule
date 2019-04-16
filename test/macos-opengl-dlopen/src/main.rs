#![allow(non_snake_case)]

use libc::c_void;

#[link(name = "OpenGL", kind = "framework")]
extern "C" {
    fn CGLFlushDrawable(ctx: *const c_void);
}

fn main() {
    unsafe {
        CGLFlushDrawable(0xDEADBEEF as *const c_void);
    }
}
