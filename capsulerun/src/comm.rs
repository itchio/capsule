use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use futures::Future;
use proto::proto_capnp::host;

use log::*;
use std::time::SystemTime;

pub struct HostImpl {
  last_fps_print: SystemTime,
  frames_this_cycle: u64,
}

impl HostImpl {
  pub fn new() -> Self {
    HostImpl {
      last_fps_print: SystemTime::now(),
      frames_this_cycle: 0,
    }
  }
}

impl host::Server for HostImpl {
  fn register_target(
    &mut self,
    params: host::RegisterTargetParams,
    mut _results: host::RegisterTargetResults,
  ) -> Promise<(), Error> {
    let req = pry!(pry!(params.get()).get_target()).get_info_request();
    Promise::from_future(req.send().promise.and_then(|v| {
      let info = pry!(v.get()).get_info().unwrap();
      let pid = info.get_pid();
      let exe = info.get_exe().unwrap();
      info!("Target registered: ({}) (PID = {})", exe, pid);
      Promise::ok(())
    }))
  }

  fn notify_frame(
    &mut self,
    params: host::NotifyFrameParams,
    _results: host::NotifyFrameResults,
  ) -> Promise<(), Error> {
    let frame_number = pry!(params.get()).get_frame_number();
    self.frames_this_cycle += 1;

    if let Ok(elapsed) = self.last_fps_print.elapsed() {
      if elapsed.as_secs() >= 1 {
        let fps = (self.frames_this_cycle as f64) / (elapsed.as_nanos() as f64 / 1e9f64);
        info!("{:.0} FPS (frame #{})", fps, frame_number);
        self.last_fps_print = SystemTime::now();
        self.frames_this_cycle = 0;
      }
    }

    Promise::ok(())
  }
}
