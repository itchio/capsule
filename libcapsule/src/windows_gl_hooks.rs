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
        // it's *NOT* safe to call libloadingapi::LoadLibraryW here
        let module = LoadLibraryW__next(wstrz!("opengl32.dll").as_ptr());
        assert_non_null!("LoadLibraryW(opengl32.dll)", module);
        LibHandle { module: module }
    };
}

unsafe fn get_opengl32_proc_address(rust_name: &str) -> *const () {
    let name = CString::new(rust_name).unwrap();
    let addr = libloaderapi::GetProcAddress(opengl32_handle.module, name.as_ptr());
    libc_println!(
        "GetProcAddress(opengl32.dll, {}) = {:x}",
        rust_name,
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
            panic!("opengl32.dll does not contain wglGetProcAddress. That seems unlikely.")
        }
        std::mem::transmute(addr)
    };
}

unsafe fn get_wgl_proc_address(rustName: &str) -> *const () {
    let name = CString::new(rustName).unwrap();
    let addr = wglGetProcAddress(name.as_ptr());
    libc_println!("wglGetProcAddress({}) = {:x}", rustName, addr as isize);
    addr as *const ()
}

hook_dynamic! {
    get_opengl32_proc_address => {
        fn wglSwapBuffers(hdc: windef::HDC) -> () {
            libc_println!(
                "[{:08}] swapping buffers! ()",
                startTime.elapsed().unwrap().as_millis(),
            );
            wglSwapBuffers__next(hdc)
        }
    }
}

////////////////////////////////////

lazy_static! {
    static ref kernel32_handle: LibHandle = unsafe {
        // this is the ONLY safe place to call libloaderapi::LoadLibraryW
        // after that, it's hooked.
        let module = libloaderapi::LoadLibraryW(wstrz!("kernel32.dll").as_ptr());
        assert_non_null!("LoadLibraryW(kernel3232.dll)", module);
        LibHandle { module: module }
    };
}

unsafe fn get_kernel32_proc_address(rust_name: &str) -> *const () {
    let name = CString::new(rust_name).unwrap();
    let addr = libloaderapi::GetProcAddress(kernel32_handle.module, name.as_ptr());
    libc_println!(
        "GetProcAddress(kernel32.dll, {}) = {:x}",
        rust_name,
        addr as isize
    );

    addr as *const ()
}

lazy_static! {
    static ref startTime: SystemTime = SystemTime::now();
}

///////////////////////////////////////////
// LoadLibraryW hook
///////////////////////////////////////////

hook_dynamic! {
    get_kernel32_proc_address => {
        fn LoadLibraryA(filename: winnt::LPSTR) -> minwindef::HMODULE {
            let res = LoadLibraryA__next(filename);
            {
                let s = std::ffi::CStr::from_ptr(filename).to_string_lossy();
                libc_println!("LoadLibraryA({}) = {:x}", s, res as usize);
            }
            hook_if_needed();
            res
        }

        fn LoadLibraryW(filename: winnt::LPCWSTR) -> minwindef::HMODULE {
            let res = LoadLibraryW__next(filename);
            {
                let s = widestring::U16CStr::from_ptr_str(filename).to_string_lossy();
                libc_println!("LoadLibraryW({}) = {:x}", s, res as usize);
            }
            hook_if_needed();
            res
        }
    }
}

static HOOK_SWAPBUFFERS_ONCE: Once = Once::new();

fn is_using_opengl() -> bool {
    let opengl_module = wincap::get_module_if_loaded("opengl32.dll");
    !opengl_module.is_null()
}

unsafe fn hook_if_needed() {
    if is_using_opengl() {
        HOOK_SWAPBUFFERS_ONCE.call_once(|| {
            libc_println!("opengl32.dll usage detected, hooking OpenGL");
            lazy_static::initialize(&wglSwapBuffers__hook);
        })
    }
}

pub fn initialize() {
    unsafe {
        lazy_static::initialize(&LoadLibraryA__hook);
        lazy_static::initialize(&LoadLibraryW__hook);
        hook_if_needed();
    }
}
