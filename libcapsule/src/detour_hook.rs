#[macro_export]
macro_rules! detour_hook {
    ($(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+) => {
        $(
            paste::item! {
                const_cstr! {
                    [<$real_fn __name>] = stringify!($real_fn);
                }

                lazy_static! {
                    pub static ref [<$real_fn __next>]: extern "C" fn ($($v: $t),*) -> $r = unsafe {
                        let sym = libc::dlsym(libc::RTLD_NEXT, [<$real_fn __name>].as_ptr());
                        ::std::mem::transmute(sym)
                    };
                }
            }

            #[no_mangle]
            pub unsafe extern "C" fn $real_fn ($($v: $t),*) -> $r {
                $body
            }
        )+
    };

}
