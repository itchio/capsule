#![cfg(target_os = "linux")]

use super::{Arch, Context};
use log::*;
use std::process::Command;

impl Context {
  pub fn run(self) {
    let mut cmd = Command::new(self.options.exec.clone());
    cmd.args(self.options.args.clone());

    // TODO: 32-on-64 bit support, or maybe don't bother.
    let target_arch = Arch::X86_64;
    let lib = self.locate_lib("libcapsule.so", target_arch).unwrap();

    cmd.env("LD_PRELOAD", lib);

    let mut child = cmd.spawn().expect("Command failed to start");
    info!("Created process, pid {}", child.id());

    self.wait_for_child(&mut child);
  }
}
