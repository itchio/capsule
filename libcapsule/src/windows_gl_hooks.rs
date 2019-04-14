#![cfg(target_os = "windows")]

use libc::c_void;
use std::ffi::CString;
use std::sync::Once;
use std::time::SystemTime;
use winapi::shared::{minwindef, windef};
use winapi::um::{libloaderapi, winnt};

///////////////////////////////////////////
// lazily-opened opengl32.dll handle
///////////////////////////////////////////

struct LibHandle {
    module: minwindef::HMODULE,
}
unsafe impl std::marker::Sync for LibHandle {}

lazy_static! {
    static ref opengl32_handle: LibHandle = unsafe {
        let module = libloaderapi::LoadLibraryW(wstrz!("opengl32.dll").as_ptr());
        assert_non_null!("LoadLibraryW(opengl32.dll)", module);
        LibHandle { module: module }
    };
}

unsafe fn getOpengl32ProcAddress(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = libloaderapi::GetProcAddress(opengl32_handle.module, name.as_ptr());
    libc_println!(
        "GetProcAddress(opengl32.dll, {}) = {:x}",
        rustName,
        addr as isize
    );
    addr as *const ()
}

///////////////////////////////////////////
// wglGetProcAddress wrapper
///////////////////////////////////////////

lazy_static! {
    static ref wglGetProcAddress: unsafe extern "C" fn(name: winnt::LPCSTR) -> *const c_void = unsafe {
        let addr = libloaderapi::GetProcAddress(opengl32_handle.module, cstr!("wglGetProcAddress"));
        if addr.is_null() {
            panic!("opengl32.dll does not contain wglGetProcAddress")
        }
        std::mem::transmute(addr)
    };
}

unsafe fn getWglProcAddress(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = wglGetProcAddress(name.as_ptr());
    libc_println!("wglGetProcAddress({}) = {:x}", rustName, addr as isize);
    addr as *const ()
}

lazy_static! {
    static ref startTime: SystemTime = SystemTime::now();
}

hook_dynamic! {
    getOpengl32ProcAddress => {
        fn wglSwapBuffers(hdc: windef::HDC) -> () {
            libc_println!(
                "[{:08}] swapping buffers! ()",
                startTime.elapsed().unwrap().as_millis(),
            );
            wglSwapBuffers__next(hdc)
        }
    }
}

static HOOK_ONCE: Once = Once::new();

fn is_using_opengl() -> bool {
    let opengl_module = wincap::get_module_if_loaded("opengl32.dll");
    !opengl_module.is_null()
}

unsafe fn hook_if_needed() {
    if is_using_opengl() {
        HOOK_ONCE.call_once(|| {
            libc_println!("opengl32.dll usage detected, hooking OpenGL");
            lazy_static::initialize(&wglSwapBuffers__hook);
        })
    }
}

pub fn initialize() {
    unsafe {
        hook_if_needed();
    }
}
