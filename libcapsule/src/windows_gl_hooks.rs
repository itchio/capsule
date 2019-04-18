#![cfg(target_os = "windows")]

use crate::gl;
use const_cstr::const_cstr;
use lazy_static::lazy_static;
use libc::c_void;
use libc_print::libc_println;
use log::*;
use std::ffi::CString;
use std::sync::Once;
use winapi::shared::{minwindef, windef};
use winapi::um::{libloaderapi, winnt};
use wincap::assert_non_null;

///////////////////////////////////////////
// lazily-opened opengl32.dll handle
///////////////////////////////////////////

struct LibHandle {
    module: minwindef::HMODULE,
}
unsafe impl std::marker::Sync for LibHandle {}

lazy_static! {
    static ref opengl32_handle: LibHandle = {
        // it's *NOT* safe to call libloadingapi::LoadLibraryA here,
        // because it's already hooked.
        let module = LoadLibraryA::next(const_cstr!("opengl32.dll").as_ptr());
        assert_non_null!("LoadLibraryW(opengl32.dll)", module);
        LibHandle { module: module }
    };
}

unsafe fn get_opengl32_proc_address(rust_name: &str) -> *const () {
    let name = CString::new(rust_name).unwrap();
    let addr = libloaderapi::GetProcAddress(opengl32_handle.module, name.as_ptr());

    info!(
        "get_opengl32_proc_address({}) = {:x}",
        rust_name, addr as isize
    );
    addr as *const ()
}

///////////////////////////////////////////
// wglGetProcAddress wrapper
///////////////////////////////////////////

lazy_static! {
    static ref wglGetProcAddress: unsafe extern "system" fn(name: winnt::LPCSTR) -> *const c_void = unsafe {
        let addr = libloaderapi::GetProcAddress(
            opengl32_handle.module,
            const_cstr!("wglGetProcAddress").as_ptr(),
        );
        if addr.is_null() {
            panic!("opengl32.dll does not contain wglGetProcAddress. That seems unlikely.")
        }
        std::mem::transmute(addr)
    };
}

unsafe fn get_wgl_proc_address(rust_name: &str) -> *const () {
    let name = CString::new(rust_name).unwrap();
    let addr = wglGetProcAddress(name.as_ptr());
    info!("get_wgl_proc_address({}) = {:x}", rust_name, addr as isize);

    if addr.is_null() {
        return get_opengl32_proc_address(rust_name);
    }
    addr as *const ()
}

hook_dynamic! {
    extern "system" use get_opengl32_proc_address => {
        fn wglSwapBuffers(hdc: windef::HDC) -> minwindef::BOOL {
            if super::SETTINGS.in_test && hdc as usize == 0xDEADBEEF as usize {
                libc_println!("caught dead beef");
                std::process::exit(0);
            }

            capture_gl_frame();
            wglSwapBuffers::next(hdc)
        }
    }
}

////////////////////////////////////

lazy_static! {
    static ref kernel32_handle: LibHandle = unsafe {
        // this is the ONLY safe place to call libloaderapi::LoadLibraryW
        // after that, it's hooked.
        let module = libloaderapi::LoadLibraryA(const_cstr!("kernel32.dll").as_ptr());
        assert_non_null!("LoadLibraryW(kernel32.dll)", module);
        LibHandle { module: module }
    };
}

unsafe fn get_kernel32_proc_address(rust_name: &str) -> *const () {
    let name = CString::new(rust_name).unwrap();
    let addr = libloaderapi::GetProcAddress(kernel32_handle.module, name.as_ptr());
    info!(
        "get_kernel32_proc_address({}) = {:x}",
        rust_name, addr as isize
    );

    addr as *const ()
}

///////////////////////////////////////////
// LoadLibrary hooks
///////////////////////////////////////////

hook_dynamic! {
    extern "system" use get_kernel32_proc_address => {
        fn LoadLibraryA(filename: winnt::LPCSTR) -> minwindef::HMODULE {
            let res = LoadLibraryA::next(filename);
            if !filename.is_null() {
                let s = std::ffi::CStr::from_ptr(filename).to_string_lossy();
                info!("LoadLibraryA({}) = {:x}", s, res as usize);
            }
            hook_if_needed();
            res
        }

        fn LoadLibraryW(filename: winnt::LPCWSTR) -> minwindef::HMODULE {
            let res = LoadLibraryW::next(filename);
            if !filename.is_null() {
                let s = widestring::U16CStr::from_ptr_str(filename).to_string_lossy();
                info!("LoadLibraryW({}) = {:x}", s, res as usize);
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
            info!("opengl32.dll usage detected, hooking OpenGL");
            wglSwapBuffers::enable_hook();
        })
    }
}

pub fn initialize() {
    unsafe {
        LoadLibraryA::enable_hook();
        LoadLibraryW::enable_hook();
        hook_if_needed();
    }
}

unsafe fn capture_gl_frame() {
    let cc = gl::get_capture_context(get_wgl_proc_address);
    cc.capture_frame();
}
