use std::ffi::CString;
use std::mem::transmute;

const RTLD_LAZY: i32 = 0x1;

#[link(name = "dl")]
extern "C" {
    fn dlopen(name: *const i8, flags: i32) -> *const i8;
    fn dlsym(handle: *const i8, name: *const i8) -> *const i8;
}

fn main() {
    unsafe {
        let handle = dlopen(CString::new("libGL.so.1").unwrap().as_ptr(), RTLD_LAZY);
        #[allow(non_snake_case)]
        let glXSwapBuffers: unsafe extern "C" fn(dpy: *const i8, drawable: u64) = {
            transmute(dlsym(
                handle,
                CString::new("glXSwapBuffers").unwrap().as_ptr(),
            ))
        };
        glXSwapBuffers(0xDEADBEEF as *const i8, 0);
    }
}
