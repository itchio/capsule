extern crate const_cstr;
extern crate wstr;

#[cfg(windows)]
extern crate wincap;

use clap::{App, Arg};
mod runner;

use simplelog;

fn main() {
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
        arch: if cfg!(target_pointer_width = "32") {
            runner::Arch::I686
        } else {
            runner::Arch::X86_64
        },
        os: match true {
            cfg!(target_os = "windows") => runner::OS::Windows,
            cfg!(target_os = "linux") => runner::OS::Linux,
            cfg!(target_os = "macos") => runner::OS::MacOS,
        },
    }
}

fn setup_logging<'a>(matches: &clap::ArgMatches<'a>) {
    use simplelog::*;

    let mut loggers = Vec::<Box<SharedLogger>>::new();
    let verbose = matches.is_present("verbose");
    if verbose {
        // TODO: this enables libcapsule logging, but it's not
        // a great way to do it because it's mixed with the target
        // program's stdout.
        std::env::set_var("RUST_LOG", "info");
    }
    let term_level = if verbose {
        LevelFilter::Info
    } else {
        LevelFilter::Warn
    };
    loggers.push(TermLogger::new(term_level, Config::default()).unwrap());

    if let Some(log_file) = matches.value_of("log-file") {
        use std::fs::File;
        let f = File::create(log_file).unwrap();
        loggers.push(WriteLogger::new(LevelFilter::Info, Config::default(), f))
    }
    CombinedLogger::init(loggers).unwrap();
}
