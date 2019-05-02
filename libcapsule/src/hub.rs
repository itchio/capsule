use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use comm::*;
use futures::Future;
use log::*;
use proto::host::*;
use std::sync::{Arc, RwLock};

struct TargetImpl;
impl TargetImpl {
  fn new() -> Self {
    Self {}
  }
}

impl target::Server for TargetImpl {
  fn start_capture(
    &mut self,
    params: target::StartCaptureParams,
    mut results: target::StartCaptureResults,
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
      video.set_pitch(gl_ctx.get_width() as u32 * 4u32);
      video.set_vertical_flip(true);
    } else {
      return Promise::err(capnp::Error {
        kind: capnp::ErrorKind::Failed,
        description: "No viable video context!".to_owned(),
      });
    }

    results
      .get()
      .set_session(session::ToClient::new(session_impl).into_client::<::capnp_rpc::Server>());

    unsafe {
      global_session = Some(session_ref);
    }
    Promise::ok(())
  }
}

pub struct Hub {}

impl comm::Hub for Hub {
  fn in_test(&self) -> bool {
    super::SETTINGS.in_test
  }

  fn register_target(&self) {
    let target = target::ToClient::new(TargetImpl::new()).into_client::<::capnp_rpc::Server>();
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
}
