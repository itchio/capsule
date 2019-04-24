use const_cstr::const_cstr;
use libc;
use log::*;
use simple_error::SimpleError;
use std::ffi::CStr;
use std::ffi::CString;
use std::fs::File;
use std::io::Write;
use std::ptr;

pub struct Context {
  c: *mut ffrust::AVCodecContext,
  frame: *mut ffrust::AVFrame,
  pkt: *mut ffrust::AVPacket,
  file: File,
}

macro_rules! fferr {
  ($ret:ident, $msg:literal) => {
    if $ret < 0 {
      return Err(Box::new(SimpleError::new(format!(
        "{}: {}",
        $msg,
        err2str($ret)
      ))));
    }
  };
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

      c.width = width as i32;
      c.height = height as i32;
      c.time_base = ffrust::AVRational { num: 1, den: 1000 };
      c.framerate = ffrust::AVRational { num: 60, den: 1 };
      c.pix_fmt = ffrust::AVPixelFormat::AV_PIX_FMT_YUV420P;
      ffrust::av_opt_set(
        c.priv_data,
        const_cstr!("preset").as_ptr(),
        const_cstr!("ultrafast").as_ptr(),
        0,
      );
    }

    let ret = ffrust::avcodec_open2(c, codec, ptr::null_mut());
    fferr!(ret, "Could not open codec");

    let frame = ffrust::av_frame_alloc();
    if frame.is_null() {
      return Err(Box::new(SimpleError::new("Could not allocate video frame")));
    }

    {
      let frame = &mut *frame;
      let c = &*c;

      frame.format = c.pix_fmt as i32;
      frame.width = c.width;
      frame.height = c.height;
    }

    let ret = ffrust::av_frame_get_buffer(frame, 32);
    fferr!(ret, "Could not allocate video frame");

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

  pub unsafe fn write_frame(
    &mut self,
    data: &[u8],
    timestamp: std::time::Duration,
  ) -> Result<(), Box<std::error::Error>> {
    {
      let c = &mut *self.c;
      let frame = &mut *self.frame;

      let ret = ffrust::av_frame_make_writable(frame);
      fferr!(ret, "Could not make frame writable");

      let linesize = c.width * 4;
      for y in 0..c.height {
        for x in 0..c.width {
          let i = ((c.height - 1 - y) * linesize + x * 4) as usize;
          let b = data[i];
          let g = data[i + 1];
          let r = data[i + 2];
          let mean = (b as i32 + g as i32 + r as i32) / 3;

          // Y
          *frame.data[0].offset((y * frame.linesize[0] + x) as isize) = mean as u8;
        }
      }

      frame.pts = timestamp.as_millis() as i64;
    }

    self.encode(self.frame)?;

    Ok(())
  }

  unsafe fn encode(&mut self, frame: *mut ffrust::AVFrame) -> Result<(), Box<std::error::Error>> {
    let enc_ctx = self.c;
    let pkt = self.pkt;
    let file = &mut self.file;

    // send the frame to the encoder
    let mut ret = ffrust::avcodec_send_frame(enc_ctx, frame);
    fferr!(ret, "Error sending a frame for encoding");

    while ret >= 0 {
      ret = ffrust::avcodec_receive_packet(enc_ctx, pkt);
      if ret == ffrust::AVERROR(libc::EAGAIN) || ret == ffrust::AVERROR_EOF {
        return Ok(());
      } else if ret < 0 {
        fferr!(ret, "Error during encoding");
      }

      {
        let pkt = &*pkt;
        let data = std::slice::from_raw_parts(pkt.data, pkt.size as usize);
        file.write(data)?;
      }
    }

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
