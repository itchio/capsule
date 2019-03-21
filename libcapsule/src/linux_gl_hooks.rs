#![cfg(target_os = "linux")]

use super::types::*;
use detour::RawDetour;

pub fn initialize() {
    lazy_static::initialize(&glXGetProcAddressARB__hook);
    lazy_static::initialize(&glXSwapBuffers__hook);
}

extern "C" {
    pub fn glXGetProcAddressARB(symbol: CharPtr) -> VoidPtr;
    pub fn glXSwapBuffers(display: VoidPtr, drawable: VoidPtr) -> ();
}

unsafe extern "C" fn glXGetProcAddressARB__hooked(symbol: CharPtr) -> VoidPtr {
    {
        let symbol = std::ffi::CStr::from_ptr(symbol)
            .to_string_lossy()
            .into_owned();
        libc_println!("glXGetProcAddressARB called with {}", symbol)
    }
    glXGetProcAddressARB__next(symbol)
}

unsafe extern "C" fn glXSwapBuffers__hooked(display: VoidPtr, drawable: VoidPtr) -> () {
    libc_println!(
        "swapping buffers! {} {}",
        display as isize,
        drawable as isize
    );
    glXSwapBuffers__next(display, drawable)
}

lazy_static! {
    // glXGetProcAddressARB
    static ref glXGetProcAddressARB__hook: std::sync::Mutex<RawDetour> = unsafe {
        let mut detour = RawDetour::new(
            glXGetProcAddressARB as *const (),
            glXGetProcAddressARB__hooked as *const (),
        )
        .unwrap();
        detour.enable().unwrap();
        std::sync::Mutex::new(detour)
    };
    static ref glXGetProcAddressARB__next: unsafe extern "C" fn(symbol: CharPtr) -> VoidPtr =
        unsafe { std::mem::transmute(glXGetProcAddressARB__hook.lock().unwrap().trampoline()) };
    // glXSwapBuffers
    static ref glXSwapBuffers__hook: std::sync::Mutex<RawDetour> = unsafe {
        let mut detour = RawDetour::new(
            glXSwapBuffers as *const (),
            glXSwapBuffers__hooked as *const (),
        )
        .unwrap();
        detour.enable().unwrap();
        std::sync::Mutex::new(detour)
    };
    static ref glXSwapBuffers__next: unsafe extern "C" fn(display: VoidPtr, drawable: VoidPtr) -> () =
        unsafe { std::mem::transmute(glXSwapBuffers__hook.lock().unwrap().trampoline()) };
}
