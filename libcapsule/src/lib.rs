#![allow(non_upper_case_globals, non_snake_case)]

use log::*;

#[macro_use]
mod hook;

mod comm;
mod gl;
mod linux_gl_hooks;
mod macos_gl_hooks;
mod windows_gl_hooks;

use comm::*;

#[cfg(target_os = "macos")]
static CURRENT_PLATFORM: &str = "macOS";
#[cfg(target_os = "linux")]
static CURRENT_PLATFORM: &str = "Linux";
#[cfg(target_os = "windows")]
static CURRENT_PLATFORM: &str = "Windows";

use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::{pry, rpc_twoparty_capnp, twoparty, RpcSystem};
use proto::proto_capnp::host;
use std::net::SocketAddr;
use tokio::runtime::current_thread;

use futures::Future;
use tokio::io::AsyncRead;

struct Settings {
    in_test: bool,
}
unsafe impl Sync for Settings {}

lazy_static::lazy_static! {
    static ref SETTINGS: Settings = {
        Settings{
            in_test: std::env::var("CAPSULE_TEST").unwrap_or_default() == "1",
        }
    };
}

struct TargetImpl;
impl TargetImpl {
    fn new() -> Self {
        Self {}
    }
}
impl host::target::Server for TargetImpl {
    fn get_info(
        &mut self,
        _params: host::target::GetInfoParams,
        mut results: host::target::GetInfoResults,
    ) -> Promise<(), Error> {
        let mut info = results.get().init_info();
        info.set_pid(std::process::id() as u64);
        info.set_exe(std::env::current_exe().unwrap().to_string_lossy().as_ref());
        Promise::ok(())
    }
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
        info!("Thanks for flying capsule on {}", CURRENT_PLATFORM);

        unsafe {
            global_runtime = Some({
                let rt = current_thread::Runtime::new().expect("could not construct tokio runtime");
                std::sync::Mutex::new(rt)
            });
        }

        let port = std::env::var("CAPSULE_PORT").expect("CAPSULE_PORT missing from environment");
        let port = port.parse::<u16>().expect("could not parse port");

        {
            let addr = format!("127.0.0.1:{}", port);
            let addr = addr.parse::<SocketAddr>().unwrap();

            // TODO: timeouts
            info!("Connecting to host {:?}...", addr);

            let stream = hope(::tokio::net::TcpStream::connect(&addr)).unwrap();
            stream.set_nodelay(true).unwrap();
            let (reader, writer) = stream.split();

            let network = twoparty::VatNetwork::new(
                reader,
                std::io::BufWriter::new(writer),
                rpc_twoparty_capnp::Side::Client,
                Default::default(),
            );
            let mut rpc_system = RpcSystem::new(Box::new(network), None);
            unsafe {
                global_host = Some(rpc_system.bootstrap(rpc_twoparty_capnp::Side::Server));
            }
            spawn(rpc_system.map_err(|e| warn!("RPC error: {:?}", e)));
            info!("Connected!");
        }

        #[cfg(target_os = "linux")]
        {
            linux_gl_hooks::initialize()
        }

        #[cfg(target_os = "windows")]
        {
            windows_gl_hooks::initialize()
        }
        #[cfg(target_os = "macos")]
        {
            macos_gl_hooks::initialize()
        }

        {
            let target =
                host::target::ToClient::new(TargetImpl::new()).into_client::<::capnp_rpc::Server>();
            let mut req = get_host().register_target_request();
            req.get().set_target(target);
            hope(req.send().promise.and_then(|response| {
                pry!(response.get());
                Promise::ok(())
            }))
            .unwrap();
            info!("Returned from register_target!",);
        }
    };
    ctor
};
