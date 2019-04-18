extern crate const_cstr;

#[cfg(windows)]
extern crate wincap;

use clap::{App, Arg};
use log::*;
mod runner;

fn main() {
    std::env::set_var("RUST_BACKTRACE", "1");

    let matches = App::new("capsulerun")
        .version("master")
        .author("Amos Wenger <amos@itch.io>")
        .about("Your very own libcapsule injector")
        .arg(
            Arg::with_name("verbose")
                .help("Print verbose output")
                .short("v"),
        )
        .arg(
            Arg::with_name("suspend")
                .long("suspend")
                .help("Suspend process, wait for keyboard input to continue"),
        )
        .arg(
            Arg::with_name("log-file")
                .long("log-file")
                .help("Log to this file")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("exec")
                .help("The executable to run")
                .required(true),
        )
        .arg(
            Arg::with_name("args")
                .help("Arguments to pass to the executable")
                .multiple(true),
        )
        .get_matches();

    setup_logging(&matches);
    let options = build_options(matches);
    runner::new_context(options).run();
}

fn build_options<'a>(matches: clap::ArgMatches<'a>) -> runner::Options {
    runner::Options {
        exec: matches.value_of("exec").unwrap().to_string(),
        args: matches
            .values_of("args")
            .unwrap_or_default()
            .map(|x| x.to_string())
            .collect(),
        arch: if cfg!(target_pointer_width = "64") {
            runner::Arch::X86_64
        } else {
            runner::Arch::I686
        },
        os: match true {
            cfg!(target_os = "windows") => runner::OS::Windows,
            cfg!(target_os = "linux") => runner::OS::Linux,
            cfg!(target_os = "macos") => runner::OS::MacOS,
        },
        suspend: matches.is_present("suspend"),
    }
}

fn setup_logging<'a>(matches: &clap::ArgMatches<'a>) {
    let verbose = matches.is_present("verbose");
    if verbose {
        // TODO: this enables libcapsule logging, but it's not
        // a great way to do it because it's mixed with the target
        // program's stdout.
        std::env::set_var("RUST_LOG", "info");
    } else {
        std::env::set_var("RUST_LOG", "warn");
    }
    env_logger::init();

    if let Some(_) = matches.value_of("log-file") {
        // TODO: unstub
        warn!("--log-file is currently a stub");
    }
}
