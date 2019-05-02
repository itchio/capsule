#![cfg(target_os = "macos")]

#[macro_export]
macro_rules! hook_dyld {
    (($link_name:literal as $link_kind:literal) => { $(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+ }) => {
        $(
            pub mod $real_fn {
                #[link(name=$link_name, kind=$link_kind)]
                extern "C" {
                    // dummy function signature so we don't have to import the types
                    pub fn $real_fn () -> ();
                }

                #[allow(non_camel_case_types)]
                pub struct S {
                    pub replacement: *const (),
                    pub replacee: *const (),
                }
                unsafe impl std::marker::Sync for S {}
            }

            #[used]
            #[allow(non_upper_case_globals)]
            #[link_section="__DATA,__interpose"]
            static $real_fn: $real_fn::S = $real_fn::S {
                replacement: $real_fn::S::hook  as *const (),
                replacee: $real_fn::$real_fn as *const (),
            };

            impl $real_fn::S {
                pub fn next(&self, $($v:$t),*) -> $r {
                    unsafe {
                        // avert your gaze for a second
                        let addr: *const u8 = $real_fn::$real_fn as *const u8;
                        let real: unsafe extern "C" fn ($($v:$t),*) -> $r = ::std::mem::transmute(addr);
                        // theeeere we go
                        real($($v),*)
                    }
                }

                unsafe extern "C" fn hook($($v: $t),*) -> $r {
                    $body
                }
            }
        )+
    };
}
