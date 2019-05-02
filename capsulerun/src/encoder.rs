use capnp::capability::Response;
use const_cstr::const_cstr;
use libc;
use log::*;
use proto::host;
use simple_error::SimpleError;
use std::ffi::CStr;
use std::ffi::CString;
use std::fs::File;
use std::io::Write;
use std::ptr;
use std::sync::Arc;

pub struct Context {
  c: *mut ffrust::AVCodecContext,
  frame: *mut ffrust::AVFrame,
  pkt: *mut ffrust::AVPacket,
  sws: *mut ffrust::SwsContext,
  file: File,
  info: Arc<Response<host::target::start_capture_results::Owned>>,
}

impl Drop for Context {
  fn drop(&mut self) {
    info!("Dropping context");
    unsafe {
      self.close().unwrap();
    }
  }
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
    info: Arc<Response<host::target::start_capture_results::Owned>>,
  ) -> Result<Self, Box<std::error::Error>> {
    let (width, height) = {
      let v = info.get()?.get_info()?.get_video()?;
      (v.get_width(), v.get_height())
    };

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

    let sws = {
      let c = &*c;
      ffrust::sws_getContext(
        c.width, // input
        c.height,
        ffrust::AVPixelFormat::AV_PIX_FMT_BGRA,
        c.width, // output
        c.height,
        c.pix_fmt,
        0,               // flags
        ptr::null_mut(), // in filter
        ptr::null_mut(), // out filter
        ptr::null_mut(), // param
      )
    };
    if sws.is_null() {
      return Err(Box::new(SimpleError::new(
        "Could not initialize scaling context",
      )));
    }

    let file = File::create(file_name)?;
    Ok(Self {
      c,
      frame,
      pkt,
      sws,
      file,
      info,
    })
  }

  fn video(&self) -> Result<host::session::info::video::Reader, Box<std::error::Error>> {
    Ok(self.info.get()?.get_info()?.get_video()?)
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

      let mut sws_linesize: [libc::c_int; 1] = std::mem::zeroed();
      let mut sws_in: [*const u8; 1] = std::mem::zeroed();
      let (linesize, vflip) = {
        let v = self.video()?;
        (v.get_pitch() as i32, v.get_vertical_flip())
      };

      if vflip {
        sws_in[0] = &data[(linesize * (c.height - 1)) as usize];
        sws_linesize[0] = -linesize;
      } else {
        sws_in[0] = &data[0];
        sws_linesize[0] = linesize;
      }

      // this returns the height of the output slice
      ffrust::sws_scale(
        self.sws,
        &sws_in[0],
        &sws_linesize[0],
        0,
        c.height,
        &frame.data[0],
        &frame.linesize[0],
      );

      frame.pts = timestamp.as_millis() as i64;
      info!("PTS = {}", frame.pts);
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

  unsafe fn close(&mut self) -> Result<(), Box<std::error::Error>> {
    info!("Flushing...");
    self.encode(ptr::null_mut())?;

    info!("Freeing FFmpeg resources...");
    ffrust::sws_freeContext(self.sws);
    self.sws = ptr::null_mut();
    ffrust::avcodec_free_context(&mut self.c);
    ffrust::av_frame_free(&mut self.frame);
    ffrust::av_packet_free(&mut self.pkt);

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
