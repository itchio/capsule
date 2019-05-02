pub mod linux;
pub mod macos;
pub mod windows;

#[cfg(target_os = "windows")]
pub use windows::*;

#[cfg(target_os = "linux")]
pub use linux::*;

#[cfg(target_os = "macos")]
pub use macos::*;

pub trait Backend {
  fn register_target(&self);
}
