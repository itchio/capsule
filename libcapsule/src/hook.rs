#[macro_export]
macro_rules! hook_extern {
    ($(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+) => {
        $(
            paste::item! {
                lazy_static! {
                    static ref [<$real_fn __hook>]: std::sync::Mutex<detour::RawDetour> = unsafe {
                        let mut detour = RawDetour::new(
                            linux_gl_hooks::dlsym(
                                linux_gl_hooks::RTLD_DEFAULT,
                                cstr!(stringify!($real_fn))
                            ) as *const(),
                            $real_fn as *const ()
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

            unsafe extern "C" fn $real_fn ($($v: $t),*) -> $r {
                $body
            }
        )+
    };

}
