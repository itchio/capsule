#![allow(non_upper_case_globals)]

use capnp::capability::Promise;
use capnp::Error;
use log::*;
use proto::host;
use std::sync::{Arc, RwLock};
use tokio::runtime::current_thread::Runtime;

// Here be dragons ok?

static mut global_hub: Option<Box<Hub>> = None;

pub fn set_hub(hub: Box<Hub>) {
  unsafe { global_hub = Some(hub) }
}

pub fn get_hub<'a>() -> &'a mut Box<Hub> {
  unsafe {
    global_hub
      .as_mut()
      .expect("internal error: no hub registered yet")
  }
}

static mut global_session: Option<Arc<RwLock<Session>>> = None;

pub fn get_session_read<'a>() -> Option<&'a Arc<RwLock<Session>>> {
  unsafe {
    let res = global_session.as_ref();
    if let Some(session) = res {
      if session.read().unwrap().alive {
        return Some(session);
      } else {
        info!("Capture session was stopped");
        global_session = None;
      }
    }
    None
  }
}

pub fn set_session(session: Arc<RwLock<Session>>) {
  unsafe { global_session = Some(session) }
}

pub struct Session {
  pub sink: host::sink::Client,
  pub alive: bool,
}

pub struct SessionImpl {
  pub session: Arc<RwLock<Session>>,
}
impl SessionImpl {}

impl host::session::Server for SessionImpl {
  fn stop(
    &mut self,
    _params: host::session::StopParams,
    _results: host::session::StopResults,
  ) -> Promise<(), Error> {
    let mut session = self.session.write().unwrap();
    session.alive = false;
    Promise::ok(())
  }
}

pub struct AudioSource {
  pub sample_rate: u32,
}

#[derive(Debug)]
pub enum VideoSourceKind {
  OpenGL,
  DXGI,
}

pub struct VideoSource {
  pub kind: VideoSourceKind,
  pub width: u32,
  pub height: u32,
  pub pitch: u32,
  pub vflip: bool,
}

pub trait Hub {
  fn in_test(&self) -> bool;
  fn runtime<'a>(&'a mut self) -> &'a mut Runtime;
  fn session<'a>(&'a mut self) -> Option<&'a Arc<RwLock<Session>>>;

  fn set_video_source(&mut self, s: VideoSource);
  fn set_audio_source(&mut self, s: AudioSource);

  fn get_video_source(&self) -> Option<&VideoSource>;
  fn get_audio_source(&self) -> Option<&AudioSource>;
}
