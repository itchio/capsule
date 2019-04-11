use libc::{c_int, c_uint, c_void};
use std::mem::transmute;

pub type GLint = c_int;
pub type GLsizei = c_int;
pub type GLenum = c_uint;
pub type GLvoid = c_void;

pub const GL_RGBA: GLenum = 0x1908;
pub const GL_UNSIGNED_BYTE: GLenum = 0x1401;

type GetProcAddress = unsafe fn(gl_func_name: &str) -> *const ();

#[derive(Debug)]
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
}

pub struct CaptureContext<'a> {
    width: i32,
    height: i32,
    pub funcs: &'a GLFunctions,
}

static mut cached_gl_functions: Option<GLFunctions> = None;

unsafe fn get_gl_functions<'a>(getProcAddress: GetProcAddress) -> &'a GLFunctions {
    if cached_gl_functions.is_none() {
        cached_gl_functions = Some(GLFunctions {
            glReadPixels: transmute(getProcAddress("glReadPixels")),
        })
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
