#![allow(non_snake_case)]

#[link(name = "OpenGL", kind = "framework")]
extern "C" {
    fn CGLFlushDrawable(ctx: *const i8);
}

fn main() {
    unsafe {
        CGLFlushDrawable(0xDEADBEEF as *const i8);
    }
}
