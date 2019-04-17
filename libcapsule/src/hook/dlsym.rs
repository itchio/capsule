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

            #[allow(non_camel_case_types)]
            pub struct $real_fn {}

            impl $real_fn {
              fn next($($v : $t),*) -> $r {
                use ::std::sync::{Once, ONCE_INIT};
                use ::std::mem::transmute;
                use $crate::hook::dlsym::dlsym_next;

                static mut NEXT: *const u8 = 0 as *const u8;
                static mut ONCE: Once = ONCE_INIT;

                unsafe {
                  ONCE.call_once(|| {
                    NEXT = dlsym_next(concat!(stringify!($real_fn), "\0"));
                  });
                  let next: unsafe extern fn ($($v : $t),*) -> $r = transmute(NEXT);
                  next($($v),+)
                }
              }

              #[allow(dead_code)]
              fn is_hooked() -> bool { Self::state(None) }

              #[allow(dead_code)]
              fn hook() { Self::state(Some(true)); }

              #[allow(dead_code)]
              fn unhook() { Self::state(Some(true)); }

              #[allow(dead_code)]
              fn state(new_value: Option<bool>) -> bool {
                static mut ENABLED: bool = false;
                unsafe {
                  if let Some(value) = new_value { ENABLED = value; }
                  ENABLED
                }
              }
            }

            #[no_mangle]
            pub unsafe extern "C" fn $real_fn ($($v: $t),*) -> $r {
              if !$real_fn::state(None) {
                return $real_fn::next($($v),*)
              }
              $body
            }
        )+
    };
}
