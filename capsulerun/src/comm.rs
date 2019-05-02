use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use futures::Future;
use proto::host;

use super::encoder;
use log::*;
use std::fmt;
use std::sync::{Arc, Mutex};

struct Sink {
  session: host::session::Client,
  encoder: Option<encoder::Context>,
}

struct SinkImpl {
  sink: Arc<Mutex<Option<Sink>>>,
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

    let mut guard = self.sink.lock().unwrap();
    let sink = guard.as_mut().unwrap();
    unsafe {
      if let Some(encoder) = sink.encoder.as_mut() {
        encoder.write_frame(data, timestamp).unwrap();
      }
    }
    Promise::ok(())
  }
}

struct Target {
  pid: u64,
  exe: String,
  client: host::target::Client,
}

impl fmt::Display for Target {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{}:{}", self.pid, self.exe)
  }
}

pub struct HostImpl {
  target: Option<Target>,
}

impl HostImpl {
  pub fn new() -> Self {
    HostImpl { target: None }
  }
}

impl host::Server for HostImpl {
  fn hotkey_pressed(
    &mut self,
    _params: host::HotkeyPressedParams,
    _results: host::HotkeyPressedResults,
  ) -> Promise<(), Error> {
    info!("Received hotkey press!");

    if let Some(target) = self.target.as_ref() {
      info!("Asking to start capture...");

      let sink_impl = SinkImpl {
        sink: Arc::new(Mutex::new(None)),
      };
      let sink_ref = sink_impl.sink.clone();
      let mut req = target.client.start_capture_request();
      req
        .get()
        .set_sink(host::sink::ToClient::new(sink_impl).into_client::<::capnp_rpc::Server>());
      Promise::from_future(req.send().promise.and_then(move |res| {
        info!("Started capture succesfully!");
        let res = Arc::new(res);
        let session = {
          let res = res.clone();
          let res = pry!(res.get());
          pry!(res.get_session())
        };

        let encoder = {
          match unsafe { encoder::Context::new("capture.h264", res.clone()) } {
            Ok(encoder) => Some(encoder),
            Err(e) => {
              return Promise::err(capnp::Error {
                kind: capnp::ErrorKind::Failed,
                description: format!("{}", e),
              })
            }
          }
        };

        {
          let res = res.clone();
          let info = pry!(pry!(res.get()).get_info());
          let video = pry!(info.get_video());
          info!("After arc: {}x{}", video.get_width(), video.get_height());
        }

        let sink = Sink { session, encoder };
        *sink_ref.lock().unwrap() = Some(sink);
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
      };
      info!("Target registered: {}", target);
      self.target = Some(target);
    }
    Promise::ok(())
  }
}
