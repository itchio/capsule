use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use futures::Future;
use proto::proto_capnp::host;

use log::*;
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::SystemTime;
use uuid::Uuid;

struct TargetEntry {
  id: Uuid,
  target: host::target::Client,
}

pub struct HostImpl {
  last_fps_print: SystemTime,
  frames_this_cycle: u64,
  targets: Arc<Mutex<HashMap<Uuid, Arc<Mutex<TargetEntry>>>>>,
}

impl HostImpl {
  pub fn new() -> Self {
    HostImpl {
      last_fps_print: SystemTime::now(),
      frames_this_cycle: 0,
      targets: Arc::new(Mutex::new(HashMap::new())),
    }
  }
}

impl host::Server for HostImpl {
  fn register_target(
    &mut self,
    params: host::RegisterTargetParams,
    mut _results: host::RegisterTargetResults,
  ) -> Promise<(), Error> {
    let target = pry!(pry!(params.get()).get_target());
    let req = target.get_info_request();
    let targets = self.targets.to_owned();
    Promise::from_future(req.send().promise.and_then(move |v| {
      let info = pry!(v.get()).get_info().unwrap();
      let pid = info.get_pid();
      let exe = info.get_exe().unwrap();
      info!("Target registered: ({}) (PID = {})", exe, pid);
      let id = Uuid::new_v4();
      let entry = TargetEntry {
        id: id,
        target: target.clone(),
      };
      let entry = Arc::new(Mutex::new(entry));
      targets.lock().unwrap().insert(id, entry);

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

        info!("Checking on all targets...");
        let target_promises: Vec<Promise<Option<Uuid>, Error>> = self
          .targets
          .lock()
          .unwrap()
          .iter()
          .map(|(&id, entry)| {
            let req = entry.lock().unwrap().target.get_info_request();
            let f = req.send().promise.and_then(|v| {
              let info = pry!(v.get()).get_info().unwrap();
              let pid = info.get_pid();
              let exe = info.get_exe().unwrap();
              info!("Target still alive: ({}) (PID = {})", exe, pid);
              Promise::ok(None)
            });
            let f = f.or_else(move |err| {
              warn!("Uh oh: {:?}", err);
              Promise::ok(Some(id))
            });
            Promise::from_future(f)
          })
          .collect();

        let f = futures::future::join_all(target_promises);

        let targets = self.targets.to_owned();
        let f = f.and_then(move |deads| {
          let mut targets = targets.lock().unwrap();
          for id in deads.iter().filter_map(|&x| x) {
            if let Some(_) = targets.remove(&id) {
              warn!("Removed target entry {:#?}", id);
            }
          }
          Promise::ok(())
        });

        return Promise::from_future(f);
      }
    }

    Promise::ok(())
  }
}
