#![cfg(target_os = "windows")]

use const_cstr::const_cstr;
use lazy_static::lazy_static;
use libc::c_void;
use log::*;
use std::ffi::CString;
use winapi::shared::minwindef;
use winapi::um::libloaderapi;
use wincap::assert_non_null;

use winapi::shared::dxgi::*;
use winapi::shared::dxgi1_2::*;
use winapi::shared::guiddef::*;
use winapi::shared::ntdef::*;
use winapi::shared::windef::HWND;
use winapi::um::unknwnbase::IUnknown;

// TODO: dedup with ../windows_gl_hooks.rs
struct LibHandle {
    module: minwindef::HMODULE,
}
unsafe impl std::marker::Sync for LibHandle {}

lazy_static! {
    static ref dxgi_handle: LibHandle = {
        let module = crate::windows_gl_hooks::LoadLibraryA::next(const_cstr!("dxgi.dll").as_ptr());
        assert_non_null!("LoadLibraryW(dxgi.dll)", module);
        LibHandle { module: module }
    };
}

unsafe fn get_dxgi_proc_address(rust_name: &str) -> *const () {
    let name = CString::new(rust_name).unwrap();
    let addr = libloaderapi::GetProcAddress(dxgi_handle.module, name.as_ptr());
    info!("get_dxgi_proc_address({}) = {:x}", rust_name, addr as isize);

    addr as *const ()
}

hook_dynamic! {
    extern "system" use get_dxgi_proc_address => {
        fn CreateDXGIFactory(riid: REFIID, ppFactory: *mut *mut c_void) -> HRESULT {
            info!("Calling CreateDXGIFactory");
            let res = CreateDXGIFactory::next(riid, ppFactory);
            if wincap::succeeded(res) {
                let ppFact: *mut *mut IDXGIFactory2 = std::mem::transmute(ppFactory);
                let vTable = (**ppFact).lpVtbl;
                CreateSwapChainForHwnd_address = Some(FunctionHandle{
                    addr: std::mem::transmute((*vTable).CreateSwapChainForHwnd),
                });
                CreateSwapChainForHwnd::enable_hook();
            }
            info!("Returned from CreateDXGIFactory");
            res
        }
    }
}

struct FunctionHandle {
    addr: *const (),
}
unsafe impl Sync for FunctionHandle {}

static mut CreateSwapChainForHwnd_address: Option<FunctionHandle> = None;

unsafe fn get_createswapchainforhwnd_address(_rust_name: &str) -> *const () {
    std::mem::transmute(
        CreateSwapChainForHwnd_address
            .as_ref()
            .expect("No address for CreateSwapChainForHwnd yet")
            .addr,
    )
}

hook_dynamic! {
    extern "system" use get_createswapchainforhwnd_address => {
        fn CreateSwapChainForHwnd(
            factory: *mut c_void,
            pDevice: *mut IUnknown,
            hWnd: HWND,
            pDesc: *const DXGI_SWAP_CHAIN_DESC1,
            pFullscreenDesc: *const DXGI_SWAP_CHAIN_FULLSCREEN_DESC,
            pRestrictToOutput: *mut IDXGIOutput,
            ppSwapChain: *mut *mut IDXGISwapChain1
        ) -> HRESULT {
            info!("Calling CreateSwapChainForHwnd");
            let res = CreateSwapChainForHwnd::next(factory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput,
                ppSwapChain);
            info!("Returned from CreateSwapChainForHwnd");
            res
        }
    }
}

pub fn initialize() {
    CreateDXGIFactory::enable_hook();
}
