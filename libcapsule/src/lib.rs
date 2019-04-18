#![allow(non_upper_case_globals, non_snake_case)]

use log::*;

#[macro_use]
mod hook;

mod gl;
mod linux_gl_hooks;
mod macos_gl_hooks;
mod windows_gl_hooks;

#[cfg(target_os = "macos")]
static CURRENT_PLATFORM: &str = "macOS";
#[cfg(target_os = "linux")]
static CURRENT_PLATFORM: &str = "Linux";
#[cfg(target_os = "windows")]
static CURRENT_PLATFORM: &str = "Windows";

struct Settings {
    in_test: bool,
}
unsafe impl Sync for Settings {}

lazy_static::lazy_static! {
    static ref SETTINGS: Settings = {
        Settings{
            in_test: std::env::var("CAPSULE_TEST").unwrap_or_default() == "1",
        }
    };
}

#[used]
#[allow(non_upper_case_globals)]
#[cfg_attr(target_os = "linux", link_section = ".ctors")]
#[cfg_attr(target_os = "macos", link_section = "__DATA,__mod_init_func")]
#[cfg_attr(windows, link_section = ".CRT$XCU")]
static ctor: extern "C" fn() = {
    extern "C" fn ctor() {
        std::env::set_var("RUST_BACKTRACE", "1");
        env_logger::init();

        info!("thanks for flying capsule on {}", CURRENT_PLATFORM);
        #[cfg(target_os = "linux")]
        {
            linux_gl_hooks::initialize()
        }
        #[cfg(target_os = "windows")]
        {
            windows_gl_hooks::initialize()
        }
        #[cfg(target_os = "macos")]
        {
            macos_gl_hooks::initialize()
        }
    };
    ctor
};
