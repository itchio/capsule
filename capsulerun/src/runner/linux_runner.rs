#![cfg(target_os = "linux")]

use super::{Arch, Context};
use log::*;
use std::process::Command;

impl Context {
  pub fn run(self) {
    let mut cmd = Command::new(self.options.exec.clone());
    cmd.args(self.options.args.clone());

    let lib32 = self
      .locate_lib("libcapsule.so", Arch::I686)
      .unwrap_or_default();
    let lib64 = self
      .locate_lib("libcapsule.so", Arch::X86_64)
      .unwrap_or_default();
    let both_libs = format!("{}:{}", lib32.to_string_lossy(), lib64.to_string_lossy());
    info!("Setting LD_PRELOAD = {}", both_libs);
    cmd.env("LD_PRELOAD", both_libs);

    let mut child = cmd.spawn().expect("Command failed to start");
    info!("Created process, pid {}", child.id());

    self.wait_for_child(&mut child);
  }
}
