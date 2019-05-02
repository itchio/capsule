#![allow(non_upper_case_globals)]

use capnp::capability::Promise;
use capnp::Error;
use proto::host;
use std::sync::{Arc, Mutex, RwLock};
use tokio::runtime::current_thread::Runtime;

// Here be dragons ok?

static mut global_hub: Option<Box<Hub>> = None;

pub fn set_hub(hub: Box<Hub>) {
  unsafe { global_hub = Some(hub) }
}

pub fn get_hub<'a>() -> &'a mut Box<Hub> {
  // warn!(
  //   "get_hub called from thread {:?}: {:?}",
  //   std::thread::current().id(),
  //   error_chain::Backtrace::new()
  // );
  unsafe {
    global_hub
      .as_mut()
      .expect("internal error: no hub registered yet")
  }
}

static mut global_session: Option<Session> = None;

pub fn get_session<'a>() -> Option<&'a Session> {
  unsafe { global_session.as_ref() }
}

pub fn set_session(session: Session) {
  unsafe { global_session = Some(session) }
}

pub fn clear_session() {
  unsafe { global_session = None }
}

pub struct Session {
  pub sink: host::sink::Client,
  pub alive: bool,
  pub start_time: std::time::SystemTime,
}

pub struct SessionImpl {}

impl host::session::Server for SessionImpl {
  fn stop(
    &mut self,
    _params: host::session::StopParams,
    _results: host::session::StopResults,
  ) -> Promise<(), Error> {
    clear_session();
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
  fn runtime<'a>(&'a mut self) -> &'a mut Mutex<Runtime>;
  fn host(&self) -> &host::Client;
  fn session<'a>(&'a mut self) -> Option<&'a Arc<RwLock<Session>>>;

  fn set_video_source(&mut self, s: VideoSource);
  fn set_audio_source(&mut self, s: AudioSource);

  fn get_video_source(&self) -> Option<&VideoSource>;
  fn get_audio_source(&self) -> Option<&AudioSource>;

  fn hotkey_pressed(&mut self);
}
