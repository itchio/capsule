#![cfg(target_os = "macos")]

use super::{Arch,Context};
use log::*;
use std::process::Command;

impl Context {
    pub fn run(self) {
        let mut cmd = Command::new(self.options.exec.clone());
        cmd.args(self.options.args.clone());

        let target_arch = Arch::X86_64;
        let lib = self.locate_lib("libcapsule.dylib", target_arch).unwrap();

        cmd.env("DYLD_INSERT_LIBRARIES", lib);

        let mut child = cmd.spawn().expect("Command failed to start");
        info!("Created process, pid {}", child.id());

        self.wait_for_child(&mut child);
    }
}
