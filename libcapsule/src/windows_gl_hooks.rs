#![cfg(target_os = "windows")]

use libc::c_void;
use std::ffi::CString;
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

pub fn initialize() {
    unsafe {
        libc_println!("opengl32_handle = {:x}", opengl32_handle.module as usize);
        libc_println!(
            "wglSwapBuffers = {:x}",
            getOpengl32ProcAddress("wglSwapBuffers") as usize
        );
        lazy_static::initialize(&wglSwapBuffers__hook);
    }

    std::thread::spawn(move || {
        let mut i = 0;
        loop {
            std::thread::sleep(std::time::Duration::from_secs(1));
            libc_println!("[capsule] saying hi (loop {})", i);
            i += 1;
        }
    });
}
