#![cfg(target_os = "macos")]

#[repr(C)]
pub struct Interposition {
  pub replacement: *const (),
  pub replacee: *const (),
}

unsafe impl Sync for Interposition {}

#[macro_export]
macro_rules! hook_extern {
    ($(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+) => {
        $(
            extern "C" {
                // it has the exact function signature we specified.
                fn $real_fn ($($v: $t),*) -> $r;
            }

            ::paste::item! {
                static [<$real_fn __enabled>]: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);
            }

            ::paste::item_with_macros! {
                ::lazy_static::lazy_static! {
                    static ref [<$real_fn __hook>]: () = {
                        [<$real_fn __enabled>].store(true, std::sync::atomic::Ordering::Relaxed);
                        ()
                    };
                }
            }

            ::paste::item_with_macros! {
                // finally, this is the definition of *our* version of the function.
                // you probably don't want to forget to call `XXX__next`
                unsafe extern "C" fn [<$real_fn __hooked>] ($($v: $t),*) -> $r {
                    if ![<$real_fn __enabled>].load(std::sync::atomic::Ordering::Relaxed) {
                        return [<$real_fn __next>]($($v),*);
                    }

                    $body
                }
            }

            ::paste::item! {
                extern "C" fn [<$real_fn __next>] ($($v: $t),*) -> $r {
                    unsafe {
                        $real_fn($($v),*)
                    }
                }
            }

            ::paste::item! {
                #[used]
                #[no_mangle]
                #[link_section = "__DATA,__interpose"]
                static [<interpose__ $real_fn __replacee>]: $crate::hook::Interposition = $crate::hook::Interposition {
                    replacement: [<$real_fn __hooked>] as *const (),
                    replacee: $real_fn as *const (),
                };
            }
        )+
    };
}
