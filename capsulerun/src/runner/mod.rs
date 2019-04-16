pub mod linux_runner;
pub mod macos_runner;
pub mod windows_runner;

#[cfg(target_os = "windows")]
pub use windows_runner::*;

#[cfg(target_os = "linux")]
pub use linux_runner::*;

#[cfg(target_os = "macos")]
pub use macos_runner::*;

use log::*;

pub struct Options {
  pub exec: String,
  pub args: Vec<String>,
  pub arch: Arch,
  pub os: OS,
  pub suspend: bool,
}

pub struct Context {
  options: Options,
}

pub fn new_context(options: Options) -> Context {
  Context { options: options }
}

#[derive(PartialEq)]
pub enum Arch {
  I686,
  X86_64,
}

impl Arch {
  pub fn as_str(&self) -> &str {
    match self {
      &Arch::I686 => "i686",
      &Arch::X86_64 => "x86_64",
    }
  }
}

pub enum OS {
  Windows,
  Linux,
  MacOS,
}

impl OS {
  pub fn as_str(&self) -> &str {
    match self {
      &OS::Windows => "pc-windows-msvc",
      &OS::Linux => "unknown-linux-gnu",
      &OS::MacOS => "apple-darwin",
    }
  }
}

use std::path::PathBuf;

impl Context {
  pub fn locate_lib(
    &self,
    name: &str,
    target_arch: Arch,
  ) -> Result<PathBuf, Box<std::error::Error>> {
    let exe_dir = PathBuf::from(std::env::current_exe()?.parent().unwrap());
    let base_dir = exe_dir.parent().unwrap().parent().unwrap();
    let profile = if cfg!(debug_assertions) {
      "debug"
    } else {
      "release"
    };

    let archos_dir = format!("{}-{}", target_arch.as_str(), self.options.os.as_str());
    let res = base_dir.join(archos_dir).join(profile).join(name);
    info!("Computed capsule library path: {:?}", res);
    Ok(res)
  }

  pub fn wait_for_child(&self, child: &mut std::process::Child) {
    info!("Now waiting for child...");
    let exit_status = child.wait().expect("Non-zero exit code");
    let code = match exit_status.code() {
      Some(code) => code,
      None => {
        #[cfg(unix)]
        {
          use std::os::unix::process::ExitStatusExt;
          warn!(
            "Child killed by signal {}",
            exit_status.signal().unwrap_or_default()
          );
        }

        127
      }
    };
    std::process::exit(code)
  }
}
