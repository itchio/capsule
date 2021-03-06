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
        let handle = dlopen(
            CString::new("/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL")
                .unwrap()
                .as_ptr(),
            RTLD_LAZY,
        );
        #[allow(non_snake_case)]
        let CGLFlushDrawable: unsafe extern "C" fn(ctx: *const i8) = {
            transmute(dlsym(
                handle,
                CString::new("CGLFlushDrawable").unwrap().as_ptr(),
            ))
        };
        CGLFlushDrawable(0xDEADBEEF as *const i8);
    }
}
