use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
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
    _params: host::RegisterTargetParams,
    mut _results: host::RegisterTargetResults,
  ) -> Promise<(), Error> {
    info!("A client has registered a capture target!");
    Promise::ok(())
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
