#[macro_export]
macro_rules! hook_dynamic {
    ($get_proc_address:ident => { $(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+ }) => {
        hook_dynamic! {
            extern "C" use $get_proc_address => { $(fn $real_fn($($v: $t),*) -> $r $body)+ }
        }
    };

    (extern $convention:tt use $get_proc_address:ident => { $(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+ }) => {
        $(
            #[allow(non_camel_case_types)]
            pub struct $real_fn {}

            impl $real_fn {
                // not allowing unused here so we don't leave hook declarations
                // that never get enabled. it's not ideal because we won't know
                // which hook isn't enabled, but.. ahwell.
                fn enable_hook() {
                    unsafe {
                        let d = Self::get_detour().expect("failed to get detour");
                        const err: &str = concat!("failed to enable ", stringify!($real_fn), " detour via ", stringify!($get_proc_address));
                        d.enable().expect(err);
                    }
                }

                #[allow(unused)]
                fn disable_hook() {
                    unsafe {
                        let d = Self::get_detour().expect("failed to get detour");
                        const err: &str = concat!("failed to disable ", stringify!($real_fn), " detour via ", stringify!($get_proc_address));
                        d.disable().expect(err);
                    }
                }

                #[allow(unused)]
                pub fn next($($v: $t),*) -> $r {
                    unsafe {
                        let real: unsafe extern $convention fn ($($t),*) -> $r = {
                            ::std::mem::transmute(Self::get_detour().unwrap().trampoline())
                        };
                        real($($v),*)
                    }
                }

                fn get_detour() -> Option<&'static mut $crate::detour::RawDetour> {
                    use ::std::sync::{Once, ONCE_INIT};
                    static mut DETOUR: Option<$crate::detour::RawDetour> = None;
                    static mut ONCE: Once = ONCE_INIT;

                    unsafe {
                        ONCE.call_once(|| {
                            let d = $crate::detour::RawDetour::new(
                                $get_proc_address(stringify!($real_fn)) as *const (),
                                Self::hook as *const()
                            ).expect(concat!("failed to detour ", stringify!($real_fn), " via ", stringify!($get_proc_address)));
                            DETOUR = Some(d)
                        });
                        DETOUR.as_mut()
                    }
                }

                unsafe extern $convention fn hook($($v: $t),*) -> $r {
                    $body
                }
            }
        )+
    };
}
