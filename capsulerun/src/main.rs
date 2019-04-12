use clap::{App, Arg};
use std::process::Command;

fn main() {
    let matches = App::new("capsulerun")
        .version("master")
        .author("Amos Wenger <amos@itch.io>")
        .about("Your very own libcapsule injector")
        .arg(
            Arg::with_name("exec")
                .help("The executable to run")
                .required(true)
                .index(1),
        )
        .arg(
            Arg::with_name("args")
                .help("Arguments to pass to the executable")
                .multiple(true)
                .index(2),
        )
        .get_matches();

    let exec = matches.value_of("exec").unwrap();
    let args: Vec<&str> = matches.values_of("args").unwrap_or_default().collect();

    let mut child = Command::new(exec)
        .args(args)
        .spawn()
        .expect("Command failed to start");
    child.wait().expect("Non-zero exit code");
}
