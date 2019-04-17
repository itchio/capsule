#![cfg(target_os = "linux")]

use libc::{c_char, c_void};

#[link(name = "dl")]
extern "C" {
  fn __libc_dlsym(handle: *const c_void, symbol: *const c_char) -> *const c_void;
  fn dlsym(handle: *const c_void, symbol: *const c_char) -> *const c_void;
}

const RTLD_NEXT: *const c_void = -1isize as *const c_void;

pub unsafe fn dlsym_next(nul_terminated_symbol: &'static str) -> *const u8 {
  let ptr = dlsym(RTLD_NEXT, nul_terminated_symbol.as_ptr() as *const c_char);
  if ptr.is_null() {
    panic!(
      "Unable to find underlying function for {}",
      nul_terminated_symbol
    );
  }
  ptr as *const u8
}

#[macro_export]
macro_rules! hook_dlsym {
    ($link_name:literal => $(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+) => {
        $(
            #[link(name = $link_name)]
            extern "C" {}

            ::paste::item! {
                ::lazy_static::lazy_static! {
                    static ref [<$real_fn __next>]:
                            extern "C" fn ($($v: $t),*) -> $r = unsafe {
                        ::std::mem::transmute($crate::hook::dlsym::dlsym_next(concat!(stringify!($real_fn), "\0")))
                    };
                }
            }

            ::paste::item_with_macros! {
                #[no_mangle]
                pub unsafe extern fn $real_fn ($($v: $t),*) -> $r {
                    $body
                }
            }
        )+
    };
}
