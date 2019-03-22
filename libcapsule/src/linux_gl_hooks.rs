#![cfg(target_os = "linux")]

use detour::RawDetour;
use libc::{c_char, c_int, c_void};
use std::ffi::{CStr, CString};
use std::sync::Mutex;
use std::time::SystemTime;

///////////////////////////////////////////
// libdl definition & link instructions
///////////////////////////////////////////

#[allow(unused)]
const RTLD_LAZY: c_int = 0x00001;
#[allow(unused)]
const RTLD_NOW: c_int = 0x00002;

#[link(name = "dl")]
extern "C" {
    pub fn dlopen(filename: *const c_char, flags: c_int) -> *const c_void;
    pub fn dlsym(handle: *const c_void, symbol: *const c_char) -> *const c_void;
}

///////////////////////////////////////////
// lazily-opened libGL handle
///////////////////////////////////////////

struct LibHandle {
    addr: *const c_void,
}
unsafe impl std::marker::Sync for LibHandle {}

lazy_static! {
    static ref libGLHandle: LibHandle = unsafe {
        let libGLName = CString::new("libGL.so").unwrap();
        let handle = dlopen(libGLName.as_ptr(), RTLD_LAZY);
        if handle.is_null() {
            panic!("could not dlopen libGL.so")
        }
        LibHandle { addr: handle }
    };
}

unsafe fn getLibGLSymbol(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = dlsym(libGLHandle.addr, name.as_ptr());
    if addr.is_null() {
        panic!(format!("dlsym({}) = NULL!", rustName))
    }
    libc_println!("dlsym({}) = {:x}", rustName, addr as isize);
    addr as *const ()
}

///////////////////////////////////////////
// glXGetProcAddressARB wrapper
///////////////////////////////////////////

lazy_static! {
    static ref glXGetProcAddressARB: unsafe extern "C" fn(name: *const c_char) -> *const c_void = unsafe {
        let rustName = "glXGetProcAddressARB";
        let name = CString::new(rustName).unwrap();
        let addr = dlsym(libGLHandle.addr, name.as_ptr());
        if addr.is_null() {
            panic!(format!("dlsym({}) = NULL!", rustName))
        }
        std::mem::transmute(addr)
    };
}

unsafe fn getProcAddressARB(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = glXGetProcAddressARB(name.as_ptr());
    libc_println!("glXGetProcAddressARB({}) = {:x}", rustName, addr as isize);
    addr as *const ()
}

lazy_static! {
    static ref startTime: SystemTime = SystemTime::now();
}

///////////////////////////////////////////
// glXGetProcAddressARB hook
///////////////////////////////////////////

lazy_static! {
    static ref glXGetProcAddressARB__hook: Mutex<RawDetour> = unsafe {
        let mut detour = RawDetour::new(
            getLibGLSymbol(&"glXGetProcAddressARB"),
            glXGetProcAddressARB__hooked as *const (),
        )
        .unwrap();
        detour.enable().unwrap();
        Mutex::new(detour)
    };
    static ref glXGetProcAddressARB__next: unsafe extern "C" fn(name: *const c_char) -> *const c_void =
        unsafe { std::mem::transmute(glXGetProcAddressARB__hook.lock().unwrap().trampoline()) };
}

unsafe extern "C" fn glXGetProcAddressARB__hooked(name: *const c_char) -> *const c_void {
    let rustName = match CStr::from_ptr(name).to_str() {
        Ok(x) => x,
        _ => "<nil>",
    };
    let result = glXGetProcAddressARB__next(name);
    libc_println!("getProcAddressARB({}) = {:x}", rustName, result as isize,);
    result
}

///////////////////////////////////////////
// glXSwapBuffers hook
///////////////////////////////////////////

lazy_static! {
    static ref glXSwapBuffers__hook: Mutex<RawDetour> = unsafe {
        let mut detour = RawDetour::new(
            getProcAddressARB(&"glXSwapBuffers"), // N.B: used to be a call getLibGLSymbol
            glXSwapBuffers__hooked as *const (),
        )
        .unwrap();
        detour.enable().unwrap();
        Mutex::new(detour)
    };
    static ref glXSwapBuffers__next: unsafe extern "C" fn(display: *const c_void, drawable: *const c_void) -> () =
        unsafe { std::mem::transmute(glXSwapBuffers__hook.lock().unwrap().trampoline()) };
}

unsafe extern "C" fn glXSwapBuffers__hooked(display: *const c_void, drawable: *const c_void) -> () {
    libc_println!(
        "[{:08}] swapping buffers! (display=0x{:X}, drawable=0x{:X})",
        startTime.elapsed().unwrap().as_millis(),
        display as isize,
        drawable as isize
    );
    glXSwapBuffers__next(display, drawable)
}

pub fn initialize() {
    lazy_static::initialize(&glXSwapBuffers__hook);
    lazy_static::initialize(&glXGetProcAddressARB__hook);
}
