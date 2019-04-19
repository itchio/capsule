extern crate const_cstr;

#[cfg(windows)]
extern crate wincap;

mod comm;
mod runner;

use clap::{App, Arg};
use comm::*;
use log::*;

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

    use futures::*;
    let (port_channel, port_receive) = oneshot::<u16>();

    // woo let's go
    std::thread::spawn(move || run_server(port_channel));
    let mut runtime = tokio::runtime::current_thread::Runtime::new().unwrap();
    info!("Waiting on RPC thread to start listening...");
    let port = runtime.block_on(port_receive).unwrap();
    info!("Cool, it's listening on port {}", port);

    let options = build_options(matches, port);
    runner::new_context(options).run();
}

fn build_options<'a>(matches: clap::ArgMatches<'a>, port: u16) -> runner::Options {
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
        port: port,
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

fn run_server(port_channel: futures::Complete<u16>) {
    use capnp_rpc::{rpc_twoparty_capnp, twoparty, RpcSystem};
    use futures::{Future, Stream};
    use proto::proto_capnp::host;
    use std::net::SocketAddr;
    use tokio::io::AsyncRead;
    use tokio::net::TcpListener;
    use tokio::runtime::current_thread;

    let addr = "127.0.0.1:0".parse::<SocketAddr>().unwrap();
    let socket = TcpListener::bind(&addr).unwrap();

    let host = host::ToClient::new(HostImpl::new()).into_client::<::capnp_rpc::Server>();

    let local_addr = socket.local_addr().unwrap();
    info!("Listening on {:?}", local_addr);
    port_channel.send(local_addr.port()).unwrap();

    let done = socket.incoming().for_each(move |socket| {
        socket.set_nodelay(true).unwrap();
        let (reader, writer) = socket.split();
        let network = twoparty::VatNetwork::new(
            reader,
            std::io::BufWriter::new(writer),
            rpc_twoparty_capnp::Side::Server,
            Default::default(),
        );
        let rpc_system = RpcSystem::new(Box::new(network), Some(host.clone().client));
        current_thread::spawn(rpc_system.map_err(|e| warn!("RPC error: {:?}", e)));
        Ok(())
    });
    match current_thread::block_on_all(done) {
        Err(err) => warn!("RPC server crashed: {:#?}", err),
        Ok(_) => warn!("RPC server wound down"),
    }
}
