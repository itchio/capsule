#![cfg(target_os = "macos")]

use std::os::unix::process::ExitStatusExt;
use std::path::PathBuf;
use std::process::Command;

pub unsafe fn run<'a>(matches: clap::ArgMatches<'a>) {
  let exec = matches.value_of("exec").unwrap();

  let args: Vec<&str> = matches.values_of("args").unwrap_or_default().collect();

  let host_is_32bit = cfg!(target_pointer_width = "32");
  // TODO: we can have a 32-bit target with 64-bit host
  let target_is_32bit = host_is_32bit;

  let mut cmd = Command::new(exec);
  cmd.args(args);

  let lib = match determine_lib_path(matches, host_is_32bit, target_is_32bit) {
    Ok(x) => x,
    Err(e) => panic!("{:?}", e),
  };

  println!("Will inject ({:?})", lib);
  cmd.env("DYLD_INSERT_LIBRARIES", lib);

  let mut child = cmd.spawn().expect("Command failed to start");
  println!("Created process, pid = {}", child.id());

  println!("Now waiting for child...");
  let exit_status = child.wait().expect("Non-zero exit code");
  std::process::exit(match exit_status.code() {
    Some(code) => code,
    None => {
      println!(
        "Child killed with signal {}",
        exit_status.signal().unwrap_or_default()
      );
      127
    }
  })
}

fn determine_lib_path<'a>(
  matches: clap::ArgMatches<'a>,
  host_is_32bit: bool,
  target_is_32bit: bool,
) -> Result<PathBuf, Box<std::error::Error>> {
  match matches.value_of("lib") {
    Some(lib) => Ok(PathBuf::from(lib)),
    _ => {
      let exe_dir = PathBuf::from(std::env::current_exe()?.parent().unwrap());
      let lib_name = PathBuf::from("libcapsule.dylib");

      match target_is_32bit {
        true => match host_is_32bit {
          true => {
            // both are 32
            Ok(exe_dir.join(lib_name))
          }
          false => {
            // target is 32, host is 64
            Ok(
              exe_dir
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("i686-apple-darwin")
                .join("debug")
                .join(lib_name),
            )
          }
        },
        false => match host_is_32bit {
          false => {
            // both are 64
            Ok(exe_dir.join(lib_name))
          }
          true => {
            // target is 64, host is 32
            Ok(
              exe_dir
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("x86_64-apple-darwin")
                .join("debug")
                .join(lib_name),
            )
          }
        },
      }
    }
  }
}
