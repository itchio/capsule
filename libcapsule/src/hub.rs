use capnp::capability::Promise;
use capnp::Error;
use capnp_rpc::pry;
use comm::*;
use futures::Future;
use log::*;
use proto::host::*;
use std::sync::{Arc, Mutex, RwLock};
use tokio::runtime::current_thread::Runtime;

struct TargetImpl;
impl TargetImpl {
  fn new() -> Self {
    Self {}
  }
}

impl target::Server for TargetImpl {
  fn start_capture(
    &mut self,
    params: target::StartCaptureParams,
    mut results: target::StartCaptureResults,
  ) -> Promise<(), Error> {
    let params = pry!(params.get());
    let sink = pry!(params.get_sink());

    info!("Starting capture into a sink!");
    let session = comm::Session {
      sink: sink,
      alive: true,
    };
    let session_ref = Arc::new(RwLock::new(session));
    let session_impl = comm::SessionImpl {
      session: session_ref.clone(),
    };

    let info = results.get().init_info();

    if let Some(vs) = get_hub().get_video_source() {
      let mut video = info.init_video();
      video.set_width(vs.width);
      video.set_height(vs.height);
      video.set_pitch(vs.pitch);
      video.set_vertical_flip(vs.vflip);
    } else {
      return Promise::err(capnp::Error {
        kind: capnp::ErrorKind::Failed,
        description: "No viable video context!".to_owned(),
      });
    }

    results
      .get()
      .set_session(session::ToClient::new(session_impl).into_client::<::capnp_rpc::Server>());

    comm::set_session(session_ref);
    Promise::ok(())
  }
}

pub struct GlobalHub {
  runtime: Mutex<Runtime>,
  host: proto::host::Client,
  session: Option<Arc<RwLock<Session>>>,

  video: Option<VideoSource>,
  audio: Option<AudioSource>,
}

pub fn new_hub(runtime: Runtime, host: proto::host::Client) -> GlobalHub {
  GlobalHub {
    runtime: Mutex::new(runtime),
    host,
    session: None,

    video: None,
    audio: None,
  }
}

impl GlobalHub {
  fn register_target(&mut self) {
    let target = target::ToClient::new(TargetImpl::new()).into_client::<::capnp_rpc::Server>();
    let mut req = self.host.register_target_request();
    let mut info = req.get().init_info();
    info.set_pid(std::process::id() as u64);
    info.set_exe(std::env::current_exe().unwrap().to_string_lossy().as_ref());
    req.get().set_target(target);
    self
      .runtime()
      .lock()
      .unwrap()
      .block_on(req.send().promise.and_then(|response| {
        pry!(response.get());
        Promise::ok(())
      }))
      .unwrap();
  }
}

impl Hub for GlobalHub {
  fn runtime<'a>(&'a mut self) -> &'a mut Mutex<Runtime> {
    &mut self.runtime
  }

  fn session<'a>(&'a mut self) -> Option<&'a Arc<RwLock<Session>>> {
    let stopped = if let Some(s) = self.session.as_ref() {
      !s.read().unwrap().alive
    } else {
      false
    };

    if stopped {
      info!("Capture session was stopped");
      self.session = None;
    }
    self.session.as_ref()
  }

  fn in_test(&self) -> bool {
    super::SETTINGS.in_test
  }

  fn set_video_source(&mut self, s: VideoSource) {
    info!(
      "Acquired video source: {:#?}, {}x{} (pitch {}, vflip {})",
      s.kind, s.width, s.height, s.pitch, s.vflip
    );
    if self.video.replace(s).is_none() {
      self.register_target();
    }
  }

  fn get_video_source(&self) -> Option<&VideoSource> {
    self.video.as_ref()
  }

  fn set_audio_source(&mut self, s: AudioSource) {
    self.audio = Some(s)
  }

  fn get_audio_source(&self) -> Option<&AudioSource> {
    self.audio.as_ref()
  }

  fn hotkey_pressed(&mut self) {
    let req = self.host.hotkey_pressed_request();
    self
      .runtime()
      .lock()
      .unwrap()
      .block_on(req.send().promise.and_then(|response| {
        pry!(response.get());
        Promise::ok(())
      }))
      .unwrap();
  }
}
