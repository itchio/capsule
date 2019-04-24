use const_cstr::const_cstr;
use libc;
use log::*;
use simple_error::SimpleError;
use std::ffi::CStr;
use std::ffi::CString;
use std::fs::File;
use std::ptr;

pub struct Context {
  c: *mut ffrust::AVCodecContext,
  frame: *mut ffrust::AVFrame,
  pkt: *mut ffrust::AVPacket,
  file: File,
}

impl Context {
  pub unsafe fn new(
    file_name: &str,
    width: u32,
    height: u32,
  ) -> Result<Self, Box<std::error::Error>> {
    let codec_name = "libx264";
    let codec = ffrust::avcodec_find_encoder_by_name(CString::new(codec_name)?.as_ptr());
    if codec.is_null() {
      return Err(Box::new(SimpleError::new("Codec not found")));
    }

    let c = ffrust::avcodec_alloc_context3(codec);
    if c.is_null() {
      return Err(Box::new(SimpleError::new(
        "Could not allocate video codec context",
      )));
    }

    {
      let c = &mut *c;

      warn!("width = {}, height = {}", width, height);
      c.width = width as i32;
      c.height = height as i32;
      c.time_base = ffrust::AVRational { num: 1, den: 1000 };
      c.framerate = ffrust::AVRational { num: 1, den: 60 };
      c.pix_fmt = ffrust::AVPixelFormat::AV_PIX_FMT_YUV420P;
      ffrust::av_opt_set(
        c.priv_data,
        const_cstr!("preset").as_ptr(),
        const_cstr!("ultrafast").as_ptr(),
        0,
      );
    }

    warn!("about to call avcodec_open2");
    let ret = ffrust::avcodec_open2(c, codec, ptr::null_mut());
    if ret < 0 {
      warn!("ret = {}", ret);
      return Err(Box::new(SimpleError::new(format!(
        "Could not open codec: {}",
        err2str(ret)
      ))));
    }

    let frame = ffrust::av_frame_alloc();
    if frame.is_null() {
      return Err(Box::new(SimpleError::new("Could not allocate video frame")));
    }

    let pkt = ffrust::av_packet_alloc();
    if pkt.is_null() {
      return Err(Box::new(SimpleError::new("Could not allocate packet")));
    }

    let file = File::create(file_name)?;
    Ok(Self {
      c,
      frame,
      pkt,
      file,
    })
  }

  pub fn write_frame(
    &self,
    data: &[u8],
    timestamp: std::time::Duration,
  ) -> Result<(), Box<std::error::Error>> {
    info!("Should write frame");
    Ok(())
  }
}

fn err2str(errnum: libc::c_int) -> String {
  unsafe {
    const BUFCAP: usize = 1024;
    let mut buf = [0i8; BUFCAP];
    ffrust::av_strerror(errnum, &mut buf[0], BUFCAP as usize) as usize;
    CStr::from_ptr(&mut buf[0])
      .to_string_lossy()
      .to_owned()
      .to_string()
  }
}
