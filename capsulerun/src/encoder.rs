use log::*;
use simple_error::SimpleError;
use std::ffi::CString;
use std::fs::File;

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

    // TODO: set codec options

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
