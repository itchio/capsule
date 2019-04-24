use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use futures::Future;
use proto::proto_capnp::host;

use super::encoder;
use log::*;
use std::fmt;
use std::sync::{Arc, Mutex};

struct Sink {
  session: host::session::Client,
  encoder: encoder::Context,
  count: u32,
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
    let index = frame.get_index();
    let data = pry!(frame.get_data());
    let millis = pry!(frame.get_timestamp()).get_millis();
    let timestamp = std::time::Duration::from_millis(millis as u64);

    let mut guard = self.sink.lock().unwrap();
    let sink = guard.as_mut().unwrap();

    sink.encoder.write_frame(data, timestamp).unwrap();

    sink.count += 1;
    if sink.count > 60 * 4 {
      let req = sink.session.stop_request();
      return Promise::from_future(req.send().promise.and_then(|_| {
        info!("Stopped capture session!");
        Promise::ok(())
      }));
    }
    Promise::ok(())
  }
}

struct Target {
  client: host::target::Client,
  pid: u64,
  exe: String,
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
        client: client.clone(),
      };
      info!("Target registered: {}", target);
      self.target = Some(target);
    }

    {
      info!("Asking to start capture...");

      // creating a sink here, but it has a "None" session
      let sink_impl = SinkImpl {
        sink: Arc::new(Mutex::new(None)),
      };
      let sink_ref = sink_impl.sink.clone();
      let mut req = client.start_capture_request();
      // moving the sink into an RPC client thingy
      req
        .get()
        .set_sink(host::sink::ToClient::new(sink_impl).into_client::<::capnp_rpc::Server>());
      Promise::from_future(req.send().promise.and_then(move |res| {
        info!("Started capture succesfully!");
        let res = pry!(res.get());
        let session = pry!(res.get_session());

        let info = pry!(res.get_info());
        let video = pry!(info.get_video());
        let (width, height) = (video.get_width(), video.get_height());

        // TODO: don't unwrap
        let sink = Sink {
          session,
          encoder: unsafe { encoder::Context::new("capture.h264", width, height).unwrap() },
          count: 0,
        };
        *sink_ref.lock().unwrap() = Some(sink);
        Promise::ok(())
      }))
    }
  }
}
