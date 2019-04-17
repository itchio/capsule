#![cfg(any(target_os = "windows", target_os = "linux"))]

#[macro_export]
macro_rules! hook_extern {
    ($(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+) => {
        $(
            extern "C" {
                // it has the exact function signature we specified.
                fn $real_fn ($($v: $t),*) -> $r;
            }

            ::paste::item! {
                ::lazy_static::lazy_static! {
                    static ref [<$real_fn __hook>]: std::sync::Mutex<::detour::RawDetour> = unsafe {
                        let mut detour = ::detour::RawDetour::new(
                            $real_fn as *const (),
                            [<$real_fn __hooked>] as *const()
                        ).unwrap();
                        detour.enable().unwrap();
                        std::sync::Mutex::new(detour)
                    };
                    static ref [<$real_fn __next>]:
                            extern "C" fn ($($v: $t),*) -> $r = unsafe {
                        ::std::mem::transmute([<$real_fn __hook>].lock().unwrap().trampoline())
                    };
                }
            }

            ::paste::item_with_macros! {
                // finally, this is the definition of *our* version of the function.
                // you probably don't want to forget to call `XXX__next`
                unsafe extern "C" fn [<$real_fn __hooked>] ($($v: $t),*) -> $r {
                    $body
                }
            }
        )+
    };
}
