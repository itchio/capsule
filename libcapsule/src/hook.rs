#[macro_export]
macro_rules! hook_extern {
    ($(fn $real_fn:ident($($v:ident : $t:ty),*) -> $r:ty $body:block)+) => {
        $(
            // this is an "extern hook" - we assume $real_fn is
            // implemented by some external library (no matter how it's linked).
            extern "C" {
                // it has the exact function signature we specified.
                fn $real_fn ($($v: $t),*) -> $r;
            }

            // we need this to generate new identifiers in our macro.
            // this is not very hygienic, and naughty, but let's do it anyway.
            paste::item! {
                lazy_static! {
                    // same thing as before we had a macro. this is a detour from detour-rs,
                    // it takes care of the actual hooking and exposes the original
                    // function through `.trampoline`
                    static ref [<$real_fn __hook>]: std::sync::Mutex<detour::RawDetour> = unsafe {
                        let mut detour = RawDetour::new(
                            // this is the original function (imported symbol)
                            $real_fn as *const (),
                            // this is our implementation (see below)
                            [<$real_fn __hooked>] as *const()
                        ).unwrap();
                        // this actually hooks the function by generating binary code
                        detour.enable().unwrap();
                        // we return the detour object, protected by a mutex
                        std::sync::Mutex::new(detour)
                    };
                    // this is a reference to the trampoline to the original function
                    // this means calling hook_extern! { fn foobar() {} }
                    // will put `foobar__next` in scope.
                    // we can re-use the same function signature (arguments and return type). DRY!
                    static ref [<$real_fn __next>]:
                            extern "C" fn ($($v: $t),*) -> $r = unsafe {
                        ::std::mem::transmute([<$real_fn __hook>].lock().unwrap().trampoline())
                    };
                }
            }

            paste::item_with_macros! {
                // finally, this is the definition of *our* version of the function.
                // you probably don't want to forget to call `XXX__next`
                unsafe extern "C" fn [<$real_fn __hooked>] ($($v: $t),*) -> $r {
                    $body
                }
            }
        )+
    };

}
