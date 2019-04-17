#[macro_use]
pub mod dlsym;
pub use dlsym::*;

#[cfg(any(target_os = "windows", target_os = "linux"))]
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

#[cfg(target_os = "macos")]
#[repr(C)]
pub struct Interposition {
    pub replacement: *const (),
    pub replacee: *const (),
}

#[cfg(target_os = "macos")]
unsafe impl Sync for Interposition {}

#[cfg(target_os = "macos")]
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

#[macro_export]
macro_rules! hook_dynamic {
    ($get_proc_address:ident => { $(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+ }) => {
        hook_dynamic! {
            extern "C" use $get_proc_address => { $(fn $real_fn($($v: $t),*) -> $r $body)+ }
        }
    };

    (extern $convention:literal use $get_proc_address:ident => { $(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+ }) => {
        $(
            ::paste::item! {
                ::lazy_static::lazy_static! {
                    static ref [<$real_fn __hook>]: std::sync::Mutex<::detour::RawDetour> = unsafe {
                        let mut detour = ::detour::RawDetour::new(
                            $get_proc_address(stringify!($real_fn)) as *const (),
                            [<$real_fn __hooked>] as *const()
                        ).unwrap();
                        detour.enable().unwrap();
                        std::sync::Mutex::new(detour)
                    };
                    static ref [<$real_fn __next>]:
                            extern $convention fn ($($v: $t),*) -> $r = unsafe {
                        ::std::mem::transmute([<$real_fn __hook>].lock().unwrap().trampoline())
                    };
                }
            }

            ::paste::item_with_macros! {
                unsafe extern $convention fn [<$real_fn __hooked>] ($($v: $t),*) -> $r {
                    $body
                }
            }
        )+
    };
}
