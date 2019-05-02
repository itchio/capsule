use log::*;

mod dxgi;
mod hub;
mod safe_env;

use capnp_rpc::{rpc_twoparty_capnp, twoparty, RpcSystem};
use comm::*;
use futures::Future;
use std::net::SocketAddr;
use tokio::io::AsyncRead;
use tokio::runtime::current_thread;

struct Settings {
    in_test: bool,
}
unsafe impl Sync for Settings {}

lazy_static::lazy_static! {
    static ref SETTINGS: Settings = {
        Settings{
            in_test: safe_env::get("CAPSULE_TEST").unwrap_or_default() == "1",
        }
    };
}

#[used]
#[allow(non_upper_case_globals)]
#[cfg_attr(target_os = "linux", link_section = ".ctors")]
#[cfg_attr(target_os = "macos", link_section = "__DATA,__mod_init_func")]
#[cfg_attr(windows, link_section = ".CRT$XCU")]
static ctor: extern "C" fn() = {
    extern "C" fn ctor() {
        std::env::set_var("RUST_BACKTRACE", "1");
        env_logger::init();

        let mut runtime =
            current_thread::Runtime::new().expect("could not construct tokio runtime");

        let port = safe_env::get("CAPSULE_PORT").expect("CAPSULE_PORT missing from environment");
        let port = port.parse::<u16>().expect("could not parse port");

        {
            let addr = format!("127.0.0.1:{}", port);
            let addr = addr.parse::<SocketAddr>().unwrap();

            // TODO: timeouts
            let connect_future = ::tokio::net::TcpStream::connect(&addr);
            let stream = runtime.block_on(connect_future).unwrap();
            stream.set_nodelay(true).unwrap();
            let (reader, writer) = stream.split();

            let network = twoparty::VatNetwork::new(
                reader,
                std::io::BufWriter::new(writer),
                rpc_twoparty_capnp::Side::Client,
                Default::default(),
            );
            let mut rpc_system = RpcSystem::new(Box::new(network), None);
            let host = rpc_system.bootstrap(rpc_twoparty_capnp::Side::Server);
            runtime.spawn(rpc_system.map_err(|e| warn!("RPC error: {:?}", e)));

            let hub = hub::new_hub(runtime, host);
            set_hub(Box::new(hub));
        }

        hotkey::initialize();
        gl::hooks::initialize();
        dxgi::hooks::initialize();
    };
    ctor
};
