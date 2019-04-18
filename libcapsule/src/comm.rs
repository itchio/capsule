use futures::Future;
use proto::proto_capnp::host;
use std::sync::Mutex;
use tokio::runtime::current_thread;

// Here be dragons ok?

pub static mut global_runtime: Option<Mutex<current_thread::Runtime>> = None;
pub static mut global_host: Option<host::Client> = None;

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
