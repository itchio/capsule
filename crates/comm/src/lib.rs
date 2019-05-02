#![allow(non_upper_case_globals)]

use capnp::capability::Promise;
use capnp::Error;
use futures::Future;
use proto::host;
use std::sync::{Arc, Mutex, RwLock};
use tokio::runtime::current_thread;

// Here be dragons ok?

pub static mut global_runtime: Option<Mutex<current_thread::Runtime>> = None;
pub static mut global_host: Option<host::Client> = None;
pub static mut global_session: Option<Arc<RwLock<Session>>> = None;
pub static mut global_hub: Option<Box<Hub>> = None;

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

pub fn hope<F, R, E>(future: F) -> Result<R, E>
where
  F: Future<Item = R, Error = E>,
{
  get_runtime().lock().unwrap().block_on(future)
}

pub fn spawn<F>(f: F)
where
  F: 'static + Future<Item = (), Error = ()>,
{
  get_runtime().lock().unwrap().spawn(f);
}

pub fn get_runtime<'a>() -> &'a mut Mutex<current_thread::Runtime> {
  unsafe { global_runtime.as_mut().unwrap() }
}

pub fn get_host<'a>() -> &'a host::Client {
  unsafe { global_host.as_ref().unwrap() }
}

pub fn set_hub(hub: Box<Hub>) {
  unsafe { global_hub = Some(hub) }
}

pub fn get_hub<'a>() -> &'a Box<Hub> {
  unsafe { global_hub.as_ref().expect("internal error: no hub registered yet") }
}

pub trait Hub {
  fn in_test(&self) -> bool;
  fn register_target(&self);
}
