#![cfg(target_os = "linux")]

use super::types::*;
use detour::RawDetour;

pub fn initialize() {
    lazy_static::initialize(&glXSwapBuffers__hook);
}

extern "C" {
    pub fn glXSwapBuffers(display: VoidPtr, drawable: VoidPtr) -> ();
}

unsafe extern "C" fn glXSwapBuffers__hooked(display: VoidPtr, drawable: VoidPtr) -> () {
    libc_println!(
        "[{:08}] swapping buffers! (display=0x{:X}, drawable=0x{:X})",
        startTime.elapsed().unwrap().as_millis(),
        display as isize,
        drawable as isize
    );
    glXSwapBuffers__next(display, drawable)
}

lazy_static! {
    static ref startTime: std::time::SystemTime = std::time::SystemTime::now();

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
