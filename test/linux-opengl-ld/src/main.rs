
#[link(name = "GL")]
extern "C" {
    fn glXSwapBuffers(dpy: *const i8, drawable: u64);
}

fn main() {
    unsafe {
        glXSwapBuffers(0xDEADBEEF as *const i8, 0);
    }
}
