#[macro_use]
extern crate wstr;
#[macro_use]
extern crate const_cstr;

#[cfg(windows)]
#[macro_use]
extern crate wincap;

use clap::{App, Arg};
mod runner;

fn main() {
    let matches = App::new("capsulerun")
        .version("master")
        .author("Amos Wenger <amos@itch.io>")
        .about("Your very own libcapsule injector")
        .arg(
            Arg::with_name("lib")
                .long("lib")
                .help("Path of the lib to inject")
                .takes_value(true),
        )
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

    unsafe { runner::run(matches) }
}
