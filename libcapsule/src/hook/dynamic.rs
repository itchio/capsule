
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