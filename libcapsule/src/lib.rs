#[macro_use]
extern crate ctor;

#[macro_use]
extern crate libc_print;

#[cfg(target_os = "macos")]
static CURRENT_PLATFORM: &str = "macOS";
#[cfg(target_os = "linux")]
static CURRENT_PLATFORM: &str = "Linux";
#[cfg(target_os = "windows")]
static CURRENT_PLATFORM: &str = "Windows";

#[ctor]
fn ctor() {
    libc_println!("thanks for flying capsule on {}", CURRENT_PLATFORM);
}
