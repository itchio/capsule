use std::mem::transmute;

mod constants;
mod types;
pub use self::constants::*;
pub use self::types::*;

type GetProcAddress = unsafe fn(gl_func_name: &str) -> *const ();

pub struct GLFunctions {
    pub glReadPixels: unsafe extern "C" fn(
        x: GLint,
        y: GLint,
        width: GLsizei,
        height: GLsizei,
        format: GLenum,
        _type: GLenum,
        data: *mut GLvoid,
    ),
    pub glGetIntegerv: unsafe extern "C" fn(pname: GLenum, data: *mut GLint),
}

pub struct CaptureContext<'a> {
    width: GLint,
    height: GLint,
    pub funcs: &'a GLFunctions,
}

static mut cached_gl_functions: Option<GLFunctions> = None;

unsafe fn get_gl_functions<'a>(getProcAddress: GetProcAddress) -> &'a GLFunctions {
    macro_rules! lookup_sym {
        ($name:ident) => {{
            let ptr = getProcAddress(stringify!($name));
            if ptr.is_null() {
                panic!(format!(
                    "{}: null pointer returned by getProcAddress",
                    stringify!($name)
                ))
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
            funcs,
        });
    }
    cached_capture_context.as_ref().unwrap()
}
