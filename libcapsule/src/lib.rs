#![allow(non_upper_case_globals, non_snake_case)]

use log::*;

#[macro_use]
mod hook;

mod comm;
mod gl;
mod linux_gl_hooks;
mod macos_gl_hooks;
mod safe_env;
mod windows_gl_hooks;

use comm::*;

use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::{pry, rpc_twoparty_capnp, twoparty, RpcSystem};
use futures::Future;
use proto::proto_capnp::host;
use std::net::SocketAddr;
use std::sync::{Arc, RwLock};
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

struct TargetImpl;
impl TargetImpl {
    fn new() -> Self {
        Self {}
    }
}

impl host::target::Server for TargetImpl {
    fn start_capture(
        &mut self,
        params: host::target::StartCaptureParams,
        mut results: host::target::StartCaptureResults,
    ) -> Promise<(), Error> {
        let params = pry!(params.get());
        let sink = pry!(params.get_sink());

        info!("Starting capture into a sink!");
        let session = Session {
            sink: sink,
            alive: true,
        };
        let session_ref = Arc::new(RwLock::new(session));
        let session_impl = SessionImpl {
            session: session_ref.clone(),
        };

        let info = results.get().init_info();
        if let Some(gl_ctx) = gl::get_cached_capture_context() {
            let mut video = info.init_video();
            video.set_width(gl_ctx.get_width() as u32);
            video.set_height(gl_ctx.get_height() as u32);
        }

        results.get().set_session(
            host::session::ToClient::new(session_impl).into_client::<::capnp_rpc::Server>(),
        );

        unsafe {
            global_session = Some(session_ref);
        }
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

        unsafe {
            global_runtime = Some({
                let rt = current_thread::Runtime::new().expect("could not construct tokio runtime");
                std::sync::Mutex::new(rt)
            });
        }

        let port = safe_env::get("CAPSULE_PORT").expect("CAPSULE_PORT missing from environment");
        let port = port.parse::<u16>().expect("could not parse port");

        {
            let addr = format!("127.0.0.1:{}", port);
            let addr = addr.parse::<SocketAddr>().unwrap();

            // TODO: timeouts
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
            let mut info = req.get().init_info();
            info.set_pid(std::process::id() as u64);
            info.set_exe(std::env::current_exe().unwrap().to_string_lossy().as_ref());
            req.get().set_target(target);
            hope(req.send().promise.and_then(|response| {
                pry!(response.get());
                Promise::ok(())
            }))
            .unwrap();
        }
    };
    ctor
};
