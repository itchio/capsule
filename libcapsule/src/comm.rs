use futures::Future;
use proto::proto_capnp::host;
use tokio::runtime::current_thread;

// Here be dragons ok?

pub static mut global_runtime: Option<current_thread::Runtime> = None;
pub static mut global_host: Option<host::Client> = None;

pub fn hope<F, R, E>(future: F) -> Result<R, E>
where
  F: Future<Item = R, Error = E>,
{
  get_runtime().block_on(future)
}

pub fn get_runtime<'a>() -> &'a mut current_thread::Runtime {
  unsafe { global_runtime.as_mut().unwrap() }
}

pub fn get_host<'a>() -> &'a host::Client {
  unsafe { global_host.as_ref().unwrap() }
}
