use std::mem::transmute;

mod constants;
mod types;
pub use self::constants::*;
pub use self::types::*;
use std::time::SystemTime;

type GetProcAddress = unsafe fn(gl_func_name: &str) -> *const ();

macro_rules! define_gl_functions {
    ($(fn $name:ident($($v: ident : $t:ty),*) -> $r:ty);+) => {
        pub struct GLFunctions {
            $(pub $name: unsafe extern "C" fn($($v: $t),*) -> $r,)+
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

pub struct CaptureContext<'a> {
    width: GLint,
    height: GLint,
    start_time: SystemTime,
    pub funcs: &'a GLFunctions,
}

impl<'a> CaptureContext<'a> {
    // manually written for now, should be auto-generated
    pub unsafe fn glGetIntegerv(&self, pname: GLenum, data: *mut GLint) {
        (self.funcs.glGetIntegerv)(pname, data)
    }
}

static mut cached_gl_functions: Option<GLFunctions> = None;

unsafe fn get_gl_functions<'a>(getProcAddress: GetProcAddress) -> &'a GLFunctions {
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
            Some(GLFunctions{
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
        cached_capture_context = Some(CaptureContext {
            width: 400,
            height: 400,
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
