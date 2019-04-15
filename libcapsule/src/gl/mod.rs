mod constants;
pub use self::constants::*;

mod types;
pub use self::types::*;

use libc_print::libc_println;
use std::mem;
use std::mem::transmute;
use std::time::SystemTime;

type GetProcAddress = unsafe fn(gl_func_name: &str) -> *const ();

macro_rules! define_gl_functions {
    ($(fn $name:ident($($v: ident : $t:ty),*) -> $r:ty);+) => {
        pub struct Functions {
            $(pub $name: unsafe extern "C" fn($($v: $t),*) -> $r,)+
        }

        pub struct CaptureContext<'a> {
            width: GLint,
            height: GLint,
            start_time: SystemTime,
            pub funcs: &'a Functions,
        }

        impl Functions {
            $(pub unsafe fn $name(&self, $($v: $t),*) -> $r {
                (self.$name)($($v),*)
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

pub unsafe fn get_capture_context<'a>(getProcAddress: GetProcAddress) -> &'a CaptureContext<'a> {
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
        println!("Viewport: {:?}", viewport);

        cached_capture_context = Some(CaptureContext {
            width: viewport.width,
            height: viewport.height,
            start_time: SystemTime::now(),
            funcs,
        });
    }
    cached_capture_context.as_ref().unwrap()
}

impl<'a> CaptureContext<'a> {
    pub fn capture_frame(&self) {
        let elapsed = self.start_time.elapsed().unwrap_or_default();
        libc_println!("[{:?}] Should capture frame...", elapsed);
    }
}
