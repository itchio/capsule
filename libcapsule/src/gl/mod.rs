mod constants;
pub use self::constants::*;

mod types;
pub use self::types::*;

use crate::comm::*;
use log::*;
use std::mem;
use std::mem::transmute;
use std::time::SystemTime;

type GetProcAddress = unsafe fn(gl_func_name: &str) -> *const ();

macro_rules! define_gl_functions {
    ($(fn $name:ident($($v:ident : $t:ty),*) -> $r:ty);+) => {
        #[cfg(target_os = "windows")]
        define_gl_functions_with_convention! {
            extern "system" => {
                $(fn $name($($v : $t),*) -> $r);+
            }
        }
        #[cfg(not(target_os = "windows"))]
        define_gl_functions_with_convention! {
            extern "C" => {
                $(fn $name($($v : $t),*) -> $r);+
            }
        }
    };
}

macro_rules! define_gl_functions_with_convention {
    (extern $convention:tt => { $(fn $name:ident($($v:ident : $t:ty),*) -> $r:ty);+ }) => {
        pub struct Functions {
            $(pub $name: unsafe extern $convention fn($($v: $t),*) -> $r,)+
        }

        #[allow(unused)]
        pub struct CaptureContext<'a> {
            width: GLint,
            height: GLint,
            frame_number: u64,
            start_time: SystemTime,
            pub funcs: &'a Functions,
        }

        impl Functions {
            #[allow(unused)]
            $(pub fn $name(&self, $($v: $t),*) -> $r {
                unsafe { (self.$name)($($v),*) }
            })+
        }
    };
}

define_gl_functions! {
     fn glReadPixels(
         x: GLint,
         y: GLint,
         width: GLsizei,
         height: GLsizei,
         format: GLenum,
         _type: GLenum,
         data: *mut GLvoid
     ) -> ();
     fn glGetIntegerv(pname: GLenum, data: *mut GLint) -> ()
}

static mut cached_gl_functions: Option<Functions> = None;

unsafe fn get_gl_functions<'a>(getProcAddress: GetProcAddress) -> &'a Functions {
    macro_rules! lookup_sym {
        ($name:ident) => {{
            let ptr = getProcAddress(stringify!($name));
            if ptr.is_null() {
                panic!(
                    "{}: null pointer returned by getProcAddress",
                    stringify!($name)
                )
            }
            transmute(ptr)
        }};
    }
    macro_rules! bind {
        ($($name:ident),+,) => {
            Some(Functions{
                $($name: lookup_sym!($name)),*
            })
        };
    }

    if cached_gl_functions.is_none() {
        cached_gl_functions = bind! {
            glReadPixels,
            glGetIntegerv,
        }
    }
    cached_gl_functions.as_ref().unwrap()
}

static mut cached_capture_context: Option<CaptureContext> = None;

pub fn get_capture_context<'a>(getProcAddress: GetProcAddress) -> &'static mut CaptureContext<'a> {
    unsafe {
        if cached_capture_context.is_none() {
            let funcs = get_gl_functions(getProcAddress);
            #[repr(C)]
            #[derive(Debug)]
            struct Viewport {
                x: GLint,
                y: GLint,
                width: GLint,
                height: GLint,
            };
            let viewport = mem::zeroed::<Viewport>();
            funcs.glGetIntegerv(GL_VIEWPORT, mem::transmute(&viewport));
            info!("Viewport: {:?}", viewport);

            cached_capture_context = Some(CaptureContext {
                width: viewport.width,
                height: viewport.height,
                start_time: SystemTime::now(),
                frame_number: 0,
                funcs,
            });
        }
        cached_capture_context.as_mut().unwrap()
    }
}

impl<'a> CaptureContext<'a> {
    pub fn capture_frame(&mut self) {
        unsafe {
            if let Some(session) = global_session.as_ref() {
                let mut req = {
                    let session = session.read().unwrap();
                    if !session.alive {
                        info!("Capture session was stopped");
                        global_session = None;
                        return;
                    }

                    session.sink.send_video_frame_request()
                };
                let mut frame = req.get().init_frame();
                let mut timestamp = frame.reborrow().init_timestamp();
                let elapsed = self.start_time.elapsed().unwrap();
                timestamp.set_millis(elapsed.as_millis() as f64);
                frame.set_index(self.frame_number);

                let Self {
                    funcs,
                    width,
                    height,
                    ..
                } = self;
                let (width, height) = (*width, *height);

                let x: GLint = 0;
                let y: GLint = 0;
                let bytes_per_pixel: usize = 4;
                let bytes_per_frame: usize = width as usize * height as usize * bytes_per_pixel;

                let mut frame_buffer = Vec::<u8>::new();
                frame_buffer.resize(bytes_per_frame, 0);

                funcs.glReadPixels(
                    x,
                    y,
                    width,
                    height,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    std::mem::transmute(frame_buffer.as_ptr()),
                );

                frame.set_data(&frame_buffer[..]);
                hope(req.send().promise).unwrap();
            }
        }
        self.frame_number += 1;
    }
}
