use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use futures::Future;
use proto::host;

use super::encoder;
use log::*;
use std::fmt;
use std::sync::{Arc, Mutex};

struct SinkImpl {
  encoder: encoder::Encoder,
}

impl host::sink::Server for SinkImpl {
  fn send_video_frame(
    &mut self,
    params: host::sink::SendVideoFrameParams,
    _results: host::sink::SendVideoFrameResults,
  ) -> Promise<(), Error> {
    let params = pry!(params.get());
    let frame = pry!(params.get_frame());
    let data = pry!(frame.get_data());
    let millis = pry!(frame.get_timestamp()).get_millis();
    let timestamp = std::time::Duration::from_millis(millis as u64);

    unsafe {
      // TODO: convert to RPC error instead
      self.encoder.write_frame(data, timestamp).unwrap();
    }
    Promise::ok(())
  }
}

struct Target {
  pid: u64,
  exe: String,
  client: host::target::Client,
  session: Option<host::session::Client>,
}

impl fmt::Display for Target {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{}:{}", self.pid, self.exe)
  }
}

pub struct HostImpl {
  target: Arc<Mutex<Option<Target>>>,
}

impl HostImpl {
  pub fn new() -> Self {
    HostImpl {
      target: Arc::new(Mutex::new(None)),
    }
  }
}

impl host::Server for HostImpl {
  fn create_sink(
    &mut self,
    params: host::CreateSinkParams,
    mut results: host::CreateSinkResults,
  ) -> Promise<(), Error> {
    info!("Creating sink");

    let params = Arc::new(params);
    let sink = SinkImpl {
      // TODO: convert error to RPC errors
      encoder: unsafe { encoder::Encoder::new("capture.h264", params.clone()).unwrap() },
    };
    results
      .get()
      .set_sink(host::sink::ToClient::new(sink).into_client::<::capnp_rpc::Server>());

    Promise::ok(())
  }

  fn hotkey_pressed(
    &mut self,
    _params: host::HotkeyPressedParams,
    _results: host::HotkeyPressedResults,
  ) -> Promise<(), Error> {
    info!("Received hotkey press!");

    let target_ref = self.target.clone();
    if let Some(target) = self.target.lock().unwrap().as_mut() {
      if let Some(session) = target.session.as_ref() {
        let req = session.stop_request();
        info!("Stopping session...");
        return Promise::from_future(req.send().promise.and_then(move |_res| {
          info!("Stopped session.");
          target_ref.lock().unwrap().as_mut().unwrap().session = None;
          Promise::ok(())
        }));
      }

      info!("Starting session...");
      let req = target.client.start_session_request();
      Promise::from_future(req.send().promise.and_then(move |res| {
        info!("Started capture succesfully!");
        let res = Arc::new(res);
        let session = {
          let res = res.clone();
          let res = pry!(res.get());
          pry!(res.get_session())
        };
        if let Some(target) = target_ref.lock().unwrap().as_mut() {
          target.session = Some(session.clone());
        }
        Promise::ok(())
      }))
    } else {
      info!("...but no target registered");
      Promise::ok(())
    }
  }

  fn register_target(
    &mut self,
    params: host::RegisterTargetParams,
    mut _results: host::RegisterTargetResults,
  ) -> Promise<(), Error> {
    let params = pry!(params.get());
    let client = pry!(params.get_target());
    let info = pry!(params.get_info());
    {
      let pid = info.get_pid();
      let exe = info.get_exe().unwrap();
      let target = Target {
        pid: pid,
        exe: exe.to_owned(),
        client: client,
        session: None,
      };
      info!("Target registered: {}", target);
      *self.target.lock().unwrap() = Some(target);
    }
    Promise::ok(())
  }
}
